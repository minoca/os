/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pty.c

Abstract:

    This module implements terminal support.

Author:

    Evan Green 10-May-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/termlib.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TERMINAL_ALLOCATION_TAG 0x216D7254 // '!mrT'

#define TERMINAL_INITIAL_PERMISSIONS                            \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE |   \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_WRITE)

#define TERMINAL_DIRECTORY_NAME "Terminal"
#define TERMINAL_MASTER_NAME_FORMAT "Master%X"
#define TERMINAL_SLAVE_NAME_FORMAT "Slave%X"
#define TERMINAL_MAX_NAME_LENGTH 23
#define TERMINAL_MAX_COMMAND_HISTORY 50
#define TERMINAL_MAX_CANONICAL_OUTPUT 8

//
// Define the number of lines to scroll in canonical mode when page up/down
// is seen.
//

#define TERMINAL_SCROLL_LINE_COUNT 5

//
// Define terminal limits. The input queue length must always be at least the
// max canonical length since the line gets dumped into the input queue.
//

#define TERMINAL_INPUT_BUFFER_SIZE 512
#define TERMINAL_CANONICAL_BUFFER_SIZE (TERMINAL_INPUT_BUFFER_SIZE - 1)
#define TERMINAL_OUTPUT_BUFFER_SIZE 256

//
// Default control characters.
//

#define TERMINAL_DEFAULT_END_OF_FILE 0x04
#define TERMINAL_DEFAULT_END_OF_LINE 0x00
#define TERMINAL_DEFAULT_ERASE 0x7F
#define TERMINAL_DEFAULT_INTERRUPT 0x03
#define TERMINAL_DEFAULT_KILL 0x15
#define TERMINAL_DEFAULT_QUIT 0x1C
#define TERMINAL_DEFAULT_SUSPEND 0x1A
#define TERMINAL_DEFAULT_START 0x11
#define TERMINAL_DEFAULT_STOP 0x13

//
// Define the default baud rate terminals come up in.
//

#define TERMINAL_DEFAULT_BAUD_RATE 115200

//
// Define the default window size that terminals get initialized to.
//

#define TERMINAL_DEFAULT_ROWS 25
#define TERMINAL_DEFAULT_COLUMNS 80

//
// Define terminal flags.
//

#define TERMINAL_FLAG_VIRGIN_LINE 0x00000001
#define TERMINAL_FLAG_UNEDITED_LINE 0x0000002
#define TERMINAL_FLAG_FAIL_OPENS 0x00000004

//
// Define the invalid session and process group IDs.
//

#define TERMINAL_INVALID_SESSION -1
#define TERMINAL_INVALID_PROCESS_GROUP -1

#define TERMINAL_POLL_ERRORS (POLL_EVENT_ERROR | POLL_EVENT_DISCONNECTED)

//
// --------------------------------------------------------------------- Macros
//

//
// The terminal master is considered open if it has more than 1 reference. An
// initial reference is taken upon creation, but that does not count towards
// being opened.
//

#define IO_IS_TERMINAL_MASTER_OPEN(_Terminal) \
    ((_Terminal)->MasterReferenceCount > 1)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _TERMINAL_SLAVE TERMINAL_SLAVE, *PTERMINAL_SLAVE;

/*++

Structure Description:

    This structure defines terminal structure.

Members:

    Header - Stores the standard object header.

    ListEntry - Stores pointers to the next and previous terminals in the
        global list.

    Number - Stores the terminal number.

    OutputBuffer - Stores the output buffer (buffer going out of the slave into
        the master).

    OutputBufferStart - Stores the first valid index of the output buffer.

    OutputBufferEnd - Stores the first invalid index of the output buffer. If
        this is equal to the start, then the buffer is empty.

    InputBuffer - Stores a pointer to the input buffer.

    InputBufferStart - Stores the first valid index of the input buffer.

    InputBufferEnd - Stores the first invalid index of the input buffer. If
        this is equal to the start, then the buffer is empty.

    WorkingInputBuffer - Stores the current (unfinished) line in canonical
        mode.

    WorkingInputCursor - Stores the current position of the cursor in the
        working input buffer.

    WorkingInputLength - Stores the valid length of the working input buffer.

    Lock - Stores a pointer to a lock serializing access to the output, intput,
        and working input buffers.

    Settings - Stores the current terminal settings.

    Flags - Stores a bitfield of flags. See TERMINAL_FLAG_* definitions. This
        field is protected by the terminal output lock.

    KeyData - Stores the data for the key currently being parsed. This is only
        used in canonical mode.

    SlaveHandles - Stores the count of open slave side handles, not counting
        those opened with no access.

    ProcessGroupId - Stores the owning process group ID of the terminal.

    SessionId - Stores the owning session ID of the terminal.

    ConnectionLock - Stores a spin lock that synchronizes the connection
        between the master and the slave, ensuring they both shut down in an
        orderly fashion.

    MasterReferenceCount - Stores the number of references on the master. This
        amounts to the number of open file handles plus one additional
        reference set on initialization.

    Slave - Stores a pointer to the corresponding slave object.

    SlaveFileObject - Stores a pointer to the slave's file object.

    MasterFileObject - Stores a pointer to the master's file object.

    WindowSize - Stores the window size of the terminal.

    ModemStatus - Stores the modem status bits.

    HardwareHandle - Stores an optional handle to the hardware device backing
        this terminal.

    SlavePathPoint - Stores the path point of the slave device, used to unlink
        it when the last master handle is closed.

--*/

typedef struct _TERMINAL {
    OBJECT_HEADER Header;
    LIST_ENTRY ListEntry;
    ULONG Number;
    PSTR OutputBuffer;
    ULONG OutputBufferStart;
    ULONG OutputBufferEnd;
    PSTR InputBuffer;
    ULONG InputBufferStart;
    ULONG InputBufferEnd;
    PSTR WorkingInputBuffer;
    ULONG WorkingInputCursor;
    ULONG WorkingInputLength;
    PQUEUED_LOCK Lock;
    TERMINAL_SETTINGS Settings;
    TERMINAL_KEY_DATA KeyData;
    ULONG Flags;
    UINTN SlaveHandles;
    PROCESS_GROUP_ID ProcessGroupId;
    SESSION_ID SessionId;
    ULONG MasterReferenceCount;
    PTERMINAL_SLAVE Slave;
    PFILE_OBJECT SlaveFileObject;
    PFILE_OBJECT MasterFileObject;
    TERMINAL_WINDOW_SIZE WindowSize;
    INT ModemStatus;
    PIO_HANDLE HardwareHandle;
    PATH_POINT SlavePathPoint;
} TERMINAL, *PTERMINAL;

/*++

Structure Description:

    This structure defines the slave terminal structure.

Members:

    Header - Stores the standard object header.

    Master - Stores a pointer to the master terminal.

--*/

struct _TERMINAL_SLAVE {
    OBJECT_HEADER Header;
    PTERMINAL Master;
};

/*++

Structure Description:

    This structure defines the parameters sent during a creation request of
    a terminal object.

Members:

    SlaveCreatePermissions - Store a pointer to the permissions used when
        creating the slave side.

    Master - Stores a pointer to the master terminal. When creating a master
        terminal, this parameter will be filled in during create. When creating
        a slave, this parameter must already be filled in and will be used.

--*/

typedef struct _TERMINAL_CREATION_PARAMETERS {
    FILE_PERMISSIONS SlaveCreatePermissions;
    PTERMINAL Master;
} TERMINAL_CREATION_PARAMETERS, *PTERMINAL_CREATION_PARAMETERS;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopCreateTerminalObject (
    FILE_PERMISSIONS CreatePermissions,
    PTERMINAL *NewTerminal
    );

VOID
IopDestroyTerminal (
    PVOID TerminalObject
    );

KSTATUS
IopTerminalMasterWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopTerminalSlaveWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopTerminalMasterRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopTerminalSlaveRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopTerminalWriteOutputBuffer (
    PTERMINAL Terminal,
    PVOID Buffer,
    UINTN SizeInBytes,
    ULONG RepeatCount,
    ULONG TimeoutInMilliseconds
    );

ULONG
IopTerminalGetInputBufferSpace (
    PTERMINAL Terminal
    );

ULONG
IopTerminalGetOutputBufferSpace (
    PTERMINAL Terminal
    );

KSTATUS
IopTerminalFixUpCanonicalLine (
    PTERMINAL Terminal,
    ULONG TimeoutInMilliseconds,
    ULONG DirtyRegionBegin,
    ULONG DirtyRegionEnd,
    ULONG CurrentScreenPosition
    );

BOOL
IopTerminalProcessEditingCharacter (
    PTERMINAL Terminal,
    CHAR Character,
    ULONG TimeoutInMilliseconds,
    PULONG DirtyRegionBegin,
    PULONG DirtyRegionEnd,
    PULONG ScreenCursorPosition,
    PBOOL OutputWritten
    );

KSTATUS
IopTerminalUserBufferCopy (
    BOOL FromKernelMode,
    BOOL FromBuffer,
    PVOID UserBuffer,
    PVOID LocalBuffer,
    UINTN Size
    );

KSTATUS
IopTerminalFlushOutputToDevice (
    PTERMINAL Terminal
    );

VOID
IopTerminalDisassociate (
    PTERMINAL Terminal
    );

BOOL
IopTerminalDisassociateIterator (
    PVOID Context,
    PKPROCESS Process
    );

KSTATUS
IopTerminalValidateGroup (
    PTERMINAL Terminal,
    BOOL Input
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the global terminal directory.
//

PVOID IoTerminalDirectory;

//
// Store the global list of terminals.
//

LIST_ENTRY IoTerminalList;
PQUEUED_LOCK IoTerminalListLock;

//
// Store a pointer to the local console terminal.
//

PIO_HANDLE IoLocalConsole;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoCreateTerminal (
    BOOL FromKernelMode,
    PIO_HANDLE MasterDirectory,
    PIO_HANDLE SlaveDirectory,
    PCSTR MasterPath,
    UINTN MasterPathLength,
    PCSTR SlavePath,
    UINTN SlavePathLength,
    ULONG MasterAccess,
    ULONG MasterOpenFlags,
    FILE_PERMISSIONS MasterCreatePermissions,
    FILE_PERMISSIONS SlaveCreatePermissions,
    PIO_HANDLE *MasterHandle
    )

/*++

Routine Description:

    This routine creates and opens a new terminal master.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (and should use the root path as a base)
        or user mode.

    MasterDirectory - Supplies an optional pointer to an open handle to a
        directory for relative paths when creating the master. Supply NULL to
        use the current working directory.

    SlaveDirectory - Supplies an optional pointer to an open handle to a
        directory for relative paths when creating the slave. Supply NULL to
        use the current working directory.

    MasterPath - Supplies an optional pointer to the path to create for the
        master.

    MasterPathLength - Supplies the length of the master side path buffer in
        bytes, including the null terminator.

    SlavePath - Supplies an optional pointer to the path to create for the
        master.

    SlavePathLength - Supplies the length of the slave side path buffer in
        bytes, including the null terminator.

    MasterAccess - Supplies the desired access permissions to the master side
        handle. See IO_ACCESS_* definitions.

    MasterOpenFlags - Supplies the open flags to use when opening the master.

    MasterCreatePermissions - Supplies the permissions to apply to the created
        master side.

    SlaveCreatePermissions - Supplies the permission to apply to the created
        slave side.

    MasterHandle - Supplies a pointer where a handle to the master side of the
        new terminal will be returned.

Return Value:

    Status code.

--*/

{

    CREATE_PARAMETERS Create;
    TERMINAL_CREATION_PARAMETERS CreationParameters;
    PIO_HANDLE SlaveHandle;
    KSTATUS Status;

    RtlZeroMemory(&CreationParameters, sizeof(TERMINAL_CREATION_PARAMETERS));
    CreationParameters.SlaveCreatePermissions = SlaveCreatePermissions;
    Create.Type = IoObjectTerminalMaster;
    Create.Context = &CreationParameters;
    Create.Permissions = MasterCreatePermissions;
    Create.Created = FALSE;

    //
    // First try to open the master.
    //

    MasterOpenFlags |= OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS;
    Status = IopOpen(FromKernelMode,
                     MasterDirectory,
                     MasterPath,
                     MasterPathLength,
                     MasterAccess,
                     MasterOpenFlags,
                     &Create,
                     MasterHandle);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // The master put itself in the creation parameters, which are now passed
    // down when trying to create the slave (which is mostly just a matter of
    // creating the path entry now).
    //

    Create.Type = IoObjectTerminalSlave;
    Create.Permissions = SlaveCreatePermissions;
    Create.Created = FALSE;
    MasterOpenFlags |= OPEN_FLAG_NO_CONTROLLING_TERMINAL;
    Status = IopOpen(FromKernelMode,
                     SlaveDirectory,
                     SlavePath,
                     SlavePathLength,
                     0,
                     MasterOpenFlags,
                     &Create,
                     &SlaveHandle);

    if (!KSUCCESS(Status)) {
        IoClose(*MasterHandle);
    }

    //
    // Copy the path entry, then close the slave handle.
    //

    ASSERT((CreationParameters.Master != NULL) &&
           (CreationParameters.Master->SlavePathPoint.PathEntry == NULL));

    IO_COPY_PATH_POINT(&(CreationParameters.Master->SlavePathPoint),
                       &(SlaveHandle->PathPoint));

    IO_PATH_POINT_ADD_REFERENCE(&(CreationParameters.Master->SlavePathPoint));
    IoClose(SlaveHandle);
    return Status;
}

KERNEL_API
KSTATUS
IoOpenLocalTerminalMaster (
    PIO_HANDLE *TerminalMaster
    )

/*++

Routine Description:

    This routine opens the master side of the local console terminal. This
    routine is intended to be used by the input and output devices that
    actually service the local console (the user input driver and video
    console driver).

Arguments:

    TerminalMaster - Supplies a pointer where the I/O handle representing the
        master side of the local console terminal will be returned.

Return Value:

    Status code.

--*/

{

    if (IoLocalConsole == NULL) {
        return STATUS_NOT_READY;
    }

    IoIoHandleAddReference(IoLocalConsole);
    *TerminalMaster = IoLocalConsole;
    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
IoOpenControllingTerminal (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine attempts to open the current process' controlling terminal.

Arguments:

    IoHandle - Supplies a pointer to an already open or opening I/O handle. The
        contents of that handle will be replaced with the controlling terminal.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PKPROCESS Process;
    KSTATUS Status;

    Process = PsGetCurrentProcess();
    KeAcquireQueuedLock(IoTerminalListLock);
    FileObject = Process->ControllingTerminal;
    if (FileObject == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;

    } else {
        IopFileObjectAddReference(FileObject);
        IopOverwriteIoHandle(IoHandle, FileObject);
        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(IoTerminalListLock);
    if (KSUCCESS(Status)) {
        Status = IopTerminalOpenSlave(IoHandle);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoSetTerminalSettings (
    PIO_HANDLE TerminalHandle,
    PTERMINAL_SETTINGS NewSettings,
    PTERMINAL_SETTINGS OriginalSettings,
    TERMINAL_CHANGE_BEHAVIOR When
    )

/*++

Routine Description:

    This routine gets or sets the current terminal settings.

Arguments:

    TerminalHandle - Supplies a pointer to the I/O handle of the terminal to
        change.

    NewSettings - Supplies an optional pointer to the new terminal settings.
        If this pointer is NULL, then the current settings will be retrieved
        but no changes will be made.

    OriginalSettings - Supplies an optional pointer where the current
        settings will be returned.

    When - Supplies when the new change should occur.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;
    PTERMINAL Terminal;
    PTERMINAL_SLAVE TerminalSlave;

    //
    // Get a pointer to the actual terminal structure.
    //

    FileObject = TerminalHandle->FileObject;
    if (FileObject->Properties.Type == IoObjectTerminalMaster) {
        Terminal = FileObject->SpecialIo;

    } else if (FileObject->Properties.Type == IoObjectTerminalSlave) {
        TerminalSlave = FileObject->SpecialIo;
        Terminal = TerminalSlave->Master;

    } else {
        return STATUS_NOT_A_TERMINAL;
    }

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Lock down the terminal for this.
    //

    KeAcquireQueuedLock(Terminal->Lock);
    Status = IopTerminalValidateGroup(Terminal, FALSE);
    if (!KSUCCESS(Status)) {
        goto SetTerminalSettingsEnd;
    }

    if (OriginalSettings != NULL) {
        RtlCopyMemory(OriginalSettings,
                      &(Terminal->Settings),
                      sizeof(TERMINAL_SETTINGS));
    }

    if (NewSettings != NULL) {

        //
        // Fail if an unsupported feature was requested. Consider adding
        // support for said feature.
        //

        if (((NewSettings->InputFlags &
              TERMINAL_UNIMPLEMENTED_INPUT_FLAGS) != 0) ||
            ((NewSettings->OutputFlags &
              TERMINAL_UNIMPLEMENTED_OUTPUT_FLAGS) != 0) ||
            ((NewSettings->ControlFlags &
              TERMINAL_UNIMPLEMENTED_CONTROL_FLAGS) != 0)) {

            ASSERT(FALSE);

            Status = STATUS_NOT_SUPPORTED;
            goto SetTerminalSettingsEnd;
        }

        RtlCopyMemory(&(Terminal->Settings),
                      NewSettings,
                      sizeof(TERMINAL_SETTINGS));
    }

    //
    // If the user asked, remove all input.
    //

    if (When == TerminalChangeAfterOutputFlushInput) {
        Terminal->InputBufferStart = 0;
        Terminal->InputBufferEnd = 0;
    }

    Status = STATUS_SUCCESS;

SetTerminalSettingsEnd:
    KeReleaseQueuedLock(Terminal->Lock);
    return Status;
}

KERNEL_API
KSTATUS
IoTerminalSetDevice (
    PIO_HANDLE TerminalMaster,
    PIO_HANDLE DeviceHandle
    )

/*++

Routine Description:

    This routine associates or disassociates a terminal object with a device.
    Writes to the terminal slave will be forwarded to the associated terminal,
    as will changes to the terminal settings. If a device is being associated
    with the terminal, then the new settings will be sent to the device
    immediately in this routine.

Arguments:

    TerminalMaster - Supplies a handle to the terminal master.

    DeviceHandle - Supplies a handle to the terminal hardware device. Any
        previously associated handle will be closed. Supply NULL to
        disassociate the terminal with a device. Upon success, this routine
        takes ownership of this device handle, and the caller should not close
        it manually.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;
    PTERMINAL Terminal;

    FileObject = TerminalMaster->FileObject;
    if (FileObject->Properties.Type != IoObjectTerminalMaster) {
        Status = STATUS_NOT_A_TERMINAL;
        goto TerminalSetDeviceEnd;
    }

    Terminal = FileObject->SpecialIo;
    Status = STATUS_SUCCESS;

    //
    // Remove the old handle.
    //

    KeAcquireQueuedLock(Terminal->Lock);
    if (Terminal->HardwareHandle != NULL) {
        IoClose(Terminal->HardwareHandle);
    }

    Terminal->HardwareHandle = DeviceHandle;

    //
    // If a new device is being associated with the terminal, send the settings
    // down to it now.
    //

    if (DeviceHandle != NULL) {
        Status = IoUserControl(DeviceHandle,
                               TerminalControlSetAttributes,
                               TRUE,
                               &(Terminal->Settings),
                               sizeof(TERMINAL_SETTINGS));
    }

    KeReleaseQueuedLock(Terminal->Lock);

TerminalSetDeviceEnd:
    return Status;
}

VOID
IoTerminalDisassociate (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine is called when a session leader dies to disassociate the
    terminal from the rest of the session.

Arguments:

    Process - Supplies a pointer to the session leader that has exited.

Return Value:

    None.

--*/

{

    PFILE_OBJECT FileObject;
    PTERMINAL_SLAVE Slave;
    PTERMINAL Terminal;

    if (Process->ControllingTerminal == NULL) {
        return;
    }

    ASSERT(PsIsSessionLeader(Process));
    ASSERT(Process->ThreadCount == 0);

    KeAcquireQueuedLock(IoTerminalListLock);
    FileObject = Process->ControllingTerminal;
    if (FileObject != NULL) {
        Slave = FileObject->SpecialIo;
        Terminal = Slave->Master;
        if (Terminal->ProcessGroupId != TERMINAL_INVALID_PROCESS_GROUP) {
            PsSignalProcessGroup(Terminal->ProcessGroupId,
                                 SIGNAL_CONTROLLING_TERMINAL_CLOSED);
        }

        IopTerminalDisassociate(Terminal);

        ASSERT(Process->ControllingTerminal == NULL);
    }

    KeReleaseQueuedLock(IoTerminalListLock);
    return;
}

KSTATUS
IopInitializeTerminalSupport (
    VOID
    )

/*++

Routine Description:

    This routine is called during system initialization to set up support for
    terminals.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    INITIALIZE_LIST_HEAD(&IoTerminalList);
    IoTerminalListLock = KeCreateQueuedLock();
    if (IoTerminalListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTerminalSupportEnd;
    }

    //
    // Create the Terminal object directory.
    //

    IoTerminalDirectory = ObCreateObject(ObjectDirectory,
                                         NULL,
                                         TERMINAL_DIRECTORY_NAME,
                                         sizeof(TERMINAL_DIRECTORY_NAME),
                                         sizeof(OBJECT_HEADER),
                                         NULL,
                                         OBJECT_FLAG_USE_NAME_DIRECTLY,
                                         TERMINAL_ALLOCATION_TAG);

    if (IoTerminalDirectory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTerminalSupportEnd;
    }

    //
    // Create a local console terminal.
    //

    Status = IoCreateTerminal(TRUE,
                              NULL,
                              NULL,
                              NULL,
                              0,
                              NULL,
                              0,
                              IO_ACCESS_READ | IO_ACCESS_WRITE,
                              0,
                              TERMINAL_DEFAULT_PERMISSIONS,
                              TERMINAL_DEFAULT_PERMISSIONS,
                              &IoLocalConsole);

    if (!KSUCCESS(Status)) {
        goto InitializeTerminalSupportEnd;
    }

    Status = STATUS_SUCCESS;

InitializeTerminalSupportEnd:
    return Status;
}

KSTATUS
IopTerminalOpenMaster (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine is called when a master terminal was just opened.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the terminal.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;
    PTERMINAL Terminal;

    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectTerminalMaster);

    //
    // If no access is requested, then the special I/O terminal object does not
    // need to be present, and in fact, may not be.
    //

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Terminal = FileObject->SpecialIo;
    if (Terminal == NULL) {
        Status = STATUS_NOT_READY;
        return Status;
    }

    ASSERT(Terminal->Header.Type == ObjectTerminalMaster);

    if (((Terminal->Flags & TERMINAL_FLAG_FAIL_OPENS) != 0) &&
        (!KSUCCESS(PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR)))) {

        return STATUS_RESOURCE_IN_USE;
    }

    //
    // Increment the number of parties that have the master terminal open. If
    // the initial reference taken on creation is gone, then this master
    // terminal is on its way out. That is, do not resurrect the master from 0
    // references.
    //

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(IoTerminalListLock);
    if (Terminal->MasterReferenceCount == 0) {
        Status = STATUS_NO_SUCH_FILE;

    } else {
        Terminal->MasterReferenceCount += 1;
    }

    KeReleaseQueuedLock(IoTerminalListLock);
    return Status;
}

KSTATUS
IopTerminalCloseMaster (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine is called when a master terminal was just closed.

Arguments:

    IoHandle - Supplies a pointer to the handle to close.

Return Value:

    Status code.

--*/

{

    ULONG Events;
    PFILE_OBJECT FileObject;
    PTERMINAL Terminal;

    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectTerminalMaster);

    //
    // Handles with no access never really counted and the special I/O object
    // may not have ever been present.
    //

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Terminal = FileObject->SpecialIo;

    ASSERT(Terminal->Header.Type == ObjectTerminalMaster);

    //
    // Just decrement the reference count.
    //

    KeAcquireQueuedLock(IoTerminalListLock);

    ASSERT((Terminal->MasterReferenceCount > 1) &&
           (Terminal->MasterReferenceCount < 0x10000000));

    Terminal->MasterReferenceCount -= 1;

    //
    // If this was the last reference to match an open of the master, close the
    // connection between the master and the slave.
    //

    if (Terminal->MasterReferenceCount == 1) {

        //
        // Send the foreground process group a hangup.
        //

        if (Terminal->ProcessGroupId != TERMINAL_INVALID_PROCESS_GROUP) {
            PsSignalProcessGroup(Terminal->ProcessGroupId,
                                 SIGNAL_CONTROLLING_TERMINAL_CLOSED);
        }

        //
        // Decrement the original reference, preventing any additional opens of
        // the master terminal during the destruction process. It is possible
        // that a path walk has already taken another reference on the master's
        // path entry.
        //

        Terminal->MasterReferenceCount -= 1;
        if (Terminal->SlaveFileObject != NULL) {
            Events = POLL_EVENT_IN | POLL_EVENT_OUT | POLL_EVENT_ERROR |
                     POLL_EVENT_DISCONNECTED;

            IoSetIoObjectState(Terminal->SlaveFileObject->IoState,
                               Events,
                               TRUE);
        }

        //
        // Unlink the master.
        //

        IopDeleteByHandle(TRUE, IoHandle, 0);

        //
        // Unlink the slave.
        //

        if (Terminal->SlavePathPoint.PathEntry != NULL) {
            IopDeletePathPoint(TRUE, &(Terminal->SlavePathPoint), 0);
            IO_PATH_POINT_RELEASE_REFERENCE(&(Terminal->SlavePathPoint));
            Terminal->SlavePathPoint.PathEntry = NULL;
        }

        //
        // Release the initial reference on the slave file object taken when
        // the master was created.
        //

        if (Terminal->SlaveFileObject != NULL) {
            IopFileObjectReleaseReference(Terminal->SlaveFileObject);
        }
    }

    KeReleaseQueuedLock(IoTerminalListLock);
    return STATUS_SUCCESS;
}

KSTATUS
IopTerminalOpenSlave (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine opens the slave side of a terminal object.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the terminal.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PIO_OBJECT_STATE MasterIoState;
    PKPROCESS Process;
    PTERMINAL_SLAVE Slave;
    KSTATUS Status;
    PTERMINAL Terminal;
    BOOL TerminalLockHeld;

    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectTerminalSlave);

    //
    // If the caller doesn't actually want any access, then just let it slide.
    // The special I/O object may not be initialized.
    //

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Slave = FileObject->SpecialIo;
    if (Slave == NULL) {
        return STATUS_NOT_READY;
    }

    ASSERT(Slave->Header.Type == ObjectTerminalSlave);

    TerminalLockHeld = FALSE;
    KeAcquireQueuedLock(IoTerminalListLock);

    //
    // Get the master terminal. Synchronize this to avoid situations where the
    // master gets cleaned up after this pointer is read. Also synchronize with
    // the setting of the owning process group and session. Some of the user
    // controls synchronize terminal lookups with session ownership changes.
    //

    Terminal = Slave->Master;
    if (((Terminal->Flags & TERMINAL_FLAG_FAIL_OPENS) != 0) &&
        (!KSUCCESS(PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR)))) {

        Status = STATUS_RESOURCE_IN_USE;
        goto TerminalOpenSlaveEnd;
    }

    if (IO_IS_TERMINAL_MASTER_OPEN(Terminal) != FALSE) {
        ObAddReference(Terminal);
        IopFileObjectAddReference(Terminal->MasterFileObject);

    } else {
        Terminal = NULL;
    }

    if (Terminal == NULL) {
        Status = STATUS_TOO_LATE;
        goto TerminalOpenSlaveEnd;
    }

    //
    // Synchronize the check and set of the owning process group and session
    // with other opens and requests to change the process group and session.
    //

    TerminalLockHeld = TRUE;
    KeAcquireQueuedLock(Terminal->Lock);

    //
    // If the terminal is already open in another session, refuse to open.
    //

    Process = PsGetCurrentProcess();
    Terminal->SlaveHandles += 1;

    //
    // Clear the error that may have been set when the last previous slave
    // was closed.
    //

    if (Terminal->SlaveHandles == 1) {
        MasterIoState = Terminal->MasterFileObject->IoState;
        IoSetIoObjectState(MasterIoState, POLL_EVENT_DISCONNECTED, FALSE);

        //
        // Also clear the master in event if there's nothing to actually read.
        //

        if (IopTerminalGetOutputBufferSpace(Terminal) ==
            TERMINAL_OUTPUT_BUFFER_SIZE - 1) {

            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, FALSE);
        }
    }

    //
    // Make this terminal the controlling terminal for the process if
    // 1) The no controlling terminal flag is not set.
    // 2) The terminal is not already assigned to another session.
    // 3) This process is a session leader.
    // 4) This process does not already have a controlling terminal.
    //

    if (((IoHandle->OpenFlags & OPEN_FLAG_NO_CONTROLLING_TERMINAL) == 0) &&
        (Terminal->SessionId == TERMINAL_INVALID_SESSION) &&
        (PsIsSessionLeader(Process) != FALSE) &&
        (Process->ControllingTerminal == NULL)) {

        Process->ControllingTerminal = FileObject;
        Terminal->ProcessGroupId = Process->Identifiers.ProcessGroupId;
        Terminal->SessionId = Process->Identifiers.SessionId;
    }

    Status = STATUS_SUCCESS;

TerminalOpenSlaveEnd:
    if (TerminalLockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->Lock);
    }

    KeReleaseQueuedLock(IoTerminalListLock);
    return Status;
}

KSTATUS
IopTerminalCloseSlave (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine is called when a master terminal was just closed.

Arguments:

    IoHandle - Supplies a pointer to the handle to close.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PTERMINAL_SLAVE Slave;
    PTERMINAL Terminal;

    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectTerminalSlave);

    //
    // Handles with no access never really counted and the special I/O object
    // may not have been initialized.
    //

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Slave = FileObject->SpecialIo;

    ASSERT(Slave->Header.Type == ObjectTerminalSlave);

    Terminal = Slave->Master;
    KeAcquireQueuedLock(IoTerminalListLock);
    KeAcquireQueuedLock(Terminal->Lock);

    ASSERT(Terminal->SlaveHandles != 0);

    Terminal->SlaveHandles -= 1;

    //
    // Tell the master no one's listening.
    //

    if (Terminal->SlaveHandles == 0) {
        IoSetIoObjectState(Terminal->MasterFileObject->IoState,
                           POLL_EVENT_IN | POLL_EVENT_DISCONNECTED,
                           TRUE);

        //
        // Remove the controlling terminal from the session.
        //

        IopTerminalDisassociate(Terminal);
    }

    KeReleaseQueuedLock(Terminal->Lock);
    KeReleaseQueuedLock(IoTerminalListLock);

    //
    // Release the reference on the master taken during opening, which may
    // allow the master to free itself.
    //

    ObReleaseReference(Slave->Master);
    IopFileObjectReleaseReference(Slave->Master->MasterFileObject);
    return STATUS_SUCCESS;
}

KSTATUS
IopPerformTerminalMasterIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from or writes to the master end of a terminal.

Arguments:

    Handle - Supplies a pointer to the master terminal I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;

    FileObject = Handle->FileObject;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(FileObject->Properties.Type == IoObjectTerminalMaster);

    if (IoContext->Write != FALSE) {
        Status = IopTerminalMasterWrite(FileObject, IoContext);

    } else {
        Status = IopTerminalMasterRead(FileObject, IoContext);
    }

    return Status;
}

KSTATUS
IopPerformTerminalSlaveIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from or writes to the slave end of a terminal.

Arguments:

    Handle - Supplies a pointer to the slave terminal I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    PFILE_OBJECT FileObject;
    KSTATUS Status;

    FileObject = Handle->FileObject;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(FileObject->Properties.Type == IoObjectTerminalSlave);

    if (IoContext->Write != FALSE) {
        Status = IopTerminalSlaveWrite(FileObject, IoContext);

    } else {
        Status = IopTerminalSlaveRead(FileObject, IoContext);
    }

    return Status;
}

KSTATUS
IopCreateTerminal (
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine creates a terminal master or slave.

Arguments:

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to the new file object
        will be returned on success.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PTERMINAL_CREATION_PARAMETERS CreationParameters;
    PLIST_ENTRY CurrentEntry;
    BOOL ListLockHeld;
    CHAR Name[TERMINAL_MAX_NAME_LENGTH];
    ULONG NameLength;
    PFILE_OBJECT NewFileObject;
    ULONG Number;
    FILE_PROPERTIES Properties;
    KSTATUS Status;
    PTERMINAL Terminal;
    PTERMINAL TerminalAfter;
    PLIST_ENTRY TerminalAfterEntry;

    CreationParameters = Create->Context;
    ListLockHeld = FALSE;

    //
    // If the object came up from out of the file system, don't actually
    // create anything. The common case here is querying file properties.
    //

    if (CreationParameters == NULL) {

        ASSERT(*FileObject != NULL);

        Status = STATUS_SUCCESS;
        goto CreateTerminalEnd;
    }

    NewFileObject = NULL;

    //
    // Create the slave file object.
    //

    if (Create->Type == IoObjectTerminalSlave) {

        ASSERT(CreationParameters->Master != NULL);

        Terminal = CreationParameters->Master;

        ASSERT(Terminal->SlaveFileObject == NULL);

        //
        // Create a new file object if there isn't one already.
        //

        if (*FileObject == NULL) {
            IopFillOutFilePropertiesForObject(&Properties,
                                              &(Terminal->Slave->Header));

            Properties.Type = IoObjectTerminalSlave;
            Properties.Permissions = Create->Permissions;
            Status = IopCreateOrLookupFileObject(&Properties,
                                                 ObGetRootObject(),
                                                 0,
                                                 0,
                                                 &NewFileObject,
                                                 &Created);

            if (!KSUCCESS(Status)) {

                //
                // Release the reference from when the properties were filled
                // out above.
                //

                ObReleaseReference(Terminal->Slave);
                goto CreateTerminalEnd;
            }

            ASSERT(Created != FALSE);

            *FileObject = NewFileObject;

            //
            // With the file object created, but not yet ready, go ahead and
            // name the terminal slave object. Once it has a name it can be
            // found by other threads via path lookup, but those threads will
            // have to wait on the file object's ready event before proceeding.
            //

            ASSERT(Terminal->Number != MAX_ULONG);

            //
            // Create the terminal name string (on the stack, it gets copied by
            // the object manager) and then set the name in the object.
            //

            NameLength = RtlPrintToString(Name,
                                          TERMINAL_MAX_NAME_LENGTH,
                                          CharacterEncodingDefault,
                                          TERMINAL_SLAVE_NAME_FORMAT,
                                          Terminal->Number);

            Status = ObNameObject(Terminal->Slave,
                                  Name,
                                  NameLength,
                                  TERMINAL_ALLOCATION_TAG,
                                  FALSE);

            if (!KSUCCESS(Status)) {

                ASSERT(Status != STATUS_TOO_LATE);

                goto CreateTerminalEnd;
            }
        }

        //
        // Add a reference since the master holds a reference to the slave
        // file object.
        //

        IopFileObjectAddReference(*FileObject);

        //
        // By setting the slave file object to non-null, this code is
        // transferring the reference originally held by the master when the
        // slave was created over to the file object special I/O field.
        //

        Terminal->SlaveFileObject = *FileObject;

        ASSERT((*FileObject)->SpecialIo == NULL);

        (*FileObject)->SpecialIo = Terminal->Slave;

    //
    // Create a master, which creates the slave object as well.
    //

    } else {

        ASSERT(Create->Type == IoObjectTerminalMaster);
        ASSERT(CreationParameters->Master == NULL);

        Terminal = NULL;

        //
        // Create the terminal object. This reference will get transferred to
        // the file object special I/O field on success.
        //

        Status = IopCreateTerminalObject(
                                    CreationParameters->SlaveCreatePermissions,
                                    &Terminal);

        if (!KSUCCESS(Status)) {
            goto CreateTerminalEnd;
        }

        //
        // Create a file object if necessary. This adds a reference on the
        // object.
        //

        if (*FileObject == NULL) {
            IopFillOutFilePropertiesForObject(&Properties, &(Terminal->Header));
            Properties.Type = IoObjectTerminalMaster;
            Properties.Permissions = Create->Permissions;
            Status = IopCreateOrLookupFileObject(&Properties,
                                                 ObGetRootObject(),
                                                 0,
                                                 0,
                                                 &NewFileObject,
                                                 &Created);

            if (!KSUCCESS(Status)) {

                //
                // Release both the references taken by creating the object and
                // filling out the file properties.
                //

                ObReleaseReference(Terminal);
                ObReleaseReference(Terminal);
                goto CreateTerminalEnd;
            }

            ASSERT(Created != FALSE);

            *FileObject = NewFileObject;

            //
            // With the file object created, but not yet ready, go ahead and
            // name the terminal master object. Once it has a name it can be
            // found by other threads via path lookup, but those threads will
            // have to wait on the file object's ready event before proceeding.
            //

            Number = 0;
            KeAcquireQueuedLock(IoTerminalListLock);
            ListLockHeld = TRUE;
            LIST_REMOVE(&(Terminal->ListEntry));
            CurrentEntry = IoTerminalList.Next;
            TerminalAfterEntry = &IoTerminalList;
            while (CurrentEntry != &IoTerminalList) {
                TerminalAfter = LIST_VALUE(CurrentEntry, TERMINAL, ListEntry);

                //
                // Assert that the list is in order.
                //

                ASSERT(TerminalAfter->Number >= Number);

                if (TerminalAfter->Number == Number) {
                    Number += 1;

                } else {
                    TerminalAfterEntry = CurrentEntry;
                    break;
                }

                CurrentEntry = CurrentEntry->Next;
            }

            //
            // Create the terminal name string (on the stack, it gets copied by
            // the object manager) and then set the name in the object.
            //

            NameLength = RtlPrintToString(Name,
                                          TERMINAL_MAX_NAME_LENGTH,
                                          CharacterEncodingDefault,
                                          TERMINAL_MASTER_NAME_FORMAT,
                                          Number);

            Status = ObNameObject(Terminal,
                                  Name,
                                  NameLength,
                                  TERMINAL_ALLOCATION_TAG,
                                  FALSE);

            if (!KSUCCESS(Status)) {

                ASSERT(Status != STATUS_TOO_LATE);

                goto CreateTerminalEnd;
            }

            ASSERT(Terminal->Number == MAX_ULONG);

            Terminal->Number = Number;
            INSERT_BEFORE(&(Terminal->ListEntry), TerminalAfterEntry);
            KeReleaseQueuedLock(IoTerminalListLock);
            ListLockHeld = FALSE;
        }

        ASSERT((*FileObject)->Properties.Type == IoObjectTerminalMaster);

        Terminal->MasterFileObject = *FileObject;
        CreationParameters->Master = Terminal;

        ASSERT((*FileObject)->SpecialIo == NULL);

        (*FileObject)->SpecialIo = Terminal;
    }

    Create->Created = TRUE;
    Status = STATUS_SUCCESS;

CreateTerminalEnd:
    if (ListLockHeld != FALSE) {
        KeReleaseQueuedLock(IoTerminalListLock);
    }

    //
    // On both success and failure, the file object's ready event needs to be
    // signaled. Other threads may be waiting on the event.
    //

    if (*FileObject != NULL) {

        ASSERT((KeGetEventState((*FileObject)->ReadyEvent) == NotSignaled) ||
               (KeGetEventState((*FileObject)->ReadyEvent) ==
                NotSignaledWithWaiters));

        KeSignalEvent((*FileObject)->ReadyEvent, SignalOptionSignalAll);
    }

    return Status;
}

KSTATUS
IopUnlinkTerminal (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine unlinks a terminal from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the terminal's file object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        terminal was successfully unlinked.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    POBJECT_HEADER Terminal;

    ASSERT((FileObject->Properties.Type == IoObjectTerminalMaster) ||
           (FileObject->Properties.Type == IoObjectTerminalSlave));

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    Terminal = FileObject->SpecialIo;

    ASSERT(Terminal != NULL);

    *Unlinked = FALSE;
    Status = ObUnlinkObject(Terminal);
    if (KSUCCESS(Status)) {
        *Unlinked = TRUE;
    }

    return Status;
}

KSTATUS
IopTerminalUserControl (
    PIO_HANDLE Handle,
    TERMINAL_USER_CONTROL_CODE CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests destined for a terminal object.

Arguments:

    Handle - Supplies the open file handle.

    CodeNumber - Supplies the minor code of the request.

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
    IO_CONTEXT Context;
    PROCESS_GROUP_ID CurrentProcessGroupId;
    SESSION_ID CurrentSessionId;
    PFILE_OBJECT FileObject;
    KSTATUS HardwareStatus;
    UINTN Index;
    BOOL InSession;
    IO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    INT ModemStatus;
    TERMINAL_SETTINGS_OLD OldSettings;
    PKPROCESS Process;
    PROCESS_GROUP_ID ProcessGroupId;
    INT QueueSize;
    SESSION_ID SessionId;
    TERMINAL_SETTINGS Settings;
    KSTATUS Status;
    PTERMINAL Terminal;
    PTERMINAL_SLAVE TerminalSlave;
    TERMINAL_CHANGE_BEHAVIOR When;
    TERMINAL_WINDOW_SIZE WindowSize;

    TerminalSlave = NULL;
    FileObject = Handle->FileObject;
    if (FileObject->Properties.Type == IoObjectTerminalMaster) {
        Terminal = FileObject->SpecialIo;

    } else if (FileObject->Properties.Type == IoObjectTerminalSlave) {
        TerminalSlave = FileObject->SpecialIo;
        Terminal = TerminalSlave->Master;

    } else {
        return STATUS_NOT_A_TERMINAL;
    }

    switch (CodeNumber) {
    case TerminalControlGetAttributes:
        Status = IoSetTerminalSettings(Handle,
                                       NULL,
                                       &Settings,
                                       TerminalChangeNone);

        if (!KSUCCESS(Status)) {
            break;
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &Settings,
                                           sizeof(TERMINAL_SETTINGS));

        break;

    case TerminalControlSetAttributes:
    case TerminalControlSetAttributesDrain:
    case TerminalControlSetAttributesFlush:
        if (CodeNumber == TerminalControlSetAttributes) {
            When = TerminalChangeNow;

        } else if (CodeNumber == TerminalControlSetAttributesDrain) {
            When = TerminalChangeAfterOutput;

        } else {

            ASSERT(CodeNumber == TerminalControlSetAttributesFlush);

            When = TerminalChangeAfterOutputFlushInput;
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &Settings,
                                           sizeof(TERMINAL_SETTINGS));

        if (!KSUCCESS(Status)) {
            break;
        }

        Status = IoSetTerminalSettings(Handle, &Settings, NULL, When);
        break;

    case TerminalControlGetAttributesOld:
        Status = IoSetTerminalSettings(Handle,
                                       NULL,
                                       &Settings,
                                       TerminalChangeNone);

        if (!KSUCCESS(Status)) {
            break;
        }

        RtlZeroMemory(&OldSettings, sizeof(TERMINAL_SETTINGS_OLD));
        OldSettings.InputFlags = Settings.InputFlags;
        OldSettings.OutputFlags = Settings.OutputFlags;
        OldSettings.ControlFlags = Settings.ControlFlags;
        OldSettings.LocalFlags = Settings.LocalFlags;
        OldSettings.LineDiscipline = 0;
        for (Index = 0;
             Index < TERMINAL_SETTINGS_OLD_CONTROL_COUNT;
             Index += 1) {

            OldSettings.ControlCharacters[Index] =
                                             Settings.ControlCharacters[Index];
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &OldSettings,
                                           sizeof(TERMINAL_SETTINGS_OLD));

        break;

    case TerminalControlSetAttributesOld:
    case TerminalControlSetAttributesDrainOld:
    case TerminalControlSetAttributesFlushOld:
        if (CodeNumber == TerminalControlSetAttributesOld) {
            When = TerminalChangeNow;

        } else if (CodeNumber == TerminalControlSetAttributesDrainOld) {
            When = TerminalChangeAfterOutput;

        } else {

            ASSERT(CodeNumber == TerminalControlSetAttributesFlushOld);

            When = TerminalChangeAfterOutputFlushInput;
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &OldSettings,
                                           sizeof(TERMINAL_SETTINGS_OLD));

        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // Get the current settings, and copy the old to the new.
        //

        Status = IoSetTerminalSettings(Handle,
                                       NULL,
                                       &Settings,
                                       TerminalChangeNone);

        if (!KSUCCESS(Status)) {
            break;
        }

        Settings.InputFlags = OldSettings.InputFlags;
        Settings.OutputFlags = OldSettings.OutputFlags;
        Settings.ControlFlags = OldSettings.ControlFlags;
        Settings.LocalFlags = OldSettings.LocalFlags;
        for (Index = 0;
             Index < TERMINAL_SETTINGS_OLD_CONTROL_COUNT;
             Index += 1) {

            Settings.ControlCharacters[Index] =
                                          OldSettings.ControlCharacters[Index];
        }

        //
        // Set the new settings.
        //

        Status = IoSetTerminalSettings(Handle, &Settings, NULL, When);
        break;

    case TerminalControlSendBreak:

        //
        // The integer argument is the pointer itself.
        //

        Argument = (UINTN)ContextBuffer;
        if (Argument == 0) {
            Status = STATUS_SUCCESS;

        } else {

            //
            // A non-zero argument is undefined. Act like "drain" here, and
            // wait for all output to complete.
            //

            Status = IopTerminalFlush(FileObject, FLUSH_FLAG_WRITE);
        }

        break;

    case TerminalControlFlowControl:
        Status = STATUS_SUCCESS;
        break;

    case TerminalControlFlush:

        //
        // The argument is an integer.
        //

        Argument = (UINTN)ContextBuffer;
        Argument &= FLUSH_FLAG_READ | FLUSH_FLAG_WRITE;
        Argument |= FLUSH_FLAG_DISCARD;
        Status = IopTerminalFlush(FileObject, Argument);
        break;

    case TerminalControlSetExclusive:
    case TerminalControlClearExclusive:
        KeAcquireQueuedLock(Terminal->Lock);
        if (CodeNumber == TerminalControlSetExclusive) {
            Terminal->Flags |= TERMINAL_FLAG_FAIL_OPENS;

        } else {
            Terminal->Flags &= ~TERMINAL_FLAG_FAIL_OPENS;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        Status = STATUS_SUCCESS;
        break;

    case TerminalControlGetOutputQueueSize:
    case TerminalControlGetInputQueueSize:
        KeAcquireQueuedLock(Terminal->Lock);
        if (CodeNumber == TerminalControlGetOutputQueueSize) {
            QueueSize = (TERMINAL_OUTPUT_BUFFER_SIZE - 1) -
                        IopTerminalGetOutputBufferSpace(Terminal);

        } else {
            QueueSize = (TERMINAL_INPUT_BUFFER_SIZE - 1) -
                        IopTerminalGetInputBufferSpace(Terminal);
        }

        KeReleaseQueuedLock(Terminal->Lock);
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &QueueSize,
                                           sizeof(INT));

        break;

    case TerminalControlInsertInInputQueue:
        IoBufferFlags = 0;
        if (FromKernelMode != FALSE) {
            IoBufferFlags |= IO_BUFFER_FLAG_KERNEL_MODE_DATA;
        }

        Status = MmInitializeIoBuffer(&IoBuffer,
                                      ContextBuffer,
                                      INVALID_PHYSICAL_ADDRESS,
                                      1,
                                      IoBufferFlags);

        if (!KSUCCESS(Status)) {
            break;
        }

        Context.IoBuffer = &IoBuffer;
        Context.SizeInBytes = 1;
        Context.Flags = 0;
        Context.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
        Status = IopTerminalMasterWrite(Terminal->MasterFileObject, &Context);
        break;

    case TerminalControlGetWindowSize:
        KeAcquireQueuedLock(Terminal->Lock);
        RtlCopyMemory(&WindowSize,
                      &(Terminal->WindowSize),
                      sizeof(TERMINAL_WINDOW_SIZE));

        KeReleaseQueuedLock(Terminal->Lock);
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &WindowSize,
                                           sizeof(TERMINAL_WINDOW_SIZE));

        break;

    case TerminalControlSetWindowSize:
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &WindowSize,
                                           sizeof(TERMINAL_WINDOW_SIZE));

        if (!KSUCCESS(Status)) {
            break;
        }

        KeAcquireQueuedLock(Terminal->Lock);
        RtlCopyMemory(&(Terminal->WindowSize),
                      &WindowSize,
                      sizeof(TERMINAL_WINDOW_SIZE));

        KeReleaseQueuedLock(Terminal->Lock);
        break;

    case TerminalControlGetModemStatus:
    case TerminalControlOrModemStatus:
    case TerminalControlClearModemStatus:
    case TerminalControlSetModemStatus:
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &ModemStatus,
                                           sizeof(INT));

        if (!KSUCCESS(Status)) {
            break;
        }

        KeAcquireQueuedLock(Terminal->Lock);
        if (CodeNumber == TerminalControlOrModemStatus) {
            Terminal->ModemStatus |= ModemStatus;

        } else if (CodeNumber == TerminalControlClearModemStatus) {
            Terminal->ModemStatus &= ~ModemStatus;

        } else if (CodeNumber == TerminalControlSetModemStatus) {
            Terminal->ModemStatus = ModemStatus;
        }

        ModemStatus = Terminal->ModemStatus;
        KeReleaseQueuedLock(Terminal->Lock);
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &ModemStatus,
                                           sizeof(INT));

        break;

    case TerminalControlGetSoftCarrier:
    case TerminalControlSetSoftCarrier:
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &Argument,
                                           sizeof(INT));

        if (!KSUCCESS(Status)) {
            break;
        }

        KeAcquireQueuedLock(Terminal->Lock);
        if (CodeNumber == TerminalControlSetSoftCarrier) {
            if (Argument != 0) {
                Terminal->Settings.ControlFlags |= TERMINAL_CONTROL_NO_HANGUP;

            } else {
                Terminal->Settings.ControlFlags &= ~TERMINAL_CONTROL_NO_HANGUP;
            }
        }

        Argument = FALSE;
        if ((Terminal->Settings.ControlFlags & TERMINAL_CONTROL_NO_HANGUP) !=
            0) {

            Argument = TRUE;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &Argument,
                                           sizeof(INT));

        break;

    case TerminalControlGetProcessGroup:

        //
        // The given terminal must be the controlling terminal of the calling
        // process.
        //

        PsGetProcessGroup(NULL, &CurrentProcessGroupId, &CurrentSessionId);
        Status = STATUS_SUCCESS;
        KeAcquireQueuedLock(Terminal->Lock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        } else {
            ProcessGroupId = Terminal->ProcessGroupId;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        if (!KSUCCESS(Status)) {
            break;
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &ProcessGroupId,
                                           sizeof(PROCESS_GROUP_ID));

        break;

    case TerminalControlSetProcessGroup:
        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           TRUE,
                                           ContextBuffer,
                                           &ProcessGroupId,
                                           sizeof(PROCESS_GROUP_ID));

        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // If the terminal does not have the slave side attached or does not
        // belong to the calling session, then the process does not have
        // permission to update its process group.
        //

        PsGetProcessGroup(NULL, &CurrentProcessGroupId, &CurrentSessionId);

        //
        // The given terminal must be in the current session.
        //

        InSession = PsIsProcessGroupInSession(ProcessGroupId, CurrentSessionId);
        if (InSession == FALSE) {
            Status = STATUS_PERMISSION_DENIED;
            break;
        }

        KeAcquireQueuedLock(Terminal->Lock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        //
        // If the calling process is not in the owning (foreground) process
        // group, then it is sent a signal unless it is blocking or ignoring
        // the background terminal output signal.
        //

        } else {
            Status = IopTerminalValidateGroup(Terminal, FALSE);
            if (!KSUCCESS(Status)) {
                if (Status == STATUS_DEVICE_IO_ERROR) {
                    Status = STATUS_NOT_A_TERMINAL;
                }

                break;
            }

            Terminal->ProcessGroupId = ProcessGroupId;
            Status = STATUS_SUCCESS;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        break;

    case TerminalControlSetControllingTerminal:
        Argument = (UINTN)ContextBuffer;
        Process = PsGetCurrentProcess();

        //
        // If this process is not a session leader or it has a controlling
        // terminal already, fail.
        //

        if ((PsIsSessionLeader(Process) == FALSE) ||
            (Process->ControllingTerminal != NULL)) {

            Status = STATUS_PERMISSION_DENIED;
            break;
        }

        //
        // If this handle is only open for write and the caller isn't an
        // administrator, fail.
        //

        if ((Handle->Access & IO_ACCESS_READ) == 0) {
            Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // If the terminal already belongs to a different session, then it
        // cannot bet set as the controlling terminal of this session unless
        // the caller is root and the argument is 1.
        //

        SessionId = Terminal->SessionId;
        CurrentSessionId = Process->Identifiers.SessionId;
        if (SessionId != TERMINAL_INVALID_SESSION) {
            if (SessionId == CurrentSessionId) {
                Status = STATUS_SUCCESS;
                break;
            }

            //
            // Allow root to steal terminals from different session if the
            // argument is non-zero.
            //

            Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
            if ((!KSUCCESS(Status)) || (Argument == 0)) {
                Status = STATUS_PERMISSION_DENIED;
                break;
            }
        }

        KeAcquireQueuedLock(IoTerminalListLock);
        KeAcquireQueuedLock(Terminal->Lock);

        //
        // Double check the controlling terminal now that the terminal list
        // lock protecting it is held.
        //

        if (Process->ControllingTerminal != NULL) {
            Status = STATUS_PERMISSION_DENIED;

        //
        // If the session changed between the unlocked check and now, fail.
        //

        } else if (Terminal->SessionId != SessionId) {
            Status = STATUS_TRY_AGAIN;

        //
        // Everyone that had the terminal as their controlling terminal no
        // longer does.
        //

        } else {
            IopTerminalDisassociate(Terminal);
            Process->ControllingTerminal = Terminal->SlaveFileObject;
            Terminal->SessionId = CurrentSessionId;
            Terminal->ProcessGroupId = Process->Identifiers.ProcessGroupId;
            Status = STATUS_SUCCESS;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        KeReleaseQueuedLock(IoTerminalListLock);
        break;

    case TerminalControlGetCurrentSessionId:
        Process = PsGetCurrentProcess();
        if ((FileObject->Properties.Type != IoObjectTerminalMaster) &&
            (Process->ControllingTerminal != FileObject)) {

            Status = STATUS_NOT_A_TERMINAL;
            break;
        }

        //
        // The given terminal must be the controlling terminal of the calling
        // process.
        //

        Process = PsGetCurrentProcess();
        CurrentSessionId = Process->Identifiers.SessionId;
        KeAcquireQueuedLock(Terminal->Lock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        } else {
            SessionId = Terminal->SessionId;
            Status = STATUS_SUCCESS;
        }

        KeReleaseQueuedLock(Terminal->Lock);
        if (!KSUCCESS(Status)) {
            break;
        }

        Status = IopTerminalUserBufferCopy(FromKernelMode,
                                           FALSE,
                                           ContextBuffer,
                                           &SessionId,
                                           sizeof(SESSION_ID));

        break;

    case TerminalControlGiveUpControllingTerminal:
        Process = PsGetCurrentProcess();

        //
        // The controlling terminal is protected by the terminal list lock.
        //

        KeAcquireQueuedLock(IoTerminalListLock);
        KeAcquireQueuedLock(Terminal->Lock);
        if (Process->ControllingTerminal != Terminal->SlaveFileObject) {
            Status = STATUS_NOT_A_TERMINAL;

        } else {
            Status = STATUS_SUCCESS;
            if (PsIsSessionLeader(Process) != FALSE) {
                PsSignalProcessGroup(Terminal->ProcessGroupId,
                                     SIGNAL_CONTROLLING_TERMINAL_CLOSED);

                PsSignalProcessGroup(Terminal->ProcessGroupId,
                                     SIGNAL_CONTINUE);

                IopTerminalDisassociate(Terminal);
            }
        }

        KeReleaseQueuedLock(Terminal->Lock);
        KeReleaseQueuedLock(IoTerminalListLock);
        break;

    case TerminalControlRedirectLocalConsole:
    case TerminalControlSetPacketMode:

        ASSERT(FALSE);

        Status = STATUS_NOT_IMPLEMENTED;
        break;

    case TerminalControlSendBreakPosix:
    case TerminalControlStartBreak:
    case TerminalControlStopBreak:
        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    //
    // Also forward the request on to the physical device if there is one.
    //

    if ((KSUCCESS(Status)) && (Terminal->HardwareHandle != NULL)) {
        HardwareStatus = IoUserControl(Terminal->HardwareHandle,
                                       CodeNumber,
                                       FromKernelMode,
                                       ContextBuffer,
                                       ContextBufferSize);

        if (HardwareStatus != STATUS_NOT_HANDLED) {
            Status = HardwareStatus;
        }
    }

    return Status;
}

KSTATUS
IopTerminalFlush (
    PFILE_OBJECT FileObject,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes a terminal object, discarding unwritten and unread
    data.

Arguments:

    FileObject - Supplies a pointer to the terminal to flush.

    Flags - Supplies the flags governing the flush operation. See FLUSH_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    PIO_OBJECT_STATE MasterIoState;
    PROCESS_GROUP_ID ProcessGroup;
    SESSION_ID Session;
    PTERMINAL_SLAVE Slave;
    PIO_OBJECT_STATE SlaveIoState;
    KSTATUS Status;
    PTERMINAL Terminal;

    if (FileObject->Properties.Type == IoObjectTerminalSlave) {
        Slave = FileObject->SpecialIo;
        Terminal = NULL;

        ASSERT(Slave->Header.Type == ObjectTerminalSlave);

        Terminal = Slave->Master;
        if (IO_IS_TERMINAL_MASTER_OPEN(Terminal) == FALSE) {
            Status = STATUS_END_OF_FILE;
            goto TerminalFlushEnd;
        }

    } else {
        Terminal = FileObject->SpecialIo;
    }

    if (Terminal->SlaveFileObject == NULL) {
        Status = STATUS_NOT_FOUND;
        goto TerminalFlushEnd;
    }

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    MasterIoState = Terminal->MasterFileObject->IoState;
    PsGetProcessGroup(NULL, &ProcessGroup, &Session);
    if (Terminal->SlaveHandles == 0) {
        Status = STATUS_NOT_READY;
        goto TerminalFlushEnd;
    }

    //
    // Make sure this process can currently write to this terminal.
    //

    Status = IopTerminalValidateGroup(Terminal, FALSE);
    if (!KSUCCESS(Status)) {
        goto TerminalFlushEnd;
    }

    //
    // If discarding, reset the buffers.
    //

    if ((Flags & FLUSH_FLAG_DISCARD) != 0) {
        if ((Flags & FLUSH_FLAG_READ) != 0) {
            KeAcquireQueuedLock(Terminal->Lock);
            Terminal->InputBufferStart = 0;
            Terminal->InputBufferEnd = 0;
            IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, FALSE);
            Terminal->WorkingInputCursor = 0;
            Terminal->WorkingInputLength = 0;
            KeReleaseQueuedLock(Terminal->Lock);
        }

        if ((Flags & FLUSH_FLAG_WRITE) != 0) {
            KeAcquireQueuedLock(Terminal->Lock);
            Terminal->OutputBufferStart = 0;
            Terminal->OutputBufferEnd = 0;
            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, FALSE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, TRUE);
            KeReleaseQueuedLock(Terminal->Lock);
        }

    //
    // If draining, wait for the output to go through.
    //

    } else {

        //
        // It doesn't make sense for the caller to try to flush a read, as
        // they're the ones that need to flush.
        //

        if ((Flags & FLUSH_FLAG_READ) != 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto TerminalFlushEnd;
        }

        //
        // Wait for the write buffer to become empty.
        //

        if ((Flags & FLUSH_FLAG_WRITE) != 0) {
            Status = STATUS_SUCCESS;
            while (KSUCCESS(Status)) {
                KeAcquireQueuedLock(Terminal->Lock);

                //
                // If the output is empty, then hooray, it's done.
                //

                if (Terminal->OutputBufferStart == Terminal->OutputBufferEnd) {
                    KeReleaseQueuedLock(Terminal->Lock);
                    break;
                }

                //
                // Hijack the out event and unsignal it. When the master reads
                // the data, it will signal it again.
                //

                IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
                KeReleaseQueuedLock(Terminal->Lock);
                Status = KeWaitForEvent(SlaveIoState->WriteEvent,
                                        TRUE,
                                        WAIT_TIME_INDEFINITE);
            }

            if (!KSUCCESS(Status)) {
                goto TerminalFlushEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

TerminalFlushEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopCreateTerminalObject (
    FILE_PERMISSIONS CreatePermissions,
    PTERMINAL *NewTerminal
    )

/*++

Routine Description:

    This routine creates a new terminal object.

Arguments:

    CreatePermissions - Supplies the initial permissions to set on the slave
        file object.

    NewTerminal - Supplies a pointer where a pointer to a new terminal will be
        returned on success.

Return Value:

    Status code.

--*/

{

    PCHAR ControlCharacters;
    PTERMINAL_SLAVE Slave;
    KSTATUS Status;
    PTERMINAL Terminal;

    //
    // Create the terminal object. This references goes to the special I/O
    // member of the file object on success.
    //

    Terminal = ObCreateObject(ObjectTerminalMaster,
                              IoTerminalDirectory,
                              NULL,
                              0,
                              sizeof(TERMINAL),
                              IopDestroyTerminal,
                              0,
                              TERMINAL_ALLOCATION_TAG);

    if (Terminal == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    //
    // Initialize the terminal with an invalid number. The number is only used
    // by terminals that are named in the terminal directory. Naming a terminal
    // happens later with the appropriate synchronization.
    //

    Terminal->Number = MAX_ULONG;

    //
    // Set the master reference count to 1. This helps determine when the
    // master is last closed by preventing new opens from succeeding if the
    // master's reference goes to 0.
    //

    Terminal->MasterReferenceCount = 1;

    //
    // Allocate the input buffers.
    //

    Terminal->InputBuffer = MmAllocatePagedPool(TERMINAL_INPUT_BUFFER_SIZE,
                                                TERMINAL_ALLOCATION_TAG);

    if (Terminal->InputBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    RtlZeroMemory(Terminal->InputBuffer, TERMINAL_INPUT_BUFFER_SIZE);
    Terminal->WorkingInputBuffer = MmAllocatePagedPool(
                                                TERMINAL_CANONICAL_BUFFER_SIZE,
                                                TERMINAL_ALLOCATION_TAG);

    if (Terminal->WorkingInputBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    Terminal->Lock = KeCreateQueuedLock();
    if (Terminal->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    //
    // Allocate the output buffers.
    //

    Terminal->OutputBuffer = MmAllocatePagedPool(TERMINAL_OUTPUT_BUFFER_SIZE,
                                                 TERMINAL_ALLOCATION_TAG);

    if (Terminal->OutputBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    //
    // Set some default flags.
    //

    Terminal->Settings.LocalFlags = TERMINAL_LOCAL_ECHO |
                                    TERMINAL_LOCAL_ECHO_ERASE |
                                    TERMINAL_LOCAL_ECHO_KILL_NEWLINE |
                                    TERMINAL_LOCAL_ECHO_KILL_EXTENDED |
                                    TERMINAL_LOCAL_ECHO_NEWLINE |
                                    TERMINAL_LOCAL_ECHO_CONTROL |
                                    TERMINAL_LOCAL_CANONICAL |
                                    TERMINAL_LOCAL_SIGNALS;

    Terminal->Settings.InputFlags = TERMINAL_INPUT_CR_TO_NEWLINE |
                                    TERMINAL_INPUT_MAX_BELL;

    Terminal->Settings.OutputFlags = TERMINAL_OUTPUT_POST_PROCESS |
                                     TERMINAL_OUTPUT_NEWLINE_TO_CRLF;

    Terminal->Settings.ControlFlags = TERMINAL_CONTROL_8_BITS_PER_CHARACTER;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    ControlCharacters[TerminalCharacterEndOfFile] =
                                                  TERMINAL_DEFAULT_END_OF_FILE;

    ControlCharacters[TerminalCharacterEndOfLine] =
                                                  TERMINAL_DEFAULT_END_OF_LINE;

    ControlCharacters[TerminalCharacterErase] = TERMINAL_DEFAULT_ERASE;
    ControlCharacters[TerminalCharacterInterrupt] = TERMINAL_DEFAULT_INTERRUPT;
    ControlCharacters[TerminalCharacterKill] = TERMINAL_DEFAULT_KILL;
    ControlCharacters[TerminalCharacterQuit] = TERMINAL_DEFAULT_QUIT;
    ControlCharacters[TerminalCharacterSuspend] = TERMINAL_DEFAULT_SUSPEND;
    ControlCharacters[TerminalCharacterStart] = TERMINAL_DEFAULT_START;
    ControlCharacters[TerminalCharacterStop] = TERMINAL_DEFAULT_STOP;
    ControlCharacters[TerminalCharacterFlushCount] = 1;
    ControlCharacters[TerminalCharacterFlushTime] = 0;
    Terminal->Settings.InputSpeed = TERMINAL_DEFAULT_BAUD_RATE;
    Terminal->Settings.OutputSpeed = TERMINAL_DEFAULT_BAUD_RATE;
    Terminal->WindowSize.Rows = TERMINAL_DEFAULT_ROWS;
    Terminal->WindowSize.Columns = TERMINAL_DEFAULT_COLUMNS;

    //
    // Initialize the owning session and process group.
    //

    Terminal->SessionId = TERMINAL_INVALID_SESSION;
    Terminal->ProcessGroupId = TERMINAL_INVALID_PROCESS_GROUP;

    //
    // Create the corresponding slave object.
    //

    Status = STATUS_SUCCESS;
    Slave = ObCreateObject(ObjectTerminalSlave,
                           IoTerminalDirectory,
                           NULL,
                           0,
                           sizeof(TERMINAL_SLAVE),
                           NULL,
                           0,
                           TERMINAL_ALLOCATION_TAG);

    if (Slave == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    //
    // Wire the master and slave together.
    //

    Terminal->Slave = Slave;
    Slave->Master = Terminal;

    //
    // Add the terminal to the end of the list.
    //

    KeAcquireQueuedLock(IoTerminalListLock);
    INSERT_BEFORE(&(Terminal->ListEntry), &IoTerminalList);
    KeReleaseQueuedLock(IoTerminalListLock);
    Status = STATUS_SUCCESS;

CreateTerminalObjectEnd:
    if (!KSUCCESS(Status)) {
        if (Terminal != NULL) {
            ObReleaseReference(Terminal);
            Terminal = NULL;
        }
    }

    *NewTerminal = Terminal;
    return Status;
}

VOID
IopDestroyTerminal (
    PVOID TerminalObject
    )

/*++

Routine Description:

    This routine is called when an terminal master's reference count drops to
    zero. It destroys all resources associated with the terminal. This occurs
    well after all the slave has been freed.

Arguments:

    TerminalObject - Supplies a pointer to the terminal object being destroyed.

Return Value:

    None.

--*/

{

    PTERMINAL Terminal;

    Terminal = (PTERMINAL)TerminalObject;

    ASSERT(Terminal->SlavePathPoint.PathEntry == NULL);

    //
    // If the slave never got a file object, then the master still has a
    // reference on the slave it needs to release.
    //

    if (Terminal->SlaveFileObject == NULL) {
        ObReleaseReference(Terminal->Slave);
    }

    if (Terminal->ListEntry.Next != NULL) {
        KeAcquireQueuedLock(IoTerminalListLock);
        LIST_REMOVE(&(Terminal->ListEntry));
        KeReleaseQueuedLock(IoTerminalListLock);
    }

    if (Terminal->HardwareHandle != NULL) {
        IoClose(Terminal->HardwareHandle);
        Terminal->HardwareHandle = NULL;
    }

    if (Terminal->InputBuffer != NULL) {
        MmFreePagedPool(Terminal->InputBuffer);
    }

    if (Terminal->WorkingInputBuffer != NULL) {
        MmFreePagedPool(Terminal->WorkingInputBuffer);
    }

    if (Terminal->OutputBuffer != NULL) {
        MmFreePagedPool(Terminal->OutputBuffer);
    }

    if (Terminal->Lock != NULL) {
        KeDestroyQueuedLock(Terminal->Lock);
    }

    return;
}

KSTATUS
IopTerminalMasterWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine writes data to the terminal slave (data that will come out
    the slave's standard input).

Arguments:

    FileObject - Supplies a pointer to the terminal master file object.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    BOOL AddCharacter;
    UCHAR Byte;
    UINTN ByteIndex;
    UCHAR Bytes[2];
    UINTN BytesSize;
    BOOL CharacterHandled;
    PCHAR ControlCharacters;
    ULONG DirtyRegionBegin;
    ULONG DirtyRegionEnd;
    ULONG EchoFlags;
    ULONG EchoMask;
    BOOL EchoThisCharacter;
    BOOL InputAdded;
    ULONG InputFlags;
    BOOL IsEndOfLine;
    UINTN LocalByteIndex;
    CHAR LocalBytes[64];
    UINTN LocalByteSize;
    ULONG LocalFlags;
    BOOL LockHeld;
    PIO_OBJECT_STATE MasterIoState;
    ULONG MoveIndex;
    BOOL OutputWritten;
    ULONG ReturnedEvents;
    ULONG ScreenCursorPosition;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;
    PTERMINAL Terminal;
    ULONG TimeoutInMilliseconds;
    BOOL TransferWorkingBuffer;

    Terminal = FileObject->SpecialIo;

    ASSERT(Terminal->Header.Type == ObjectTerminalMaster);
    ASSERT(FileObject == Terminal->MasterFileObject);

    MasterIoState = FileObject->IoState;
    if (Terminal->SlaveFileObject == NULL) {
        IoContext->BytesCompleted = 0;
        return STATUS_NOT_READY;
    }

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    InputFlags = Terminal->Settings.InputFlags;
    LocalFlags = Terminal->Settings.LocalFlags;
    EchoMask = TERMINAL_LOCAL_ECHO | TERMINAL_LOCAL_ECHO_ERASE |
               TERMINAL_LOCAL_ECHO_KILL_NEWLINE |
               TERMINAL_LOCAL_ECHO_KILL_EXTENDED |
               TERMINAL_LOCAL_ECHO_NEWLINE |
               TERMINAL_LOCAL_ECHO_CONTROL;

    EchoFlags = LocalFlags & EchoMask;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    InputAdded = FALSE;
    DirtyRegionBegin = Terminal->WorkingInputCursor;
    DirtyRegionEnd = Terminal->WorkingInputCursor;
    OutputWritten = FALSE;
    ScreenCursorPosition = Terminal->WorkingInputCursor;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    KeAcquireQueuedLock(Terminal->Lock);
    LockHeld = TRUE;

    //
    // Loop through every byte.
    //

    LocalByteIndex = 0;
    LocalByteSize = 0;
    for (ByteIndex = 0; ByteIndex < IoContext->SizeInBytes; ByteIndex += 1) {
        TransferWorkingBuffer = FALSE;
        AddCharacter = TRUE;

        //
        // Get the particular byte in question. Keep a local bounce buffer to
        // avoid calling the copy I/O buffer data function for every single
        // byte.
        //

        if (LocalByteIndex < LocalByteSize) {
            Byte = LocalBytes[LocalByteIndex];
            LocalByteIndex += 1;

        } else {
            LocalByteSize = IoContext->SizeInBytes - ByteIndex;
            if (LocalByteSize > sizeof(LocalBytes)) {
                LocalByteSize = sizeof(LocalBytes);
            }

            Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                        LocalBytes,
                                        ByteIndex,
                                        LocalByteSize,
                                        FALSE);

            if (!KSUCCESS(Status)) {
                goto TerminalMasterWriteEnd;
            }

            Byte = LocalBytes[0];
            LocalByteIndex = 1;
        }

        //
        // The input lock should be held.
        //

        ASSERT(LockHeld != FALSE);

        //
        // Process signal generating characters.
        //

        if (Byte == ControlCharacters[TerminalCharacterInterrupt]) {
            if ((LocalFlags & TERMINAL_LOCAL_SIGNALS) != 0) {
                AddCharacter = FALSE;
                if ((Terminal->SlaveHandles != 0) &&
                    (Terminal->ProcessGroupId !=
                     TERMINAL_INVALID_PROCESS_GROUP)) {

                    PsSignalProcessGroup(Terminal->ProcessGroupId,
                                         SIGNAL_KEYBOARD_INTERRUPT);
                }
            }
        }

        if (Byte == ControlCharacters[TerminalCharacterQuit]) {
            if ((LocalFlags & TERMINAL_LOCAL_SIGNALS) != 0) {
                AddCharacter = FALSE;
                if ((Terminal->SlaveHandles != 0) &&
                    (Terminal->ProcessGroupId !=
                     TERMINAL_INVALID_PROCESS_GROUP)) {

                    PsSignalProcessGroup(Terminal->ProcessGroupId,
                                         SIGNAL_REQUEST_CORE_DUMP);
                }
            }
        }

        //
        // Run through the input flags.
        //

        if ((InputFlags & TERMINAL_INPUT_STRIP) != 0) {
            Byte &= 0x7F;
        }

        if (Byte == '\r') {
            if ((InputFlags & TERMINAL_INPUT_CR_TO_NEWLINE) != 0) {
                Byte = '\n';

            } else if ((InputFlags & TERMINAL_INPUT_IGNORE_CR) != 0) {
                AddCharacter = FALSE;
            }

        } else if (Byte == '\n') {
            if ((InputFlags & TERMINAL_INPUT_NEWLINE_TO_CR) != 0) {
                Byte = '\r';
            }
        }

        //
        // Process the byte in cooked mode.
        //

        if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) != 0) {
            IsEndOfLine = FALSE;

            //
            // First let an editing function take a look at it.
            //

            CharacterHandled = IopTerminalProcessEditingCharacter(
                                                        Terminal,
                                                        Byte,
                                                        TimeoutInMilliseconds,
                                                        &DirtyRegionBegin,
                                                        &DirtyRegionEnd,
                                                        &ScreenCursorPosition,
                                                        &OutputWritten);

            if (CharacterHandled != FALSE) {
                AddCharacter = FALSE;

            //
            // Pushing return transfers the working buffer to the slave's input.
            //

            } else if ((Byte ==
                        ControlCharacters[TerminalCharacterEndOfLine]) ||
                       (Byte == '\n')) {

                TransferWorkingBuffer = TRUE;
                Terminal->WorkingInputCursor = Terminal->WorkingInputLength;
                IsEndOfLine = TRUE;

            //
            // End of file also causes output to be flushed.
            //

            } else if (Byte == ControlCharacters[TerminalCharacterEndOfFile]) {
                TransferWorkingBuffer = TRUE;
            }

            //
            // If the character should be added but the input is full, then
            // the max bell flag comes into play.
            //

            if ((AddCharacter != FALSE) &&
                (Terminal->WorkingInputLength >=
                 TERMINAL_CANONICAL_BUFFER_SIZE)) {

                //
                // If the max bell flag is set, beep at the user and keep
                // what's currently in the input buffer.
                //

                if ((Terminal->Settings.InputFlags &
                     TERMINAL_INPUT_MAX_BELL) != 0) {

                    AddCharacter = FALSE;
                    Byte = '\a';
                    IopTerminalWriteOutputBuffer(Terminal,
                                                 &Byte,
                                                 1,
                                                 1,
                                                 TimeoutInMilliseconds);

                //
                // Just discard the input buffer and reset it. This will
                // look quite weird.
                //

                } else {
                    InputAdded = FALSE;
                    Terminal->WorkingInputCursor = 0;
                    Terminal->WorkingInputLength = 0;
                    DirtyRegionBegin = 0;
                    DirtyRegionEnd = 0;
                    ScreenCursorPosition = 0;
                    Terminal->Flags |= TERMINAL_FLAG_VIRGIN_LINE |
                                       TERMINAL_FLAG_UNEDITED_LINE;
                }
            }

            //
            // Add the character to the working buffer if needed.
            //

            if (AddCharacter != FALSE) {

                ASSERT(Terminal->WorkingInputLength <
                       TERMINAL_CANONICAL_BUFFER_SIZE);

                if (Terminal->WorkingInputCursor < DirtyRegionBegin) {
                    DirtyRegionBegin = Terminal->WorkingInputCursor;
                }

                //
                // Make a hole.
                //

                for (MoveIndex = Terminal->WorkingInputLength;
                     MoveIndex > Terminal->WorkingInputCursor;
                     MoveIndex -= 1) {

                    Terminal->WorkingInputBuffer[MoveIndex] =
                                   Terminal->WorkingInputBuffer[MoveIndex - 1];
                }

                Terminal->WorkingInputBuffer[Terminal->WorkingInputCursor] =
                                                                          Byte;

                Terminal->WorkingInputCursor += 1;
                Terminal->WorkingInputLength += 1;
                if ((IsEndOfLine == FALSE) &&
                    (Terminal->WorkingInputLength > DirtyRegionEnd)) {

                    DirtyRegionEnd = Terminal->WorkingInputLength;
                    Terminal->Flags &= ~(TERMINAL_FLAG_VIRGIN_LINE |
                                         TERMINAL_FLAG_UNEDITED_LINE);
                }
            }

            //
            // Flush the buffer if desired.
            //

            if (TransferWorkingBuffer != FALSE) {

                //
                // Fix up the line before abandoning it.
                //

                if ((DirtyRegionBegin != DirtyRegionEnd) &&
                    ((EchoFlags & TERMINAL_LOCAL_ECHO) != 0)) {

                    IopTerminalFixUpCanonicalLine(Terminal,
                                                  TimeoutInMilliseconds,
                                                  DirtyRegionBegin,
                                                  DirtyRegionEnd,
                                                  ScreenCursorPosition);

                    ScreenCursorPosition = Terminal->WorkingInputCursor;
                    OutputWritten = TRUE;
                }

                //
                // Wait for there to be enough space.
                //

                while (TRUE) {
                    InputFlags = Terminal->Settings.InputFlags;
                    LocalFlags = Terminal->Settings.LocalFlags;
                    EchoFlags = LocalFlags & EchoMask;
                    Space = IopTerminalGetInputBufferSpace(Terminal);
                    if (Space >= Terminal->WorkingInputLength) {
                        break;
                    }

                    IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, FALSE);
                    IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, TRUE);
                    InputAdded = FALSE;
                    KeReleaseQueuedLock(Terminal->Lock);
                    LockHeld = FALSE;
                    Status = IoWaitForIoObjectState(MasterIoState,
                                                    POLL_EVENT_OUT,
                                                    TRUE,
                                                    TimeoutInMilliseconds,
                                                    &ReturnedEvents);

                    if (!KSUCCESS(Status)) {
                        goto TerminalMasterWriteEnd;
                    }

                    if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                        Status = STATUS_DEVICE_IO_ERROR;
                        goto TerminalMasterWriteEnd;
                    }

                    KeAcquireQueuedLock(Terminal->Lock);
                    LockHeld = TRUE;
                }

                //
                // Move the bytes to the input buffer.
                //

                for (MoveIndex = 0;
                     MoveIndex < Terminal->WorkingInputLength;
                     MoveIndex += 1) {

                    Terminal->InputBuffer[Terminal->InputBufferEnd] =
                                       Terminal->WorkingInputBuffer[MoveIndex];

                    Terminal->InputBufferEnd += 1;
                    if (Terminal->InputBufferEnd ==
                        TERMINAL_INPUT_BUFFER_SIZE) {

                        Terminal->InputBufferEnd = 0;
                    }

                    ASSERT(Terminal->InputBufferEnd !=
                           Terminal->InputBufferStart);
                }

                InputAdded = TRUE;
                Terminal->WorkingInputCursor = 0;
                Terminal->WorkingInputLength = 0;
                DirtyRegionBegin = 0;
                DirtyRegionEnd = 0;
                ScreenCursorPosition = 0;
                Terminal->Flags |= TERMINAL_FLAG_VIRGIN_LINE |
                                   TERMINAL_FLAG_UNEDITED_LINE;
            }

        //
        // Input is not canonical, it just goes directly in the input buffer.
        //

        } else {
            if (AddCharacter == FALSE) {
                continue;
            }

            //
            // Wait if there's not enough space available.
            //

            while (IopTerminalGetInputBufferSpace(Terminal) == 0) {
                IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, FALSE);
                IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, TRUE);
                KeReleaseQueuedLock(Terminal->Lock);
                LockHeld = FALSE;
                InputAdded = FALSE;
                Status = IoWaitForIoObjectState(MasterIoState,
                                                POLL_EVENT_OUT,
                                                TRUE,
                                                TimeoutInMilliseconds,
                                                &ReturnedEvents);

                if (!KSUCCESS(Status)) {
                    goto TerminalMasterWriteEnd;
                }

                if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                    Status = STATUS_DEVICE_IO_ERROR;
                    goto TerminalMasterWriteEnd;
                }

                KeAcquireQueuedLock(Terminal->Lock);
                LockHeld = TRUE;
            }

            //
            // Add the character to the input buffer.
            //

            Terminal->InputBuffer[Terminal->InputBufferEnd] = Byte;
            Terminal->InputBufferEnd += 1;
            if (Terminal->InputBufferEnd == TERMINAL_INPUT_BUFFER_SIZE) {
                Terminal->InputBufferEnd = 0;
            }

            ASSERT(Terminal->InputBufferEnd != Terminal->InputBufferStart);

            InputAdded = TRUE;
        }

        //
        // Potentially echo the byte. Failure to echo is not necessarily
        // considered a failure.
        //

        if (EchoFlags != 0) {

            //
            // In raw mode, echo everything unless disallowed.
            //

            if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) == 0) {
                EchoThisCharacter = FALSE;
                if ((EchoFlags & TERMINAL_LOCAL_ECHO) != 0) {
                    EchoThisCharacter = TRUE;

                } else if ((Byte == '\n') &&
                           ((EchoFlags & TERMINAL_LOCAL_ECHO_NEWLINE) != 0)) {

                    EchoThisCharacter = TRUE;
                }

            //
            // In canonical mode, only consider echoing newlines. Everything
            // else is handled automatically.
            //

            } else {
                EchoThisCharacter = FALSE;
                if (Byte == '\n') {
                    if ((EchoFlags &
                         (TERMINAL_LOCAL_ECHO_NEWLINE |
                          TERMINAL_LOCAL_ECHO)) != 0) {

                        EchoThisCharacter = TRUE;
                    }
                }
            }

            if (EchoThisCharacter != FALSE) {
                Bytes[0] = Byte;
                BytesSize = 1;
                if ((Byte < ' ') &&
                    ((EchoFlags & TERMINAL_LOCAL_ECHO_CONTROL) != 0) &&
                    (!RtlIsCharacterSpace(Byte)) && (Byte != '\0')) {

                    Bytes[1] = Byte + '@';
                    Bytes[0] = '^';
                    BytesSize = 2;
                }

                IopTerminalWriteOutputBuffer(Terminal,
                                             Bytes,
                                             BytesSize,
                                             1,
                                             TimeoutInMilliseconds);

                OutputWritten = TRUE;
            }
        }
    }

    //
    // In canonical mode, the line may need to be fixed up.
    //

    if ((DirtyRegionBegin != DirtyRegionEnd) &&
        ((EchoFlags & TERMINAL_LOCAL_ECHO) != 0)) {

        ASSERT(LockHeld != FALSE);

        IopTerminalFixUpCanonicalLine(Terminal,
                                      TimeoutInMilliseconds,
                                      DirtyRegionBegin,
                                      DirtyRegionEnd,
                                      ScreenCursorPosition);

        OutputWritten = TRUE;
    }

    Status = STATUS_SUCCESS;

TerminalMasterWriteEnd:

    //
    // Signal the input and/or output that there's stuff to do.
    //

    if (OutputWritten != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
    }

    if (InputAdded != FALSE) {

        ASSERT(LockHeld != FALSE);

        IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, TRUE);
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->Lock);
    }

    IoContext->BytesCompleted = ByteIndex;
    return Status;
}

KSTATUS
IopTerminalSlaveWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine writes data to the terminal master (ie writes to the slaves
    standard out).

Arguments:

    FileObject - Supplies a pointer to the slave terminal file object.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    BOOL AnythingWritten;
    UINTN BytesThisRound;
    UINTN BytesWritten;
    UCHAR LocalBytes[64];
    BOOL LockHeld;
    PIO_OBJECT_STATE MasterIoState;
    ULONG ReturnedEvents;
    PTERMINAL_SLAVE Slave;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;
    PTERMINAL Terminal;
    ULONG TimeoutInMilliseconds;

    AnythingWritten = FALSE;
    BytesWritten = 0;
    LockHeld = FALSE;
    MasterIoState = NULL;
    Slave = FileObject->SpecialIo;
    Terminal = Slave->Master;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;

    ASSERT(Slave->Header.Type == ObjectTerminalSlave);

    if (IO_IS_TERMINAL_MASTER_OPEN(Terminal) == FALSE) {
        Status = STATUS_BROKEN_PIPE;
        goto TerminalSlaveWriteEnd;
    }

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    MasterIoState = Terminal->MasterFileObject->IoState;

    //
    // Synchronize the checks on the terminal attachment and the owning session
    // and process group with the IOCTLs that may modify them.
    //

    KeAcquireQueuedLock(Terminal->Lock);
    LockHeld = TRUE;

    //
    // Make sure this process can currently write to this terminal.
    //

    Status = IopTerminalValidateGroup(Terminal, FALSE);
    if (!KSUCCESS(Status)) {
        goto TerminalSlaveWriteEnd;
    }

    //
    // Loop writing bytes until it's done.
    //

    Status = STATUS_SUCCESS;
    Space = IopTerminalGetOutputBufferSpace(Terminal);
    while (BytesWritten != IoContext->SizeInBytes) {

        //
        // If there's no space, release the lock and wait for space to open up.
        //

        if (Space == 0) {
            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
            KeReleaseQueuedLock(Terminal->Lock);
            LockHeld = FALSE;
            Status = IoWaitForIoObjectState(SlaveIoState,
                                            POLL_EVENT_OUT,
                                            TRUE,
                                            TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto TerminalSlaveWriteEnd;
            }

            if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                Status = STATUS_DEVICE_IO_ERROR;
                goto TerminalSlaveWriteEnd;
            }

            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
            Space = IopTerminalGetOutputBufferSpace(Terminal);
            continue;
        }

        BytesThisRound = Space;
        if (IoContext->SizeInBytes - BytesWritten < Space) {
            BytesThisRound = IoContext->SizeInBytes - BytesWritten;
        }

        //
        // Copy the data from the I/O buffer to a local bounce buffer, then
        // into the output buffer.
        //

        if (BytesThisRound > sizeof(LocalBytes)) {
            BytesThisRound = sizeof(LocalBytes);
        }

        Status = MmCopyIoBufferData(IoContext->IoBuffer,
                                    LocalBytes,
                                    BytesWritten,
                                    BytesThisRound,
                                    FALSE);

        if (!KSUCCESS(Status)) {
            break;
        }

        Status = IopTerminalWriteOutputBuffer(Terminal,
                                              LocalBytes,
                                              BytesThisRound,
                                              1,
                                              TimeoutInMilliseconds);

        if (!KSUCCESS(Status)) {
            goto TerminalSlaveWriteEnd;
        }

        Space = IopTerminalGetOutputBufferSpace(Terminal);
        AnythingWritten = TRUE;
        BytesWritten += BytesThisRound;
    }

    //
    // Unsignal the write event if this routine just wrote the last of the
    // space.
    //

    ASSERT(LockHeld != FALSE);

    if ((AnythingWritten != FALSE) && (Space == 0)) {
        IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
    }

TerminalSlaveWriteEnd:
    if (AnythingWritten != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->Lock);
    }

    IoContext->BytesCompleted = BytesWritten;
    return Status;
}

KSTATUS
IopTerminalMasterRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads data from the master side (the slave's standard out).

Arguments:

    FileObject - Supplies a pointer to the master terminal file object.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    BOOL AnythingRead;
    UINTN BytesRead;
    UINTN CopySize;
    BOOL LockHeld;
    PIO_OBJECT_STATE MasterIoState;
    ULONG ReturnedEvents;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;
    PTERMINAL Terminal;
    ULONG TimeoutInMilliseconds;

    Terminal = FileObject->SpecialIo;

    ASSERT(Terminal->Header.Type == ObjectTerminalMaster);
    ASSERT(Terminal->MasterFileObject == FileObject);

    if (Terminal->SlaveFileObject == NULL) {
        IoContext->BytesCompleted = 0;
        return STATUS_NOT_READY;
    }

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    MasterIoState = FileObject->IoState;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    AnythingRead = FALSE;
    BytesRead = 0;
    KeAcquireQueuedLock(Terminal->Lock);
    LockHeld = TRUE;
    Space = IopTerminalGetOutputBufferSpace(Terminal);
    while (BytesRead < IoContext->SizeInBytes) {

        //
        // Wait for data to be ready.
        //

        while (Space == TERMINAL_OUTPUT_BUFFER_SIZE - 1) {

            //
            // If the caller got something already, just return immediately
            // instead of waiting for the full buffer amount.
            //

            if (AnythingRead != FALSE) {
                Status = STATUS_SUCCESS;
                goto TerminalMasterReadEnd;
            }

            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, FALSE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, TRUE);
            KeReleaseQueuedLock(Terminal->Lock);
            LockHeld = FALSE;
            Status = IoWaitForIoObjectState(MasterIoState,
                                            POLL_EVENT_IN,
                                            TRUE,
                                            TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto TerminalMasterReadEnd;
            }

            if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                Status = STATUS_DEVICE_IO_ERROR;
                goto TerminalMasterReadEnd;
            }

            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
            Space = IopTerminalGetOutputBufferSpace(Terminal);
        }

        //
        // Copy the bytes out. Don't wrap across the terminal's circular buffer.
        //

        CopySize = (TERMINAL_OUTPUT_BUFFER_SIZE - 1) - Space;
        if (CopySize > IoContext->SizeInBytes - BytesRead) {
            CopySize = IoContext->SizeInBytes - BytesRead;
        }

        if (CopySize >
            TERMINAL_OUTPUT_BUFFER_SIZE - Terminal->OutputBufferStart) {

            CopySize = TERMINAL_OUTPUT_BUFFER_SIZE -
                       Terminal->OutputBufferStart;
        }

        Status = MmCopyIoBufferData(
                          IoContext->IoBuffer,
                          Terminal->OutputBuffer + Terminal->OutputBufferStart,
                          BytesRead,
                          CopySize,
                          TRUE);

        if (!KSUCCESS(Status)) {
            goto TerminalMasterReadEnd;
        }

        Terminal->OutputBufferStart += CopySize;

        ASSERT(Terminal->OutputBufferStart <= TERMINAL_OUTPUT_BUFFER_SIZE);

        if (Terminal->OutputBufferStart == TERMINAL_OUTPUT_BUFFER_SIZE) {
            Terminal->OutputBufferStart = 0;
        }

        Space += CopySize;
        AnythingRead = TRUE;
        BytesRead += CopySize;
    }

    Status = STATUS_SUCCESS;

TerminalMasterReadEnd:
    if (AnythingRead != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, TRUE);
        Space = IopTerminalGetOutputBufferSpace(Terminal);
        if (Space == TERMINAL_OUTPUT_BUFFER_SIZE - 1) {
            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, FALSE);
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->Lock);
    }

    IoContext->BytesCompleted = BytesRead;
    return Status;
}

KSTATUS
IopTerminalSlaveRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads data from the slave side (the slave's standard in).

Arguments:

    FileObject - Supplies a pointer to the slave terminal file object.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    UINTN AdvanceSize;
    BOOL AnythingRead;
    BOOL BreakForNewline;
    UINTN BytesRead;
    CHAR Character;
    PCHAR ControlCharacters;
    UINTN CopyIndex;
    UINTN CopySize;
    UCHAR FlushCount;
    UCHAR FlushTime;
    UINTN InputIndex;
    ULONG LocalFlags;
    BOOL LockHeld;
    PIO_OBJECT_STATE MasterIoState;
    ULONG ReturnedEvents;
    PTERMINAL_SLAVE Slave;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;
    PTERMINAL Terminal;
    ULONG TimeoutInMilliseconds;

    Slave = FileObject->SpecialIo;

    ASSERT(Slave->Header.Type == ObjectTerminalSlave);

    Terminal = Slave->Master;

    ASSERT(FileObject == Terminal->SlaveFileObject);

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    MasterIoState = Terminal->MasterFileObject->IoState;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    AnythingRead = FALSE;
    BytesRead = 0;

    //
    // Synchronize the checks on the terminal attachment and the owning session
    // and process group with the IOCTLs that may modify them.
    //

    KeAcquireQueuedLock(Terminal->Lock);
    LockHeld = TRUE;
    LocalFlags = Terminal->Settings.LocalFlags;

    //
    // Make sure this process can currently read from this terminal.
    //

    Status = IopTerminalValidateGroup(Terminal, TRUE);
    if (!KSUCCESS(Status)) {
        goto TerminalSlaveReadEnd;
    }

    //
    // Wait the designated amount of time, or block indefinitely.
    //

    if (TimeoutInMilliseconds == WAIT_TIME_INDEFINITE) {
        FlushTime = ControlCharacters[TerminalCharacterFlushTime];
        if (FlushTime != 0) {
            TimeoutInMilliseconds = FlushTime * 100;
        }
    }

    BreakForNewline = FALSE;
    Status = STATUS_SUCCESS;
    Space = IopTerminalGetInputBufferSpace(Terminal);
    while (BytesRead < IoContext->SizeInBytes) {

        //
        // Wait for data to be ready.
        //

        if (Space == TERMINAL_INPUT_BUFFER_SIZE - 1) {

            //
            // In non-canonical mode, observe the minimum and timeout counts.
            //

            if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) == 0) {
                FlushCount = ControlCharacters[TerminalCharacterFlushCount];
                if (FlushCount != 0) {

                    //
                    // If there's a minimum and it's been met, stop now.
                    //

                    if (BytesRead >= FlushCount) {
                        break;
                    }

                //
                // The minimum is zero. If time is also zero, then do not block.
                //

                } else {
                    if (ControlCharacters[TerminalCharacterFlushTime] == 0) {
                        TimeoutInMilliseconds = 0;
                    }
                }
            }

            //
            // If all open handles to the master were closed, there's never
            // going to be any more data.
            //

            if (IO_IS_TERMINAL_MASTER_OPEN(Terminal) == FALSE) {
                Status = STATUS_END_OF_FILE;
                break;
            }

            IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, FALSE);
            IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
            KeReleaseQueuedLock(Terminal->Lock);
            LockHeld = FALSE;
            Status = IoWaitForIoObjectState(SlaveIoState,
                                            POLL_EVENT_IN,
                                            TRUE,
                                            TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto TerminalSlaveReadEnd;
            }

            if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                Status = STATUS_DEVICE_IO_ERROR;
                goto TerminalSlaveReadEnd;
            }

            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
            LocalFlags = Terminal->Settings.LocalFlags;
            Space = IopTerminalGetInputBufferSpace(Terminal);
            if (Space == TERMINAL_INPUT_BUFFER_SIZE - 1) {
                break;
            }
        }

        //
        // Determine how much to copy out of the terminal's input buffer.
        //

        CopySize = (TERMINAL_INPUT_BUFFER_SIZE - 1) - Space;
        if (CopySize > IoContext->SizeInBytes - BytesRead) {
            CopySize = IoContext->SizeInBytes - BytesRead;
        }

        if (CopySize >
            TERMINAL_INPUT_BUFFER_SIZE - Terminal->InputBufferStart) {

            CopySize = TERMINAL_INPUT_BUFFER_SIZE - Terminal->InputBufferStart;
        }

        //
        // If it's canonical, look for a newline and break on that.
        //

        AdvanceSize = CopySize;
        if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) != 0) {
            for (CopyIndex = 0; CopyIndex < CopySize; CopyIndex += 1) {
                InputIndex = Terminal->InputBufferStart + CopyIndex;
                Character = Terminal->InputBuffer[InputIndex];
                if ((Character ==
                     ControlCharacters[TerminalCharacterEndOfLine]) ||
                    (Character == '\n')) {

                    CopySize = CopyIndex + 1;
                    AdvanceSize = CopySize;
                    BreakForNewline = TRUE;
                    break;

                //
                // An EOF character is treated like a "return now" character,
                // but is not reported to the user.
                //

                } else if (Character ==
                           ControlCharacters[TerminalCharacterEndOfFile]) {

                    CopySize = CopyIndex;
                    AdvanceSize = CopySize + 1;
                    BreakForNewline = TRUE;
                    break;
                }
            }
        }

        Status = MmCopyIoBufferData(
                            IoContext->IoBuffer,
                            Terminal->InputBuffer + Terminal->InputBufferStart,
                            BytesRead,
                            CopySize,
                            TRUE);

        if (!KSUCCESS(Status)) {
            break;
        }

        Terminal->InputBufferStart += AdvanceSize;

        ASSERT(Terminal->InputBufferStart <= TERMINAL_INPUT_BUFFER_SIZE);

        if (Terminal->InputBufferStart == TERMINAL_INPUT_BUFFER_SIZE) {
            Terminal->InputBufferStart = 0;
        }

        BytesRead += CopySize;
        Space += CopySize;
        AnythingRead = TRUE;

        //
        // If this was a newline and it's canonical mode, then let the user
        // chew on that.
        //

        if (BreakForNewline != FALSE) {
            break;
        }
    }

    ASSERT(LockHeld != FALSE);

TerminalSlaveReadEnd:
    if (AnythingRead != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->Lock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
        Space = IopTerminalGetInputBufferSpace(Terminal);
        if (Space == TERMINAL_INPUT_BUFFER_SIZE - 1) {
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, FALSE);
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->Lock);
    }

    IoContext->BytesCompleted = BytesRead;
    return Status;
}

KSTATUS
IopTerminalWriteOutputBuffer (
    PTERMINAL Terminal,
    PVOID Buffer,
    UINTN SizeInBytes,
    ULONG RepeatCount,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine writes data to the terminal output buffer. It assumes the
    output lock is already held and it does not set any events. It may
    release and reacquire the output lock during the course of the routine, but
    the routine will always return with the output lock held (just like it
    started with).

Arguments:

    Terminal - Supplies a pointer to the terminal.

    Buffer - Supplies a pointer to the buffer that contains the data to write.

    SizeInBytes - Supplies the number of bytes to write.

    RepeatCount - Supplies the number of times to write the buffer to the
        output.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

Return Value:

    Status code.

--*/

{

    UCHAR Byte;
    PUCHAR ByteBuffer;
    UINTN ByteIndex;
    BOOL DidLeadingCharacter;
    BOOL LockHeld;
    ULONG Mask;
    PIO_OBJECT_STATE MasterIoState;
    ULONG OutputFlags;
    ULONG RepeatIndex;
    ULONG ReturnedEvents;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;

    ByteBuffer = (PUCHAR)Buffer;
    DidLeadingCharacter = FALSE;
    LockHeld = TRUE;
    OutputFlags = Terminal->Settings.OutputFlags;
    MasterIoState = Terminal->MasterFileObject->IoState;
    SlaveIoState = Terminal->SlaveFileObject->IoState;
    Space = IopTerminalGetOutputBufferSpace(Terminal);
    for (RepeatIndex = 0; RepeatIndex < RepeatCount; RepeatIndex += 1) {
        for (ByteIndex = 0; ByteIndex < SizeInBytes; ByteIndex += 1) {

            //
            // Wait for space to become available.
            //

            if ((Space == 0) && (Terminal->HardwareHandle != NULL)) {
                Status = IopTerminalFlushOutputToDevice(Terminal);
                if (!KSUCCESS(Status)) {
                    goto TerminalWriteOutputBufferEnd;
                }

                Space = IopTerminalGetOutputBufferSpace(Terminal);

                ASSERT(Space != 0);
            }

            while (Space == 0) {
                IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
                IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
                KeReleaseQueuedLock(Terminal->Lock);
                LockHeld = FALSE;
                Status = IoWaitForIoObjectState(SlaveIoState,
                                                POLL_EVENT_OUT,
                                                TRUE,
                                                TimeoutInMilliseconds,
                                                &ReturnedEvents);

                if (!KSUCCESS(Status)) {
                    goto TerminalWriteOutputBufferEnd;
                }

                if ((ReturnedEvents & TERMINAL_POLL_ERRORS) != 0) {
                    Status = STATUS_DEVICE_IO_ERROR;
                    goto TerminalWriteOutputBufferEnd;
                }

                KeAcquireQueuedLock(Terminal->Lock);
                LockHeld = TRUE;
                Space = IopTerminalGetOutputBufferSpace(Terminal);
            }

            //
            // Process any output flags.
            //

            Byte = ByteBuffer[ByteIndex];
            if (Byte == '\r') {
                if ((OutputFlags & TERMINAL_OUTPUT_CR_TO_NEWLINE) != 0) {
                    Byte = '\n';
                }

            } else if (Byte == '\n') {

                //
                // If \n should be translated to \r\n, then change the byte to
                // \r, and decrement the loop counter to go around again on the
                // same byte. The second time around, just output the \n.
                //

                Mask = TERMINAL_OUTPUT_POST_PROCESS |
                       TERMINAL_OUTPUT_NEWLINE_TO_CRLF;

                if ((OutputFlags & Mask) == Mask) {
                    if (DidLeadingCharacter == FALSE) {
                        Byte = '\r';
                        ByteIndex -= 1;
                        DidLeadingCharacter = TRUE;

                    } else {
                        DidLeadingCharacter = FALSE;
                    }
                }
            }

            //
            // Write the byte in.
            //

            Terminal->OutputBuffer[Terminal->OutputBufferEnd] = Byte;
            Terminal->OutputBufferEnd += 1;
            if (Terminal->OutputBufferEnd == TERMINAL_OUTPUT_BUFFER_SIZE) {
                Terminal->OutputBufferEnd = 0;
            }

            Space -= 1;
        }
    }

    if (Terminal->HardwareHandle != NULL) {

        ASSERT(LockHeld != FALSE);

        Status = IopTerminalFlushOutputToDevice(Terminal);
        if (!KSUCCESS(Status)) {
            goto TerminalWriteOutputBufferEnd;
        }
    }

    Status = STATUS_SUCCESS;

TerminalWriteOutputBufferEnd:
    if (LockHeld == FALSE) {
        KeAcquireQueuedLock(Terminal->Lock);
    }

    return Status;
}

ULONG
IopTerminalGetInputBufferSpace (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine returns the amount of space available in bytes in the input
    buffer of a terminal.

Arguments:

    Terminal - Supplies a pointer to the terminal.

Return Value:

    returns the number of bytes available in the input buffer.

--*/

{

    ULONG Space;

    if (Terminal->InputBufferEnd >= Terminal->InputBufferStart) {
        Space = TERMINAL_INPUT_BUFFER_SIZE - 1 -
                (Terminal->InputBufferEnd - Terminal->InputBufferStart);

    //
    // The buffer has wrapped around.
    //

    } else {
        Space = Terminal->InputBufferStart - Terminal->InputBufferEnd - 1;
    }

    return Space;
}

ULONG
IopTerminalGetOutputBufferSpace (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine returns the amount of space available in bytes in the output
    buffer of a terminal.

Arguments:

    Terminal - Supplies a pointer to the terminal.

Return Value:

    returns the number of bytes available in the output buffer.

--*/

{

    ULONG Space;

    if (Terminal->OutputBufferEnd >= Terminal->OutputBufferStart) {
        Space = TERMINAL_OUTPUT_BUFFER_SIZE - 1 -
                (Terminal->OutputBufferEnd - Terminal->OutputBufferStart);

    //
    // The buffer has wrapped around.
    //

    } else {
        Space = Terminal->OutputBufferStart - Terminal->OutputBufferEnd - 1;
    }

    return Space;
}

KSTATUS
IopTerminalFixUpCanonicalLine (
    PTERMINAL Terminal,
    ULONG TimeoutInMilliseconds,
    ULONG DirtyRegionBegin,
    ULONG DirtyRegionEnd,
    ULONG CurrentScreenPosition
    )

/*++

Routine Description:

    This routine fixes up the terminal output for canonical mode processing
    either when a block of input or a valid line is finished. It does not
    acquire any locks or set any events, it assumes that is handled by the
    caller. Specifically, the working input lock and output lock must both
    be held.

Arguments:

    Terminal - Supplies a pointer to the terminal.

    TimeoutInMilliseconds - Supplies the amount of time to wait for output
        operations before giving up.

    DirtyRegionBegin - Supplies the first character in the dirty region, as an
        offset in characters from the beginning of the line. The beginning of
        the line may not be column zero.

    DirtyRegionEnd - Supplies the first character not in the dirty region, as
        an offset in characters from the beginning of the line.

    CurrentScreenPosition - Supplies the position of the screen's cursor as an
        offset in characters from the beginning of the line.

Return Value:

    Status code.

--*/

{

    CHAR Character;
    PCHAR ControlCharacters;
    KSTATUS Status;
    ULONG ValidLineEnd;
    ULONG WorkingInputLength;

    ControlCharacters = Terminal->Settings.ControlCharacters;

    //
    // If the last character is a newline, pretend it's not there.
    //

    WorkingInputLength = Terminal->WorkingInputLength;
    if ((WorkingInputLength != 0) &&
        ((Terminal->WorkingInputBuffer[WorkingInputLength - 1] ==
          ControlCharacters[TerminalCharacterEndOfLine]) ||
         (Terminal->WorkingInputBuffer[WorkingInputLength - 1] == '\n'))) {

        WorkingInputLength -= 1;
    }

    //
    // Back up to the start of the dirty region.
    //

    ASSERT(DirtyRegionBegin <= CurrentScreenPosition);

    if (DirtyRegionBegin < CurrentScreenPosition) {
        Character = '\b';
        Status = IopTerminalWriteOutputBuffer(
                                  Terminal,
                                  &Character,
                                  1,
                                  CurrentScreenPosition - DirtyRegionBegin,
                                  TimeoutInMilliseconds);

        if (!KSUCCESS(Status)) {
            goto TerminalFixUpCanonicalLineEnd;
        }

        CurrentScreenPosition = DirtyRegionBegin;
    }

    //
    // Write out the portion of the dirty region that's still a valid line.
    //

    ValidLineEnd = DirtyRegionEnd;
    if (WorkingInputLength < ValidLineEnd) {
        ValidLineEnd = WorkingInputLength;
    }

    if (ValidLineEnd > DirtyRegionBegin) {
        Status = IopTerminalWriteOutputBuffer(
                               Terminal,
                               Terminal->WorkingInputBuffer + DirtyRegionBegin,
                               ValidLineEnd - DirtyRegionBegin,
                               1,
                               TimeoutInMilliseconds);

        if (!KSUCCESS(Status)) {
            goto TerminalFixUpCanonicalLineEnd;
        }

        CurrentScreenPosition += ValidLineEnd - DirtyRegionBegin;
    }

    //
    // Write spaces to erase any additional portion that goes beyond the valid
    // line end.
    //

    if (CurrentScreenPosition < DirtyRegionEnd) {
        Character = ' ';
        Status = IopTerminalWriteOutputBuffer(
                                        Terminal,
                                        &Character,
                                        1,
                                        DirtyRegionEnd - CurrentScreenPosition,
                                        TimeoutInMilliseconds);

        if (!KSUCCESS(Status)) {
            goto TerminalFixUpCanonicalLineEnd;
        }

        CurrentScreenPosition = DirtyRegionEnd;
    }

    //
    // Finally, back up to the cursor position.
    //

    if (CurrentScreenPosition > Terminal->WorkingInputCursor) {
        Character = '\b';
        Status = IopTerminalWriteOutputBuffer(
                          Terminal,
                          &Character,
                          1,
                          CurrentScreenPosition - Terminal->WorkingInputCursor,
                          TimeoutInMilliseconds);

        if (!KSUCCESS(Status)) {
            goto TerminalFixUpCanonicalLineEnd;
        }
    }

    Status = STATUS_SUCCESS;

TerminalFixUpCanonicalLineEnd:
    return Status;
}

BOOL
IopTerminalProcessEditingCharacter (
    PTERMINAL Terminal,
    CHAR Character,
    ULONG TimeoutInMilliseconds,
    PULONG DirtyRegionBegin,
    PULONG DirtyRegionEnd,
    PULONG ScreenCursorPosition,
    PBOOL OutputWritten
    )

/*++

Routine Description:

    This routine processes any characters that change the working buffer in a
    non-straightforward way. This routine operates in canonical mode only.

Arguments:

    Terminal - Supplies a pointer to the terminal.

    Character - Supplies the character to process.

    TimeoutInMilliseconds - Supplies the number of milliseconds to wait if the
        output buffer is written to.

    DirtyRegionBegin - Supplies a pointer that contains the region of the line
        that needs to be redrawn. This may get expanded on output.

    DirtyRegionEnd - Supplies a pointer that contains the end of the region of
        the line that needs to be redrawn. This also may get expanded on
        output.

    ScreenCursorPosition - Supplies a pointer that contains the screen's
        current cursor position on input, and may get altered on output.

    OutputWritten - Supplies a pointer where TRUE will be returned if the
        output buffer was written to. Otherwise, this value will be left
        untouched.

Return Value:

    TRUE if the byte was handled by this routine and should not be added to the
    working buffer.

    FALSE if the character was not handled by this routine.

--*/

{

    TERMINAL_COMMAND_DATA CommandData;
    PCHAR ControlCharacters;
    UINTN LastIndex;
    ULONG LocalFlags;
    UINTN MoveIndex;
    CHAR OutputString[TERMINAL_MAX_CANONICAL_OUTPUT];
    UINTN OutputStringLength;
    TERMINAL_PARSE_RESULT ParseResult;
    BOOL Result;

    ControlCharacters = Terminal->Settings.ControlCharacters;
    LocalFlags = Terminal->Settings.LocalFlags;
    ParseResult = TermProcessInput(&(Terminal->KeyData), Character);
    switch (ParseResult) {
    case TerminalParseResultNormalCharacter:

        //
        // Erase backs up one.
        //

        Result = FALSE;
        if (Character == ControlCharacters[TerminalCharacterErase]) {
            if (Terminal->WorkingInputCursor != 0) {
                Terminal->WorkingInputCursor -= 1;

                ASSERT(Terminal->WorkingInputLength != 0);

                //
                // Potentially expand the portion of the screen that will
                // need cleaning up.
                //

                if ((LocalFlags & TERMINAL_LOCAL_ECHO_ERASE) != 0) {
                    if (Terminal->WorkingInputCursor < *DirtyRegionBegin) {
                        *DirtyRegionBegin = Terminal->WorkingInputCursor;
                    }

                    if (Terminal->WorkingInputLength + 1 > *DirtyRegionEnd) {
                        *DirtyRegionEnd = Terminal->WorkingInputLength + 1;
                    }

                //
                // If not echoing erase, print the character that was just
                // erased to indicate what happened. This is useful for
                // line printers.
                //

                } else {
                    LastIndex = Terminal->WorkingInputCursor + 1;
                    OutputString[0] = Terminal->WorkingInputBuffer[LastIndex];
                    IopTerminalWriteOutputBuffer(Terminal,
                                                 OutputString,
                                                 1,
                                                 1,
                                                 TimeoutInMilliseconds);
                }

                //
                // Move the characters after the cursor back one.
                //

                for (MoveIndex = Terminal->WorkingInputCursor;
                     MoveIndex < Terminal->WorkingInputLength - 1;
                     MoveIndex += 1) {

                    Terminal->WorkingInputBuffer[MoveIndex] =
                               Terminal->WorkingInputBuffer[MoveIndex + 1];
                }

                Terminal->WorkingInputLength -= 1;
            }

            Result = TRUE;

        //
        // Kill erases the whole line.
        //

        } else if (Character == ControlCharacters[TerminalCharacterKill]) {

            //
            // If the extended bit is set, visually erase the whole line.
            //

            if ((LocalFlags & TERMINAL_LOCAL_ECHO_KILL_EXTENDED) != 0) {
                Result = TRUE;
                *DirtyRegionBegin = 0;
                if (Terminal->WorkingInputLength > *DirtyRegionEnd) {
                    *DirtyRegionEnd = Terminal->WorkingInputLength;
                }

            //
            // Otherwise if the old echo kill is set, add a newline.
            //

            } else if ((LocalFlags & TERMINAL_LOCAL_ECHO_KILL_NEWLINE) != 0) {
                Result = TRUE;
                OutputString[0] = Character;
                OutputString[1] = '\n';
                IopTerminalWriteOutputBuffer(Terminal,
                                             OutputString,
                                             2,
                                             1,
                                             TimeoutInMilliseconds);

            //
            // Just echo the kill character.
            //

            } else {
                Result = FALSE;
            }

            Terminal->WorkingInputCursor = 0;
            Terminal->WorkingInputLength = 0;
            Terminal->Flags &= ~(TERMINAL_FLAG_VIRGIN_LINE |
                                 TERMINAL_FLAG_UNEDITED_LINE);

        //
        // These other characters are simply not printed.
        //

        } else {
            if ((Character == ControlCharacters[TerminalCharacterStart]) ||
                (Character == ControlCharacters[TerminalCharacterStop])) {

                Result = TRUE;
            }
        }

        return Result;

    case TerminalParseResultPartialCommand:
        return TRUE;

    case TerminalParseResultCompleteCommand:
        break;

    default:

        ASSERT(FALSE);

        return FALSE;
    }

    //
    // Handle the complete key that just came in.
    //

    RtlZeroMemory(&CommandData, sizeof(TERMINAL_COMMAND_DATA));
    switch (Terminal->KeyData.Key) {
    case TerminalKeyPageUp:
    case TerminalKeyPageDown:
        if (Terminal->KeyData.Key == TerminalKeyPageUp) {
            CommandData.Command = TerminalCommandScrollUp;

        } else {
            CommandData.Command = TerminalCommandScrollDown;
        }

        CommandData.ParameterCount = 1;
        CommandData.Parameter[0] = TERMINAL_SCROLL_LINE_COUNT;
        Result = TermCreateOutputSequence(&CommandData,
                                          OutputString,
                                          sizeof(OutputString));

        if (Result != FALSE) {
            OutputString[sizeof(OutputString) - 1] = '\0';
            OutputStringLength = RtlStringLength(OutputString);
            IopTerminalWriteOutputBuffer(Terminal,
                                         OutputString,
                                         OutputStringLength,
                                         1,
                                         TimeoutInMilliseconds);

            *OutputWritten = TRUE;
        }

        break;

    case TerminalKeyRight:
        if (Terminal->WorkingInputCursor != Terminal->WorkingInputLength) {
            Terminal->WorkingInputCursor += 1;
            if (Terminal->WorkingInputCursor > *DirtyRegionEnd) {
                *DirtyRegionEnd = Terminal->WorkingInputCursor;
            }
        }

        break;

    case TerminalKeyLeft:
        if (Terminal->WorkingInputCursor != 0) {
            Terminal->WorkingInputCursor -= 1;
            if (Terminal->WorkingInputCursor < *DirtyRegionBegin) {
                *DirtyRegionBegin = Terminal->WorkingInputCursor;
            }
        }

        break;

    default:
        break;
    }

    return TRUE;
}

KSTATUS
IopTerminalUserBufferCopy (
    BOOL FromKernelMode,
    BOOL FromBuffer,
    PVOID UserBuffer,
    PVOID LocalBuffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine copies to or from a user mode or kernel mode buffer.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    FromBuffer - Supplies a boolean indicating whether to copy to the user
        buffer (FALSE) or from the user buffer (TRUE).

    UserBuffer - Supplies the user buffer pointer.

    LocalBuffer - Supplies the local kernel mode buffer.

    Size - Supplies the number of bytes to copy.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // If the caller says it's from kernel mode, it better be a kernel mode
    // address.
    //

    ASSERT((UserBuffer >= KERNEL_VA_START) || (FromKernelMode == FALSE));

    Status = STATUS_SUCCESS;
    if (FromBuffer != FALSE) {
        if (FromKernelMode != FALSE) {
            RtlCopyMemory(LocalBuffer, UserBuffer, Size);

        } else {
            Status = MmCopyFromUserMode(LocalBuffer, UserBuffer, Size);
        }

    } else {
        if (FromKernelMode != FALSE) {
            RtlCopyMemory(UserBuffer, LocalBuffer, Size);

        } else {
            Status = MmCopyToUserMode(UserBuffer, LocalBuffer, Size);
        }
    }

    return Status;
}

KSTATUS
IopTerminalFlushOutputToDevice (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine writes the currently buffered output data to the hardware
    device. This routine assumes the output lock is already held.

Arguments:

    Terminal - Supplies a pointer to the terminal whose output data should be
        flushed.

Return Value:

    Status code.

--*/

{

    UINTN BytesWritten;
    IO_BUFFER IoBuffer;
    UINTN Size;
    KSTATUS Status;

    ASSERT(Terminal->HardwareHandle != NULL);

    Status = STATUS_SUCCESS;

    //
    // Loop writing portions of the output buffer.
    //

    while (TRUE) {

        //
        // If the start is greater than the end, then the buffer has wrapped
        // around and needs to be written in two steps.
        //

        if (Terminal->OutputBufferEnd < Terminal->OutputBufferStart) {
            Size = TERMINAL_OUTPUT_BUFFER_SIZE - Terminal->OutputBufferStart;
            Status = MmInitializeIoBuffer(
                          &IoBuffer,
                          Terminal->OutputBuffer + Terminal->OutputBufferStart,
                          INVALID_PHYSICAL_ADDRESS,
                          Size,
                          IO_BUFFER_FLAG_KERNEL_MODE_DATA);

            if (!KSUCCESS(Status)) {
                return Status;
            }

            Status = IoWrite(Terminal->HardwareHandle,
                             &IoBuffer,
                             Size,
                             0,
                             WAIT_TIME_INDEFINITE,
                             &BytesWritten);

            if (!KSUCCESS(Status)) {
                return Status;
            }

            Terminal->OutputBufferStart += BytesWritten;
            if (Terminal->OutputBufferStart == TERMINAL_OUTPUT_BUFFER_SIZE) {
                Terminal->OutputBufferStart = 0;
            }

            continue;
        }

        //
        // If the buffer is empty, stop.
        //

        if (Terminal->OutputBufferStart == Terminal->OutputBufferEnd) {
            break;
        }

        Size = Terminal->OutputBufferEnd - Terminal->OutputBufferStart;
        Status = MmInitializeIoBuffer(
                          &IoBuffer,
                          Terminal->OutputBuffer + Terminal->OutputBufferStart,
                          INVALID_PHYSICAL_ADDRESS,
                          Size,
                          IO_BUFFER_FLAG_KERNEL_MODE_DATA);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = IoWrite(Terminal->HardwareHandle,
                         &IoBuffer,
                         Size,
                         0,
                         WAIT_TIME_INDEFINITE,
                         &BytesWritten);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Terminal->OutputBufferStart += BytesWritten;

        ASSERT(Terminal->OutputBufferStart != TERMINAL_OUTPUT_BUFFER_SIZE);

    }

    return Status;
}

VOID
IopTerminalDisassociate (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine clears the controlling terminal from every process in the
    terminal's session. This routine assumes the terminal list lock and
    terminal locks are already held.

Arguments:

    Terminal - Supplies a pointer to the controlling terminal.

Return Value:

    None.

--*/

{

    SESSION_ID SessionId;

    ASSERT(KeIsQueuedLockHeld(IoTerminalListLock) != FALSE);

    SessionId = Terminal->SessionId;
    if (SessionId != TERMINAL_INVALID_SESSION) {
        PsIterateProcess(ProcessIdSession,
                         SessionId,
                         IopTerminalDisassociateIterator,
                         NULL);
    }

    Terminal->SessionId = TERMINAL_INVALID_SESSION;
    Terminal->ProcessGroupId = TERMINAL_INVALID_PROCESS_GROUP;
    return;
}

BOOL
IopTerminalDisassociateIterator (
    PVOID Context,
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine describes the prototype for the process list iterator. This
    routine is called with the process list lock held.

Arguments:

    Context - Supplies a pointer's worth of context passed into the iterate
        routine.

    Process - Supplies the process to examine.

Return Value:

    FALSE always to indicate continuing to iterate.

--*/

{

    Process->ControllingTerminal = NULL;
    return FALSE;
}

KSTATUS
IopTerminalValidateGroup (
    PTERMINAL Terminal,
    BOOL Input
    )

/*++

Routine Description:

    This routine validates that the given terminal can be written to by the
    current process.

Arguments:

    Terminal - Supplies a pointer to the terminal to check.

    Input - Supplies a boolean indicating whether to send a terminal input
        signal on failure (TRUE) or a terminal output signal (FALSE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the current process group is orphaned.

    STATUS_TRY_AGAIN if the process group is not orphaned. The process will
    also be set a terminal output signal.

--*/

{

    PKPROCESS Process;
    PROCESS_GROUP_ID ProcessGroup;
    ULONG Signal;
    KSTATUS Status;

    Signal = SIGNAL_BACKGROUND_TERMINAL_OUTPUT;
    if (Input != FALSE) {
        Signal = SIGNAL_BACKGROUND_TERMINAL_INPUT;
    }

    Process = PsGetCurrentProcess();
    ProcessGroup = Process->Identifiers.ProcessGroupId;
    Status = STATUS_SUCCESS;
    if ((Process->ControllingTerminal == Terminal->SlaveFileObject) &&
        (ProcessGroup != Terminal->ProcessGroupId) &&
        ((Terminal->Settings.LocalFlags &
          TERMINAL_LOCAL_STOP_BACKGROUND_WRITES) != 0)) {

        //
        // If the process is accepting that signal, send it to it and tell it
        // to try again later. The exception is an orphaned process group, in
        // which case an error is returned. If the process is not accepting the
        // signal, just let the flush go through.
        //

        if (PsIsThreadAcceptingSignal(NULL, Signal) != FALSE) {
            if (PsIsProcessGroupOrphaned(ProcessGroup) != FALSE) {
                Status = STATUS_DEVICE_IO_ERROR;

            } else {
                PsSignalProcessGroup(ProcessGroup, Signal);
                Status = STATUS_TRY_AGAIN;
            }
        }
    }

    return Status;
}

