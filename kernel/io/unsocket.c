/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    unsocket.c

Abstract:

    This module implements support for Unix domain sockets.

Author:

    Evan Green 15-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"
#include "unsocket.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default maximum pending send size for a Unix socket. This is
// how much data a socket can send without the receiver receiving it before the
// sender is blocked.
//

#define UNIX_SOCKET_DEFAULT_SEND_MAX 0x20000

//
// Define the maximum number of file descriptors that can be passed in a
// rights control message.
//

#define UNIX_SOCKET_MAX_CONTROL_HANDLES 256

//
// Define the maximum size of control data.
//

#define UNIX_SOCKET_MAX_CONTROL_DATA 32768

//
// Define local socket flags.
//

//
// This flag is set when the socket should send and receive credentials in
// control data automatically.
//

#define UNIX_SOCKET_FLAG_SEND_CREDENTIALS 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _UNIX_SOCKET_STATE {
    UnixSocketStateInvalid,
    UnixSocketStateInitialized,
    UnixSocketStateBound,
    UnixSocketStateListening,
    UnixSocketStateConnected,
    UnixSocketStateShutdownWrite,
    UnixSocketStateShutdownRead,
    UnixSocketStateShutdown,
    UnixSocketStateClosed
} UNIX_SOCKET_STATE, *PUNIX_SOCKET_STATE;

typedef struct _UNIX_SOCKET UNIX_SOCKET, *PUNIX_SOCKET;

/*++

Structure Description:

    This structure defines a set of Unix socket credentials. This structure
    matches up with struct ucred in the C library.

Members:

    ProcessId - Stores the ID of the process that sent the message, or -1 if
        not set.

    UserId - Stores the user ID of the process that sent the message, or -1 if
        not set.

    GroupId - Stores the group ID of the process that sent the message, or -1
        if not set.

--*/

typedef struct _UNIX_SOCKET_CREDENTIALS {
    PROCESS_ID ProcessId;
    USER_ID UserId;
    GROUP_ID GroupId;
} UNIX_SOCKET_CREDENTIALS, *PUNIX_SOCKET_CREDENTIALS;

/*++

Structure Description:

    This structure defines a Unix socket object.

Members:

    ListEntry - Stores pointers to the next and previous packets to be received.

    Data - Stores a pointer to the data.

    Length - Stores the length of the data, in bytes.

    Offset - Stores the number of bytes the receiver has already returned.

    Sender - Stores a pointer to the sender. This structure holds a reference
        to the sender which must be released when this structure is destroyed.

    Credentials - Stores the credentials of the sender.

    Handles - Stores an optional pointer to an array of file handles being
        passed in this message.

    HandleCount - Stores the number of file handles being passed in this
        message.

--*/

typedef struct _UNIX_SOCKET_PACKET {
    LIST_ENTRY ListEntry;
    PVOID Data;
    UINTN Length;
    UINTN Offset;
    PUNIX_SOCKET Sender;
    UNIX_SOCKET_CREDENTIALS Credentials;
    PIO_HANDLE *Handles;
    UINTN HandleCount;
} UNIX_SOCKET_PACKET, *PUNIX_SOCKET_PACKET;

/*++

Structure Description:

    This structure defines a Unix socket object.

Members:

    KernelSocket - Stores the standard kernel socket portions.

    Lock - Stores a pointer to the lock that protects this socket.

    State - Stores the current state of the socket.

    Name - Stores a pointer to the name of the socket, allocated from paged
        pool.

    NameSize - Stores the size of the name buffer in bytes, including the null
        terminator.

    PathPoint - Stores a pointer to the path point where this socket resides if
        it is bound. The socket holds a reference on the path point.

    ReceiveList - Stores the head of the list of outgoing packets. Entries on
        this list are Unix socket packets.

    SendListMax - Stores the maximum number of bytes that can be queued on the
        send list before the socket blocks.

    SendListSize - Stores the current number of bytes queued to be sent.

    MaxBacklog - Stores the maximum incoming connection backlog.

    CurrentBacklog - Stores the current number of incoming connections.

    ConnectionListEntry - Stores a dual-purpose list entry. For servers, this
        represents the head of the list of sockets trying to connect to it.
        For clients, this is the list entry that goes on the server's incoming
        connection list.

    Remote - Stores a pointer to the other side of the connection, for a
        connection oriented socket.

    Flags - Stores a bitfield of flags governing the socket. See
        UNIX_SOCKET_FLAG_* definitions.

    Credentials - Stores the credentials of the process when the socket was
        connected.

--*/

struct _UNIX_SOCKET {
    SOCKET KernelSocket;
    PQUEUED_LOCK Lock;
    UNIX_SOCKET_STATE State;
    PSTR Name;
    UINTN NameSize;
    PATH_POINT PathPoint;
    LIST_ENTRY ReceiveList;
    UINTN SendListMax;
    UINTN SendListSize;
    UINTN MaxBacklog;
    UINTN CurrentBacklog;
    LIST_ENTRY ConnectionListEntry;
    PUNIX_SOCKET Remote;
    ULONG Flags;
    UNIX_SOCKET_CREDENTIALS Credentials;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopUnixSocketEnsureConnected (
    PUNIX_SOCKET Socket,
    BOOL Write
    );

VOID
IopUnixSocketFlushData (
    PUNIX_SOCKET Socket
    );

KSTATUS
IopUnixSocketCreatePacket (
    PUNIX_SOCKET Sender,
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN DataSize,
    PUNIX_SOCKET_PACKET *NewPacket
    );

VOID
IopUnixSocketDestroyPacket (
    PUNIX_SOCKET_PACKET Packet
    );

KSTATUS
IopUnixSocketSendControlData (
    BOOL FromKernelMode,
    PUNIX_SOCKET Sender,
    PUNIX_SOCKET_PACKET Packet,
    PVOID ControlData,
    UINTN ControlDataSize
    );

KSTATUS
IopUnixSocketReceiveControlData (
    BOOL FromKernelMode,
    PUNIX_SOCKET Socket,
    PUNIX_SOCKET_PACKET Packet,
    PSOCKET_IO_PARAMETERS Parameters
    );

VOID
IopUnixSocketInitializeCredentials (
    PUNIX_SOCKET Socket
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopCreateUnixSocketPair (
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE NewSockets[2]
    )

/*++

Routine Description:

    This routine creates a pair of connected local domain sockets.

Arguments:

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value for this socket used on the
        network. This value is network specific.

    OpenFlags - Supplies a bitfield of open flags governing the new handles.
        See OPEN_FLAG_* definitions.

    NewSockets - Supplies an array where the two pointers to connected sockets
        will be returned.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE IoHandles[2];
    PSOCKET Sockets[2];
    KSTATUS Status;
    PUNIX_SOCKET UnixSockets[2];

    IoHandles[0] = NULL;
    IoHandles[1] = NULL;
    Status = IoSocketCreate(NetDomainLocal,
                            Type,
                            Protocol,
                            OpenFlags,
                            &(IoHandles[0]));

    if (!KSUCCESS(Status)) {
        goto CreateUnixSocketPairEnd;
    }

    Status = IoSocketCreate(NetDomainLocal,
                            Type,
                            Protocol,
                            OpenFlags,
                            &(IoHandles[1]));

    if (!KSUCCESS(Status)) {
        goto CreateUnixSocketPairEnd;
    }

    Status = IoGetSocketFromHandle(IoHandles[0], &(Sockets[0]));

    ASSERT(KSUCCESS(Status));

    Status = IoGetSocketFromHandle(IoHandles[1], &(Sockets[1]));

    ASSERT(KSUCCESS(Status));

    UnixSockets[0] = (PUNIX_SOCKET)(Sockets[0]);
    UnixSockets[1] = (PUNIX_SOCKET)(Sockets[1]);

    //
    // Connect the two sockets directly together.
    //

    UnixSockets[0]->Remote = UnixSockets[1];
    IoSocketAddReference(Sockets[1]);
    UnixSockets[1]->Remote = UnixSockets[0];
    IoSocketAddReference(Sockets[0]);
    UnixSockets[0]->State = UnixSocketStateConnected;
    UnixSockets[1]->State = UnixSocketStateConnected;
    IopUnixSocketInitializeCredentials(UnixSockets[0]);
    IopUnixSocketInitializeCredentials(UnixSockets[1]);
    IoSetIoObjectState(UnixSockets[0]->KernelSocket.IoState,
                       POLL_EVENT_OUT,
                       TRUE);

    IoSetIoObjectState(UnixSockets[1]->KernelSocket.IoState,
                       POLL_EVENT_OUT,
                       TRUE);

    Status = STATUS_SUCCESS;

CreateUnixSocketPairEnd:
    if (!KSUCCESS(Status)) {
        if (IoHandles[0] != NULL) {
            IoClose(IoHandles[0]);
            IoHandles[0] = NULL;
        }

        if (IoHandles[1] != NULL) {
            IoClose(IoHandles[1]);
            IoHandles[1] = NULL;
        }
    }

    NewSockets[0] = IoHandles[0];
    NewSockets[1] = IoHandles[1];
    return Status;
}

KSTATUS
IopCreateUnixSocket (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    PSOCKET *NewSocket
    )

/*++

Routine Description:

    This routine creates a new Unix socket object.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value for this socket used on the
        network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The kernel will fill in the standard socket structure after
        this routine returns.

Return Value:

    Status code.

--*/

{

    PUNIX_SOCKET Socket;
    KSTATUS Status;

    ASSERT(Domain == NetDomainLocal);

    Socket = MmAllocatePagedPool(sizeof(UNIX_SOCKET),
                                 UNIX_SOCKET_ALLOCATION_TAG);

    if (Socket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateUnixSocketEnd;
    }

    RtlZeroMemory(Socket, sizeof(UNIX_SOCKET));
    Socket->KernelSocket.Protocol = Protocol;
    Socket->KernelSocket.ReferenceCount = 1;
    Socket->KernelSocket.IoState = IoCreateIoObjectState(FALSE, FALSE);
    if (Socket->KernelSocket.IoState == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateUnixSocketEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState,
                       POLL_EVENT_IN | POLL_EVENT_OUT,
                       FALSE);

    Socket->Lock = KeCreateQueuedLock();
    if (Socket->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateUnixSocketEnd;
    }

    Socket->State = UnixSocketStateInitialized;
    INITIALIZE_LIST_HEAD(&(Socket->ReceiveList));
    INITIALIZE_LIST_HEAD(&(Socket->ConnectionListEntry));
    Socket->SendListMax = UNIX_SOCKET_DEFAULT_SEND_MAX;
    Socket->Credentials.ProcessId = -1;
    Socket->Credentials.UserId = -1;
    Socket->Credentials.GroupId = -1;
    Status = STATUS_SUCCESS;

CreateUnixSocketEnd:
    if (!KSUCCESS(Status)) {
        if (Socket != NULL) {
            if (Socket->Lock != NULL) {
                KeDestroyQueuedLock(Socket->Lock);
            }

            if (Socket->KernelSocket.IoState != NULL) {
                IoDestroyIoObjectState(Socket->KernelSocket.IoState, FALSE);
            }

            MmFreePagedPool(Socket);
            Socket = NULL;
        }
    }

    *NewSocket = &(Socket->KernelSocket);
    return Status;
}

VOID
IopDestroyUnixSocket (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine destroys the Unix socket object.

Arguments:

    Socket - Supplies a pointer to the socket.

Return Value:

    None.

--*/

{

    PUNIX_SOCKET UnixSocket;

    UnixSocket = (PUNIX_SOCKET)Socket;

    ASSERT(Socket->Domain == NetDomainLocal);
    ASSERT(UnixSocket->CurrentBacklog == 0);
    ASSERT(UnixSocket->SendListSize == 0);
    ASSERT(LIST_EMPTY(&(UnixSocket->ReceiveList)) != FALSE);

    if (UnixSocket->PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&(UnixSocket->PathPoint));
        UnixSocket->PathPoint.PathEntry = NULL;
    }

    if (UnixSocket->Name != NULL) {
        MmFreePagedPool(UnixSocket->Name);
    }

    if (UnixSocket->Lock != NULL) {
        KeDestroyQueuedLock(UnixSocket->Lock);
    }

    UnixSocket->State = UnixSocketStateInvalid;
    MmFreePagedPool(UnixSocket);
    return;
}

KSTATUS
IopUnixSocketBindToAddress (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PNETWORK_ADDRESS Address,
    PCSTR Path,
    UINTN PathSize
    )

/*++

Routine Description:

    This routine binds the Unix socket to the given path and starts listening
    for client requests.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the handle to bind.

    Address - Supplies a pointer to the address to bind the socket to.

    Path - Supplies an optional pointer to a path, required if the network
        address is a local socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

{

    CREATE_PARAMETERS Create;
    SOCKET_CREATION_PARAMETERS CreationParameters;
    PFILE_OBJECT FileObject;
    ULONG OpenFlags;
    PSTR PathCopy;
    PATH_POINT PathPoint;
    PSOCKET Socket;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;
    PCSTR WalkedPath;
    ULONG WalkedPathSize;

    PathCopy = NULL;
    IO_INITIALIZE_PATH_POINT(&PathPoint);
    UnixSocket = NULL;
    FileObject = Handle->FileObject;
    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    ASSERT(Socket->Domain == NetDomainLocal);

    UnixSocket = (PUNIX_SOCKET)Socket;
    KeAcquireQueuedLock(UnixSocket->Lock);

    //
    // If the socket isn't a fresh one, fail.
    //

    if (UnixSocket->State != UnixSocketStateInitialized) {
        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketBindToAddressEnd;
    }

    if (PathSize != 0) {
        if (FromKernelMode != FALSE) {
            PathCopy = MmAllocatePagedPool(PathSize,
                                           UNIX_SOCKET_ALLOCATION_TAG);

            if (PathCopy == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto UnixSocketBindToAddressEnd;
            }

            RtlCopyMemory(PathCopy, Path, PathSize);

        //
        // If the request came from user mode, copy the string.
        //

        } else {
            Status = MmCreateCopyOfUserModeString(Path,
                                                  PathSize,
                                                  UNIX_SOCKET_ALLOCATION_TAG,
                                                  &PathCopy);

            if (!KSUCCESS(Status)) {
                goto UnixSocketBindToAddressEnd;
            }
        }
    }

    //
    // If the caller wants an anonymous socket, then just bind it.
    //

    if ((PathSize == 0) || (PathCopy[0] == '\0')) {
        UnixSocket->State = UnixSocketStateBound;
        Status = STATUS_SUCCESS;
        goto UnixSocketBindToAddressEnd;
    }

    //
    // "Create" a socket at the given path, but use this socket rather than
    // actually creating a new one. This opens up a new handle whose path entry
    // points to the named object.
    //

    RtlZeroMemory(&CreationParameters, sizeof(SOCKET_CREATION_PARAMETERS));
    CreationParameters.ExistingSocket = Socket;
    Create.Type = IoObjectSocket;
    Create.Context = &CreationParameters;
    Create.Permissions = FileObject->Properties.Permissions;
    Create.Created = FALSE;
    WalkedPath = PathCopy;
    WalkedPathSize = PathSize;
    OpenFlags = Handle->OpenFlags | OPEN_FLAG_CREATE | OPEN_FLAG_FAIL_IF_EXISTS;
    Status = IopPathWalk(FromKernelMode,
                         NULL,
                         &WalkedPath,
                         &WalkedPathSize,
                         OpenFlags,
                         &Create,
                         &PathPoint);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_FILE_EXISTS) {
            Status = STATUS_ADDRESS_IN_USE;
        }

        goto UnixSocketBindToAddressEnd;
    }

    Status = IopCheckPermissions(FromKernelMode,
                                 &PathPoint,
                                 IO_ACCESS_READ | IO_ACCESS_WRITE);

    if (!KSUCCESS(Status)) {
        goto UnixSocketBindToAddressEnd;
    }

    //
    // Take a reference on the path point before closing the named handle.
    //

    ASSERT(UnixSocket->PathPoint.PathEntry == NULL);

    IO_COPY_PATH_POINT(&(UnixSocket->PathPoint), &PathPoint);
    IO_INITIALIZE_PATH_POINT(&PathPoint);

    //
    // Set the state of the socket to be bound.
    //

    UnixSocket->Name = PathCopy;
    UnixSocket->NameSize = PathSize;
    PathCopy = NULL;
    UnixSocket->State = UnixSocketStateBound;
    Status = STATUS_SUCCESS;

UnixSocketBindToAddressEnd:
    KeReleaseQueuedLock(UnixSocket->Lock);
    if ((PathCopy != NULL) && (PathCopy != Path)) {
        MmFreePagedPool(PathCopy);
    }

    if (PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&PathPoint);
    }

    return Status;
}

KSTATUS
IopUnixSocketListen (
    PSOCKET Socket,
    ULONG BacklogCount
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing sockets to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listening.

    BacklogCount - Supplies the number of attempted connections that can be
        queued before additional connections are refused.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;

    ASSERT(Socket->Domain == NetDomainLocal);

    UnixSocket = (PUNIX_SOCKET)Socket;
    KeAcquireQueuedLock(UnixSocket->Lock);

    //
    // Only connection-oriented sockets can listen.
    //

    if ((Socket->Type != NetSocketStream) &&
        (Socket->Type != NetSocketSequencedPacket)) {

        Status = STATUS_NOT_SUPPORTED;
        goto UnixSocketListenEnd;
    }

    //
    // The socket had better have just been bound and that's it. Allow folks
    // that have already called listen to call listen again with a new backlog
    // parameter.
    //

    if ((UnixSocket->State != UnixSocketStateBound) &&
        (UnixSocket->State != UnixSocketStateListening)) {

        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketListenEnd;
    }

    UnixSocket->MaxBacklog = BacklogCount;
    if (UnixSocket->State != UnixSocketStateListening) {
        IoSetIoObjectState(Socket->IoState, POLL_EVENT_IN, FALSE);
        IoSetIoObjectState(Socket->IoState, POLL_EVENT_OUT, TRUE);
        UnixSocket->State = UnixSocketStateListening;
    }

    IopUnixSocketInitializeCredentials(UnixSocket);
    Status = STATUS_SUCCESS;

UnixSocketListenEnd:
    KeReleaseQueuedLock(UnixSocket->Lock);
    return Status;
}

KSTATUS
IopUnixSocketAccept (
    PSOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress,
    PCSTR *RemotePath,
    PUINTN RemotePathSize
    )

/*++

Routine Description:

    This routine accepts an incoming connection on a listening connection-based
    socket.

Arguments:

    Socket - Supplies a pointer to the socket to accept a connection from.

    NewConnectionSocket - Supplies a pointer where a new socket will be
        returned that represents the accepted connection with the remote
        host.

    RemoteAddress - Supplies a pointer where the address of the connected
        remote host will be returned.

    RemotePath - Supplies a pointer where a string containing the remote path
        will be returned on success. The caller does not own this string, it is
        connected with the new socket coming out.

    RemotePathSize - Supplies a pointer where the size of the remote path in
        bytes will be returned on success.

Return Value:

    Status code.

--*/

{

    BOOL LockHeld;
    PIO_HANDLE NewSocketHandle;
    PUNIX_SOCKET NewUnixSocket;
    ULONG OpenFlags;
    ULONG ReturnedEvents;
    KSTATUS Status;
    ULONG Timeout;
    PUNIX_SOCKET UnixSocket;

    ASSERT(Socket->Domain == NetDomainLocal);

    LockHeld = FALSE;
    NewSocketHandle = NULL;
    Timeout = WAIT_TIME_INDEFINITE;
    OpenFlags = IoGetIoHandleOpenFlags(Socket->IoHandle);
    if ((OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
        Timeout = 0;
    }

    UnixSocket = (PUNIX_SOCKET)Socket;

    //
    // Only connection-oriented sockets can accept.
    //

    if ((Socket->Type != NetSocketStream) &&
        (Socket->Type != NetSocketSequencedPacket)) {

        Status = STATUS_NOT_SUPPORTED;
        goto UnixSocketAcceptEnd;
    }

    //
    // Wait for a new connection to come in.
    //

    while (TRUE) {

        //
        // Race to acquire the lock and be the one to win the new connection.
        //

        KeAcquireQueuedLock(UnixSocket->Lock);
        LockHeld = TRUE;

        //
        // The socket had better be listening.
        //

        if ((UnixSocket->State != UnixSocketStateListening) &&
            (UnixSocket->State != UnixSocketStateShutdown)) {

            Status = STATUS_INVALID_PARAMETER;
            goto UnixSocketAcceptEnd;
        }

        //
        // If there's a connection ready, break out with the lock held and get
        // it.
        //

        if (UnixSocket->CurrentBacklog != 0) {
            break;
        }

        //
        // If the socket is shut down, don't wait.
        //

        if (UnixSocket->State != UnixSocketStateListening) {
            Status = STATUS_INVALID_PARAMETER;
            goto UnixSocketAcceptEnd;
        }

        //
        // Whoever did win the new connection better have reset the event.
        //

        ASSERT((Socket->IoState->Events & POLL_EVENT_IN) == 0);

        KeReleaseQueuedLock(UnixSocket->Lock);
        Status = IoWaitForIoObjectState(Socket->IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        Timeout,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            if (Status == STATUS_TIMEOUT) {
                Status = STATUS_OPERATION_WOULD_BLOCK;
            }

            goto UnixSocketAcceptEnd;
        }

        ASSERT((ReturnedEvents & POLL_EVENT_IN) != 0);
    }

    //
    // Grab the new server side socket.
    //

    ASSERT(LIST_EMPTY(&(UnixSocket->ConnectionListEntry)) == FALSE);

    NewUnixSocket = LIST_VALUE(UnixSocket->ConnectionListEntry.Next,
                               UNIX_SOCKET,
                               ConnectionListEntry);

    LIST_REMOVE(&(NewUnixSocket->ConnectionListEntry));
    Status = STATUS_SUCCESS;

    ASSERT(NewUnixSocket->State == UnixSocketStateConnected);
    ASSERT(UnixSocket->CurrentBacklog != 0);

    UnixSocket->CurrentBacklog -= 1;
    if (UnixSocket->CurrentBacklog < UnixSocket->MaxBacklog) {
        IoSetIoObjectState(Socket->IoState, POLL_EVENT_OUT, TRUE);
    }

    //
    // Unsignal the server if that was the last connection.
    //

    if (LIST_EMPTY(&(UnixSocket->ConnectionListEntry)) != FALSE) {
        IoSetIoObjectState(UnixSocket->KernelSocket.IoState,
                           POLL_EVENT_IN,
                           FALSE);
    }

    RtlZeroMemory(RemoteAddress, sizeof(NETWORK_ADDRESS));
    RemoteAddress->Domain = NetDomainLocal;
    *RemotePath = NewUnixSocket->Remote->Name;
    *RemotePathSize = NewUnixSocket->Remote->NameSize;
    NewSocketHandle = NewUnixSocket->KernelSocket.IoHandle;

UnixSocketAcceptEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(UnixSocket->Lock);
    }

    if (!KSUCCESS(Status)) {
        NewSocketHandle = NULL;
    }

    *NewConnectionSocket = NewSocketHandle;
    return Status;
}

KSTATUS
IopUnixSocketConnect (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PNETWORK_ADDRESS Address,
    PCSTR RemotePath,
    UINTN RemotePathSize
    )

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

    RemotePath - Supplies a pointer to the string containing the path to
        connect to.

    RemotePathSize - Supplies the size fo the remote path string in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PSOCKET NewSocket;
    PIO_HANDLE NewSocketHandle;
    PUNIX_SOCKET NewUnixSocket;
    ULONG OpenFlags;
    PATH_POINT PathPoint;
    ULONG ReturnedEvents;
    BOOL ServerLockHeld;
    PSOCKET ServerSocket;
    PUNIX_SOCKET ServerUnixSocket;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;
    PCSTR WalkedPath;
    ULONG WalkedPathSize;

    NewSocketHandle = NULL;
    IO_INITIALIZE_PATH_POINT(&PathPoint);
    ServerLockHeld = FALSE;
    ServerUnixSocket = NULL;
    UnixSocket = (PUNIX_SOCKET)Socket;
    KeAcquireQueuedLock(UnixSocket->Lock);
    if (Address->Domain != NetDomainLocal) {
        Status = STATUS_UNEXPECTED_TYPE;
        goto UnixSocketConnectEnd;
    }

    if ((Socket->Type != NetSocketStream) &&
        (Socket->Type != NetSocketSequencedPacket) &&
        (Socket->Type != NetSocketDatagram)) {

        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketConnectEnd;
    }

    if (RemotePathSize == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketConnectEnd;
    }

    if (UnixSocket->State == UnixSocketStateConnected) {
        Status = STATUS_CONNECTION_EXISTS;
        goto UnixSocketConnectEnd;
    }

    if ((UnixSocket->State != UnixSocketStateInitialized) &&
        (UnixSocket->State != UnixSocketStateBound)) {

        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketConnectEnd;
    }

    //
    // Attempt to open a handle to the socket.
    //

    ASSERT((PVOID)RemotePath >= KERNEL_VA_START);

    WalkedPath = RemotePath;
    WalkedPathSize = RemotePathSize;
    Status = IopPathWalk(FromKernelMode,
                         NULL,
                         &WalkedPath,
                         &WalkedPathSize,
                         0,
                         NULL,
                         &PathPoint);

    if (!KSUCCESS(Status)) {
        goto UnixSocketConnectEnd;
    }

    Status = IopCheckPermissions(FromKernelMode,
                                 &PathPoint,
                                 IO_ACCESS_READ | IO_ACCESS_WRITE);

    if (!KSUCCESS(Status)) {
        goto UnixSocketConnectEnd;
    }

    //
    // Get the socket from the file object.
    //

    FileObject = PathPoint.PathEntry->FileObject;
    if (FileObject->Properties.Type != IoObjectSocket) {
        Status = STATUS_NOT_A_SOCKET;
        goto UnixSocketConnectEnd;
    }

    ServerSocket = FileObject->SpecialIo;

    ASSERT(ServerSocket->Domain == NetDomainLocal);

    ServerUnixSocket = (PUNIX_SOCKET)ServerSocket;

    //
    // Fail if the types disagree.
    //

    if (ServerSocket->Type != Socket->Type) {
        Status = STATUS_INVALID_ADDRESS;
        goto UnixSocketConnectEnd;
    }

    //
    // For datagram sockets, just set it as the remote and go on.
    //

    if (Socket->Type == NetSocketDatagram) {
        UnixSocket->Remote = ServerUnixSocket;

    //
    // For connection-based sockets, really connect the two.
    //

    } else {

        //
        // Stream sockets are not allowed to be both the server and the client.
        // Fail if it's the same socket. The socket's state should not be
        // listening, but special case it here by comparing the sockets to
        // prevent the attempt to reacquire the lock.
        //

        if (ServerSocket == Socket) {

            ASSERT(ServerUnixSocket->State != UnixSocketStateListening);

            Status = STATUS_CONNECTION_CLOSED;
            goto UnixSocketConnectEnd;
        }

        //
        // Loop until the lock is held and there's space for a new connection.
        //

        OpenFlags = IoGetIoHandleOpenFlags(Socket->IoHandle);
        while (TRUE) {
            KeAcquireQueuedLock(ServerUnixSocket->Lock);
            ServerLockHeld = TRUE;
            if (ServerUnixSocket->State != UnixSocketStateListening) {
                Status = STATUS_CONNECTION_CLOSED;
                goto UnixSocketConnectEnd;
            }

            if (ServerUnixSocket->CurrentBacklog >=
                ServerUnixSocket->MaxBacklog) {

                ASSERT((ServerSocket->IoState->Events & POLL_EVENT_OUT) == 0);

                KeReleaseQueuedLock(ServerUnixSocket->Lock);
                ServerLockHeld = FALSE;

                //
                // If it was opened non-blocking, then return immediately.
                //

                if ((OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
                    Status = STATUS_OPERATION_WOULD_BLOCK;
                    goto UnixSocketConnectEnd;
                }

                Status = IoWaitForIoObjectState(ServerSocket->IoState,
                                                POLL_EVENT_OUT,
                                                TRUE,
                                                WAIT_TIME_INDEFINITE,
                                                &ReturnedEvents);

                if (!KSUCCESS(Status)) {
                    goto UnixSocketConnectEnd;
                }

            //
            // The lock is held and there's space.
            //

            } else {
                break;
            }
        }

        //
        // The server's lock is held. Create a new socket on the server side to
        // represent this connection. Creating the server's socket in connect
        // allows the connection to be fully established when this routine
        // returns, even if the accept call hasn't happened yet. This is in
        // line with behavior of other systems.
        //

        Status = IoSocketCreate(Socket->Domain,
                                Socket->Type,
                                Socket->Protocol,
                                0,
                                &NewSocketHandle);

        if (!KSUCCESS(Status)) {
            goto UnixSocketConnectEnd;
        }

        Status = IoGetSocketFromHandle(NewSocketHandle, &NewSocket);
        if (!KSUCCESS(Status)) {
            goto UnixSocketConnectEnd;
        }

        ASSERT(NewSocket->Domain == NetDomainLocal);

        NewUnixSocket = (PUNIX_SOCKET)NewSocket;

        //
        // Copy the path.
        //

        if (ServerUnixSocket->Name != NULL) {
            NewUnixSocket->Name = MmAllocatePagedPool(
                                                   ServerUnixSocket->NameSize,
                                                   UNIX_SOCKET_ALLOCATION_TAG);

            if (NewUnixSocket->Name == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto UnixSocketConnectEnd;
            }

            NewUnixSocket->NameSize = ServerUnixSocket->NameSize;
            RtlCopyMemory(NewUnixSocket->Name,
                          ServerUnixSocket->Name,
                          NewUnixSocket->NameSize);
        }

        ASSERT(ServerUnixSocket->Credentials.ProcessId != -1);

        RtlCopyMemory(&(NewUnixSocket->Credentials),
                      &(ServerUnixSocket->Credentials),
                      sizeof(UNIX_SOCKET_CREDENTIALS));

        //
        // Wire the two sockets together in a connection.
        //

        ASSERT(ServerUnixSocket->CurrentBacklog < ServerUnixSocket->MaxBacklog);

        IoSetIoObjectState(NewUnixSocket->KernelSocket.IoState,
                           POLL_EVENT_OUT,
                           TRUE);

        IoSetIoObjectState(UnixSocket->KernelSocket.IoState,
                           POLL_EVENT_OUT,
                           TRUE);

        UnixSocket->Remote = NewUnixSocket;
        IoSocketAddReference(&(NewUnixSocket->KernelSocket));
        NewUnixSocket->Remote = UnixSocket;
        IoSocketAddReference(&(UnixSocket->KernelSocket));
        UnixSocket->State = UnixSocketStateConnected;
        NewUnixSocket->State = UnixSocketStateConnected;
        INSERT_BEFORE(&(NewUnixSocket->ConnectionListEntry),
                      &(ServerUnixSocket->ConnectionListEntry));

        if (ServerUnixSocket->CurrentBacklog == 0) {
            IoSetIoObjectState(ServerSocket->IoState, POLL_EVENT_IN, TRUE);
        }

        ServerUnixSocket->CurrentBacklog += 1;
        KeReleaseQueuedLock(ServerUnixSocket->Lock);
        ServerLockHeld = FALSE;
    }

    IopUnixSocketInitializeCredentials(UnixSocket);
    Status = STATUS_SUCCESS;

UnixSocketConnectEnd:
    KeReleaseQueuedLock(UnixSocket->Lock);
    if (PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&PathPoint);
    }

    if (ServerLockHeld != FALSE) {
        KeReleaseQueuedLock(ServerUnixSocket->Lock);
    }

    return Status;
}

KSTATUS
IopUnixSocketSendData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the local socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Socket - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

{

    UINTN BytesCompleted;
    PNETWORK_ADDRESS Destination;
    NETWORK_ADDRESS DestinationLocal;
    PFILE_OBJECT FileObject;
    ULONG OpenFlags;
    PUNIX_SOCKET_PACKET Packet;
    UINTN PacketSize;
    PATH_POINT PathPoint;
    PKPROCESS Process;
    PSTR RemoteCopy;
    PSTR RemotePath;
    PSOCKET RemoteSocket;
    PUNIX_SOCKET RemoteUnixSocket;
    ULONG ReturnedEvents;
    UINTN Size;
    KSTATUS Status;
    PKTHREAD Thread;
    PUNIX_SOCKET UnixSocket;
    BOOL UnixSocketLockHeld;
    PCSTR WalkedPath;
    ULONG WalkedPathSize;

    BytesCompleted = 0;
    Packet = NULL;
    IO_INITIALIZE_PATH_POINT(&PathPoint);
    RemoteCopy = NULL;
    UnixSocket = (PUNIX_SOCKET)Socket;
    KeAcquireQueuedLock(UnixSocket->Lock);
    UnixSocketLockHeld = TRUE;

    //
    // Make sure the socket is properly connected (or as connected as it needs
    // to be).
    //

    Status = IopUnixSocketEnsureConnected(UnixSocket, TRUE);
    if (!KSUCCESS(Status)) {
        goto UnixSocketSendDataEnd;
    }

    //
    // Get or open the remote socket being sent to.
    //

    RemoteUnixSocket = UnixSocket->Remote;
    Destination = Parameters->NetworkAddress;
    if ((Destination != NULL) && (FromKernelMode == FALSE)) {
        Status = MmCopyFromUserMode(&DestinationLocal,
                                    Destination,
                                    sizeof(NETWORK_ADDRESS));

        Destination = &DestinationLocal;
        if (!KSUCCESS(Status)) {
            goto UnixSocketSendDataEnd;
        }
    }

    if ((Destination != NULL) &&
        (Destination->Domain != NetDomainInvalid)) {

        if (UnixSocket->KernelSocket.Type != NetSocketDatagram) {
            Status = STATUS_NOT_SUPPORTED;
            goto UnixSocketSendDataEnd;
        }

        if (Destination->Domain != NetDomainLocal) {
            Status = STATUS_UNEXPECTED_TYPE;
            goto UnixSocketSendDataEnd;
        }

        if (Parameters->RemotePathSize == 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto UnixSocketSendDataEnd;
        }

        RemotePath = Parameters->RemotePath;
        if (FromKernelMode == FALSE) {
            Status = MmCreateCopyOfUserModeString(RemotePath,
                                                  Parameters->RemotePathSize,
                                                  UNIX_SOCKET_ALLOCATION_TAG,
                                                  &RemoteCopy);

            RemotePath = RemoteCopy;
            if (!KSUCCESS(Status)) {
                goto UnixSocketSendDataEnd;
            }
        }

        ASSERT((PVOID)RemotePath >= KERNEL_VA_START);

        WalkedPath = RemotePath;
        WalkedPathSize = Parameters->RemotePathSize;
        Status = IopPathWalk(FromKernelMode,
                             NULL,
                             &WalkedPath,
                             &WalkedPathSize,
                             0,
                             NULL,
                             &PathPoint);

        if (!KSUCCESS(Status)) {
            goto UnixSocketSendDataEnd;
        }

        Status = IopCheckPermissions(FromKernelMode,
                                     &PathPoint,
                                     IO_ACCESS_READ | IO_ACCESS_WRITE);

        if (!KSUCCESS(Status)) {
            goto UnixSocketSendDataEnd;
        }

        //
        // Get the socket from the file object.
        //

        FileObject = PathPoint.PathEntry->FileObject;
        if (FileObject->Properties.Type != IoObjectSocket) {
            return STATUS_NOT_A_SOCKET;
        }

        ASSERT((UINTN)(FileObject->Properties.DeviceId) ==
               OBJECT_MANAGER_DEVICE_ID);

        RemoteSocket = (PSOCKET)(UINTN)(FileObject->Properties.FileId);

        ASSERT(RemoteSocket->Domain == NetDomainLocal);

        RemoteUnixSocket = (PUNIX_SOCKET)RemoteSocket;
    }

    if (RemoteUnixSocket == NULL) {
        Status = STATUS_NOT_CONNECTED;
        goto UnixSocketSendDataEnd;
    }

    OpenFlags = IoGetIoHandleOpenFlags(Socket->IoHandle);

    //
    // Loop while there's data to send.
    //

    Size = Parameters->Size;
    while (Size != 0) {
        if (UnixSocketLockHeld == FALSE) {
            KeAcquireQueuedLock(UnixSocket->Lock);
            UnixSocketLockHeld = TRUE;
        }

        //
        // Make sure a close didn't sneak in while the lock was not held.
        //

        Status = IopUnixSocketEnsureConnected(UnixSocket, TRUE);
        if (!KSUCCESS(Status)) {
            goto UnixSocketSendDataEnd;
        }

        //
        // For types where the message boundaries matter, the size must fit in
        // a single packet.
        //

        if ((Socket->Type == NetSocketDatagram) ||
            (Socket->Type == NetSocketSequencedPacket)) {

            if (Size > UnixSocket->SendListMax) {
                Status = STATUS_MESSAGE_TOO_LONG;
                goto UnixSocketSendDataEnd;
            }
        }

        if (UnixSocket->SendListSize >= UnixSocket->SendListMax) {
            PacketSize = 0;

        } else {
            PacketSize = UnixSocket->SendListMax - UnixSocket->SendListSize;
        }

        if (PacketSize > Size) {
            PacketSize = Size;
        }

        //
        // If the whole packet needs to be sent in one go, block to wait for
        // more space to free up, and try again.
        //

        if ((Socket->Type == NetSocketDatagram) ||
            (Socket->Type == NetSocketSequencedPacket)) {

            if (Size > PacketSize) {
                PacketSize = 0;
            }

        //
        // Streams can send multiple packets at a time.
        //

        } else {

            ASSERT(Socket->Type == NetSocketStream);
        }

        //
        // Create the packet.
        //

        Packet = NULL;
        if (PacketSize != 0) {
            Status = IopUnixSocketCreatePacket(UnixSocket,
                                               IoBuffer,
                                               BytesCompleted,
                                               PacketSize,
                                               &Packet);

            if (!KSUCCESS(Status)) {
                goto UnixSocketSendDataEnd;
            }

            //
            // Charge the socket for the data while the lock is still held.
            //

            UnixSocket->SendListSize += PacketSize;
            if (UnixSocket->SendListSize >= UnixSocket->SendListMax) {
                IoSetIoObjectState(Socket->IoState, POLL_EVENT_OUT, FALSE);
            }

            if ((Parameters->ControlData != NULL) &&
                (Parameters->ControlDataSize != 0)) {

                Status = IopUnixSocketSendControlData(
                                                  FromKernelMode,
                                                  UnixSocket,
                                                  Packet,
                                                  Parameters->ControlData,
                                                  Parameters->ControlDataSize);

                if (!KSUCCESS(Status)) {
                    goto UnixSocketSendDataEnd;
                }
            }

            //
            // Send the credentials if either side has that option set.
            //

            if ((Packet->Credentials.ProcessId == -1) &&
                (((UnixSocket->Flags | RemoteUnixSocket->Flags) &
                  UNIX_SOCKET_FLAG_SEND_CREDENTIALS) != 0)) {

                Thread = KeGetCurrentThread();
                Process = Thread->OwningProcess;
                Packet->Credentials.ProcessId = Process->Identifiers.ProcessId;
                Packet->Credentials.UserId = Thread->Identity.RealUserId;
                Packet->Credentials.GroupId = Thread->Identity.RealGroupId;
            }

        //
        // The packet doesn't fit, so block until data is flushed out.
        //

        } else {
            IoSetIoObjectState(Socket->IoState, POLL_EVENT_OUT, FALSE);
        }

        KeReleaseQueuedLock(UnixSocket->Lock);
        UnixSocketLockHeld = FALSE;

        //
        // If no packet was created, wait for some space to open up.
        //

        if (Packet == NULL) {
            if ((OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
                if (BytesCompleted != 0) {
                    Status = STATUS_SUCCESS;

                } else {
                    Status = STATUS_OPERATION_WOULD_BLOCK;
                }

                goto UnixSocketSendDataEnd;
            }

            Status = IoWaitForIoObjectState(Socket->IoState,
                                            POLL_EVENT_OUT,
                                            TRUE,
                                            Parameters->TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto UnixSocketSendDataEnd;
            }

            //
            // Try again to send some data.
            //

            continue;
        }

        //
        // Both locks should not be held at once because it could create lock
        // ordering issues.
        //

        ASSERT(UnixSocketLockHeld == FALSE);

        KeAcquireQueuedLock(RemoteUnixSocket->Lock);
        if (RemoteUnixSocket->KernelSocket.Type != Socket->Type) {
            KeReleaseQueuedLock(RemoteUnixSocket->Lock);
            Status = STATUS_UNEXPECTED_TYPE;
            goto UnixSocketSendDataEnd;
        }

        //
        // Make sure the server is connected.
        //

        Status = IopUnixSocketEnsureConnected(RemoteUnixSocket, FALSE);
        if (!KSUCCESS(Status)) {
            Status = STATUS_BROKEN_PIPE;
            KeReleaseQueuedLock(RemoteUnixSocket->Lock);
            goto UnixSocketSendDataEnd;
        }

        INSERT_BEFORE(&(Packet->ListEntry), &(RemoteUnixSocket->ReceiveList));

        //
        // If this is the only item on the list, signal the remote socket.
        //

        if (Packet->ListEntry.Previous == &(RemoteUnixSocket->ReceiveList)) {
            IoSetIoObjectState(RemoteUnixSocket->KernelSocket.IoState,
                               POLL_EVENT_IN,
                               TRUE);
        }

        KeReleaseQueuedLock(RemoteUnixSocket->Lock);
        Packet = NULL;
        BytesCompleted += PacketSize;
        Size -= PacketSize;
    }

UnixSocketSendDataEnd:
    if (RemoteCopy != NULL) {
        MmFreePagedPool(RemoteCopy);
    }

    if (!KSUCCESS(Status)) {

        //
        // Roll back the charge to the socket if the packet is still around.
        //

        if (Packet != NULL) {
            if (UnixSocketLockHeld == FALSE) {
                KeAcquireQueuedLock(UnixSocket->Lock);
                UnixSocketLockHeld = TRUE;
            }

            ASSERT(UnixSocket->SendListSize >= Packet->Length);

            UnixSocket->SendListSize -= Packet->Length;
            if (UnixSocket->SendListSize < UnixSocket->SendListMax) {
                IoSetIoObjectState(Socket->IoState, POLL_EVENT_OUT, TRUE);
            }

            IopUnixSocketDestroyPacket(Packet);
        }
    }

    if (UnixSocketLockHeld != FALSE) {
        KeReleaseQueuedLock(UnixSocket->Lock);
    }

    if (PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&PathPoint);
    }

    Parameters->BytesCompleted = BytesCompleted;
    return Status;
}

KSTATUS
IopUnixSocketReceiveData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Socket - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    Status code.

--*/

{

    UINTN ByteCount;
    UINTN BytesReceived;
    PUNIX_SOCKET FirstSender;
    ULONG OpenFlags;
    PUNIX_SOCKET_PACKET Packet;
    PUNIX_SOCKET Remote;
    NETWORK_ADDRESS RemoteAddressLocal;
    ULONG ReturnedEvents;
    UINTN SenderCopySize;
    UINTN Size;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;
    BOOL UnixSocketLockHeld;

    BytesReceived = 0;
    FirstSender = NULL;
    Size = Parameters->Size;
    UnixSocket = (PUNIX_SOCKET)Socket;
    UnixSocketLockHeld = FALSE;

    ASSERT(Socket->Domain == NetDomainLocal);

    //
    // Loop reading stuff.
    //

    OpenFlags = IoGetIoHandleOpenFlags(Socket->IoHandle);
    while (Size != 0) {
        if (UnixSocketLockHeld == FALSE) {
            KeAcquireQueuedLock(UnixSocket->Lock);
            UnixSocketLockHeld = TRUE;
        }

        Status = IopUnixSocketEnsureConnected(UnixSocket, FALSE);
        if (!KSUCCESS(Status)) {
            goto UnixSocketReceiveDataEnd;
        }

        //
        // If the list is empty, wait and try again.
        //

        if (LIST_EMPTY(&(UnixSocket->ReceiveList)) != FALSE) {
            IoSetIoObjectState(Socket->IoState, POLL_EVENT_IN, FALSE);

            //
            // If something was retrieved already, just use that.
            //

            if (BytesReceived != 0) {
                break;
            }

            //
            // If this is a connection oriented socket and the remote is shut
            // down for writing, end now.
            //

            if ((Socket->Type == NetSocketStream) ||
                (Socket->Type == NetSocketSequencedPacket)) {

                Remote = UnixSocket->Remote;
                if ((Remote != NULL) &&
                    (Remote->State != UnixSocketStateConnected) &&
                    (Remote->State != UnixSocketStateShutdownRead)) {

                    Status = STATUS_END_OF_FILE;
                    goto UnixSocketReceiveDataEnd;
                }
            }

            if ((OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
                Status = STATUS_OPERATION_WOULD_BLOCK;
                goto UnixSocketReceiveDataEnd;
            }

            KeReleaseQueuedLock(UnixSocket->Lock);
            UnixSocketLockHeld = FALSE;
            Status = IoWaitForIoObjectState(Socket->IoState,
                                            POLL_EVENT_IN,
                                            TRUE,
                                            Parameters->TimeoutInMilliseconds,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto UnixSocketReceiveDataEnd;
            }

            continue;

        //
        // Grab stuff off the list.
        //

        } else {
            Packet = LIST_VALUE(UnixSocket->ReceiveList.Next,
                                UNIX_SOCKET_PACKET,
                                ListEntry);

            //
            // Don't cross boundaries of different senders or packets with
            // control data.
            //

            if (FirstSender != NULL) {
                if ((Packet->Sender != FirstSender) ||
                    (Packet->Credentials.ProcessId != -1) ||
                    (Packet->Credentials.UserId != -1) ||
                    (Packet->Credentials.GroupId != -1) ||
                    (Packet->HandleCount != 0)) {

                    break;
                }
            }

            FirstSender = Packet->Sender;
            ByteCount = Packet->Length - Packet->Offset;
            if (ByteCount > Size) {
                ByteCount = Size;
            }

            Status = MmCopyIoBufferData(IoBuffer,
                                        Packet->Data + Packet->Offset,
                                        BytesReceived,
                                        ByteCount,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto UnixSocketReceiveDataEnd;
            }

            Packet->Offset += ByteCount;
            BytesReceived += ByteCount;
            Size -= ByteCount;

            //
            // Copy the ancillary data as well.
            //

            Status = IopUnixSocketReceiveControlData(FromKernelMode,
                                                     UnixSocket,
                                                     Packet,
                                                     Parameters);

            if (!KSUCCESS(Status)) {
                goto UnixSocketReceiveDataEnd;
            }

            //
            // Return the remote path if requested.
            //

            if ((Parameters->RemotePath != NULL) &&
                (Parameters->RemotePathSize != 0)) {

                SenderCopySize = Packet->Sender->NameSize;
                if (SenderCopySize > Parameters->RemotePathSize) {
                    SenderCopySize = Parameters->RemotePathSize;
                }

                Parameters->RemotePathSize = Packet->Sender->NameSize;
                if (SenderCopySize != 0) {
                    if (FromKernelMode != FALSE) {
                        RtlCopyMemory(Parameters->RemotePath,
                                      Packet->Sender->Name,
                                      SenderCopySize);

                    } else {
                        Status = MmCopyToUserMode(Parameters->RemotePath,
                                                  Packet->Sender->Name,
                                                  SenderCopySize);

                        if (!KSUCCESS(Status)) {
                            goto UnixSocketReceiveDataEnd;
                        }
                    }
                }
            }

            //
            // Copy the network address portion of the sender address as well.
            //

            if (Parameters->NetworkAddress != NULL) {
                if (FromKernelMode != FALSE) {
                    Parameters->NetworkAddress->Domain = NetDomainLocal;

                } else {
                    RtlZeroMemory(&RemoteAddressLocal, sizeof(NETWORK_ADDRESS));
                    RemoteAddressLocal.Domain = NetDomainLocal;
                    Status = MmCopyToUserMode(Parameters->NetworkAddress,
                                              &RemoteAddressLocal,
                                              sizeof(NETWORK_ADDRESS));

                    if (!KSUCCESS(Status)) {
                        goto UnixSocketReceiveDataEnd;
                    }
                }
            }

            //
            // If the packet was completely consumed or this is datagram mode,
            // destroy the packet.
            //

            if ((Packet->Offset >= Packet->Length) ||
                (Socket->Type == NetSocketDatagram)) {

                LIST_REMOVE(&(Packet->ListEntry));
                if (LIST_EMPTY(&(UnixSocket->ReceiveList)) != FALSE) {
                    IoSetIoObjectState(Socket->IoState, POLL_EVENT_IN, FALSE);
                }

                //
                // Release the sender if needed. Release the lock first so that
                // both locks are not held at once, which could cause lock
                // ordering problems.
                //

                KeReleaseQueuedLock(UnixSocket->Lock);
                UnixSocketLockHeld = FALSE;
                KeAcquireQueuedLock(Packet->Sender->Lock);

                ASSERT(Packet->Sender->SendListSize >= Packet->Length);

                IoSetIoObjectState(Packet->Sender->KernelSocket.IoState,
                                   POLL_EVENT_OUT,
                                   TRUE);

                Packet->Sender->SendListSize -= Packet->Length;
                KeReleaseQueuedLock(Packet->Sender->Lock);

                //
                // Destroy the sender's packet.
                //

                IopUnixSocketDestroyPacket(Packet);
            }

            //
            // For datagram or sequenced packet sockets, only return one packet
            // at a time.
            //

            if ((Socket->Type == NetSocketDatagram) ||
                (Socket->Type == NetSocketSequencedPacket)) {

                break;
            }
        }
    }

    Status = STATUS_SUCCESS;

UnixSocketReceiveDataEnd:
    if (UnixSocketLockHeld != FALSE) {
        KeReleaseQueuedLock(UnixSocket->Lock);
    }

    if ((Status == STATUS_END_OF_FILE) && (BytesReceived != 0)) {
        Status = STATUS_SUCCESS;
    }

    Parameters->BytesCompleted = BytesReceived;
    return Status;
}

KSTATUS
IopUnixSocketGetSetSocketInformation (
    PSOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets properties of the given socket.

Arguments:

    Socket - Supplies a pointer to the socket to get or set information for.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    Option - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input constains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the information type is incorrect.

    STATUS_BUFFER_TOO_SMALL if the data buffer is too small to receive the
        requested option.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option is not supported by
        the socket.

--*/

{

    NETWORK_ADDRESS Address;
    SOCKET_BASIC_OPTION BasicOption;
    UINTN CopySize;
    ULONG PassCredentials;
    UINTN RemainingSize;
    UINTN RequiredSize;
    UINTN SendListMax;
    PVOID Source;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;

    UnixSocket = (PUNIX_SOCKET)Socket;

    ASSERT(Socket->Domain == NetDomainLocal);

    Source = NULL;
    RequiredSize = 0;
    Status = STATUS_SUCCESS;
    switch (InformationType) {
    case SocketInformationBasic:
        BasicOption = (SOCKET_BASIC_OPTION)Option;
        switch (BasicOption) {
        case SocketBasicOptionType:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            Source = &(Socket->Type);
            RequiredSize = sizeof(NET_SOCKET_TYPE);
            break;

        case SocketBasicOptionDomain:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            Source = &(Socket->Domain);
            RequiredSize = sizeof(NET_DOMAIN_TYPE);
            break;

        case SocketBasicOptionErrorStatus:

            //
            // Currently there are no errors in a local socket, so wire it up
            // to the local status (success).
            //

            Source = &Status;
            RequiredSize = sizeof(KSTATUS);
            break;

        //
        // Switch to the remote socket and fall through.
        //

        case SocketBasicOptionRemoteAddress:
            UnixSocket = UnixSocket->Remote;

        case SocketBasicOptionLocalAddress:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            if (UnixSocket == NULL) {
                Status = STATUS_NOT_CONNECTED;
                break;
            }

            RtlZeroMemory(&Address, sizeof(NETWORK_ADDRESS));
            Address.Domain = NetDomainLocal;
            KeAcquireQueuedLock(UnixSocket->Lock);
            RequiredSize = sizeof(NETWORK_ADDRESS) + UnixSocket->NameSize;
            if (*DataSize > RequiredSize) {
                *DataSize = RequiredSize;
            }

            RemainingSize = *DataSize;
            CopySize = RemainingSize;
            if (CopySize > sizeof(NETWORK_ADDRESS)) {
                CopySize = sizeof(NETWORK_ADDRESS);
            }

            //
            // The lock must be held while the name is copied, so this cannot
            // be done below.
            //

            RtlCopyMemory(Data, &Address, CopySize);
            RemainingSize -= CopySize;
            if ((RemainingSize != 0) && (UnixSocket->NameSize != 0)) {

                ASSERT(RemainingSize <= UnixSocket->NameSize);

                RtlCopyMemory(Data + CopySize, UnixSocket->Name, RemainingSize);
            }

            KeReleaseQueuedLock(UnixSocket->Lock);
            break;

        case SocketBasicOptionSendBufferSize:
            if ((Set != FALSE) && (*DataSize < sizeof(ULONG))) {
                *DataSize = sizeof(ULONG);
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RequiredSize = sizeof(ULONG);
            if (Set != FALSE) {
                SendListMax = *((PULONG)Data);
                if (SendListMax > SOCKET_OPTION_MAX_ULONG) {
                    SendListMax = SOCKET_OPTION_MAX_ULONG;
                }

                //
                // TODO: Are there limits to Unix socket buffer sizes?
                //

                KeAcquireQueuedLock(UnixSocket->Lock);
                UnixSocket->SendListMax = SendListMax;
                KeReleaseQueuedLock(UnixSocket->Lock);

            } else {
                Source = &SendListMax;
                SendListMax = UnixSocket->SendListMax;
            }

            break;

        case SocketBasicOptionPassCredentials:
            if ((Set != FALSE) && (*DataSize < sizeof(ULONG))) {
                *DataSize = sizeof(ULONG);
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            RequiredSize = sizeof(ULONG);
            KeAcquireQueuedLock(UnixSocket->Lock);
            if (Set != FALSE) {
                UnixSocket->Flags &= ~UNIX_SOCKET_FLAG_SEND_CREDENTIALS;
                if (*((PULONG)Data) != FALSE) {
                    UnixSocket->Flags |= UNIX_SOCKET_FLAG_SEND_CREDENTIALS;
                }

            } else {
                Source = &PassCredentials;
                PassCredentials = FALSE;
                if ((UnixSocket->Flags &
                     UNIX_SOCKET_FLAG_SEND_CREDENTIALS) != 0) {

                    PassCredentials = TRUE;
                }
            }

            KeReleaseQueuedLock(UnixSocket->Lock);
            break;

        case SocketBasicOptionPeerCredentials:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            Source = &(UnixSocket->Remote->Credentials);
            RequiredSize = sizeof(UNIX_SOCKET_CREDENTIALS);
            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    //
    // Complete the common information processing if this call succeeded.
    //

    if (KSUCCESS(Status)) {

        //
        // Truncate all copies for get requests down to the required size and
        // only return the required size on set requests.
        //

        if (*DataSize > RequiredSize) {
            *DataSize = RequiredSize;
        }

        //
        // For get requests, copy the gathered information to the supplied data
        // buffer.
        //

        if (Set == FALSE) {
            if (Source != NULL) {
                RtlCopyMemory(Data, Source, *DataSize);
            }

            //
            // If the copy truncated the data, report that the given buffer was
            // too small. The caller can choose to ignore this if the truncated
            // data is enough.
            //

            if (*DataSize < RequiredSize) {
                *DataSize = RequiredSize;
                Status = STATUS_BUFFER_TOO_SMALL;
            }
        }
    }

    return Status;
}

KSTATUS
IopUnixSocketShutdown (
    PSOCKET Socket,
    ULONG ShutdownType
    )

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    Socket - Supplies a pointer to the socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    Status code.

--*/

{

    PUNIX_SOCKET RemoteToRelease;
    PUNIX_SOCKET RemoteToSignal;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;

    RemoteToRelease = NULL;
    RemoteToSignal = NULL;
    UnixSocket = (PUNIX_SOCKET)Socket;

    ASSERT(Socket->Domain == NetDomainLocal);

    KeAcquireQueuedLock(UnixSocket->Lock);
    if (UnixSocket->State == UnixSocketStateInitialized) {
        UnixSocket->State = UnixSocketStateShutdown;

    } else if ((UnixSocket->State >= UnixSocketStateBound) &&
               (UnixSocket->State < UnixSocketStateShutdown)) {

        if ((ShutdownType & SOCKET_SHUTDOWN_READ) != 0) {

            //
            // A listening socket can only shut down read, so treat it as a
            // full shutdown.
            //

            if (UnixSocket->State == UnixSocketStateListening) {
                UnixSocket->State = UnixSocketStateShutdown;

            //
            // It might already be shut down for writing.
            //

            } else if (UnixSocket->State == UnixSocketStateShutdownWrite) {
                UnixSocket->State = UnixSocketStateShutdown;

            } else if (UnixSocket->State != UnixSocketStateShutdown) {
                UnixSocket->State = UnixSocketStateShutdownRead;
            }

            IopUnixSocketFlushData(UnixSocket);
        }

        if ((ShutdownType & SOCKET_SHUTDOWN_WRITE) != 0) {

            //
            // Listening sockets can still accept new connections while
            // shutdown for write. Everything else closes.
            //

            if (UnixSocket->State == UnixSocketStateShutdownRead) {
                UnixSocket->State = UnixSocketStateShutdown;

            } else if (UnixSocket->State != UnixSocketStateListening) {
                if (UnixSocket->State != UnixSocketStateShutdown) {
                    UnixSocket->State = UnixSocketStateShutdownWrite;
                }
            }

            //
            // If there's a remote connection for a connection-oriented socket,
            // signal it as no more data will be sent.
            //

            if ((Socket->Type == NetSocketStream) ||
                (Socket->Type == NetSocketSequencedPacket)) {

                RemoteToSignal = UnixSocket->Remote;
            }
        }

        //
        // Release the reference on the remote if the socket is completely
        // disconnected.
        //

        if (UnixSocket->State == UnixSocketStateShutdown) {
            RemoteToRelease = UnixSocket->Remote;
            UnixSocket->Remote = NULL;
        }
    }

    KeReleaseQueuedLock(UnixSocket->Lock);

    //
    // Signal the remote outside holding this socket lock to avoid lock
    // ordering problems.
    //

    if (RemoteToSignal != NULL) {
        KeAcquireQueuedLock(RemoteToSignal->Lock);
        IoSetIoObjectState(RemoteToSignal->KernelSocket.IoState,
                           POLL_EVENT_IN | POLL_EVENT_DISCONNECTED,
                           TRUE);

        KeReleaseQueuedLock(RemoteToSignal->Lock);
    }

    if (RemoteToRelease != NULL) {
        IoSocketReleaseReference(&(RemoteToRelease->KernelSocket));
    }

    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
IopUnixSocketClose (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine closes down a local socket.

Arguments:

    Socket - Supplies a pointer to the socket.

Return Value:

    Status code.

--*/

{

    ULONG Backlog;
    PUNIX_SOCKET Connection;
    LIST_ENTRY LocalList;
    ULONG ShutdownFlags;
    KSTATUS Status;
    PUNIX_SOCKET UnixSocket;

    UnixSocket = (PUNIX_SOCKET)Socket;

    ASSERT(Socket->Domain == NetDomainLocal);

    //
    // Shut the socket down.
    //

    ShutdownFlags = SOCKET_SHUTDOWN_READ | SOCKET_SHUTDOWN_WRITE;
    Status = IopUnixSocketShutdown(Socket, ShutdownFlags);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    KeAcquireQueuedLock(UnixSocket->Lock);
    if (UnixSocket->State == UnixSocketStateClosed) {
        KeReleaseQueuedLock(UnixSocket->Lock);
        return STATUS_SUCCESS;
    }

    //
    // Move the incoming connections over to a list to be destroyed when the
    // lock is released and the socket is closed.
    //

    if (UnixSocket->CurrentBacklog != 0) {
        MOVE_LIST(&(UnixSocket->ConnectionListEntry), &LocalList);
        Backlog = UnixSocket->CurrentBacklog;
        UnixSocket->CurrentBacklog = 0;

    } else {
        INITIALIZE_LIST_HEAD(&LocalList);
        Backlog = 0;
    }

    //
    // Release the reference on the path entry.
    //

    if (UnixSocket->PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&(UnixSocket->PathPoint));
        UnixSocket->PathPoint.PathEntry = NULL;
    }

    ASSERT(UnixSocket->State == UnixSocketStateShutdown);

    UnixSocket->State = UnixSocketStateClosed;
    KeReleaseQueuedLock(UnixSocket->Lock);

    //
    // Shut down any incoming connections.
    //

    while (Backlog != 0) {

        ASSERT(!LIST_EMPTY(&LocalList));

        Connection = LIST_VALUE(LocalList.Next,
                                UNIX_SOCKET,
                                ConnectionListEntry);

        LIST_REMOVE(&(Connection->ConnectionListEntry));
        Connection->ConnectionListEntry.Next = NULL;
        Backlog -= 1;
        Status = IopUnixSocketShutdown(&(Connection->KernelSocket),
                                       ShutdownFlags);

        ASSERT(KSUCCESS(Status));
    }

    ASSERT(LIST_EMPTY(&LocalList));

    IoSocketReleaseReference(&(UnixSocket->KernelSocket));
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopUnixSocketEnsureConnected (
    PUNIX_SOCKET Socket,
    BOOL Write
    )

/*++

Routine Description:

    This routine ensures a socket is connected and okay to send data. This
    routine assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket to check.

    Write - Supplies a boolean indicating if the caller is going to do a write
        operation on the socket (TRUE) or a read operation (FALSE).

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = STATUS_SUCCESS;
    if (Write != FALSE) {
        if (Socket->State == UnixSocketStateShutdownRead) {
            return STATUS_SUCCESS;

        } else if ((Socket->State == UnixSocketStateShutdownWrite) ||
                   (Socket->State == UnixSocketStateShutdown)) {

            return STATUS_BROKEN_PIPE;
        }

    } else {
        if (Socket->State == UnixSocketStateShutdownWrite) {
            return STATUS_SUCCESS;

        } else if ((Socket->State == UnixSocketStateShutdownRead) ||
                   (Socket->State == UnixSocketStateShutdown)) {

            return STATUS_END_OF_FILE;
        }
    }

    if (Socket->State != UnixSocketStateConnected) {
        if (Socket->KernelSocket.Type == NetSocketDatagram) {
            if ((Socket->State != UnixSocketStateInitialized) &&
                (Socket->State != UnixSocketStateBound)) {

                Status = STATUS_BROKEN_PIPE;
            }

        } else {
            Status = STATUS_NOT_CONNECTED;
        }
    }

    return Status;
}

VOID
IopUnixSocketFlushData (
    PUNIX_SOCKET Socket
    )

/*++

Routine Description:

    This routine flushes all incoming data on the given socket. This routine
    assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket to flush.

Return Value:

    Status code.

--*/

{

    PUNIX_SOCKET_PACKET Packet;

    while (LIST_EMPTY(&(Socket->ReceiveList)) == FALSE) {
        Packet = LIST_VALUE(Socket->ReceiveList.Next,
                            UNIX_SOCKET_PACKET,
                            ListEntry);

        LIST_REMOVE(&(Packet->ListEntry));
        KeAcquireQueuedLock(Packet->Sender->Lock);

        ASSERT(Packet->Sender->SendListSize >= Packet->Length);

        IoSetIoObjectState(Packet->Sender->KernelSocket.IoState,
                           POLL_EVENT_OUT,
                           TRUE);

        Packet->Sender->SendListSize -= Packet->Length;
        KeReleaseQueuedLock(Packet->Sender->Lock);
        IopUnixSocketDestroyPacket(Packet);
    }

    return;
}

KSTATUS
IopUnixSocketCreatePacket (
    PUNIX_SOCKET Sender,
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN DataSize,
    PUNIX_SOCKET_PACKET *NewPacket
    )

/*++

Routine Description:

    This routine creates a socket packet structure, and takes a reference on
    the sender on success.

Arguments:

    Sender - Supplies a pointer to the socket sending the data.

    IoBuffer - Supplies a pointer to the I/O buffer to base the data on. A
        copy of this data will be made.

    Offset - Supplies the offset from the start of the I/O buffer to copy from.

    DataSize - Supplies the number of bytes to send.

    NewPacket - Supplies a pointer where a pointer to a newly allocated packet
        will be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PUNIX_SOCKET_PACKET Packet;
    KSTATUS Status;

    AllocationSize = sizeof(UNIX_SOCKET_PACKET) + DataSize;
    Packet = MmAllocatePagedPool(AllocationSize, UNIX_SOCKET_ALLOCATION_TAG);
    if (Packet == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Packet->Sender = Sender;
    Packet->Data = (PVOID)(Packet + 1);
    Packet->Length = DataSize;
    Packet->Offset = 0;
    Packet->Credentials.ProcessId = -1;
    Packet->Credentials.UserId = -1;
    Packet->Credentials.GroupId = -1;
    Packet->Handles = NULL;
    Packet->HandleCount = 0;
    if (DataSize != 0) {
        Status = MmCopyIoBufferData(IoBuffer,
                                    Packet->Data,
                                    Offset,
                                    DataSize,
                                    FALSE);

        if (!KSUCCESS(Status)) {
            MmFreePagedPool(Packet);
            return Status;
        }
    }

    IoSocketAddReference(&(Sender->KernelSocket));
    *NewPacket = Packet;
    return STATUS_SUCCESS;;
}

VOID
IopUnixSocketDestroyPacket (
    PUNIX_SOCKET_PACKET Packet
    )

/*++

Routine Description:

    This routine destroys a socket packet structure.

Arguments:

    Packet - Supplies a pointer to the packet to destroy.

Return Value:

    None.

--*/

{

    UINTN Index;
    PIO_HANDLE *IoHandleArray;

    //
    // Release any handles and free the array if present.
    //

    IoHandleArray = Packet->Handles;
    if (IoHandleArray != NULL) {
        for (Index = 0; Index < Packet->HandleCount; Index += 1) {
            IoIoHandleReleaseReference(IoHandleArray[Index]);
        }

        MmFreePagedPool(IoHandleArray);
    }

    IoSocketReleaseReference(&(Packet->Sender->KernelSocket));
    MmFreePagedPool(Packet);
    return;
}

KSTATUS
IopUnixSocketSendControlData (
    BOOL FromKernelMode,
    PUNIX_SOCKET Sender,
    PUNIX_SOCKET_PACKET Packet,
    PVOID ControlData,
    UINTN ControlDataSize
    )

/*++

Routine Description:

    This routine parses and attaches ancillary data to a packet.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the send is originating
        from user mode (FALSE) or kernel mode (TRUE). If user mode, then the
        control data pointer is expected to be a user mode pointer.

    Sender - Supplies a pointer to the socket doing the sending.

    Packet - Supplies a pointer to the packet that will be sent.

    ControlData - Supplies a pointer to the control data.

    ControlDataSize - Supplies the size of the control data.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PSOCKET_CONTROL_MESSAGE Control;
    PVOID ControlDataCopy;
    PUNIX_SOCKET_CREDENTIALS Credentials;
    PHANDLE DescriptorArray;
    UINTN DescriptorCount;
    UINTN DescriptorIndex;
    PIO_HANDLE *IoHandleArray;
    UINTN IoHandleCount;
    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;
    UINTN TotalDescriptorCount;

    ControlDataCopy = NULL;
    IoHandleArray = NULL;
    IoHandleCount = 0;

    ASSERT(FromKernelMode == FALSE);

    if (ControlDataSize > UNIX_SOCKET_MAX_CONTROL_DATA) {
        Status = STATUS_INVALID_PARAMETER;
        goto UnixSocketSendControlDataEnd;
    }

    Process = PsGetCurrentProcess();

    //
    // Make a copy of the control data to avoid a million tiny accesses to user
    // mode memory.
    //

    ControlDataCopy = MmAllocatePagedPool(ControlDataSize,
                                          UNIX_SOCKET_ALLOCATION_TAG);

    if (ControlDataCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto UnixSocketSendControlDataEnd;
    }

    Status = MmCopyFromUserMode(ControlDataCopy, ControlData, ControlDataSize);
    if (!KSUCCESS(Status)) {
        goto UnixSocketSendControlDataEnd;
    }

    //
    // Loop once to count the number of file descriptors.
    //

    TotalDescriptorCount = 0;
    Control = SOCKET_CONTROL_FIRST(ControlDataCopy, ControlDataSize);
    while (Control != NULL) {
        if (Control->Protocol != SOCKET_LEVEL_SOCKET) {
            Status = STATUS_NOT_SUPPORTED;
            goto UnixSocketSendControlDataEnd;
        }

        if (Control->Type == SOCKET_CONTROL_RIGHTS) {

            //
            // TODO: HANDLE won't work in 64-bit, as PVOID will go to 8
            // bytes but the C library int will stay at 4. Create a new
            // DESCRIPTOR type that is always 32-bits, and use that as the Ob
            // handle table type.
            //

            ASSERT(sizeof(HANDLE) == sizeof(int));

            DescriptorCount = (Control->Length - SOCKET_CONTROL_LENGTH(0)) /
                              sizeof(HANDLE);

            TotalDescriptorCount += DescriptorCount;

        } else if (Control->Type != SOCKET_CONTROL_CREDENTIALS) {
            Status = STATUS_NOT_SUPPORTED;
            goto UnixSocketSendControlDataEnd;
        }

        SOCKET_CONTROL_NEXT(ControlDataCopy, ControlDataSize, Control);
    }

    //
    // Allocate the handle array if needed.
    //

    if (TotalDescriptorCount > 0) {
        if (TotalDescriptorCount > UNIX_SOCKET_MAX_CONTROL_HANDLES) {
            Status = STATUS_INVALID_PARAMETER;
            goto UnixSocketSendControlDataEnd;
        }

        AllocationSize = sizeof(PIO_HANDLE) * TotalDescriptorCount;
        IoHandleArray = MmAllocatePagedPool(AllocationSize,
                                          UNIX_SOCKET_ALLOCATION_TAG);

        if (IoHandleArray == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto UnixSocketSendControlDataEnd;
        }

        RtlZeroMemory(IoHandleArray, AllocationSize);
    }

    //
    // Go through the control messages again and populate the data.
    //

    Control = SOCKET_CONTROL_FIRST(ControlDataCopy, ControlDataSize);
    while (Control != NULL) {

        ASSERT(Control->Protocol == SOCKET_LEVEL_SOCKET);

        //
        // For passing descriptors, loop through all descriptors getting their
        // associated I/O handles. The routine that gets the handles adds a
        // reference on them, which is left there as the I/O handle sits in the
        // array.
        //

        if (Control->Type == SOCKET_CONTROL_RIGHTS) {
            DescriptorArray = SOCKET_CONTROL_DATA(Control);

            ASSERT(sizeof(HANDLE) == sizeof(int));

            DescriptorCount = (Control->Length - SOCKET_CONTROL_LENGTH(0)) /
                              sizeof(HANDLE);

            for (DescriptorIndex = 0;
                 DescriptorIndex < DescriptorCount;
                 DescriptorIndex += 1) {

                IoHandleArray[IoHandleCount] = ObGetHandleValue(
                                              Process->HandleTable,
                                              DescriptorArray[DescriptorIndex],
                                              NULL);

                if (IoHandleArray[IoHandleCount] == NULL) {
                    Status = STATUS_INVALID_HANDLE;
                    goto UnixSocketSendControlDataEnd;
                }

                IoHandleCount += 1;
            }

        } else {

            ASSERT(Control->Type == SOCKET_CONTROL_CREDENTIALS);

            Credentials = SOCKET_CONTROL_DATA(Control);
            if ((Control->Length - SOCKET_CONTROL_LENGTH(0)) <
                sizeof(UNIX_SOCKET_CREDENTIALS)) {

                Status = STATUS_DATA_LENGTH_MISMATCH;
                goto UnixSocketSendControlDataEnd;
            }

            Thread = KeGetCurrentThread();

            //
            // Validate the credentials, unless the user has the proper
            // permissions to send forgeries.
            //

            Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
            if ((!KSUCCESS(Status)) &&
                (Credentials->ProcessId != Process->Identifiers.ProcessId)) {

                goto UnixSocketSendControlDataEnd;
            }

            Status = PsCheckPermission(PERMISSION_SET_USER_ID);
            if ((!KSUCCESS(Status)) &&
                (Credentials->UserId != Thread->Identity.RealUserId) &&
                (Credentials->UserId != Thread->Identity.EffectiveUserId) &&
                (Credentials->UserId != Thread->Identity.SavedUserId)) {

                goto UnixSocketSendControlDataEnd;
            }

            Status = PsCheckPermission(PERMISSION_SET_GROUP_ID);
            if ((!KSUCCESS(Status)) &&
                (Credentials->GroupId != Thread->Identity.RealGroupId) &&
                (Credentials->GroupId != Thread->Identity.EffectiveGroupId) &&
                (Credentials->GroupId != Thread->Identity.SavedGroupId)) {

                goto UnixSocketSendControlDataEnd;
            }

            //
            // The sent credentials passed, add them in the packet.
            //

            Packet->Credentials.ProcessId = Credentials->ProcessId;
            Packet->Credentials.UserId = Credentials->UserId;
            Packet->Credentials.GroupId = Credentials->GroupId;
        }

        SOCKET_CONTROL_NEXT(ControlDataCopy, ControlDataSize, Control);
    }

    ASSERT(IoHandleCount == TotalDescriptorCount);

    Packet->Handles = IoHandleArray;
    Packet->HandleCount = IoHandleCount;
    IoHandleArray = NULL;
    Status = STATUS_SUCCESS;

UnixSocketSendControlDataEnd:
    if (ControlDataCopy != NULL) {
        MmFreePagedPool(ControlDataCopy);
    }

    if (IoHandleArray != NULL) {
        for (DescriptorIndex = 0;
             DescriptorIndex < IoHandleCount;
             DescriptorIndex += 1) {

            IoIoHandleReleaseReference(IoHandleArray[DescriptorIndex]);
        }

        MmFreePagedPool(IoHandleArray);
    }

    return Status;
}

KSTATUS
IopUnixSocketReceiveControlData (
    BOOL FromKernelMode,
    PUNIX_SOCKET Socket,
    PUNIX_SOCKET_PACKET Packet,
    PSOCKET_IO_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine receives ancillary data.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the receive is
        originating from user mode (FALSE) or kernel mode (TRUE). If user mode,
        then the control data pointer is expected to be a user mode pointer.

    Socket - Supplies a pointer to the receiving socket.

    Packet - Supplies a pointer to the packet that is being received.

    Parameters - Supplies a pointer to the I/O parameters.

Return Value:

    Status code. Truncation or not receiving the ancillary data is not a
    failure.

--*/

{

    PSOCKET_CONTROL_MESSAGE Control;
    PVOID ControlData;
    UINTN ControlSize;
    PHANDLE DescriptorArray;
    UINTN Index;
    PKPROCESS Process;
    KSTATUS Status;

    //
    // Compute the size of the control data needed.
    //

    ControlSize = 0;
    if ((Packet->Credentials.ProcessId != -1) ||
        (Packet->Credentials.UserId != -1) ||
        (Packet->Credentials.GroupId != -1)) {

        ControlSize += SOCKET_CONTROL_SPACE(sizeof(UNIX_SOCKET_CREDENTIALS));
    }

    if (Packet->HandleCount != 0) {

        ASSERT(sizeof(HANDLE) == sizeof(int));

        ControlSize += SOCKET_CONTROL_SPACE(
                                         sizeof(HANDLE) * Packet->HandleCount);
    }

    if (ControlSize == 0) {
        Parameters->ControlDataSize = 0;
        return STATUS_SUCCESS;
    }

    //
    // There is ancillary data to be received. If the caller didn't provide
    // a buffer, then do nothing. Perhaps this is a stream socket and they're
    // only partially receiving the buffer, in which case they get another shot
    // when they receive the rest.
    //

    if ((Parameters->ControlData == NULL) ||
        (Parameters->ControlDataSize < ControlSize)) {

        Parameters->SocketIoFlags |= SOCKET_IO_CONTROL_TRUNCATED;
        return STATUS_SUCCESS;
    }

    //
    // Construct the complete control buffer in kernel mode.
    //

    ControlData = MmAllocatePagedPool(ControlSize, UNIX_SOCKET_ALLOCATION_TAG);
    if (ControlData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto UnixSocketReceiveControlDataEnd;
    }

    //
    // Zero the buffer to avoid leaking uninitialized kernel pool to user mode.
    //

    RtlZeroMemory(ControlData, ControlSize);
    Control = SOCKET_CONTROL_FIRST(ControlData, ControlSize);
    if (((Socket->Flags & UNIX_SOCKET_FLAG_SEND_CREDENTIALS) != 0) &&
        ((Packet->Credentials.ProcessId != -1) ||
         (Packet->Credentials.UserId != -1) ||
         (Packet->Credentials.GroupId != -1))) {

        Control->Length = SOCKET_CONTROL_LENGTH(
                                              sizeof(UNIX_SOCKET_CREDENTIALS));

        Control->Protocol = SOCKET_LEVEL_SOCKET;
        Control->Type = SOCKET_CONTROL_CREDENTIALS;
        RtlCopyMemory(SOCKET_CONTROL_DATA(Control),
                      &(Packet->Credentials),
                      sizeof(UNIX_SOCKET_CREDENTIALS));

        Packet->Credentials.ProcessId = -1;
        Packet->Credentials.UserId = -1;
        Packet->Credentials.GroupId = -1;
        SOCKET_CONTROL_NEXT(ControlData, ControlSize, Control);
    }

    if (Packet->HandleCount != 0) {

        //
        // The kernel process doesn't have a handle table, so this would get
        // weird.
        //

        Process = PsGetCurrentProcess();

        ASSERT(FromKernelMode == FALSE);
        ASSERT(Process != PsGetKernelProcess());
        ASSERT(sizeof(HANDLE) == sizeof(int));

        Control->Length = SOCKET_CONTROL_LENGTH(
                                    sizeof(HANDLE) * Packet->HandleCount);

        Control->Protocol = SOCKET_LEVEL_SOCKET;
        Control->Type = SOCKET_CONTROL_RIGHTS;
        DescriptorArray = SOCKET_CONTROL_DATA(Control);

        //
        // Create the handles in the receiving process. If failure occurs,
        // the process may end up with handles it doesn't know about. These
        // handles will get cleaned up when the process exits or does something
        // drastic like closes everything.
        //

        Status = STATUS_SUCCESS;
        for (Index = 0; Index < Packet->HandleCount; Index += 1) {
            Status = ObCreateHandle(Process->HandleTable,
                                    Packet->Handles[Index],
                                    0,
                                    &(DescriptorArray[Index]));

            if (!KSUCCESS(Status)) {

                //
                // If creating a handle failed, release the reference on the
                // rest of the handles, and stop.
                //

                while (Index < Packet->HandleCount) {
                    IoIoHandleReleaseReference(Packet->Handles[Index]);
                    Index += 1;
                }

                break;
            }
        }

        //
        // Destroy the handle array, as all the references on the handles were
        // either transferred to the handle table or explicitly released in
        // the failure case.
        //

        MmFreePagedPool(Packet->Handles);
        Packet->Handles = NULL;
        Packet->HandleCount = 0;
        if (!KSUCCESS(Status)) {
            goto UnixSocketReceiveControlDataEnd;
        }
    }

    //
    // Try to copy this ancillary data buffer to user mode.
    //

    Status = MmCopyToUserMode(Parameters->ControlData,
                              ControlData,
                              ControlSize);

    Parameters->ControlDataSize = ControlSize;
    if (!KSUCCESS(Status)) {
        goto UnixSocketReceiveControlDataEnd;
    }

    Status = STATUS_SUCCESS;

UnixSocketReceiveControlDataEnd:
    if (ControlData != NULL) {
        MmFreePagedPool(ControlData);
    }

    return Status;
}

VOID
IopUnixSocketInitializeCredentials (
    PUNIX_SOCKET Socket
    )

/*++

Routine Description:

    This routine initializes the credentials in the given socket.

Arguments:

    Socket - Supplies a pointer to the socket to initialize.

Return Value:

    None.

--*/

{

    PKPROCESS Process;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Process = PsGetCurrentProcess();
    Socket->Credentials.ProcessId = Process->Identifiers.ProcessId;
    Socket->Credentials.UserId = Thread->Identity.RealUserId;
    Socket->Credentials.GroupId = Thread->Identity.RealGroupId;
    return;
}

