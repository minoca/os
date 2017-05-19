/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    userio.c

Abstract:

    This module implements support for interfacing the I/O subsystem with
    user mode.

Author:

    Evan Green 25-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// --------------------------------------------------------------------- Macros
//

#define ASSERT_SYS_OPEN_FLAGS_EQUIVALENT() \
    ASSERT((SYS_OPEN_FLAG_CREATE == OPEN_FLAG_CREATE) && \
           (SYS_OPEN_FLAG_TRUNCATE == OPEN_FLAG_TRUNCATE) && \
           (SYS_OPEN_FLAG_FAIL_IF_EXISTS == OPEN_FLAG_FAIL_IF_EXISTS) && \
           (SYS_OPEN_FLAG_APPEND == OPEN_FLAG_APPEND) && \
           (SYS_OPEN_FLAG_DIRECTORY == OPEN_FLAG_DIRECTORY) && \
           (SYS_OPEN_FLAG_NON_BLOCKING == OPEN_FLAG_NON_BLOCKING) && \
           (SYS_OPEN_FLAG_SHARED_MEMORY == OPEN_FLAG_SHARED_MEMORY) && \
           (SYS_OPEN_FLAG_NO_SYMBOLIC_LINK == OPEN_FLAG_NO_SYMBOLIC_LINK) && \
           (SYS_OPEN_FLAG_SYNCHRONIZED == OPEN_FLAG_SYNCHRONIZED) && \
           (SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL == \
            OPEN_FLAG_NO_CONTROLLING_TERMINAL) && \
           (SYS_OPEN_FLAG_NO_ACCESS_TIME == OPEN_FLAG_NO_ACCESS_TIME)  && \
           (SYS_OPEN_FLAG_ASYNCHRONOUS == OPEN_FLAG_ASYNCHRONOUS))

//
// ---------------------------------------------------------------- Definitions
//

#define CLOSE_EXECUTE_HANDLE_INITIAL_ARRAY_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context during a copy of a handle table.

Members:

    DestinationTable - Stores a pointer to the destination handle table.

    SourceTable - Stores a pointer to the handle table to be copied.

    Status - Stores the current status of the copy operation. Initialize to
        STATUS_SUCCESS.

--*/

typedef struct _COPY_HANDLES_ITERATION_CONTEXT {
    PHANDLE_TABLE DestinationTable;
    PHANDLE_TABLE SourceTable;
    KSTATUS Status;
} COPY_HANDLES_ITERATION_CONTEXT, *PCOPY_HANDLES_ITERATION_CONTEXT;

/*++

Structure Description:

    This structure stores context during a copy of a handle table.

Members:

    HandleArray - Stores the array of handles to close, allocated in paged pool.

    HandleArraySize - Stores the number of valid entries in the array.

    HandleArrayCapacity - Stores the maximum number of elements in the handle
        array.

    Status - Stores the current status of the iteration operation. Initialize to
        STATUS_SUCCESS.

--*/

typedef struct _CLOSE_EXECUTE_HANDLES_CONTEXT {
    PHANDLE HandleArray;
    ULONG HandleArraySize;
    ULONG HandleArrayCapacity;
    KSTATUS Status;
} CLOSE_EXECUTE_HANDLES_CONTEXT, *PCLOSE_EXECUTE_HANDLES_CONTEXT;

/*++

Structure Description:

    This structure stores context during a check for open directory handles.

Members:

    Handle - Stores a handle to a directory that is to be excluded from the
        check.

    Status - Stores the current status of the check operation. Initialize to
        STATUS_SUCCESS.

--*/

typedef struct _CHECK_FOR_DIRECTORY_HANDLES_CONTEXT {
    HANDLE Handle;
    KSTATUS Status;
} CHECK_FOR_DIRECTORY_HANDLES_CONTEXT, *PCHECK_FOR_DIRECTORY_HANDLES_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopSysClose (
    PKPROCESS Process,
    HANDLE Handle
    );

VOID
IopCopyHandleIterateRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    );

VOID
IopCloseExecuteHandleIterateRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    );

VOID
IopCheckForDirectoryHandlesIterationRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    );

KSTATUS
IopHandleCommonUserControl (
    PIO_HANDLE Handle,
    HANDLE Descriptor,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INTN
IoSysOpen (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine opens a file or other I/O object on behalf of a user mode
    application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG Access;
    PKPROCESS CurrentProcess;
    PIO_HANDLE Directory;
    PSTR FileName;
    ULONG FileNameLength;
    PIO_HANDLE Handle;
    ULONG HandleFlags;
    ULONG OpenFlags;
    PSYSTEM_CALL_OPEN Parameters;
    KSTATUS Status;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    Directory = NULL;
    Handle = NULL;
    Parameters = (PSYSTEM_CALL_OPEN)SystemCallParameter;
    FileName = NULL;
    FileNameLength = Parameters->PathBufferLength;
    Parameters->Handle = INVALID_HANDLE;
    Status = MmCreateCopyOfUserModeString(Parameters->Path,
                                          FileNameLength,
                                          FI_ALLOCATION_TAG,
                                          &FileName);

    if (!KSUCCESS(Status)) {
        goto SysOpenEnd;
    }

    //
    // Set up the flags.
    //

    ASSERT_SYS_OPEN_FLAGS_EQUIVALENT();

    Access = (Parameters->Flags >> SYS_OPEN_ACCESS_SHIFT) & IO_ACCESS_MASK;
    OpenFlags = Parameters->Flags & SYS_OPEN_FLAG_MASK;
    if (Parameters->Directory != INVALID_HANDLE) {
        Directory = ObGetHandleValue(CurrentProcess->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysOpenEnd;
        }
    }

    //
    // Open up the file.
    //

    Status = IoOpen(FALSE,
                    Directory,
                    FileName,
                    FileNameLength,
                    Access,
                    OpenFlags,
                    Parameters->CreatePermissions,
                    &Handle);

    if (!KSUCCESS(Status)) {
        goto SysOpenEnd;
    }

    //
    // Create a handle table entry for this open file.
    //

    HandleFlags = 0;
    if ((Parameters->Flags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    Status = ObCreateHandle(CurrentProcess->HandleTable,
                            Handle,
                            HandleFlags,
                            &(Parameters->Handle));

    if (!KSUCCESS(Status)) {
        goto SysOpenEnd;
    }

    Status = STATUS_SUCCESS;

SysOpenEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (FileName != NULL) {
        MmFreePagedPool(FileName);
    }

    if (!KSUCCESS(Status)) {
        if (Handle != NULL) {
            IoClose(Handle);
        }

        //
        // Open is allowed to restart if interrupted and the signal handler
        // allows restarts.
        //

        if (Status == STATUS_INTERRUPTED) {
            Status = STATUS_RESTART_AFTER_SIGNAL;
        }
    }

    return Status;
}

INTN
IoSysOpenDevice (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine opens a direct handle to a device on behalf of a user mode
    application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG Access;
    PKPROCESS CurrentProcess;
    PDEVICE Device;
    PIO_HANDLE Handle;
    ULONG HandleFlags;
    ULONG OpenFlags;
    PSYSTEM_CALL_OPEN_DEVICE Parameters;
    KSTATUS Status;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    Handle = NULL;
    Parameters = (PSYSTEM_CALL_OPEN_DEVICE)SystemCallParameter;
    Parameters->Handle = INVALID_HANDLE;

    //
    // Set up the flags.
    //

    ASSERT_SYS_OPEN_FLAGS_EQUIVALENT();

    Access = (Parameters->Flags >> SYS_OPEN_ACCESS_SHIFT) & IO_ACCESS_MASK;
    OpenFlags = Parameters->Flags & SYS_OPEN_FLAG_MASK;

    //
    // Look up the device.
    //

    Device = IoGetDeviceByNumericId(Parameters->DeviceId);
    if (Device == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SysOpenDeviceEnd;
    }

    //
    // Open up the device.
    //

    Status = IoOpenDevice(Device, Access, OpenFlags, &Handle, NULL, NULL, NULL);
    ObReleaseReference(Device);
    if (!KSUCCESS(Status)) {
        goto SysOpenDeviceEnd;
    }

    //
    // Create a handle table entry for this open device.
    //

    HandleFlags = 0;
    if ((Parameters->Flags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    Status = ObCreateHandle(CurrentProcess->HandleTable,
                            Handle,
                            HandleFlags,
                            &(Parameters->Handle));

    if (!KSUCCESS(Status)) {
        goto SysOpenDeviceEnd;
    }

    Status = STATUS_SUCCESS;

SysOpenDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Handle != NULL) {
            IoClose(Handle);
        }
    }

    return Status;
}

INTN
IoSysClose (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine closes an I/O handle opened in user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the user mode handle returned during the
        open system call. It is passed to the kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    return IopSysClose(CurrentProcess, (HANDLE)SystemCallParameter);
}

INTN
IoSysPerformIo (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine performs I/O for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of bytes completed (a positive integer) on
    success.

    Error status code (a negative integer) on failure.

--*/

{

    UINTN BytesCompleted;
    PKPROCESS CurrentProcess;
    PIO_HANDLE HandleValue;
    IO_BUFFER IoBuffer;
    PSYSTEM_CALL_PERFORM_IO Parameters;
    INTN Result;
    INTN Size;
    KSTATUS Status;
    ULONG Timeout;

    CurrentProcess = PsGetCurrentProcess();
    Parameters = (PSYSTEM_CALL_PERFORM_IO)SystemCallParameter;
    Size = Parameters->Size;
    BytesCompleted = 0;
    HandleValue = ObGetHandleValue(CurrentProcess->HandleTable,
                                   Parameters->Handle,
                                   NULL);

    if (HandleValue == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysPerformIoEnd;
    }

    //
    // The proper system call interface doesn't pass negative values, but
    // treat them the same as zero if they find a way through.
    //

    if (Size <= 0) {
        Status = STATUS_SUCCESS;
        goto SysPerformIoEnd;
    }

    if ((Parameters->Buffer + Size > KERNEL_VA_START) ||
        (Parameters->Buffer + Size < Parameters->Buffer)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysPerformIoEnd;
    }

    Timeout = Parameters->TimeoutInMilliseconds;

    ASSERT(SYS_WAIT_TIME_INDEFINITE == WAIT_TIME_INDEFINITE);

    //
    // Hopefully this I/O buffer will never reach a driver and only be used by
    // the cache. As such, don't pin down the pages just yet, allowing the
    // opportunity to stack-allocate the I/O buffer structure. If this buffer
    // does make it to a driver, a new I/O buffer structure will be temporarily
    // allocated to pin down the pages.
    //

    Status = MmInitializeIoBuffer(&IoBuffer,
                                  Parameters->Buffer,
                                  INVALID_PHYSICAL_ADDRESS,
                                  Size,
                                  0);

    if (!KSUCCESS(Status)) {
        goto SysPerformIoEnd;
    }

    //
    // Perform the file I/O.
    //

    if ((Parameters->Flags & SYS_IO_FLAG_WRITE) != 0) {
        Status = IoWriteAtOffset(HandleValue,
                                 &IoBuffer,
                                 Parameters->Offset,
                                 Size,
                                 0,
                                 Timeout,
                                 &BytesCompleted,
                                 NULL);

        if (Status == STATUS_BROKEN_PIPE) {

            ASSERT(CurrentProcess != PsGetKernelProcess());

            PsSignalProcess(CurrentProcess, SIGNAL_BROKEN_PIPE, NULL);
        }

    } else {
        Status = IoReadAtOffset(HandleValue,
                                &IoBuffer,
                                Parameters->Offset,
                                Size,
                                0,
                                Timeout,
                                &BytesCompleted,
                                NULL);
    }

    if (!KSUCCESS(Status)) {
        goto SysPerformIoEnd;
    }

SysPerformIoEnd:
    if (HandleValue != NULL) {
        IoIoHandleReleaseReference(HandleValue);
    }

    //
    // If the I/O got interrupted and no bytes were transferred, then the
    // system call can be restarted if the signal handler allows. If bytes were
    // transferred, convert to a success status.
    //

    if (Status == STATUS_INTERRUPTED) {
        if (BytesCompleted == 0) {
            Status = STATUS_RESTART_AFTER_SIGNAL;

        } else {
            Status = STATUS_SUCCESS;
        }
    }

    Result = Status;
    if (KSUCCESS(Status) ||
        (Status == STATUS_MORE_PROCESSING_REQUIRED) ||
        ((Status == STATUS_TIMEOUT) && (BytesCompleted != 0))) {

        //
        // The internal APIs allow UINTN sizes, but the system call size was
        // limited to MAX_INTN. The bytes completed should never exceed the
        // maximum supplied size.
        //

        ASSERT(BytesCompleted <= (UINTN)MAX_INTN);

        Result = (INTN)BytesCompleted;
    }

    return Result;
}

INTN
IoSysPerformVectoredIo (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine performs vectored I/O for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of bytes completed (a positive integer) on
    success.

    Error status code (a negative integer) on failure.

--*/

{

    UINTN BytesCompleted;
    PKPROCESS CurrentProcess;
    PIO_HANDLE HandleValue;
    PIO_BUFFER IoBuffer;
    PSYSTEM_CALL_PERFORM_VECTORED_IO Parameters;
    INTN Result;
    INTN Size;
    KSTATUS Status;
    ULONG Timeout;

    CurrentProcess = PsGetCurrentProcess();
    Parameters = (PSYSTEM_CALL_PERFORM_VECTORED_IO)SystemCallParameter;
    Size = Parameters->Size;
    BytesCompleted = 0;
    IoBuffer = NULL;
    HandleValue = ObGetHandleValue(CurrentProcess->HandleTable,
                                   Parameters->Handle,
                                   NULL);

    if (HandleValue == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysPerformVectoredIoEnd;
    }

    //
    // The proper system call interface doesn't pass negative values, but
    // treat them the same as zero if they find a way through.
    //

    if (Size <= 0) {
        Status = STATUS_SUCCESS;
        goto SysPerformVectoredIoEnd;
    }

    Timeout = Parameters->TimeoutInMilliseconds;

    ASSERT(SYS_WAIT_TIME_INDEFINITE == WAIT_TIME_INDEFINITE);

    //
    // Allocate an I/O buffer for this user mode buffer. Keep it in paged-pool,
    // and not pinned for now. If the particular I/O requests something more
    // serious, it will lock the buffer.
    //

    Status = MmCreateIoBufferFromVector(Parameters->VectorArray,
                                        FALSE,
                                        Parameters->VectorCount,
                                        &IoBuffer);

    if (!KSUCCESS(Status)) {
        goto SysPerformVectoredIoEnd;
    }

    //
    // Perform the file I/O.
    //

    if ((Parameters->Flags & SYS_IO_FLAG_WRITE) != 0) {
        Status = IoWriteAtOffset(HandleValue,
                                 IoBuffer,
                                 Parameters->Offset,
                                 (UINTN)Size,
                                 0,
                                 Timeout,
                                 &BytesCompleted,
                                 NULL);

        if (Status == STATUS_BROKEN_PIPE) {

            ASSERT(CurrentProcess != PsGetKernelProcess());

            PsSignalProcess(CurrentProcess, SIGNAL_BROKEN_PIPE, NULL);
        }

    } else {
        Status = IoReadAtOffset(HandleValue,
                                IoBuffer,
                                Parameters->Offset,
                                (UINTN)Size,
                                0,
                                Timeout,
                                &BytesCompleted,
                                NULL);
    }

    if (!KSUCCESS(Status)) {
        goto SysPerformVectoredIoEnd;
    }

SysPerformVectoredIoEnd:
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    if (HandleValue != NULL) {
        IoIoHandleReleaseReference(HandleValue);
    }

    //
    // If the I/O got interrupted and no bytes were transferred, then the
    // system call can be restarted if the signal handler allows. If bytes were
    // transferred, convert to a success status.
    //

    if (Status == STATUS_INTERRUPTED) {
        if (BytesCompleted == 0) {
            Status = STATUS_RESTART_AFTER_SIGNAL;

        } else {
            Status = STATUS_SUCCESS;
        }
    }

    Result = Status;
    if (KSUCCESS(Status) ||
        (Status == STATUS_MORE_PROCESSING_REQUIRED) ||
        ((Status == STATUS_TIMEOUT) && (BytesCompleted != 0))) {

        //
        // The internal APIs allow UINTN sizes, but the system call size was
        // limited to MAX_INTN. The bytes completed should never exceed the
        // maximum supplied size.
        //

        ASSERT(BytesCompleted <= (UINTN)MAX_INTN);

        Result = (INTN)BytesCompleted;
    }

    return Result;
}

INTN
IoSysFlush (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine flushes data to its backing device for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    ULONG FlushFlags;
    PIO_HANDLE HandleValue;
    PSYSTEM_CALL_FLUSH Parameters;
    KSTATUS Status;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    FlushFlags = 0;
    Parameters = (PSYSTEM_CALL_FLUSH)SystemCallParameter;
    if ((Parameters->Flags & SYS_FLUSH_FLAG_ALL) != 0) {
        FlushFlags = FLUSH_FLAG_ALL;
        HandleValue = NULL;

    } else {
        HandleValue = ObGetHandleValue(CurrentProcess->HandleTable,
                                       Parameters->Handle,
                                       NULL);

        if (HandleValue == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysFlushEnd;
        }

        ASSERT(HandleValue != INVALID_HANDLE);

        if ((Parameters->Flags & SYS_FLUSH_FLAG_READ) != 0) {
            FlushFlags |= FLUSH_FLAG_READ;
        }

        if ((Parameters->Flags & SYS_FLUSH_FLAG_WRITE) != 0) {
            FlushFlags |= FLUSH_FLAG_WRITE;
        }

        if ((Parameters->Flags & SYS_FLUSH_FLAG_DISCARD) != 0) {
            FlushFlags |= FLUSH_FLAG_DISCARD;
        }
    }

    Status = IoFlush(HandleValue, 0, -1, FlushFlags);
    if (!KSUCCESS(Status)) {
        goto SysFlushEnd;
    }

SysFlushEnd:
    if (HandleValue != NULL) {
        IoIoHandleReleaseReference(HandleValue);
    }

    return Status;
}

INTN
IoSysCreatePipe (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine creates a pipe on behalf of a user mode application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    PIO_HANDLE Directory;
    ULONG HandleFlags;
    ULONG OpenFlags;
    PSYSTEM_CALL_CREATE_PIPE Parameters;
    PSTR PipePath;
    PIO_HANDLE ReadHandle;
    KSTATUS Status;
    PIO_HANDLE WriteHandle;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    Directory = NULL;
    Parameters = (PSYSTEM_CALL_CREATE_PIPE)SystemCallParameter;
    Parameters->ReadHandle = INVALID_HANDLE;
    Parameters->WriteHandle = INVALID_HANDLE;
    ReadHandle = NULL;
    PipePath = NULL;
    WriteHandle = NULL;
    if (Parameters->PathLength != 0) {
        Status = MmCreateCopyOfUserModeString(Parameters->Path,
                                              Parameters->PathLength,
                                              FI_ALLOCATION_TAG,
                                              &PipePath);

        if (!KSUCCESS(Status)) {
            goto SysCreatePipeEnd;
        }
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Directory = ObGetHandleValue(CurrentProcess->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysCreatePipeEnd;
        }
    }

    if ((Parameters->OpenFlags &
         ~(SYS_OPEN_FLAG_NON_BLOCKING | SYS_OPEN_FLAG_CLOSE_ON_EXECUTE)) != 0) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysCreatePipeEnd;
    }

    OpenFlags = Parameters->OpenFlags & SYS_OPEN_FLAG_NON_BLOCKING;

    //
    // Create the pipe.
    //

    Status = IoCreatePipe(FALSE,
                          Directory,
                          PipePath,
                          Parameters->PathLength,
                          OpenFlags,
                          Parameters->Permissions,
                          &ReadHandle,
                          &WriteHandle);

    if (!KSUCCESS(Status)) {
        goto SysCreatePipeEnd;
    }

    //
    // Create user mode handles if no path was specified.
    //

    if (PipePath == NULL) {

        //
        // Create handle table entries for these handles.
        //

        HandleFlags = 0;
        if ((Parameters->OpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
            HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
        }

        Status = ObCreateHandle(CurrentProcess->HandleTable,
                                ReadHandle,
                                HandleFlags,
                                &(Parameters->ReadHandle));

        if (!KSUCCESS(Status)) {
            goto SysCreatePipeEnd;
        }

        Status = ObCreateHandle(CurrentProcess->HandleTable,
                                WriteHandle,
                                HandleFlags,
                                &(Parameters->WriteHandle));

        if (!KSUCCESS(Status)) {
            goto SysCreatePipeEnd;
        }

    } else {
        IoClose(ReadHandle);
        IoClose(WriteHandle);
        ReadHandle = NULL;
        WriteHandle = NULL;
    }

    Status = STATUS_SUCCESS;

SysCreatePipeEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (PipePath != NULL) {
        MmFreePagedPool(PipePath);
    }

    if (!KSUCCESS(Status)) {
        if (ReadHandle != NULL) {
            IoClose(ReadHandle);
        }

        if (WriteHandle != NULL) {
            IoClose(WriteHandle);
        }

        if (Parameters->ReadHandle != INVALID_HANDLE) {
            ObDestroyHandle(CurrentProcess->HandleTable,
                            Parameters->ReadHandle);
        }

        if (Parameters->WriteHandle != INVALID_HANDLE) {
            ObDestroyHandle(CurrentProcess->HandleTable,
                            Parameters->WriteHandle);
        }
    }

    return Status;
}

INTN
IoSysGetCurrentDirectory (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call requesting the path of the current
    working directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_GET_CURRENT_DIRECTORY Parameters;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_GET_CURRENT_DIRECTORY)SystemCallParameter;
    Status = IoGetCurrentDirectory(FALSE,
                                   Parameters->Root,
                                   &(Parameters->Buffer),
                                   &(Parameters->BufferSize));

    return Status;
}

INTN
IoSysChangeDirectory (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call requesting to change the current
    working directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    CHECK_FOR_DIRECTORY_HANDLES_CONTEXT Context;
    BOOL EscapeRoot;
    PIO_HANDLE ExistingHandle;
    PFILE_OBJECT FileObject;
    PIO_HANDLE NewHandle;
    PSTR NewPath;
    ULONG NewPathSize;
    PATH_POINT OldPathPoint;
    PSYSTEM_CALL_CHANGE_DIRECTORY Parameters;
    PPATH_POINT PathPoint;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_CHANGE_DIRECTORY)SystemCallParameter;
    EscapeRoot = FALSE;
    ExistingHandle = NULL;
    PathPoint = NULL;
    NewHandle = NULL;
    NewPath = NULL;
    Process = PsGetCurrentProcess();

    ASSERT(Process != PsGetKernelProcess());

    //
    // There are a few rules if the caller is trying to change the root. The
    // usual way to escape a changed root is to use an open directory
    // descriptor outside the changed root. Disallow this by refusing to change
    // the root if there's an open directory descriptor. Enforce a thread
    // count of one to prevent race conditions where directories are opened
    // just as the root changes.
    //

    if (Parameters->Root != FALSE) {
        Status = PsCheckPermission(PERMISSION_CHROOT);
        if (!KSUCCESS(Status)) {
            goto SysChangeDirectoryEnd;
        }

        if (Process->ThreadCount != 1) {
            Status = STATUS_PERMISSION_DENIED;
            goto SysChangeDirectoryEnd;
        }

        Context.Handle = Parameters->Handle;
        Context.Status = STATUS_SUCCESS;
        ObHandleTableIterate(Process->HandleTable,
                             IopCheckForDirectoryHandlesIterationRoutine,
                             &Context);

        if (!KSUCCESS(Context.Status)) {
            Status = Context.Status;
            goto SysChangeDirectoryEnd;
        }

        //
        // If all parameters are invalid, the caller is requesting to escape
        // the root.
        //

        if ((Parameters->Handle == INVALID_HANDLE) &&
            (Parameters->Buffer == NULL) &&
            (Parameters->BufferLength == 0)) {

            Status = PsCheckPermission(PERMISSION_ESCAPE_CHROOT);
            if (!KSUCCESS(Status)) {
                goto SysChangeDirectoryEnd;
            }

            EscapeRoot = TRUE;
        }
    }

    //
    // If a handle was supplied, use the handle.
    //

    if (Parameters->Handle != INVALID_HANDLE) {
        ExistingHandle = ObGetHandleValue(Process->HandleTable,
                                          Parameters->Handle,
                                          NULL);

        if (ExistingHandle == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysChangeDirectoryEnd;
        }

        PathPoint = &(ExistingHandle->PathPoint);
        FileObject = PathPoint->PathEntry->FileObject;
        if ((FileObject->Properties.Type != IoObjectRegularDirectory) &&
            (FileObject->Properties.Type != IoObjectObjectDirectory)) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto SysChangeDirectoryEnd;
        }

        ASSERT(FileObject == ExistingHandle->FileObject);

    //
    // More commonly a path was supplied, so open the path.
    //

    } else if (EscapeRoot == FALSE) {

        //
        // Create a copy of the user mode string so it cannot be manipulated
        // during the call.
        //

        NewPathSize = Parameters->BufferLength;
        Status = MmCreateCopyOfUserModeString(Parameters->Buffer,
                                              NewPathSize,
                                              FI_ALLOCATION_TAG,
                                              &NewPath);

        if (!KSUCCESS(Status)) {
            goto SysChangeDirectoryEnd;
        }

        //
        // Open up the new working directory.
        //

        Status = IoOpen(FALSE,
                        NULL,
                        NewPath,
                        NewPathSize,
                        0,
                        OPEN_FLAG_DIRECTORY,
                        FILE_PERMISSION_NONE,
                        &NewHandle);

        if (!KSUCCESS(Status)) {
            goto SysChangeDirectoryEnd;
        }

        PathPoint = &(NewHandle->PathPoint);

    //
    // The caller is escaping back to the one true root.
    //

    } else {
        PathPoint = &IoPathPointRoot;
    }

    //
    // Replace the current working directory or root directory.
    //

    IO_PATH_POINT_ADD_REFERENCE(PathPoint);
    KeAcquireQueuedLock(Process->Paths.Lock);
    if (Parameters->Root != FALSE) {
        IO_COPY_PATH_POINT(&OldPathPoint, &(Process->Paths.Root));
        IO_COPY_PATH_POINT(&(Process->Paths.Root), PathPoint);

    } else {
        IO_COPY_PATH_POINT(&OldPathPoint, &(Process->Paths.CurrentDirectory));
        IO_COPY_PATH_POINT(&(Process->Paths.CurrentDirectory), PathPoint);
    }

    KeReleaseQueuedLock(Process->Paths.Lock);
    if (OldPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&OldPathPoint);
    }

    Status = STATUS_SUCCESS;

SysChangeDirectoryEnd:
    if (ExistingHandle != NULL) {
        IoIoHandleReleaseReference(ExistingHandle);
    }

    if (NewHandle != NULL) {
        IoClose(NewHandle);
    }

    if (NewPath != NULL) {
        MmFreePagedPool(NewPath);
    }

    return Status;
}

INTN
IoSysPoll (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the poll system call, which waits on several I/O
    handles.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of descriptors selected (a positive integer)
    on success.

    Error status code (a negative integer) on failure.

--*/

{

    ULONG AllocationSize;
    LONG DescriptorCount;
    LONG DescriptorIndex;
    PPOLL_DESCRIPTOR Descriptors;
    PFILE_OBJECT FileObject;
    HANDLE Handle;
    PIO_HANDLE IoHandle;
    PIO_OBJECT_STATE IoObjectState;
    ULONG MaskedEvents;
    ULONGLONG Microseconds;
    ULONGLONG ObjectIndex;
    SIGNAL_SET OldSignalSet;
    PSYSTEM_CALL_POLL PollInformation;
    PKPROCESS Process;
    BOOL RestoreSignalMask;
    INTN Result;
    INTN SelectedDescriptors;
    SIGNAL_SET SignalMask;
    KSTATUS Status;
    PKTHREAD Thread;
    PPOLL_DESCRIPTOR UserDescriptors;
    ULONG WaitEvents;
    PVOID *WaitObjects;

    Descriptors = NULL;
    PollInformation = (PSYSTEM_CALL_POLL)SystemCallParameter;
    DescriptorCount = PollInformation->DescriptorCount;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    RestoreSignalMask = FALSE;
    SelectedDescriptors = 0;
    WaitObjects = NULL;

    //
    // Set the signal mask if supplied.
    //

    if (PollInformation->SignalMask != NULL) {
        Status = MmCopyFromUserMode(&SignalMask,
                                    PollInformation->SignalMask,
                                    sizeof(SIGNAL_SET));

        if (!KSUCCESS(Status)) {
            goto PollEnd;
        }

        PsSetSignalMask(&SignalMask, &OldSignalSet);
        RestoreSignalMask = TRUE;
    }

    //
    // Polling nothing is easy.
    //

    if ((PollInformation->Descriptors == NULL) || (DescriptorCount <= 0)) {
        Microseconds = PollInformation->TimeoutInMilliseconds *
                       MICROSECONDS_PER_MILLISECOND;

        Status = KeDelayExecution(TRUE, FALSE, Microseconds);
        goto PollEnd;
    }

    UserDescriptors = PollInformation->Descriptors;

    //
    // Allocate space for a kernel mode array of poll descriptors.
    //

    AllocationSize = sizeof(POLL_DESCRIPTOR) * DescriptorCount;
    Descriptors = MmAllocatePagedPool(AllocationSize, IO_ALLOCATION_TAG);
    if (Descriptors == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PollEnd;
    }

    Status = MmCopyFromUserMode(Descriptors, UserDescriptors, AllocationSize);
    if (!KSUCCESS(Status)) {
        RtlZeroMemory(Descriptors, AllocationSize);
        goto PollEnd;
    }

    //
    // Clear the returned events in the array, and translate the user mode
    // handles into kernel mode handles.
    //

    for (DescriptorIndex = 0;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        Handle = Descriptors[DescriptorIndex].Handle;
        Descriptors[DescriptorIndex].ReturnedEvents = 0;
        if ((INTN)Handle < 0) {
            Descriptors[DescriptorIndex].Handle = NULL;
            continue;
        }

        Descriptors[DescriptorIndex].Handle =
                          ObGetHandleValue(Process->HandleTable, Handle, NULL);

        if (Descriptors[DescriptorIndex].Handle == NULL) {
            Status = MmUserWrite16(
                            &(UserDescriptors[DescriptorIndex].ReturnedEvents),
                            POLL_EVENT_INVALID_HANDLE);

            if (Status == FALSE) {
                Status = STATUS_ACCESS_VIOLATION;
                goto PollEnd;
            }

            SelectedDescriptors += 1;
        }
    }

    //
    // Allocate space for the wait objects, assuming the worst case that every
    // descriptor wants to wait on error, read, high priority read, write, and
    // high priority write.
    //

    AllocationSize = 5 * DescriptorCount * sizeof(PVOID);
    WaitObjects = MmAllocatePagedPool(AllocationSize, IO_ALLOCATION_TAG);
    if (WaitObjects == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PollEnd;
    }

    RtlZeroMemory(WaitObjects, AllocationSize);

    //
    // Add all the qualifying objects to the array of things to wait for.
    //

    ObjectIndex = 0;
    for (DescriptorIndex = 0;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        WaitEvents = Descriptors[DescriptorIndex].Events;
        IoHandle = Descriptors[DescriptorIndex].Handle;
        if ((WaitEvents == 0) || (IoHandle == NULL)) {
            continue;
        }

        FileObject = IoHandle->FileObject;
        IoObjectState = FileObject->IoState;
        if (IoObjectState == NULL) {

            ASSERT((FileObject->Properties.Type == IoObjectRegularFile) ||
                   (FileObject->Properties.Type == IoObjectRegularDirectory) ||
                   (FileObject->Properties.Type == IoObjectObjectDirectory) ||
                   (FileObject->Properties.Type == IoObjectSharedMemoryObject));

            SelectedDescriptors += 1;
            continue;
        }

        //
        // Always wait on the error event.
        //

        WaitObjects[ObjectIndex] = IoObjectState->ErrorEvent;
        ObjectIndex += 1;
        if ((WaitEvents & POLL_EVENT_IN) != 0) {
            WaitObjects[ObjectIndex] = IoObjectState->ReadEvent;
            ObjectIndex += 1;
        }

        if (((WaitEvents & POLL_EVENT_IN_HIGH_PRIORITY) != 0) &&
            (IoObjectState->ReadHighPriorityEvent != NULL)) {

            WaitObjects[ObjectIndex] = IoObjectState->ReadHighPriorityEvent;
            ObjectIndex += 1;
        }

        if ((WaitEvents & POLL_EVENT_OUT) != 0) {
            WaitObjects[ObjectIndex] = IoObjectState->WriteEvent;
            ObjectIndex += 1;
        }

        if (((WaitEvents & POLL_EVENT_OUT_HIGH_PRIORITY) != 0) &&
            (IoObjectState->WriteHighPriorityEvent != NULL)) {

            WaitObjects[ObjectIndex] = IoObjectState->WriteHighPriorityEvent;
            ObjectIndex += 1;
        }
    }

    ASSERT(ObjectIndex <= 5 * DescriptorCount);

    //
    // Wait on this list of objects.
    //

    ASSERT((ULONG)ObjectIndex == ObjectIndex);

    if (SelectedDescriptors == 0) {
        Status = ObWaitOnObjects(
                               WaitObjects,
                               ObjectIndex,
                               WAIT_FLAG_INTERRUPTIBLE,
                               PollInformation->TimeoutInMilliseconds,
                               NULL,
                               NULL);

        if (!KSUCCESS(Status)) {
            goto PollEnd;
        }
    }

    //
    // Loop through and read out all the poll flags.
    //

    for (DescriptorIndex = 0;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        WaitEvents = Descriptors[DescriptorIndex].Events;
        IoHandle = Descriptors[DescriptorIndex].Handle;
        if ((IoHandle == NULL) || (WaitEvents == 0)) {
            continue;
        }

        //
        // If this descriptor fits the bill, increment the count of
        // selected descriptors.
        //

        FileObject = IoHandle->FileObject;
        if ((FileObject->Properties.Type == IoObjectRegularFile) ||
            (FileObject->Properties.Type == IoObjectRegularDirectory) ||
            (FileObject->Properties.Type == IoObjectObjectDirectory) ||
            (FileObject->Properties.Type == IoObjectSharedMemoryObject)) {

            MaskedEvents = WaitEvents & POLL_NONMASKABLE_FILE_EVENTS;

        } else {
            IoObjectState = FileObject->IoState;

            ASSERT(IoObjectState != NULL);

            //
            // The I/O object state maintains a bitmask of all the currently
            // signaled poll events. AND this with the requested events to get
            // the returned events for this descriptor.
            //

            MaskedEvents = IoObjectState->Events &
                           (WaitEvents | POLL_NONMASKABLE_EVENTS);

            if (MaskedEvents != 0) {
                SelectedDescriptors += 1;
            }
        }

        Descriptors[DescriptorIndex].ReturnedEvents |= MaskedEvents;
        Status = MmUserWrite16(
                            &(UserDescriptors[DescriptorIndex].ReturnedEvents),
                            Descriptors[DescriptorIndex].ReturnedEvents);

        if (Status == FALSE) {
            Status = STATUS_ACCESS_VIOLATION;
            goto PollEnd;
        }
    }

    Status = STATUS_SUCCESS;

PollEnd:
    if (RestoreSignalMask != FALSE) {

        //
        // If a signal arrived during the poll, then do not restore the blocked
        // mask until it gets a chance to be dispatched. Save the old signal
        // set to be restored during signal dispatch.
        //

        PsCheckRuntimeTimers(Thread);
        if (Thread->SignalPending == ThreadSignalPending) {
            Thread->RestoreSignals = OldSignalSet;
            Thread->Flags |= THREAD_FLAG_RESTORE_SIGNALS;

        //
        // Otherwise restore the signal mask now. The period under the
        // temporary mask is now over.
        //

        } else {
            PsSetSignalMask(&OldSignalSet, NULL);
        }
    }

    if (WaitObjects != NULL) {
        MmFreePagedPool(WaitObjects);
    }

    if (Descriptors != NULL) {

        //
        // For any handle that was successfully looked up, release the
        // reference that lookup added.
        //

        for (DescriptorIndex = 0;
             DescriptorIndex < DescriptorCount;
             DescriptorIndex += 1) {

            if (Descriptors[DescriptorIndex].Handle != NULL) {
                IoIoHandleReleaseReference(Descriptors[DescriptorIndex].Handle);
            }
        }

        MmFreePagedPool(Descriptors);
    }

    //
    // On success, return the positive descriptor count. Otherwise return the
    // failure status.
    //

    Result = Status;
    if (KSUCCESS(Result)) {
        Result = SelectedDescriptors;
    }

    return Result;
}

INTN
IoSysDuplicateHandle (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for duplicating a file handle.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG HandleFlags;
    PIO_HANDLE IoHandle;
    PVOID OldValue;
    PSYSTEM_CALL_DUPLICATE_HANDLE Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_DUPLICATE_HANDLE)SystemCallParameter;
    Process = PsGetCurrentProcess();

    //
    // First check to see if the old handle is valid (and take a reference on
    // it).
    //

    IoHandle = ObGetHandleValue(Process->HandleTable,
                                Parameters->OldHandle,
                                NULL);

    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysDuplicateHandleEnd;
    }

    HandleFlags = 0;
    if ((Parameters->OpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    //
    // If the new handle and the old handle are the same, then just return
    // success. That is unless the caller was trying to set the close on
    // execute flag, which is an illegal way to try and do that.
    //

    if (Parameters->OldHandle == Parameters->NewHandle) {
        Status = STATUS_SUCCESS;
        if (HandleFlags != 0) {
            Status = STATUS_INVALID_PARAMETER;
        }

        goto SysDuplicateHandleEnd;
    }

    //
    // If the caller doesn't care where the handle comes from, then allocate
    // any handle.
    //

    if (Parameters->NewHandle == INVALID_HANDLE) {
        Status = ObCreateHandle(Process->HandleTable,
                                IoHandle,
                                HandleFlags,
                                &(Parameters->NewHandle));

        if (!KSUCCESS(Status)) {
            goto SysDuplicateHandleEnd;
        }

    //
    // Replace a specific handle value.
    //

    } else {
        Status = ObReplaceHandleValue(Process->HandleTable,
                                      Parameters->NewHandle,
                                      IoHandle,
                                      HandleFlags,
                                      &OldValue,
                                      NULL);

        if (!KSUCCESS(Status)) {
            goto SysDuplicateHandleEnd;
        }

        //
        // Close the old handle.
        //

        if (OldValue != NULL) {
            IopRemoveFileLocks(OldValue, Process);
            IoClose(OldValue);
        }
    }

    //
    // The reference taken during lookup is given to the new handle, so
    // NULL out the I/O handle so it doesn't get released at the end.
    //

    IoHandle = NULL;
    Status = STATUS_SUCCESS;

SysDuplicateHandleEnd:
    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysFileControl (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the file control system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    BOOL Asynchronous;
    PIO_ASYNC_STATE AsyncState;
    BOOL Blocking;
    UINTN CopyOutSize;
    KSTATUS CopyOutStatus;
    PSYSTEM_CALL_FILE_CONTROL FileControl;
    PFILE_OBJECT FileObject;
    ULONG Flags;
    PIO_HANDLE IoHandle;
    PIO_OBJECT_STATE IoState;
    FILE_CONTROL_PARAMETERS_UNION LocalParameters;
    ULONG Mask;
    PKPROCESS Process;
    PATH_POINT RootPathPoint;
    ULONG SetFlags;
    KSTATUS Status;
    PKTHREAD Thread;

    Blocking = FALSE;
    CopyOutSize = 0;
    Flags = 0;
    FileControl = (PSYSTEM_CALL_FILE_CONTROL)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = NULL;

    //
    // Get the handle and the flags. The "close from" operation is the only
    // exception, it doesn't actually need a valid handle.
    //

    if (FileControl->Command != FileControlCommandCloseFrom) {
        IoHandle = ObGetHandleValue(Process->HandleTable,
                                    FileControl->File,
                                    &Flags);

        if (IoHandle == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysFileControlEnd;
        }
    }

    switch (FileControl->Command) {
    case FileControlCommandDuplicate:
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(HANDLE));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        if (LocalParameters.DuplicateDescriptor >= (HANDLE)OB_MAX_HANDLES) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Flags &= ~FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;

        //
        // Increment the reference count on the I/O handle first, since as soon
        // as the new descriptor is created user mode could come in on a
        // different thread and close it.
        //

        IoIoHandleAddReference(IoHandle);
        Status = ObCreateHandle(Process->HandleTable,
                                IoHandle,
                                Flags,
                                &(LocalParameters.DuplicateDescriptor));

        if (KSUCCESS(Status)) {
            CopyOutSize = sizeof(HANDLE);

        } else {
            IoIoHandleReleaseReference(IoHandle);
        }

        break;

    case FileControlCommandGetFlags:
        LocalParameters.Flags = Flags;
        CopyOutSize = sizeof(ULONG);
        Status = STATUS_SUCCESS;
        break;

    case FileControlCommandSetFlags:
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(ULONG));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        SetFlags = LocalParameters.Flags;
        Flags &= ~FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
        if ((SetFlags & FILE_DESCRIPTOR_CLOSE_ON_EXECUTE) != 0) {
            Flags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
            SetFlags &= ~FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
        }

        if (SetFlags != 0) {
            Status = STATUS_INVALID_PARAMETER;

        } else {
            Status = ObGetSetHandleFlags(Process->HandleTable,
                                         FileControl->File,
                                         TRUE,
                                         &Flags);

            if (!KSUCCESS(Status)) {
                goto SysFileControlEnd;
            }
        }

        break;

    case FileControlCommandGetStatusAndAccess:

        ASSERT_SYS_OPEN_FLAGS_EQUIVALENT();

        Flags = (IoHandle->OpenFlags & SYS_OPEN_FLAG_MASK) |
                (IoHandle->Access << SYS_OPEN_ACCESS_SHIFT);

        LocalParameters.Flags = Flags;
        CopyOutSize = sizeof(ULONG);
        Status = STATUS_SUCCESS;
        break;

    case FileControlCommandSetStatus:
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(ULONG));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        //
        // Set the new flags except for the asynchronous flag, which is handled
        // by another function.
        //

        ASSERT_SYS_OPEN_FLAGS_EQUIVALENT();

        Mask = SYS_FILE_CONTROL_EDITABLE_STATUS_FLAGS & ~OPEN_FLAG_ASYNCHRONOUS;
        Flags = LocalParameters.Flags & Mask;
        IoHandle->OpenFlags = (IoHandle->OpenFlags & ~Mask) | Flags;
        Status = STATUS_SUCCESS;

        //
        // If the asynchronous flag changed, make adjustments.
        //

        Flags = LocalParameters.Flags;
        if (((Flags ^ IoHandle->OpenFlags) & OPEN_FLAG_ASYNCHRONOUS) != 0) {
            Asynchronous = FALSE;
            if ((Flags & OPEN_FLAG_ASYNCHRONOUS) != 0) {
                Asynchronous = TRUE;
            }

            Status = IoSetHandleAsynchronous(IoHandle,
                                             FileControl->File,
                                             Asynchronous);
        }

        break;

    //
    // Return the process ID that gets async IO signals.
    //

    case FileControlCommandGetSignalOwner:
        LocalParameters.Owner = 0;
        IoState = IoHandle->FileObject->IoState;
        if (IoState->Async != NULL) {
            LocalParameters.Owner = IoState->Async->Owner;
        }

        CopyOutSize = sizeof(PROCESS_ID);
        Status = STATUS_SUCCESS;
        break;

    //
    // Set the process ID that gets async IO signals. Also record the user
    // identity and permissions to ensure that IO signals are not sent to
    // processes this process would not ordinarily have been able to send
    // signals to.
    //

    case FileControlCommandSetSignalOwner:
        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(PROCESS_ID));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        IoState = IoHandle->FileObject->IoState;

        //
        // Signaling process groups is currently not supported.
        //

        if (LocalParameters.Owner <= 0) {
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        AsyncState = IopGetAsyncState(IoState);
        if (AsyncState == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        Thread = KeGetCurrentThread();
        KeAcquireQueuedLock(AsyncState->Lock);
        AsyncState->Owner = LocalParameters.Owner;
        AsyncState->SetterUserId = Thread->Identity.RealUserId;
        AsyncState->SetterEffectiveUserId = Thread->Identity.EffectiveUserId;
        AsyncState->SetterPermissions = Thread->Permissions.Effective;
        KeReleaseQueuedLock(AsyncState->Lock);
        Status = STATUS_SUCCESS;
        break;

    case FileControlCommandGetLock:
        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(FILE_LOCK));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        Status = IopGetFileLock(IoHandle, &(LocalParameters.FileLock));
        if (KSUCCESS(Status)) {
            CopyOutSize = sizeof(FILE_LOCK);
        }

        break;

    //
    // Toggle the blocking local and fall through to the set lock command.
    //

    case FileControlCommandBlockingSetLock:
        Blocking = TRUE;

    case FileControlCommandSetLock:
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(FILE_LOCK));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        Status = IopSetFileLock(IoHandle,
                                &(LocalParameters.FileLock),
                                Blocking);

        break;

    case FileControlCommandGetFileInformation:
        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(SET_FILE_INFORMATION));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        LocalParameters.SetFileInformation.FieldsToSet = 0;
        Status = IoSetFileInformation(
                                FALSE,
                                IoHandle,
                                &(LocalParameters.SetFileInformation));

        break;

    case FileControlCommandSetFileInformation:
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(SET_FILE_INFORMATION));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        Status = IoSetFileInformation(
                                FALSE,
                                IoHandle,
                                &(LocalParameters.SetFileInformation));

        break;

    //
    // Attempt to set the directory flag on the given descriptor so that
    // reads from the directory will succeed. This is used to support the
    // fdopendir C library function.
    //

    case FileControlCommandSetDirectoryFlag:
        Status = STATUS_NOT_A_DIRECTORY;
        FileObject = IoHandle->FileObject;
        if ((FileObject->Properties.Type == IoObjectRegularDirectory) ||
            (FileObject->Properties.Type == IoObjectObjectDirectory)) {

            IoHandle->OpenFlags |= OPEN_FLAG_DIRECTORY;
            Status = STATUS_SUCCESS;
        }

        break;

    case FileControlCommandCloseFrom:
        Status = IoCloseProcessHandles(Process, FileControl->File);
        break;

    //
    // Get the full path of path entry associated with the given I/O handle, if
    // possible.
    //

    case FileControlCommandGetPath:
        KeAcquireQueuedLock(Process->Paths.Lock);
        if (Process->Paths.Root.PathEntry != NULL) {
            IO_COPY_PATH_POINT(&RootPathPoint, &(Process->Paths.Root));
            IO_PATH_POINT_ADD_REFERENCE(&RootPathPoint);

        } else {
            IO_COPY_PATH_POINT(&RootPathPoint, &IoPathPointRoot);
            IO_PATH_POINT_ADD_REFERENCE(&RootPathPoint);
        }

        KeReleaseQueuedLock(Process->Paths.Lock);
        if (FileControl->Parameters == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysFileControlEnd;
        }

        Status = MmCopyFromUserMode(&LocalParameters,
                                    FileControl->Parameters,
                                    sizeof(FILE_PATH));

        if (!KSUCCESS(Status)) {
            goto SysFileControlEnd;
        }

        Status = IopGetUserFilePath(&(IoHandle->PathPoint),
                                    &RootPathPoint,
                                    LocalParameters.FilePath.Path,
                                    &(LocalParameters.FilePath.PathSize));

        IO_PATH_POINT_RELEASE_REFERENCE(&RootPathPoint);
        if (KSUCCESS(Status) || (Status == STATUS_BUFFER_TOO_SMALL)) {
            CopyOutSize = sizeof(FILE_PATH);
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

SysFileControlEnd:
    if (CopyOutSize != 0) {
        CopyOutStatus = STATUS_INVALID_PARAMETER;
        if (FileControl->Parameters != NULL) {
            CopyOutStatus = MmCopyToUserMode(FileControl->Parameters,
                                             &LocalParameters,
                                             CopyOutSize);
        }

        if (!KSUCCESS(CopyOutStatus)) {
            Status = CopyOutStatus;
        }
    }

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysGetSetFileInformation (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the get/set file information system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE Directory;
    PIO_HANDLE IoHandle;
    ULONG OpenFlags;
    PSYSTEM_CALL_GET_SET_FILE_INFORMATION Parameters;
    PSTR PathCopy;
    PKPROCESS Process;
    KSTATUS Status;

    Directory = NULL;
    IoHandle = NULL;
    PathCopy = NULL;
    Parameters = (PSYSTEM_CALL_GET_SET_FILE_INFORMATION)SystemCallParameter;

    //
    // Copy the path string out of user mode.
    //

    Status = MmCreateCopyOfUserModeString(Parameters->FilePath,
                                          Parameters->FilePathSize,
                                          IO_ALLOCATION_TAG,
                                          &PathCopy);

    if (!KSUCCESS(Status)) {
        goto SysGetSetFileInformationEnd;
    }

    //
    // Open up the file for getting or setting the information.
    //

    OpenFlags = 0;
    if (Parameters->FollowLink == FALSE) {
        OpenFlags |= OPEN_FLAG_SYMBOLIC_LINK;
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Process = PsGetCurrentProcess();
        Directory = ObGetHandleValue(Process->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysGetSetFileInformationEnd;
        }
    }

    Status = IoOpen(FALSE,
                    Directory,
                    PathCopy,
                    Parameters->FilePathSize,
                    0,
                    OpenFlags,
                    FILE_PERMISSION_NONE,
                    &IoHandle);

    if (!KSUCCESS(Status)) {
        goto SysGetSetFileInformationEnd;
    }

    Status = IoSetFileInformation(FALSE, IoHandle, &(Parameters->Request));

SysGetSetFileInformationEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (IoHandle != NULL) {
        IoClose(IoHandle);
    }

    if (PathCopy != NULL) {
        MmFreePagedPool(PathCopy);
    }

    return Status;
}

INTN
IoSysSeek (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the file seek system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE IoHandle;
    PKPROCESS Process;
    PSYSTEM_CALL_SEEK Request;
    KSTATUS Status;

    Request = (PSYSTEM_CALL_SEEK)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Request->Handle, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSeekEnd;
    }

    Status = IoSeek(IoHandle,
                    Request->Command,
                    Request->Offset,
                    &(Request->Offset));

    if (!KSUCCESS(Status)) {
        goto SysSeekEnd;
    }

SysSeekEnd:
    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysCreateSymbolicLink (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE Directory;
    PSTR Link;
    ULONG LinkSize;
    PSTR LinkTarget;
    ULONG LinkTargetSize;
    PSYSTEM_CALL_CREATE_SYMBOLIC_LINK Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Directory = NULL;
    Parameters = (PSYSTEM_CALL_CREATE_SYMBOLIC_LINK)SystemCallParameter;
    Link = NULL;
    LinkTarget = NULL;
    LinkSize = Parameters->PathSize;
    LinkTargetSize = Parameters->LinkDestinationBufferSize;
    Status = MmCreateCopyOfUserModeString(Parameters->Path,
                                          LinkSize,
                                          PATH_ALLOCATION_TAG,
                                          &Link);

    if (!KSUCCESS(Status)) {
        goto SysCreateSymbolicLinkEnd;
    }

    Status = MmCreateCopyOfUserModeString(Parameters->LinkDestinationBuffer,
                                          LinkTargetSize,
                                          PATH_ALLOCATION_TAG,
                                          &LinkTarget);

    if (!KSUCCESS(Status)) {
        goto SysCreateSymbolicLinkEnd;
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Process = PsGetCurrentProcess();
        Directory = ObGetHandleValue(Process->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysCreateSymbolicLinkEnd;
        }
    }

    Status = IoCreateSymbolicLink(FALSE,
                                  Directory,
                                  Link,
                                  LinkSize,
                                  LinkTarget,
                                  LinkTargetSize);

SysCreateSymbolicLinkEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (Link != NULL) {
        MmFreePagedPool(Link);
    }

    if (LinkTarget != NULL) {
        MmFreePagedPool(LinkTarget);
    }

    return Status;
}

INTN
IoSysReadSymbolicLink (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine reads and returns the destination of a symbolic link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE Directory;
    PIO_HANDLE Handle;
    PSTR Link;
    ULONG LinkSize;
    PSTR LinkTarget;
    ULONG LinkTargetSize;
    PSYSTEM_CALL_READ_SYMBOLIC_LINK Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Directory = NULL;
    Handle = NULL;
    Link = NULL;
    LinkTarget = NULL;
    LinkTargetSize = 0;
    Parameters = (PSYSTEM_CALL_READ_SYMBOLIC_LINK)SystemCallParameter;
    LinkSize = Parameters->PathSize;
    Status = MmCreateCopyOfUserModeString(Parameters->Path,
                                          LinkSize,
                                          PATH_ALLOCATION_TAG,
                                          &Link);

    if (!KSUCCESS(Status)) {
        goto SysReadSymbolicLinkEnd;
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Process = PsGetCurrentProcess();
        Directory = ObGetHandleValue(Process->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysReadSymbolicLinkEnd;
        }
    }

    Status = IoOpen(FALSE,
                    Directory,
                    Link,
                    LinkSize,
                    IO_ACCESS_READ,
                    OPEN_FLAG_SYMBOLIC_LINK,
                    FILE_PERMISSION_NONE,
                    &Handle);

    if (!KSUCCESS(Status)) {
        goto SysReadSymbolicLinkEnd;
    }

    Status = IoReadSymbolicLink(Handle,
                                PATH_ALLOCATION_TAG,
                                &LinkTarget,
                                &LinkTargetSize);

    if (!KSUCCESS(Status)) {
        goto SysReadSymbolicLinkEnd;
    }

    if (LinkTargetSize != 0) {
        LinkTargetSize -= 1;
    }

    if (Parameters->LinkDestinationBufferSize < LinkTargetSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto SysReadSymbolicLinkEnd;
    }

    Status = MmCopyToUserMode(Parameters->LinkDestinationBuffer,
                              LinkTarget,
                              LinkTargetSize);

    if (!KSUCCESS(Status)) {
        goto SysReadSymbolicLinkEnd;
    }

SysReadSymbolicLinkEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (Handle != NULL) {
        IoClose(Handle);
    }

    if (Link != NULL) {
        MmFreePagedPool(Link);
    }

    if (LinkTarget != NULL) {
        MmFreePagedPool(LinkTarget);
    }

    Parameters->LinkDestinationSize = LinkTargetSize;
    return Status;
}

INTN
IoSysCreateHardLink (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine creates a hard link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    //
    // TODO: Add support for creating a hard link.
    //

    return STATUS_NOT_SUPPORTED;
}

INTN
IoSysDelete (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine deletes an entry from a directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG DeleteFlags;
    PIO_HANDLE Directory;
    PSYSTEM_CALL_DELETE Parameters;
    PSTR PathCopy;
    PKPROCESS Process;
    KSTATUS Status;

    Directory = NULL;
    Parameters = (PSYSTEM_CALL_DELETE)SystemCallParameter;
    PathCopy = NULL;
    Status = MmCreateCopyOfUserModeString(Parameters->Path,
                                          Parameters->PathSize,
                                          IO_ALLOCATION_TAG,
                                          &PathCopy);

    if (!KSUCCESS(Status)) {
        goto SysDeleteEnd;
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Process = PsGetCurrentProcess();
        Directory = ObGetHandleValue(Process->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysDeleteEnd;
        }
    }

    DeleteFlags = 0;
    if ((Parameters->Flags & SYS_DELETE_FLAG_SHARED_MEMORY) != 0) {
        DeleteFlags |= DELETE_FLAG_SHARED_MEMORY;
    }

    if ((Parameters->Flags & SYS_DELETE_FLAG_DIRECTORY) != 0) {
        DeleteFlags |= DELETE_FLAG_DIRECTORY;
    }

    Status = IoDelete(FALSE,
                      Directory,
                      PathCopy,
                      Parameters->PathSize,
                      DeleteFlags);

    if (!KSUCCESS(Status)) {
        goto SysDeleteEnd;
    }

SysDeleteEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (PathCopy != NULL) {
        MmFreePagedPool(PathCopy);
    }

    return Status;
}

INTN
IoSysRename (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine renames a file or directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSTR DestinationCopy;
    PIO_HANDLE DestinationDirectory;
    PSYSTEM_CALL_RENAME Parameters;
    PKPROCESS Process;
    PSTR SourceCopy;
    PIO_HANDLE SourceDirectory;
    KSTATUS Status;

    DestinationCopy = NULL;
    DestinationDirectory = NULL;
    Parameters = (PSYSTEM_CALL_RENAME)SystemCallParameter;
    Process = PsGetCurrentProcess();
    SourceCopy = NULL;
    SourceDirectory = NULL;
    if (Parameters->SourceDirectory != INVALID_HANDLE) {
        SourceDirectory = ObGetHandleValue(Process->HandleTable,
                                           Parameters->SourceDirectory,
                                           NULL);

        if (SourceDirectory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysRenameEnd;
        }
    }

    if (Parameters->DestinationDirectory != INVALID_HANDLE) {
        DestinationDirectory = ObGetHandleValue(
                                              Process->HandleTable,
                                              Parameters->DestinationDirectory,
                                              NULL);

        if (DestinationDirectory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysRenameEnd;
        }
    }

    Status = MmCreateCopyOfUserModeString(Parameters->SourcePath,
                                          Parameters->SourcePathSize,
                                          IO_ALLOCATION_TAG,
                                          &SourceCopy);

    if (!KSUCCESS(Status)) {
        goto SysRenameEnd;
    }

    Status = MmCreateCopyOfUserModeString(Parameters->DestinationPath,
                                          Parameters->DestinationPathSize,
                                          IO_ALLOCATION_TAG,
                                          &DestinationCopy);

    if (!KSUCCESS(Status)) {
        goto SysRenameEnd;
    }

    Status = IoRename(FALSE,
                      SourceDirectory,
                      SourceCopy,
                      Parameters->SourcePathSize,
                      DestinationDirectory,
                      DestinationCopy,
                      Parameters->DestinationPathSize);

    if (!KSUCCESS(Status)) {
        goto SysRenameEnd;
    }

SysRenameEnd:
    if (SourceCopy != NULL) {
        MmFreePagedPool(SourceCopy);
    }

    if (DestinationCopy != NULL) {
        MmFreePagedPool(DestinationCopy);
    }

    if (SourceDirectory != NULL) {
        IoIoHandleReleaseReference(SourceDirectory);
    }

    if (DestinationDirectory != NULL) {
        IoIoHandleReleaseReference(DestinationDirectory);
    }

    return Status;
}

INTN
IoSysUserControl (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the user control system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE IoHandle;
    PKPROCESS Process;
    PSYSTEM_CALL_USER_CONTROL Request;
    KSTATUS Status;

    Request = (PSYSTEM_CALL_USER_CONTROL)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Request->Handle, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysUserControlEnd;
    }

    Status = IopHandleCommonUserControl(IoHandle,
                                        Request->Handle,
                                        Request->RequestCode,
                                        FALSE,
                                        Request->Context,
                                        Request->ContextSize);

    if (Status == STATUS_NOT_SUPPORTED) {
        Status = IoUserControl(IoHandle,
                               Request->RequestCode,
                               FALSE,
                               Request->Context,
                               Request->ContextSize);
    }

    if (!KSUCCESS(Status)) {
        goto SysUserControlEnd;
    }

SysUserControlEnd:
    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    //
    // If the user control system call got interrupted, then it can be
    // restarted if the signal handler allows.
    //

    if (Status == STATUS_INTERRUPTED) {
        Status = STATUS_RESTART_AFTER_SIGNAL;
    }

    return Status;
}

INTN
IoSysMountOrUnmount (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine mounts or unmounts a file, directory, volume, pipe, socket, or
    device.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG AccessFlags;
    ULONG MountFlags;
    PSTR MountPointCopy;
    PSYSTEM_CALL_MOUNT_UNMOUNT Parameters;
    KSTATUS Status;
    PSTR TargetCopy;

    AccessFlags = 0;
    MountFlags = 0;
    MountPointCopy = NULL;
    Parameters = (PSYSTEM_CALL_MOUNT_UNMOUNT)SystemCallParameter;
    TargetCopy = NULL;

    //
    // A mount point path is always required. Create a copy.
    //

    Status = MmCreateCopyOfUserModeString(Parameters->MountPointPath,
                                          Parameters->MountPointPathSize,
                                          IO_ALLOCATION_TAG,
                                          &MountPointCopy);

    if (!KSUCCESS(Status)) {
        goto SysMountUnmountEnd;
    }

    //
    // The target path is not required during unmount. Do not copy it.
    //

    if ((Parameters->Flags & SYS_MOUNT_FLAG_UNMOUNT) == 0) {
        Status = MmCreateCopyOfUserModeString(Parameters->TargetPath,
                                              Parameters->TargetPathSize,
                                              IO_ALLOCATION_TAG,
                                              &TargetCopy);

        if (!KSUCCESS(Status)) {
            goto SysMountUnmountEnd;
        }
    }

    //
    // Convert any additional flags.
    //

    if ((Parameters->Flags & SYS_MOUNT_FLAG_READ) != 0) {
        AccessFlags |= IO_ACCESS_READ;
    }

    if ((Parameters->Flags & SYS_MOUNT_FLAG_WRITE) != 0) {
        AccessFlags |= IO_ACCESS_WRITE;
    }

    if ((Parameters->Flags & SYS_MOUNT_FLAG_BIND) != 0) {
        MountFlags |= MOUNT_FLAG_BIND;
    }

    if ((Parameters->Flags & SYS_MOUNT_FLAG_RECURSIVE) != 0) {
        MountFlags |= MOUNT_FLAG_RECURSIVE;
    }

    //
    // A detach call is always recursive.
    //

    if ((Parameters->Flags & SYS_MOUNT_FLAG_DETACH) != 0) {
        MountFlags |= MOUNT_FLAG_DETACH | MOUNT_FLAG_RECURSIVE;
    }

    //
    // Call the appropriate mount or unmount routine.
    //

    if ((Parameters->Flags & SYS_MOUNT_FLAG_UNMOUNT) == 0) {
        Status = IoMount(FALSE,
                         MountPointCopy,
                         Parameters->MountPointPathSize,
                         TargetCopy,
                         Parameters->TargetPathSize,
                         MountFlags,
                         AccessFlags);

    } else {
        Status = IoUnmount(FALSE,
                           MountPointCopy,
                           Parameters->MountPointPathSize,
                           MountFlags,
                           AccessFlags);
    }

    if (!KSUCCESS(Status)) {
        goto SysMountUnmountEnd;
    }

SysMountUnmountEnd:
    if (MountPointCopy != NULL) {
        MmFreePagedPool(MountPointCopy);
    }

    if (TargetCopy != NULL) {
        MmFreePagedPool(TargetCopy);
    }

    return Status;
}

INTN
IoSysGetEffectiveAccess (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for getting the current user's
    access permission to a given path.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_HANDLE Directory;
    FILE_PROPERTIES Information;
    PIO_HANDLE IoHandle;
    PSYSTEM_CALL_GET_EFFECTIVE_ACCESS Parameters;
    PSTR Path;
    PKPROCESS Process;
    KSTATUS Status;

    Directory = NULL;
    IoHandle = NULL;
    Parameters = (PSYSTEM_CALL_GET_EFFECTIVE_ACCESS)SystemCallParameter;
    Parameters->EffectiveAccess = 0;
    Path = NULL;
    Status = MmCreateCopyOfUserModeString(Parameters->FilePath,
                                          Parameters->FilePathSize,
                                          FI_ALLOCATION_TAG,
                                          &Path);

    if (!KSUCCESS(Status)) {
        goto SysGetEffectiveAccessEnd;
    }

    if (Parameters->Directory != INVALID_HANDLE) {
        Process = PsGetCurrentProcess();
        Directory = ObGetHandleValue(Process->HandleTable,
                                     Parameters->Directory,
                                     NULL);

        if (Directory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysGetEffectiveAccessEnd;
        }
    }

    Status = IoOpen(FALSE,
                    Directory,
                    Path,
                    Parameters->FilePathSize,
                    0,
                    0,
                    FILE_PERMISSION_NONE,
                    &IoHandle);

    if (!KSUCCESS(Status)) {
        goto SysGetEffectiveAccessEnd;
    }

    Status = IoGetFileInformation(IoHandle, &Information);
    if (!KSUCCESS(Status)) {
        goto SysGetEffectiveAccessEnd;
    }

    if ((Parameters->DesiredFlags & EFFECTIVE_ACCESS_READ) != 0) {
        Status = IopCheckPermissions(FALSE,
                                     &(IoHandle->PathPoint),
                                     IO_ACCESS_READ);

        if (KSUCCESS(Status)) {
            Parameters->EffectiveAccess |= EFFECTIVE_ACCESS_READ;
        }
    }

    if ((Parameters->DesiredFlags & EFFECTIVE_ACCESS_WRITE) != 0) {
        Status = IopCheckPermissions(FALSE,
                                     &(IoHandle->PathPoint),
                                     IO_ACCESS_WRITE);

        if (KSUCCESS(Status)) {
            Parameters->EffectiveAccess |= EFFECTIVE_ACCESS_WRITE;
        }
    }

    if ((Parameters->DesiredFlags & EFFECTIVE_ACCESS_EXECUTE) != 0) {
        Status = IopCheckPermissions(FALSE,
                                     &(IoHandle->PathPoint),
                                     IO_ACCESS_EXECUTE);

        if (KSUCCESS(Status)) {
            Parameters->EffectiveAccess |= EFFECTIVE_ACCESS_EXECUTE;
        }
    }

    Status = STATUS_SUCCESS;

SysGetEffectiveAccessEnd:
    if (Directory != NULL) {
        IoIoHandleReleaseReference(Directory);
    }

    if (IoHandle != NULL) {
        IoClose(IoHandle);
    }

    if (Path != NULL) {
        MmFreePagedPool(Path);
    }

    return Status;
}

INTN
IoSysCreateTerminal (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for creating and opening a new
    terminal.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG Access;
    PKPROCESS CurrentProcess;
    ULONG HandleFlags;
    PIO_HANDLE MasterDirectory;
    PIO_HANDLE MasterHandle;
    PSTR MasterPath;
    ULONG OpenFlags;
    PSYSTEM_CALL_CREATE_TERMINAL Parameters;
    PIO_HANDLE SlaveDirectory;
    PSTR SlavePath;
    KSTATUS Status;

    CurrentProcess = PsGetCurrentProcess();

    ASSERT(CurrentProcess != PsGetKernelProcess());

    MasterDirectory = NULL;
    MasterHandle = NULL;
    Parameters = (PSYSTEM_CALL_CREATE_TERMINAL)SystemCallParameter;
    Parameters->MasterHandle = INVALID_HANDLE;
    MasterPath = NULL;
    SlaveDirectory = NULL;
    SlavePath = NULL;
    if (Parameters->MasterPathLength != 0) {
        Status = MmCreateCopyOfUserModeString(Parameters->MasterPath,
                                              Parameters->MasterPathLength,
                                              FI_ALLOCATION_TAG,
                                              &MasterPath);

        if (!KSUCCESS(Status)) {
            goto SysCreateTerminalEnd;
        }
    }

    if (Parameters->SlavePathLength != 0) {
        Status = MmCreateCopyOfUserModeString(Parameters->SlavePath,
                                              Parameters->SlavePathLength,
                                              FI_ALLOCATION_TAG,
                                              &SlavePath);

        if (!KSUCCESS(Status)) {
            goto SysCreateTerminalEnd;
        }
    }

    if (Parameters->MasterDirectory != INVALID_HANDLE) {
        MasterDirectory = ObGetHandleValue(CurrentProcess->HandleTable,
                                           Parameters->MasterDirectory,
                                           NULL);

        if (MasterDirectory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysCreateTerminalEnd;
        }
    }

    if (Parameters->SlaveDirectory != INVALID_HANDLE) {
        SlaveDirectory = ObGetHandleValue(CurrentProcess->HandleTable,
                                          Parameters->SlaveDirectory,
                                          NULL);

        if (SlaveDirectory == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysCreateTerminalEnd;
        }
    }

    ASSERT_SYS_OPEN_FLAGS_EQUIVALENT();

    Access = (Parameters->MasterOpenFlags >> SYS_OPEN_ACCESS_SHIFT) &
             (IO_ACCESS_READ | IO_ACCESS_WRITE);

    OpenFlags = Parameters->MasterOpenFlags & OPEN_FLAG_NO_CONTROLLING_TERMINAL;
    Status = IoCreateTerminal(FALSE,
                              MasterDirectory,
                              SlaveDirectory,
                              MasterPath,
                              Parameters->MasterPathLength,
                              SlavePath,
                              Parameters->SlavePathLength,
                              Access,
                              OpenFlags,
                              Parameters->MasterCreatePermissions,
                              Parameters->SlaveCreatePermissions,
                              &MasterHandle);

    if (!KSUCCESS(Status)) {
        goto SysCreateTerminalEnd;
    }

    HandleFlags = 0;
    if ((Parameters->MasterOpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    Status = ObCreateHandle(CurrentProcess->HandleTable,
                            MasterHandle,
                            HandleFlags,
                            &(Parameters->MasterHandle));

    if (!KSUCCESS(Status)) {
        IoClose(MasterHandle);
        goto SysCreateTerminalEnd;
    }

SysCreateTerminalEnd:
    if (MasterDirectory != NULL) {
        IoIoHandleReleaseReference(MasterDirectory);
    }

    if (SlaveDirectory != NULL) {
        IoIoHandleReleaseReference(SlaveDirectory);
    }

    if (MasterPath != NULL) {
        MmFreePagedPool(MasterPath);
    }

    if (SlavePath != NULL) {
        MmFreePagedPool(SlavePath);
    }

    return Status;
}

KSTATUS
IoCloseProcessHandles (
    PKPROCESS Process,
    HANDLE MinimumHandle
    )

/*++

Routine Description:

    This routine closes all remaining open handles in the given process.

Arguments:

    Process - Supplies a pointer to the process being terminated.

    MinimumHandle - Supplies the lowest handle to clean up to, inclusive.
        Handles below this one will not be closed.

Return Value:

    Status code.

--*/

{

    HANDLE Handle;
    HANDLE PreviousHandle;
    KSTATUS Status;
    KSTATUS TotalStatus;

    //
    // Loop getting the highest numbered handle and closing it until there are
    // no more open handles.
    //

    PreviousHandle = INVALID_HANDLE;
    TotalStatus = STATUS_SUCCESS;
    while (TRUE) {
        Handle = ObGetHighestHandle(Process->HandleTable);
        if (Handle == INVALID_HANDLE) {
            break;
        }

        if (Handle < MinimumHandle) {
            break;
        }

        ASSERT(Handle != PreviousHandle);

        Status = IopSysClose(Process, Handle);
        if (!KSUCCESS(Status)) {
            if (KSUCCESS(TotalStatus)) {
                TotalStatus = Status;
            }
        }

        PreviousHandle = Handle;
    }

    return TotalStatus;
}

KSTATUS
IoCopyProcessHandles (
    PKPROCESS SourceProcess,
    PKPROCESS DestinationProcess
    )

/*++

Routine Description:

    This routine copies all handles in the source process to the destination
    process. This is used during process forking.

Arguments:

    SourceProcess - Supplies a pointer to the process being copied.

    DestinationProcess - Supplies a pointer to the fledgling destination
        process. This process' handle hables must be empty.

Return Value:

    Status code.

--*/

{

    COPY_HANDLES_ITERATION_CONTEXT Context;
    PHANDLE_TABLE DestinationTable;
    PHANDLE_TABLE SourceTable;

    DestinationTable = DestinationProcess->HandleTable;
    SourceTable = SourceProcess->HandleTable;
    Context.DestinationTable = DestinationTable;
    Context.SourceTable = SourceTable;
    Context.Status = STATUS_SUCCESS;

    //
    // Assert that the destination process handle table is empty.
    //

    ASSERT(ObGetHighestHandle(DestinationTable) == INVALID_HANDLE);

    ObHandleTableIterate(SourceTable, IopCopyHandleIterateRoutine, &Context);

    //
    // If the operation was not successful, clean up any partial progress.
    //

    if (!KSUCCESS(Context.Status)) {
        IoCloseProcessHandles(DestinationProcess, 0);
    }

    return Context.Status;
}

KSTATUS
IoCloseHandlesOnExecute (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine closes any handles marked for "close on execute".

Arguments:

    Process - Supplies a pointer to the process undergoing the execution
        transformation.

Return Value:

    Status code.

--*/

{

    CLOSE_EXECUTE_HANDLES_CONTEXT Context;
    ULONG HandleIndex;
    KSTATUS Status;

    //
    // Get the array of handles to be closed. This can't be done in the
    // iterate routine because the iterate routine needs the tree to stay
    // static while it's cruising around.
    //

    Context.HandleArray = NULL;
    Context.HandleArraySize = 0;
    Context.HandleArrayCapacity = 0;
    Context.Status = STATUS_SUCCESS;
    ObHandleTableIterate(Process->HandleTable,
                         IopCloseExecuteHandleIterateRoutine,
                         &Context);

    Status = Context.Status;
    if (!KSUCCESS(Status)) {
        goto CloseHandlesOnExecuteEnd;
    }

    for (HandleIndex = 0;
         HandleIndex < Context.HandleArraySize;
         HandleIndex += 1) {

        IopSysClose(Process, Context.HandleArray[HandleIndex]);
    }

    Status = STATUS_SUCCESS;

CloseHandlesOnExecuteEnd:
    if (Context.HandleArray != NULL) {
        MmFreePagedPool(Context.HandleArray);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopSysClose (
    PKPROCESS Process,
    HANDLE Handle
    )

/*++

Routine Description:

    This routine closes a handle opened in user mode.

Arguments:

    Process - Supplies a pointer to the process the file handle was opened
        under.

    Handle - Supplies the handle returned during the open system call.

Return Value:

    Status code.

--*/

{

    PVOID HandleValue;
    KSTATUS Status;

    HandleValue = ObGetHandleValue(Process->HandleTable, Handle, NULL);
    if (HandleValue == NULL) {
        return STATUS_INVALID_HANDLE;
    }

    IopRemoveFileLocks(HandleValue, Process);
    Status = IoClose(HandleValue);
    if (!KSUCCESS(Status)) {
        goto SysCloseEnd;
    }

    ObDestroyHandle(Process->HandleTable, Handle);

SysCloseEnd:

    //
    // Release the handle reference that was added by the get handle value
    // routine.
    //

    IoIoHandleReleaseReference(HandleValue);
    return Status;
}

VOID
IopCopyHandleIterateRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called on each handle in the handle table for which it was
    invoked. It will copy the given handle into the destination table (passed
    via context).

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    Flags - Supplies the flags associated with this handle.

    HandleValue - Supplies the handle value for the current handle.

    Context - Supplies an opaque pointer of context that was provided when the
        iteration was requested. In this case, a pointer to the copy handles
        iteration context.

Return Value:

    None.

--*/

{

    PCOPY_HANDLES_ITERATION_CONTEXT IterationContext;
    HANDLE NewHandle;
    KSTATUS Status;

    IterationContext = (PCOPY_HANDLES_ITERATION_CONTEXT)Context;

    //
    // If the operation has already failed (on a previous handle), stop trying.
    //

    if (!KSUCCESS(IterationContext->Status)) {
        return;
    }

    NewHandle = Descriptor;
    Status = ObCreateHandle(IterationContext->DestinationTable,
                            HandleValue,
                            Flags,
                            &NewHandle);

    if (!KSUCCESS(Status)) {
        IterationContext->Status = Status;
        return;
    }

    ASSERT(NewHandle == Descriptor);

    IoIoHandleAddReference(HandleValue);
    return;
}

VOID
IopCloseExecuteHandleIterateRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called on each handle in the handle table for which it was
    invoked. It will add the handle to an array stored internally if the
    handle is marked to be close on execute.

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    Flags - Supplies the flags for this descriptor.

    HandleValue - Supplies the handle value for the current handle.

    Context - Supplies an opaque pointer of context that was provided when the
        iteration was requested. In this case, a pointer to the close execute
        handles iteration context.

Return Value:

    None.

--*/

{

    PCLOSE_EXECUTE_HANDLES_CONTEXT IterationContext;
    PHANDLE NewArray;
    ULONG NewCapacity;
    UINTN OldSize;

    IterationContext = (PCLOSE_EXECUTE_HANDLES_CONTEXT)Context;

    //
    // If the operation has already failed (on a previous handle), stop trying.
    //

    if (!KSUCCESS(IterationContext->Status)) {
        return;
    }

    //
    // If the handle doesn't need to be added to the array, exit early.
    //

    if ((Flags & FILE_DESCRIPTOR_CLOSE_ON_EXECUTE) == 0) {
        return;
    }

    //
    // Expand the array if needed.
    //

    if (IterationContext->HandleArraySize ==
        IterationContext->HandleArrayCapacity) {

        NewCapacity = IterationContext->HandleArrayCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = CLOSE_EXECUTE_HANDLE_INITIAL_ARRAY_SIZE;
        }

        NewArray = MmAllocatePagedPool(NewCapacity * sizeof(PHANDLE),
                                       IO_ALLOCATION_TAG);

        if (NewArray == NULL) {
            IterationContext->Status = STATUS_INSUFFICIENT_RESOURCES;
            return;
        }

        if (IterationContext->HandleArray != NULL) {
            OldSize = IterationContext->HandleArrayCapacity * sizeof(PHANDLE);
            RtlCopyMemory(NewArray, IterationContext->HandleArray, OldSize);
            MmFreePagedPool(IterationContext->HandleArray);
        }

        IterationContext->HandleArray = NewArray;
        IterationContext->HandleArrayCapacity = NewCapacity;

        ASSERT(IterationContext->HandleArrayCapacity >
               IterationContext->HandleArraySize);

    }

    IterationContext->HandleArray[IterationContext->HandleArraySize] =
                                                                    Descriptor;

    IterationContext->HandleArraySize += 1;
    return;
}

VOID
IopCheckForDirectoryHandlesIterationRoutine (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called on each handle in the handle table checking for an
    open handle to a directory.

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    Flags - Supplies the flags associated with this handle.

    HandleValue - Supplies the handle value for the current handle.

    Context - Supplies an opaque pointer of context that was provided when the
        iteration was requested.

Return Value:

    None.

--*/

{

    PFILE_OBJECT FileObject;
    PIO_HANDLE IoHandle;
    PCHECK_FOR_DIRECTORY_HANDLES_CONTEXT IterationContext;

    IterationContext = Context;
    IoHandle = HandleValue;
    FileObject = IoHandle->FileObject;
    if ((Descriptor != IterationContext->Handle) &&
        ((FileObject->Properties.Type == IoObjectRegularDirectory) ||
         (FileObject->Properties.Type == IoObjectObjectDirectory))) {

        IterationContext->Status = STATUS_TOO_MANY_HANDLES;
    }

    return;
}

KSTATUS
IopHandleCommonUserControl (
    PIO_HANDLE Handle,
    HANDLE Descriptor,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine performs user control operations common to many types of
    devices.

Arguments:

    Handle - Supplies the open file handle.

    Descriptor - Supplies the descriptor corresponding to the handle.

    MinorCode - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    INT Argument;
    BOOL Asynchronous;
    KSTATUS Status;

    switch (MinorCode) {
    case FileIoControlAsync:
        if (ContextBufferSize < sizeof(INT)) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            Argument = *((PULONG)ContextBuffer);

        } else {
            Argument = 0;
            Status = MmCopyFromUserMode(&Argument, ContextBuffer, sizeof(INT));
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        Asynchronous = FALSE;
        if (Argument != 0) {
            Asynchronous = TRUE;
        }

        Status = IoSetHandleAsynchronous(Handle, Descriptor, Asynchronous);
        break;

    case FileIoControlNonBlocking:
        if (ContextBufferSize < sizeof(INT)) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            break;
        }

        if (FromKernelMode != FALSE) {
            Argument = *((PULONG)ContextBuffer);

        } else {
            Argument = 0;
            Status = MmCopyFromUserMode(&Argument, ContextBuffer, sizeof(INT));
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if (Argument != 0) {
            Handle->OpenFlags |= OPEN_FLAG_NON_BLOCKING;

        } else {
            Handle->OpenFlags &= ~OPEN_FLAG_NON_BLOCKING;
        }

        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

