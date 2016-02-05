/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include <minoca/termlib.h>
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

    This structure defines an entry in the terminal history list.

Members:

    ListEntry - Stores pointers to the next and previous history entries.

    CommandLength - Stores the length of the command, in bytes. There is no
        null terminator on this string.

    Command - Stores the command buffer.

--*/

typedef struct _TERMINAL_HISTORY_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG CommandLength;
    CHAR Command[ANYSIZE_ARRAY];
} TERMINAL_HISTORY_ENTRY, *PTERMINAL_HISTORY_ENTRY;

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

    OutputLock - Stores a pointer to a lock serializing access to the output
        buffer.

    InputBuffer - Stores a pointer to the input buffer.

    InputBufferStart - Stores the first valid index of the input buffer.

    InputBufferEnd - Stores the first invalid index of the input buffer. If
        this is equal to the start, then the buffer is empty.

    WorkingInputBuffer - Stores the current (unfinished) line in canonical
        mode.

    WorkingInputCursor - Stores the current position of the cursor in the
        working input buffer.

    WorkingInputLength - Stores the valid length of the working input buffer.

    InputLock - Stores a pointer to a lock serializing access to the
        working input buffer.

    WorkingInputLock - Stores a pointer to the working input buffer lock.

    Settings - Stores the current terminal settings.

    CommandHistory - Stores the list of historical commands.

    CommandHistorySize - Stores the length of the linked list of commands.

    LastCommand - Stores a pointer to the most recent command.

    Flags - Stores a bitfield of flags. See TERMINAL_FLAG_* definitions. This
        field is protected by the terminal output lock.

    KeyData - Stores the data for the key currently being parsed. This is only
        used in canonical mode.

    SlaveHandles - Stores the count of open slave side handles, not counting
        those opened with no access.

    ProcessGroupId - Stores the owning process group ID of the terminal.

    SessionId - Stores the owning session ID of the terminal.

    SessionProcess - Stores a pointer to the session leader process for the
        owning session.

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
    PQUEUED_LOCK OutputLock;
    PSTR InputBuffer;
    ULONG InputBufferStart;
    ULONG InputBufferEnd;
    PSTR WorkingInputBuffer;
    ULONG WorkingInputCursor;
    ULONG WorkingInputLength;
    PQUEUED_LOCK InputLock;
    PQUEUED_LOCK WorkingInputLock;
    TERMINAL_SETTINGS Settings;
    LIST_ENTRY CommandHistory;
    ULONG CommandHistorySize;
    PTERMINAL_HISTORY_ENTRY LastCommand;
    TERMINAL_KEY_DATA KeyData;
    ULONG Flags;
    UINTN SlaveHandles;
    PROCESS_GROUP_ID ProcessGroupId;
    SESSION_ID SessionId;
    PKPROCESS SessionProcess;
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

VOID
IopTerminalAddHistoryEntry (
    PTERMINAL Terminal
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

PTERMINAL
IopLookupTerminal (
    SESSION_ID SessionId
    );

VOID
IopRelinquishTerminal (
    PTERMINAL Terminal
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
    PSTR MasterPath,
    UINTN MasterPathLength,
    PSTR SlavePath,
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

    TERMINAL_CREATION_PARAMETERS CreationParameters;
    PIO_HANDLE SlaveHandle;
    KSTATUS Status;

    RtlZeroMemory(&CreationParameters, sizeof(TERMINAL_CREATION_PARAMETERS));
    CreationParameters.SlaveCreatePermissions = SlaveCreatePermissions;

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
                     IoObjectTerminalMaster,
                     &CreationParameters,
                     MasterCreatePermissions,
                     MasterHandle);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // The master put itself in the creation parameters, which are now passed
    // down when trying to create the slave (which is mostly just a matter of
    // creating the path entry now).
    //

    MasterOpenFlags |= OPEN_FLAG_NO_CONTROLLING_TERMINAL;
    Status = IopOpen(FromKernelMode,
                     SlaveDirectory,
                     SlavePath,
                     SlavePathLength,
                     0,
                     MasterOpenFlags,
                     IoObjectTerminalSlave,
                     &CreationParameters,
                     SlaveCreatePermissions,
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

    FileObject = TerminalHandle->PathPoint.PathEntry->FileObject;
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
    // Lock down the terminal for this. The order of lock acquisition is
    // important.
    //

    KeAcquireQueuedLock(Terminal->OutputLock);
    KeAcquireQueuedLock(Terminal->InputLock);
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
    KeReleaseQueuedLock(Terminal->OutputLock);
    KeReleaseQueuedLock(Terminal->InputLock);
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

    FileObject = TerminalMaster->PathPoint.PathEntry->FileObject;
    if (FileObject->Properties.Type != IoObjectTerminalMaster) {
        Status = STATUS_NOT_A_TERMINAL;
        goto TerminalSetDeviceEnd;
    }

    Terminal = FileObject->SpecialIo;
    Status = STATUS_SUCCESS;

    //
    // Remove the old handle.
    //

    KeAcquireQueuedLock(Terminal->OutputLock);
    KeAcquireQueuedLock(Terminal->InputLock);
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

    KeReleaseQueuedLock(Terminal->OutputLock);
    KeReleaseQueuedLock(Terminal->InputLock);

TerminalSetDeviceEnd:
    return Status;
}

VOID
IoRelinquishTerminal (
    PVOID Terminal,
    SESSION_ID SessionId,
    BOOL TerminalLocked
    )

/*++

Routine Description:

    This routine clears the controlling session and process group ID from
    the given terminal. It should only be called by process termination as
    a session leader is dying.

Arguments:

    Terminal - Supplies a pointer to the controlling terminal.

    SessionId - Supplies the session ID of the session leader that's dying.

    TerminalLocked - Supplies a boolean indicating if the appropriate
        internal terminal locks are already held or not.

Return Value:

    None.

--*/

{

    PTERMINAL TypedTerminal;

    TypedTerminal = Terminal;
    if (TerminalLocked == FALSE) {
        KeAcquireQueuedLock(TypedTerminal->OutputLock);
        KeAcquireQueuedLock(TypedTerminal->InputLock);
    }

    if (TypedTerminal->SessionId == SessionId) {
        IopRelinquishTerminal(TypedTerminal);
    }

    if (TerminalLocked == FALSE) {
        KeReleaseQueuedLock(TypedTerminal->OutputLock);
        KeReleaseQueuedLock(TypedTerminal->InputLock);
    }

    //
    // Release the reference acquired when the slave was opened and became
    // the process' controlling terminal.
    //

    ObReleaseReference(TypedTerminal);
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

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;

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

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;

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
    PKPROCESS Process;
    PROCESS_GROUP_ID ProcessGroup;
    SESSION_ID Session;
    PTERMINAL_SLAVE Slave;
    KSTATUS Status;
    PTERMINAL Terminal;
    BOOL TerminalLocksHeld;

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;

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

    TerminalLocksHeld = FALSE;
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

    TerminalLocksHeld = TRUE;
    KeAcquireQueuedLock(Terminal->OutputLock);
    KeAcquireQueuedLock(Terminal->InputLock);

    //
    // If the terminal is already open in another session, refuse to open.
    //

    Process = PsGetCurrentProcess();
    PsGetProcessGroup(Process, &ProcessGroup, &Session);
    if ((Terminal->SessionId != Session) &&
        (Terminal->SessionId != TERMINAL_INVALID_SESSION)) {

        Status = STATUS_RESOURCE_IN_USE;
        goto TerminalOpenSlaveEnd;
    }

    Terminal->SlaveHandles += 1;

    //
    // Clear the error that may have been set when the last previous slave
    // was closed.
    //

    if (Terminal->SlaveHandles == 1) {
        IoSetIoObjectState(Terminal->MasterFileObject->IoState,
                           POLL_EVENT_DISCONNECTED,
                           FALSE);
    }

    //
    // Make the terminal's owning session this one if it is a session leader
    // and doesn't already have a controlling terminal.
    //

    if ((IoHandle->OpenFlags & OPEN_FLAG_NO_CONTROLLING_TERMINAL) == 0) {
        if ((Terminal->SessionId == TERMINAL_INVALID_SESSION) &&
            (Process->Identifiers.ProcessId == Session)) {

            if (PsSetControllingTerminal(Process, Terminal) == NULL) {
                Terminal->ProcessGroupId = ProcessGroup;
                Terminal->SessionId = Session;
                Terminal->SessionProcess = Process;

                //
                // Add a reference that is released when the terminal is
                // relinquished.
                //

                ObAddReference(Terminal);
            }
        }
    }

    Status = STATUS_SUCCESS;

TerminalOpenSlaveEnd:
    if (TerminalLocksHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
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
    PTERMINAL PreviousTerminal;
    PTERMINAL_SLAVE Slave;
    PTERMINAL Terminal;

    FileObject = IoHandle->PathPoint.PathEntry->FileObject;

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
    KeAcquireQueuedLock(Terminal->OutputLock);
    KeAcquireQueuedLock(Terminal->InputLock);

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
        // Also clear the controlling terminal. This may race with the process
        // dying. If this thread wins, it needs to release the reference.
        // Otherwise, it does not.
        //

        if (Terminal->SessionId != TERMINAL_INVALID_SESSION) {
            PreviousTerminal =
                      PsSetControllingTerminal(Terminal->SessionProcess, NULL);

            if (PreviousTerminal != NULL) {

                ASSERT(PreviousTerminal == Terminal);

                IoRelinquishTerminal(Terminal, Terminal->SessionId, TRUE);

            } else {
                IopRelinquishTerminal(Terminal);
            }
        }
    }

    KeReleaseQueuedLock(Terminal->OutputLock);
    KeReleaseQueuedLock(Terminal->InputLock);

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

    FileObject = Handle->PathPoint.PathEntry->FileObject;

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

    FileObject = Handle->PathPoint.PathEntry->FileObject;

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
    IO_OBJECT_TYPE Type,
    PVOID OverrideParameter,
    FILE_PERMISSIONS CreatePermissions,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine creates a terminal master or slave.

Arguments:

    Type - Supplies the type of special object to create.

    OverrideParameter - Supplies an optional parameter to send along with the
        override type.

    CreatePermissions - Supplies the permissions to assign to the new file.

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

    CreationParameters = OverrideParameter;
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

    if (Type == IoObjectTerminalSlave) {

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
            Properties.Permissions = CreatePermissions;
            Status = IopCreateOrLookupFileObject(&Properties,
                                                 ObGetRootObject(),
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

        ASSERT(Type == IoObjectTerminalMaster);
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
            Properties.Permissions = CreatePermissions;
            Status = IopCreateOrLookupFileObject(&Properties,
                                                 ObGetRootObject(),
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

    BOOL AcceptingSignal;
    INT Argument;
    IO_CONTEXT Context;
    PTERMINAL ControllingTerminal;
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
    PTERMINAL PreviousTerminal;
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
    FileObject = Handle->PathPoint.PathEntry->FileObject;
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
        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (CodeNumber == TerminalControlSetExclusive) {
            Terminal->Flags |= TERMINAL_FLAG_FAIL_OPENS;

        } else {
            Terminal->Flags &= ~TERMINAL_FLAG_FAIL_OPENS;
        }

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
        Status = STATUS_SUCCESS;
        break;

    case TerminalControlGetOutputQueueSize:
    case TerminalControlGetInputQueueSize:
        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (CodeNumber == TerminalControlGetOutputQueueSize) {
            QueueSize = IopTerminalGetOutputBufferSpace(Terminal) -
                        TERMINAL_OUTPUT_BUFFER_SIZE;

        } else {
            QueueSize = IopTerminalGetInputBufferSpace(Terminal) -
                        TERMINAL_INPUT_BUFFER_SIZE;
        }

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
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
        KeAcquireQueuedLock(Terminal->OutputLock);
        RtlCopyMemory(&WindowSize,
                      &(Terminal->WindowSize),
                      sizeof(TERMINAL_WINDOW_SIZE));

        KeReleaseQueuedLock(Terminal->OutputLock);
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

        KeAcquireQueuedLock(Terminal->OutputLock);
        RtlCopyMemory(&(Terminal->WindowSize),
                      &WindowSize,
                      sizeof(TERMINAL_WINDOW_SIZE));

        KeReleaseQueuedLock(Terminal->OutputLock);
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

        KeAcquireQueuedLock(Terminal->OutputLock);
        if (CodeNumber == TerminalControlOrModemStatus) {
            Terminal->ModemStatus |= ModemStatus;

        } else if (CodeNumber == TerminalControlClearModemStatus) {
            Terminal->ModemStatus &= ~ModemStatus;

        } else if (CodeNumber == TerminalControlSetModemStatus) {
            Terminal->ModemStatus = ModemStatus;
        }

        ModemStatus = Terminal->ModemStatus;
        KeReleaseQueuedLock(Terminal->OutputLock);
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

        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (CodeNumber == TerminalControlSetSoftCarrier) {
            Terminal->ModemStatus |= ModemStatus;
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

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
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
        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        } else {
            ProcessGroupId = Terminal->ProcessGroupId;
        }

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
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

        AcceptingSignal = FALSE;
        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        //
        // If the calling process is not in the owning (foreground) process
        // group, then it is sent a signal unless it is blocking or ignoring
        // the background terminal output signal.
        //

        } else {
            if (CurrentProcessGroupId != Terminal->ProcessGroupId) {
                AcceptingSignal = PsIsThreadAcceptingSignal(
                                            NULL,
                                            SIGNAL_BACKGROUND_TERMINAL_OUTPUT);
            }

            //
            // If the process is not accepting signals or the signal did not
            // need to be checked, then the process group is free to change.
            //

            if (AcceptingSignal == FALSE) {
                Terminal->ProcessGroupId = ProcessGroupId;
                Status = STATUS_SUCCESS;
            }
        }

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);

        //
        // If the process is accepting the signal checked above, send it and
        // tell the caller to try again later. If it's not accepting it, just
        // let it go through.
        //

        if (AcceptingSignal != FALSE) {
            PsSignalProcessGroup(CurrentProcessGroupId,
                                 SIGNAL_BACKGROUND_TERMINAL_OUTPUT);

            Status = STATUS_TRY_AGAIN;
        }

        break;

    case TerminalControlSetControllingTerminal:
        Argument = (UINTN)ContextBuffer;

        //
        // The calling process must be a session leader to set a controlling
        // terminal, and must not have a controlling terminal already.
        //

        Process = PsGetCurrentProcess();
        PsGetProcessGroup(Process, &CurrentProcessGroupId, &CurrentSessionId);
        if (CurrentProcessGroupId != CurrentSessionId) {
            Status = STATUS_PERMISSION_DENIED;
            break;
        }

        //
        // If the terminal already belongs to a different session, then it
        // cannot bet set as the controlling terminal of this session unless
        // the caller is root and the argument is 1.
        //

        if (Terminal->SessionId != TERMINAL_INVALID_SESSION) {
            if (Terminal->SessionId == CurrentSessionId) {
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

        //
        // The calling process is also not allowed to already have a
        // controlling terminal. Check the terminal list for a terminal with
        // the current session.
        //

        KeAcquireQueuedLock(IoTerminalListLock);
        ControllingTerminal = IopLookupTerminal(CurrentSessionId);
        if (ControllingTerminal != NULL) {
            Status = STATUS_PERMISSION_DENIED;

        //
        // If there is no controlling terminal for the session, then proceed to
        // set the current terminal as the controlling terminal.
        //

        } else {
            Status = STATUS_SUCCESS;
            KeAcquireQueuedLock(Terminal->OutputLock);
            KeAcquireQueuedLock(Terminal->InputLock);
            if (Terminal->SessionId != TERMINAL_INVALID_SESSION) {
                if (Terminal->SessionId != CurrentSessionId) {
                    Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
                }
            }

            if (KSUCCESS(Status)) {
                if (Terminal->SessionId != TERMINAL_INVALID_SESSION) {

                    //
                    // Clear the controlling terminal in the process. If the
                    // race was won, this thread needs to release the reference
                    // on the terminal.
                    //

                    PreviousTerminal = PsSetControllingTerminal(
                                                      Terminal->SessionProcess,
                                                      NULL);

                    if (PreviousTerminal != NULL) {

                        ASSERT(PreviousTerminal == Terminal);

                        IoRelinquishTerminal(Terminal,
                                             Terminal->SessionId,
                                             TRUE);

                    } else {
                        IopRelinquishTerminal(Terminal);
                    }
                }

                //
                // Try to set the controlling terminal in the new process. Add
                // a reference that will be released when the controlling
                // terminal is relinquished by the process.
                //

                if (PsSetControllingTerminal(Process, Terminal) == NULL) {
                    Terminal->SessionId = CurrentSessionId;
                    Terminal->ProcessGroupId = Terminal->SessionId;
                    Terminal->SessionProcess = Process;
                    ObAddReference(Terminal);
                }
            }

            KeReleaseQueuedLock(Terminal->InputLock);
            KeReleaseQueuedLock(Terminal->OutputLock);
        }

        KeReleaseQueuedLock(IoTerminalListLock);
        break;

    case TerminalControlGetCurrentSessionId:

        //
        // TODO: Fail TIOCGSID if the terminal is not a master pseudoterminal.
        //

        //
        // The given terminal must be the controlling terminal of the calling
        // process.
        //

        PsGetProcessGroup(NULL, &CurrentProcessGroupId, &CurrentSessionId);
        Status = STATUS_SUCCESS;
        KeAcquireQueuedLock(Terminal->OutputLock);
        KeAcquireQueuedLock(Terminal->InputLock);
        if (Terminal->SessionId != CurrentSessionId) {
            Status = STATUS_NOT_A_TERMINAL;

        } else {
            SessionId = Terminal->SessionId;
        }

        KeReleaseQueuedLock(Terminal->InputLock);
        KeReleaseQueuedLock(Terminal->OutputLock);
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
        Status = STATUS_SUCCESS;
        Process = PsGetCurrentProcess();
        PreviousTerminal = PsSetControllingTerminal(Process, NULL);
        if (PreviousTerminal != NULL) {
            PsGetProcessGroup(Process, NULL, &CurrentSessionId);
            IoRelinquishTerminal(PreviousTerminal, CurrentSessionId, FALSE);
        }

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

    BOOL AcceptingSignal;
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
    // If the flushing process is not in the same process group, send the
    // process group a signal unless the flushing process is ignoring or
    // blocking that signal.
    //

    if ((ProcessGroup != Terminal->ProcessGroupId) &&
        ((Terminal->Settings.LocalFlags &
          TERMINAL_LOCAL_STOP_BACKGROUND_WRITES) != 0)) {

        AcceptingSignal = PsIsThreadAcceptingSignal(
                                            NULL,
                                            SIGNAL_BACKGROUND_TERMINAL_OUTPUT);

        //
        // If the process is accepting that signal, send it to it and tell it
        // to try again later. The exception is an orphaned process group, in
        // which case an error is returned. If the process is not accepting the
        // signal, just let the flush go through.
        //

        if (AcceptingSignal != FALSE) {
            if (PsIsProcessGroupOrphaned(ProcessGroup) != FALSE) {
                Status = STATUS_DEVICE_IO_ERROR;

            } else {
                PsSignalProcessGroup(ProcessGroup,
                                     SIGNAL_BACKGROUND_TERMINAL_OUTPUT);

                Status = STATUS_TRY_AGAIN;
            }

            goto TerminalFlushEnd;
        }
    }

    //
    // If discarding, reset the buffers.
    //

    if ((Flags & FLUSH_FLAG_DISCARD) != 0) {
        if ((Flags & FLUSH_FLAG_READ) != 0) {
            KeAcquireQueuedLock(Terminal->InputLock);
            Terminal->InputBufferStart = 0;
            Terminal->InputBufferEnd = 0;
            IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, FALSE);
            KeReleaseQueuedLock(Terminal->InputLock);
            KeAcquireQueuedLock(Terminal->WorkingInputLock);
            Terminal->WorkingInputCursor = 0;
            Terminal->WorkingInputLength = 0;
            KeReleaseQueuedLock(Terminal->WorkingInputLock);
        }

        if ((Flags & FLUSH_FLAG_WRITE) != 0) {
            KeAcquireQueuedLock(Terminal->OutputLock);
            Terminal->OutputBufferStart = 0;
            Terminal->OutputBufferEnd = 0;
            IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, FALSE);
            IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, TRUE);
            KeReleaseQueuedLock(Terminal->OutputLock);
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
                KeAcquireQueuedLock(Terminal->OutputLock);

                //
                // If the output is empty, then hooray, it's done.
                //

                if (Terminal->OutputBufferStart == Terminal->OutputBufferEnd) {
                    KeReleaseQueuedLock(Terminal->OutputLock);
                    break;
                }

                //
                // Hijack the out event and unsignal it. When the master reads
                // the data, it will signal it again.
                //

                IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
                KeReleaseQueuedLock(Terminal->OutputLock);
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
    INITIALIZE_LIST_HEAD(&(Terminal->CommandHistory));
    Terminal->WorkingInputBuffer = MmAllocatePagedPool(
                                                TERMINAL_CANONICAL_BUFFER_SIZE,
                                                TERMINAL_ALLOCATION_TAG);

    if (Terminal->WorkingInputBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    Terminal->WorkingInputLock = KeCreateQueuedLock();
    if (Terminal->WorkingInputLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTerminalObjectEnd;
    }

    Terminal->InputLock = KeCreateQueuedLock();
    if (Terminal->InputLock == NULL) {
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

    Terminal->OutputLock = KeCreateQueuedLock();
    if (Terminal->OutputLock == NULL) {
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

    Terminal->Settings.InputFlags = TERMINAL_INPUT_CR_TO_NEWLINE;
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

    PTERMINAL_HISTORY_ENTRY HistoryEntry;
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

    while (LIST_EMPTY(&(Terminal->CommandHistory)) == FALSE) {
        HistoryEntry = LIST_VALUE(Terminal->CommandHistory.Next,
                                  TERMINAL_HISTORY_ENTRY,
                                  ListEntry);

        LIST_REMOVE(&(HistoryEntry->ListEntry));
        MmFreePagedPool(HistoryEntry);
    }

    if (Terminal->InputBuffer != NULL) {
        MmFreePagedPool(Terminal->InputBuffer);
    }

    if (Terminal->WorkingInputBuffer != NULL) {
        MmFreePagedPool(Terminal->WorkingInputBuffer);
    }

    if (Terminal->WorkingInputLock != NULL) {
        KeDestroyQueuedLock(Terminal->WorkingInputLock);
    }

    if (Terminal->InputLock != NULL) {
        KeDestroyQueuedLock(Terminal->InputLock);
    }

    if (Terminal->OutputBuffer != NULL) {
        MmFreePagedPool(Terminal->OutputBuffer);
    }

    if (Terminal->OutputLock != NULL) {
        KeDestroyQueuedLock(Terminal->OutputLock);
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
    BOOL EchoThisCharacter;
    BOOL InputAdded;
    ULONG InputFlags;
    BOOL InputLockHeld;
    BOOL IsEndOfLine;
    UINTN LocalByteIndex;
    CHAR LocalBytes[64];
    UINTN LocalByteSize;
    ULONG LocalFlags;
    PIO_OBJECT_STATE MasterIoState;
    ULONG MoveIndex;
    BOOL OutputLockHeld;
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
    EchoFlags = TERMINAL_LOCAL_ECHO | TERMINAL_LOCAL_ECHO_ERASE |
                TERMINAL_LOCAL_ECHO_KILL_NEWLINE |
                TERMINAL_LOCAL_ECHO_KILL_EXTENDED |
                TERMINAL_LOCAL_ECHO_NEWLINE |
                TERMINAL_LOCAL_ECHO_CONTROL;

    EchoFlags = EchoFlags & LocalFlags;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    InputAdded = FALSE;
    InputLockHeld = FALSE;
    DirtyRegionBegin = Terminal->WorkingInputCursor;
    DirtyRegionEnd = Terminal->WorkingInputCursor;
    OutputLockHeld = FALSE;
    OutputWritten = FALSE;
    ScreenCursorPosition = Terminal->WorkingInputCursor;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    if (EchoFlags != 0) {
        KeAcquireQueuedLock(Terminal->OutputLock);
        OutputLockHeld = TRUE;
    }

    //
    // In canonical mode, acquire the lock to prevent others from using the
    // working buffer.
    //

    if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) != 0) {
        KeAcquireQueuedLock(Terminal->WorkingInputLock);

    //
    // In raw mode, acquire the input lock now to avoid locking and unlocking
    // for every character.
    //

    } else {
        KeAcquireQueuedLock(Terminal->InputLock);
        InputLockHeld = TRUE;
    }

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
        // Grab the output lock if it's not already held.
        //

        if ((OutputLockHeld == FALSE) && (EchoFlags != 0)) {
            KeAcquireQueuedLock(Terminal->OutputLock);
            OutputLockHeld = TRUE;
        }

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
            // End of file is like pushing enter except there's no enter
            // character.
            //

            } else if (Byte == ControlCharacters[TerminalCharacterEndOfFile]) {
                TransferWorkingBuffer = TRUE;
                AddCharacter = FALSE;
            }

            //
            // Add the character to the working buffer if needed.
            //

            if (AddCharacter != FALSE) {
                if (Terminal->WorkingInputLength !=
                    TERMINAL_CANONICAL_BUFFER_SIZE) {

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

                    if (OutputLockHeld == FALSE) {
                        KeAcquireQueuedLock(Terminal->OutputLock);
                        OutputLockHeld = TRUE;
                    }

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

                    ASSERT(InputLockHeld == FALSE);

                    KeAcquireQueuedLock(Terminal->InputLock);
                    InputLockHeld = TRUE;
                    Space = IopTerminalGetInputBufferSpace(Terminal);
                    if (Space >= Terminal->WorkingInputLength) {
                        break;
                    }

                    IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, FALSE);
                    IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, TRUE);
                    InputAdded = FALSE;
                    KeReleaseQueuedLock(Terminal->InputLock);
                    InputLockHeld = FALSE;
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

                KeReleaseQueuedLock(Terminal->InputLock);
                InputLockHeld = FALSE;
                IopTerminalAddHistoryEntry(Terminal);
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
                KeReleaseQueuedLock(Terminal->InputLock);
                InputLockHeld = FALSE;
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

                KeAcquireQueuedLock(Terminal->InputLock);
                InputLockHeld = TRUE;
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

        if (OutputLockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->OutputLock);
            OutputLockHeld = TRUE;
        }

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
        if (OutputLockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->OutputLock);
            OutputLockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
    }

    if (InputAdded != FALSE) {
        if (InputLockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->InputLock);
            InputLockHeld = TRUE;
        }

        IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, TRUE);
    }

    //
    // Release the various locks that are held.
    //

    if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) != 0) {
        KeReleaseQueuedLock(Terminal->WorkingInputLock);
    }

    if (InputLockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->InputLock);
    }

    if (OutputLockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->OutputLock);
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

    BOOL AcceptingSignal;
    BOOL AnythingWritten;
    UINTN BytesThisRound;
    UINTN BytesWritten;
    UCHAR LocalBytes[64];
    BOOL LockHeld;
    PIO_OBJECT_STATE MasterIoState;
    PROCESS_GROUP_ID ProcessGroup;
    ULONG ReturnedEvents;
    SESSION_ID Session;
    BOOL SignalProcessGroup;
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
    SignalProcessGroup = FALSE;
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
    PsGetProcessGroup(NULL, &ProcessGroup, &Session);

    //
    // Synchronize the checks on the terminal attachment and the owning session
    // and process group with the IOCTLs that may modify them.
    //

    KeAcquireQueuedLock(Terminal->OutputLock);
    LockHeld = TRUE;

    //
    // If the writing process is not in the same process group, send the
    // process group a signal unless the writing process is ignoring or
    // blocking that signal.
    //

    if ((ProcessGroup != Terminal->ProcessGroupId) &&
        ((Terminal->Settings.LocalFlags &
          TERMINAL_LOCAL_STOP_BACKGROUND_WRITES) != 0)) {

        AcceptingSignal = PsIsThreadAcceptingSignal(
                                            NULL,
                                            SIGNAL_BACKGROUND_TERMINAL_OUTPUT);

        //
        // If the process is accepting that signal, send it to it and tell it
        // to try again later. The exception is an orphaned process group, in
        // which case an error is returned. If the process is not accepting the
        // signal, just let the write go through.
        //

        if (AcceptingSignal != FALSE) {
            if (PsIsProcessGroupOrphaned(ProcessGroup) != FALSE) {
                Status = STATUS_DEVICE_IO_ERROR;

            } else {
                SignalProcessGroup = TRUE;
                Status = STATUS_TRY_AGAIN;
            }

            goto TerminalSlaveWriteEnd;
        }
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
            KeReleaseQueuedLock(Terminal->OutputLock);
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

            KeAcquireQueuedLock(Terminal->OutputLock);
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
        IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
        IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, FALSE);
    }

TerminalSlaveWriteEnd:
    if (AnythingWritten != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->OutputLock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_IN, TRUE);
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->OutputLock);
    }

    if (SignalProcessGroup != FALSE) {
        PsSignalProcessGroup(ProcessGroup, SIGNAL_BACKGROUND_TERMINAL_OUTPUT);
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
    KeAcquireQueuedLock(Terminal->OutputLock);
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
            KeReleaseQueuedLock(Terminal->OutputLock);
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

            KeAcquireQueuedLock(Terminal->OutputLock);
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
            KeAcquireQueuedLock(Terminal->OutputLock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(SlaveIoState, POLL_EVENT_OUT, TRUE);
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->OutputLock);
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

    BOOL AcceptingSignal;
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
    PROCESS_GROUP_ID ProcessGroup;
    ULONG ReturnedEvents;
    SESSION_ID Session;
    BOOL SignalProcessGroup;
    PTERMINAL_SLAVE Slave;
    PIO_OBJECT_STATE SlaveIoState;
    ULONG Space;
    KSTATUS Status;
    PTERMINAL Terminal;
    ULONG TimeoutInMilliseconds;

    SignalProcessGroup = FALSE;
    Slave = FileObject->SpecialIo;

    ASSERT(Slave->Header.Type == ObjectTerminalSlave);

    Terminal = Slave->Master;

    ASSERT(FileObject == Terminal->SlaveFileObject);

    SlaveIoState = Terminal->SlaveFileObject->IoState;
    MasterIoState = Terminal->MasterFileObject->IoState;
    LocalFlags = Terminal->Settings.LocalFlags;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    AnythingRead = FALSE;
    BytesRead = 0;
    LockHeld = FALSE;
    PsGetProcessGroup(NULL, &ProcessGroup, &Session);

    //
    // Synchronize the checks on the terminal attachment and the owning session
    // and process group with the IOCTLs that may modify them.
    //

    KeAcquireQueuedLock(Terminal->InputLock);
    LockHeld = TRUE;

    //
    // If the reading process is not in the same process group, send the
    // process group a signal unless the reading process is ignoring or
    // blocking that signal.
    //

    if (ProcessGroup != Terminal->ProcessGroupId) {

        //
        // If it's an orphaned process, fail the I/O.
        //

        if (PsIsProcessGroupOrphaned(ProcessGroup) != FALSE) {
            Status = STATUS_DEVICE_IO_ERROR;
            goto TerminalSlaveReadEnd;
        }

        AcceptingSignal = PsIsThreadAcceptingSignal(
                                             NULL,
                                             SIGNAL_BACKGROUND_TERMINAL_INPUT);

        //
        // If the process is accepting that signal, send it to it and tell it
        // to try again later. If it's not accepting it, just let it go through.
        //

        if (AcceptingSignal != FALSE) {
            SignalProcessGroup = TRUE;
            Status = STATUS_TRY_AGAIN;
            goto TerminalSlaveReadEnd;
        }
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
            KeReleaseQueuedLock(Terminal->InputLock);
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

            KeAcquireQueuedLock(Terminal->InputLock);
            LockHeld = TRUE;
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

        if ((LocalFlags & TERMINAL_LOCAL_CANONICAL) != 0) {
            for (CopyIndex = 0; CopyIndex < CopySize; CopyIndex += 1) {
                InputIndex = Terminal->InputBufferStart + CopyIndex;
                Character = Terminal->InputBuffer[InputIndex];
                if ((Character ==
                     ControlCharacters[TerminalCharacterEndOfLine]) ||
                    (Character == '\n')) {

                    CopySize = CopyIndex + 1;
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

        Terminal->InputBufferStart += CopySize;

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

    //
    // Unsignal the input event if this routine read the last of the available
    // data.
    //

    if ((AnythingRead != FALSE) && (Space == TERMINAL_INPUT_BUFFER_SIZE - 1)) {
        IoSetIoObjectState(SlaveIoState, POLL_EVENT_IN, FALSE);
        IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
    }

TerminalSlaveReadEnd:
    if (AnythingRead != FALSE) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(Terminal->InputLock);
            LockHeld = TRUE;
        }

        IoSetIoObjectState(MasterIoState, POLL_EVENT_OUT, TRUE);
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Terminal->InputLock);
    }

    if (SignalProcessGroup != FALSE) {
        PsSignalProcessGroup(ProcessGroup, SIGNAL_BACKGROUND_TERMINAL_INPUT);
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
                KeReleaseQueuedLock(Terminal->OutputLock);
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

                KeAcquireQueuedLock(Terminal->OutputLock);
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
        KeAcquireQueuedLock(Terminal->OutputLock);
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
    PTERMINAL_HISTORY_ENTRY HistoryEntry;
    UINTN LastIndex;
    ULONG LocalFlags;
    UINTN MoveIndex;
    PLIST_ENTRY NextListEntry;
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

    case TerminalKeyUp:
    case TerminalKeyDown:

        //
        // If the list is empty, there's nothing to do.
        //

        if (LIST_EMPTY(&(Terminal->CommandHistory)) != FALSE) {
            break;
        }

        if (Terminal->LastCommand != NULL) {
            NextListEntry = &(Terminal->LastCommand->ListEntry);

        } else {
            NextListEntry = &(Terminal->CommandHistory);
        }

        if (Terminal->KeyData.Key == TerminalKeyUp) {

            //
            // If it's a virgin line, use the one currently
            // pointed at (the real last command). Otherwise, go
            // up one.
            //

            if ((Terminal->Flags & TERMINAL_FLAG_VIRGIN_LINE) == 0) {
                NextListEntry = NextListEntry->Next;
            }

            if (NextListEntry == &(Terminal->CommandHistory)) {
                NextListEntry = NextListEntry->Next;
            }

        //
        // Move to the next command.
        //

        } else {
            NextListEntry = NextListEntry->Previous;
            if (NextListEntry == &(Terminal->CommandHistory)) {
                NextListEntry = NextListEntry->Previous;
            }
        }

        ASSERT(NextListEntry != &(Terminal->CommandHistory));

        HistoryEntry = LIST_VALUE(NextListEntry,
                                  TERMINAL_HISTORY_ENTRY,
                                  ListEntry);

        //
        // Mark the whole current command region as dirty.
        //

        *DirtyRegionBegin = 0;
        if (Terminal->WorkingInputLength > *DirtyRegionEnd) {
            *DirtyRegionEnd = Terminal->WorkingInputLength;
        }

        //
        // Copy in the new command.
        //

        ASSERT(HistoryEntry->CommandLength < TERMINAL_CANONICAL_BUFFER_SIZE);

        RtlCopyMemory(Terminal->WorkingInputBuffer,
                      HistoryEntry->Command,
                      HistoryEntry->CommandLength);

        Terminal->WorkingInputLength = HistoryEntry->CommandLength;
        Terminal->WorkingInputCursor = HistoryEntry->CommandLength;
        if (Terminal->WorkingInputLength > *DirtyRegionEnd) {
            *DirtyRegionEnd = Terminal->WorkingInputLength;
        }

        Terminal->LastCommand = HistoryEntry;
        Terminal->Flags &= ~TERMINAL_FLAG_VIRGIN_LINE;
        Terminal->Flags |= TERMINAL_FLAG_UNEDITED_LINE;
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

VOID
IopTerminalAddHistoryEntry (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine processes a new command in canonical mode, adding it to the
    command history list.

Arguments:

    Terminal - Supplies a pointer to the terminal.

Return Value:

    None. The routine may fail, but it's largely inconsequential.

--*/

{

    ULONG AllocationSize;
    ULONG CommandLength;
    PCHAR ControlCharacters;
    PTERMINAL_HISTORY_ENTRY HistoryEntry;

    HistoryEntry = NULL;
    CommandLength = Terminal->WorkingInputLength;
    ControlCharacters = Terminal->Settings.ControlCharacters;
    if ((CommandLength != 0) &&
        ((Terminal->WorkingInputBuffer[CommandLength - 1] ==
          ControlCharacters[TerminalCharacterEndOfLine]) ||
         (Terminal->WorkingInputBuffer[CommandLength - 1] == '\n'))) {

        CommandLength -= 1;
    }

    if (CommandLength == 0) {
        return;
    }

    if (TERMINAL_MAX_COMMAND_HISTORY == 0) {
        return;
    }

    //
    // If it came straight from the command history, don't add enother entry.
    //

    if ((Terminal->Flags & TERMINAL_FLAG_UNEDITED_LINE) != 0) {
        return;
    }

    //
    // Calculate the size needed for the new entry.
    //

    AllocationSize = sizeof(TERMINAL_HISTORY_ENTRY) +
                     (sizeof(CHAR) * (CommandLength - ANYSIZE_ARRAY));

    //
    // Remove the last history entry if the list is full.
    //

    if (Terminal->CommandHistorySize >= TERMINAL_MAX_COMMAND_HISTORY) {

        ASSERT(Terminal->CommandHistory.Previous !=
               &(Terminal->CommandHistory));

        HistoryEntry = LIST_VALUE(Terminal->CommandHistory.Previous,
                                  TERMINAL_HISTORY_ENTRY,
                                  ListEntry);

        if (Terminal->LastCommand == HistoryEntry) {

            ASSERT(HistoryEntry->ListEntry.Previous !=
                   &(Terminal->CommandHistory));

            Terminal->LastCommand = LIST_VALUE(HistoryEntry->ListEntry.Previous,
                                               TERMINAL_HISTORY_ENTRY,
                                               ListEntry);
        }

        LIST_REMOVE(&(HistoryEntry->ListEntry));
        Terminal->CommandHistorySize -= 1;

        //
        // If the entry cannot be reused, then free it.
        //

        if (HistoryEntry->CommandLength < CommandLength) {
            MmFreePagedPool(HistoryEntry);
            HistoryEntry = NULL;
        }
    }

    //
    // Allocate a new entry if one is not being reused.
    //

    if (HistoryEntry == NULL) {
        HistoryEntry = MmAllocatePagedPool(AllocationSize,
                                           TERMINAL_ALLOCATION_TAG);

        if (HistoryEntry == NULL) {
            return;
        }
    }

    //
    // Initialize the entry and add it to the list.
    //

    RtlCopyMemory(HistoryEntry->Command,
                  Terminal->WorkingInputBuffer,
                  CommandLength);

    HistoryEntry->CommandLength = CommandLength;
    INSERT_AFTER(&(HistoryEntry->ListEntry), &(Terminal->CommandHistory));
    Terminal->CommandHistorySize += 1;
    Terminal->LastCommand = HistoryEntry;
    return;
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

PTERMINAL
IopLookupTerminal (
    SESSION_ID SessionId
    )

/*++

Routine Description:

    This routine attempts to find the controlling terminal of the given
    session. This routine assumes that the terminal list lock is held and does
    not take a reference on the terminal.

Arguments:

    SessionId - Supplies the ID of the session whose controlling terminal is to
        be looked up.

Return Value:

    Returns a pointer to a terminal if found, or NULL otherwise.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTERMINAL FoundTerminal;
    PTERMINAL Terminal;

    ASSERT(KeIsQueuedLockHeld(IoTerminalListLock) != FALSE);

    FoundTerminal = NULL;
    CurrentEntry = IoTerminalList.Next;
    while (CurrentEntry != &IoTerminalList) {
        Terminal = LIST_VALUE(CurrentEntry, TERMINAL, ListEntry);
        if (Terminal->SessionId == SessionId) {
            FoundTerminal = Terminal;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return FoundTerminal;
}

VOID
IopRelinquishTerminal (
    PTERMINAL Terminal
    )

/*++

Routine Description:

    This routine clears the controlling session and process group ID from
    the given terminal, and signals everyone in the old process group.

Arguments:

    Terminal - Supplies a pointer to the controlling terminal.

Return Value:

    None.

--*/

{

    PROCESS_GROUP_ID ProcessGroupId;

    ProcessGroupId = Terminal->ProcessGroupId;
    Terminal->SessionId = TERMINAL_INVALID_SESSION;
    Terminal->ProcessGroupId = TERMINAL_INVALID_PROCESS_GROUP;
    Terminal->SessionProcess = NULL;

    ASSERT(ProcessGroupId != TERMINAL_INVALID_PROCESS_GROUP);

    PsSignalProcessGroup(ProcessGroupId,
                         SIGNAL_CONTROLLING_TERMINAL_CLOSED);

    PsSignalProcessGroup(ProcessGroupId, SIGNAL_CONTINUE);
    return;
}

