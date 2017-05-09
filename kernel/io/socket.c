/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    socket.c

Abstract:

    This module implements kernel support for sockets.

Author:

    Evan Green 4-Apr-2013

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
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDestroySocket (
    PSOCKET Socket
    );

KSTATUS
IopConvertInterruptedSocketStatus (
    PIO_HANDLE Handle,
    UINTN BytesComplete,
    BOOL Output
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the core networking interface.
//

NET_INTERFACE IoNetInterface;
BOOL IoNetInterfaceInitialized = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
IoInitializeCoreNetworking (
    PNET_INTERFACE Interface
    )

/*++

Routine Description:

    This routine initializes the interface between the kernel and the core
    networking library. This routine should not be called by random drivers.

Arguments:

    Interface - Supplies a pointer to the core networking library interface.

Return Value:

    None.

--*/

{

    ASSERT((Interface->CreateSocket != NULL) &&
           (Interface->DestroySocket != NULL) &&
           (Interface->BindToAddress != NULL) &&
           (Interface->Listen != NULL) &&
           (Interface->Accept != NULL) &&
           (Interface->Connect != NULL) &&
           (Interface->CloseSocket != NULL) &&
           (Interface->Send != NULL) &&
           (Interface->Receive != NULL) &&
           (Interface->GetSetSocketInformation != NULL) &&
           (Interface->Shutdown != NULL) &&
           (Interface->UserControl != NULL));

    if (IoNetInterfaceInitialized != FALSE) {

        ASSERT(FALSE);

        return;
    }

    RtlCopyMemory(&IoNetInterface, Interface, sizeof(NET_INTERFACE));
    IoNetInterfaceInitialized = TRUE;
    return;
}

KERNEL_API
ULONG
IoSocketAddReference (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine increases the reference count on a socket.

Arguments:

    Socket - Supplies a pointer to the socket whose reference count should be
        incremented.

Return Value:

    Returns the old reference count.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Socket->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    return OldReferenceCount;
}

KERNEL_API
ULONG
IoSocketReleaseReference (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine decreases the reference count of a socket, and destroys the
    socket if in this call the reference count drops to zero.

Arguments:

    Socket - Supplies a pointer to the socket whose reference count should be
        decremented.

Return Value:

    Returns the old reference count.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Socket->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    if (OldReferenceCount == 1) {
        IopDestroySocket(Socket);
    }

    return OldReferenceCount;
}

KERNEL_API
KSTATUS
IoSocketCreatePair (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE IoHandles[2]
    )

/*++

Routine Description:

    This routine creates a pair of sockets that are connected to each other.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies a bitfield of open flags governing the new handles.
        See OPEN_FLAG_* definitions.

    IoHandles - Supplies an array where the two I/O handles to the connected
        sockets will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (Domain == NetDomainLocal) {
        Status = IopCreateUnixSocketPair(Type, Protocol, OpenFlags, IoHandles);

    } else {
        Status = STATUS_DOMAIN_NOT_SUPPORTED;
    }

    return Status;
}

KERNEL_API
KSTATUS
IoSocketCreate (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE *IoHandle
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies the open flags for the socket. See OPEN_FLAG_*
        definitions.

    IoHandle - Supplies a pointer where a pointer to the new socket's I/O
        handle will be returned.

Return Value:

    Status code.

--*/

{

    CREATE_PARAMETERS Create;
    SOCKET_CREATION_PARAMETERS Parameters;
    PSOCKET Socket;
    KSTATUS Status;

    Parameters.Domain = Domain;
    Parameters.Type = Type;
    Parameters.Protocol = Protocol;
    Parameters.ExistingSocket = NULL;
    Create.Type = IoObjectSocket;
    Create.Context = &Parameters;
    Create.Permissions = FILE_PERMISSION_ALL;
    Create.Created = FALSE;
    Status = IopOpen(FALSE,
                     NULL,
                     NULL,
                     0,
                     IO_ACCESS_READ | IO_ACCESS_WRITE,
                     OpenFlags | OPEN_FLAG_CREATE,
                     &Create,
                     IoHandle);

    if (KSUCCESS(Status)) {
        Socket = NULL;
        Status = IoGetSocketFromHandle(*IoHandle, &Socket);

        ASSERT(KSUCCESS(Status));
        ASSERT(Socket != NULL);

        Socket->IoHandle = *IoHandle;
    }

    return Status;
}

KERNEL_API
KSTATUS
IoSocketBindToAddress (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PVOID Link,
    PNETWORK_ADDRESS Address,
    PCSTR Path,
    UINTN PathSize
    )

/*++

Routine Description:

    This routine binds the socket to the given address and starts listening for
    client requests.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the socket handle to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

    Path - Supplies an optional pointer to a path, required if the network
        address is a local socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketBindToAddressEnd;
    }

    //
    // If it's a local domain socket and there's an address, bind it into the
    // file system.
    //

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketBindToAddress(FromKernelMode,
                                            Handle,
                                            Address,
                                            Path,
                                            PathSize);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.BindToAddress(Socket, Link, Address);
        }
    }

    if (!KSUCCESS(Status)) {
        goto SocketBindToAddressEnd;
    }

SocketBindToAddressEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketListen (
    PIO_HANDLE Handle,
    ULONG BacklogCount
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing sockets to attempt to connect to it.

Arguments:

    Handle - Supplies a pointer to the socket to mark as listening.

    BacklogCount - Supplies the number of attempted connections that can be
        queued before additional connections are refused.

Return Value:

    Status code.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketListenEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketListen(Socket, BacklogCount);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Listen(Socket, BacklogCount);
        }
    }

SocketListenEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketAccept (
    PIO_HANDLE Handle,
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

    Handle - Supplies a pointer to the socket to accept a connection from.

    NewConnectionSocket - Supplies a pointer where a new socket will be
        returned that represents the accepted connection with the remote
        host.

    RemoteAddress - Supplies a pointer where the address of the connected
        remote host will be returned.

    RemotePath - Supplies a pointer where a string containing the remote path
        will be returned on success. The caller does not own this string, it is
        connected with the new socket coming out. This only applies to local
        sockets.

    RemotePathSize - Supplies a pointer where the size of the remote path in
        bytes will be returned on success.

Return Value:

    Status code.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    *RemotePath = NULL;
    *RemotePathSize = 0;
    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketAcceptEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketAccept(Socket,
                                     NewConnectionSocket,
                                     RemoteAddress,
                                     RemotePath,
                                     RemotePathSize);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Accept(Socket,
                                           NewConnectionSocket,
                                           RemoteAddress);
        }
    }

SocketAcceptEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketConnect (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PNETWORK_ADDRESS Address,
    PCSTR RemotePath,
    UINTN RemotePathSize
    )

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode.

    Handle - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

    RemotePath - Supplies a pointer to the path to connect to, if this is a
        local socket.

    RemotePathSize - Supplies the size of the remote path buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketConnectEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketConnect(FromKernelMode,
                                      Socket,
                                      Address,
                                      RemotePath,
                                      RemotePathSize);

    } else {
        if (RemotePathSize != 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto SocketConnectEnd;
        }

        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Connect(Socket, Address);
        }
    }

SocketConnectEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketSendData (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Socket = NULL;
    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketSendData;
    }

    if ((Parameters->SocketIoFlags & SOCKET_IO_NON_BLOCKING) != 0) {
        Parameters->TimeoutInMilliseconds = 0;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketSendData(FromKernelMode,
                                       Socket,
                                       Parameters,
                                       IoBuffer);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Send(FromKernelMode,
                                         Socket,
                                         Parameters,
                                         IoBuffer);
        }
    }

    if (((Parameters->SocketIoFlags & SOCKET_IO_NON_BLOCKING) != 0) &&
        (Status == STATUS_TIMEOUT)) {

        Status = STATUS_OPERATION_WOULD_BLOCK;
    }

SocketSendData:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketReceiveData (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
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

    Handle - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    STATUS_SUCCESS if any bytes were read.

    STATUS_TIMEOUT if the request timed out.

    STATUS_BUFFER_TOO_SMALL if the incoming datagram was too large for the
        buffer. The remainder of the datagram is discarded in this case.

    Other error codes on other failures.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Socket = NULL;
    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketReceiveDataEnd;
    }

    if ((Parameters->SocketIoFlags & SOCKET_IO_NON_BLOCKING) != 0) {
        Parameters->TimeoutInMilliseconds = 0;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketReceiveData(FromKernelMode,
                                          Socket,
                                          Parameters,
                                          IoBuffer);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Receive(FromKernelMode,
                                            Socket,
                                            Parameters,
                                            IoBuffer);
        }
    }

    if (((Parameters->SocketIoFlags & SOCKET_IO_NON_BLOCKING) != 0) &&
        (Status == STATUS_TIMEOUT)) {

        Status = STATUS_OPERATION_WOULD_BLOCK;
    }

SocketReceiveDataEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketGetSetInformation (
    PIO_HANDLE IoHandle,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets information about the given socket.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle of the socket.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    SocketOption - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation. If the
        data buffer is too small on a get operation, the truncated information
        will be copied to the data buffer and the routine will return
        STATUS_BUFFER_TOO_SMALL.

    DataSize - Supplies a pointer that on input constains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the data is not appropriate for the socket
        option.

    STATUS_BUFFER_TOO_SMALL if the socket option information does not fit in
        the supplied buffer. On a get request, the data buffer will be filled
        with the truncated socket information.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option or information type
        is not supported by the socket.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    ASSERT((Data == NULL) || (Data >= KERNEL_VA_START));

    Status = IoGetSocketFromHandle(IoHandle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketGetSetInformationEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketGetSetSocketInformation(Socket,
                                                      InformationType,
                                                      SocketOption,
                                                      Data,
                                                      DataSize,
                                                      Set);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.GetSetSocketInformation(Socket,
                                                            InformationType,
                                                            SocketOption,
                                                            Data,
                                                            DataSize,
                                                            Set);
        }
    }

    if (KSUCCESS(Status) && (InformationType == SocketInformationBasic)) {
        if (SocketOption == SocketBasicOptionSendTimeout) {
            RtlAtomicOr32(&(Socket->Flags), SOCKET_FLAG_SEND_TIMEOUT_SET);

        } else if (SocketOption == SocketBasicOptionReceiveTimeout) {
            RtlAtomicOr32(&(Socket->Flags), SOCKET_FLAG_RECEIVE_TIMEOUT_SET);
        }
    }

SocketGetSetInformationEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketShutdown (
    PIO_HANDLE IoHandle,
    ULONG ShutdownType
    )

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle of the socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

    Other error codes on failure.

--*/

{

    PSOCKET Socket;
    KSTATUS Status;

    Status = IoGetSocketFromHandle(IoHandle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketShutdownEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketShutdown(Socket, ShutdownType);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.Shutdown(Socket, ShutdownType);
        }
    }

SocketShutdownEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoSocketUserControl (
    PIO_HANDLE Handle,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests destined for a socket.

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

    PSOCKET Socket;
    KSTATUS Status;

    Status = IoGetSocketFromHandle(Handle, &Socket);
    if (!KSUCCESS(Status)) {
        goto SocketUserControlEnd;
    }

    if (Socket->Domain == NetDomainLocal) {
        Status = STATUS_NOT_SUPPORTED;

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.UserControl(Socket,
                                                CodeNumber,
                                                FromKernelMode,
                                                ContextBuffer,
                                                ContextBufferSize);
        }
    }

SocketUserControlEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoGetSocketFromHandle (
    PIO_HANDLE IoHandle,
    PSOCKET *Socket
    )

/*++

Routine Description:

    This routine returns the socket structure from inside an I/O handle. This
    routine is usually only used by networking protocol to get their own
    structures for the socket they create in the "accept" function.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle whose corresponding socket
        is desired.

    Socket - Supplies a pointer where a pointer to the socket corresponding to
        the given handle will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

--*/

{

    PFILE_OBJECT FileObject;

    FileObject = IoHandle->FileObject;
    if (FileObject->Properties.Type != IoObjectSocket) {
        return STATUS_NOT_A_SOCKET;
    }

    *Socket = FileObject->SpecialIo;
    return STATUS_SUCCESS;
}

INTN
IoSysSocketCreatePair (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that creates a pair of connected
    sockets.

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
    ULONG OpenFlags;
    PSYSTEM_CALL_SOCKET_CREATE_PAIR Parameters;
    PKPROCESS Process;
    PIO_HANDLE Sockets[2];
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_CREATE_PAIR)SystemCallParameter;
    Parameters->Socket1 = INVALID_HANDLE;
    Parameters->Socket2 = INVALID_HANDLE;
    Process = PsGetCurrentProcess();
    Sockets[0] = NULL;
    Sockets[1] = NULL;
    HandleFlags = 0;
    if ((Parameters->OpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    OpenFlags = Parameters->OpenFlags & OPEN_FLAG_NON_BLOCKING;
    Status = IoSocketCreatePair(Parameters->Domain,
                                Parameters->Type,
                                Parameters->Protocol,
                                OpenFlags,
                                Sockets);

    if (!KSUCCESS(Status)) {
        goto CreateSocketPairEnd;
    }

    //
    // Create the handles for the sockets.
    //

    Status = ObCreateHandle(Process->HandleTable,
                            Sockets[0],
                            HandleFlags,
                            &(Parameters->Socket1));

    if (!KSUCCESS(Status)) {
        goto CreateSocketPairEnd;
    }

    Status = ObCreateHandle(Process->HandleTable,
                            Sockets[1],
                            HandleFlags,
                            &(Parameters->Socket2));

    if (!KSUCCESS(Status)) {

        //
        // Destory the first handle manually.
        //

        ObDestroyHandle(Process->HandleTable, Parameters->Socket1);
        Parameters->Socket1 = INVALID_HANDLE;
        goto CreateSocketPairEnd;
    }

    Status = STATUS_SUCCESS;

CreateSocketPairEnd:
    if (!KSUCCESS(Status)) {

        ASSERT((Parameters->Socket1 == INVALID_HANDLE) &&
               (Parameters->Socket2 == INVALID_HANDLE));

        if (Sockets[0] != NULL) {
            IoClose(Sockets[0]);
            Sockets[0] = NULL;
        }

        if (Sockets[1] != NULL) {
            IoClose(Sockets[1]);
            Sockets[1] = NULL;
        }
    }

    return Status;
}

INTN
IoSysSocketCreate (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that creates a new socket.

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
    ULONG OpenFlags;
    PSYSTEM_CALL_SOCKET_CREATE Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_CREATE)SystemCallParameter;
    Parameters->Socket = INVALID_HANDLE;
    Process = PsGetCurrentProcess();

    ASSERT(Process != PsGetKernelProcess());

    HandleFlags = 0;
    if ((Parameters->OpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    OpenFlags = Parameters->OpenFlags & SYS_OPEN_FLAG_NON_BLOCKING;
    IoHandle = NULL;
    Status = IoSocketCreate(Parameters->Domain,
                            Parameters->Type,
                            Parameters->Protocol,
                            OpenFlags,
                            &IoHandle);

    if (!KSUCCESS(Status)) {
        goto CreateSocketEnd;
    }

    //
    // Create a handle table entry for this socket.
    //

    Status = ObCreateHandle(Process->HandleTable,
                            IoHandle,
                            HandleFlags,
                            &(Parameters->Socket));

    if (!KSUCCESS(Status)) {
        goto CreateSocketEnd;
    }

CreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (IoHandle != NULL) {
            IoIoHandleReleaseReference(IoHandle);
        }

        Parameters->Socket = INVALID_HANDLE;
    }

    return Status;
}

INTN
IoSysSocketBind (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine attempts to bind a socket to a local address.

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
    PSYSTEM_CALL_SOCKET_BIND Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_BIND)SystemCallParameter;
    Process = PsGetCurrentProcess();

    //
    // Get the I/O handle.
    //

    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketBindEnd;
    }

    Status = IoSocketBindToAddress(FALSE,
                                   IoHandle,
                                   NULL,
                                   &(Parameters->Address),
                                   Parameters->Path,
                                   Parameters->PathSize);

    if (!KSUCCESS(Status)) {
        goto SysSocketBindEnd;
    }

SysSocketBindEnd:

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysSocketListen (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that makes a socket listen and become
    eligible to accept new incoming connections.

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
    PSYSTEM_CALL_SOCKET_LISTEN Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_LISTEN)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketListenEnd;
    }

    Status = IoSocketListen(IoHandle, Parameters->BacklogCount);
    if (!KSUCCESS(Status)) {
        goto SysSocketListenEnd;
    }

SysSocketListenEnd:

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysSocketAccept (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that accepts a new incoming
    connection on a socket and spins it off into another socket.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    UINTN CopySize;
    ULONG HandleFlags;
    PIO_HANDLE IoHandle;
    PIO_HANDLE NewHandle;
    PSYSTEM_CALL_SOCKET_ACCEPT Parameters;
    PKPROCESS Process;
    PCSTR RemotePath;
    UINTN RemotePathSize;
    KSTATUS Status;

    NewHandle = NULL;
    Parameters = (PSYSTEM_CALL_SOCKET_ACCEPT)SystemCallParameter;
    Parameters->NewSocket = INVALID_HANDLE;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketAcceptEnd;
    }

    //
    // Run the actual accept function, which will pop out a new socket that is
    // unconnected to an I/O handle.
    //

    RemotePath = NULL;
    RemotePathSize = 0;
    Status = IoSocketAccept(IoHandle,
                            &NewHandle,
                            &(Parameters->Address),
                            &RemotePath,
                            &RemotePathSize);

    if (!KSUCCESS(Status)) {
        goto SysSocketAcceptEnd;
    }

    if ((Parameters->OpenFlags & SYS_OPEN_FLAG_NON_BLOCKING) != 0) {
        NewHandle->OpenFlags |= OPEN_FLAG_NON_BLOCKING;
    }

    HandleFlags = 0;
    if ((Parameters->OpenFlags & SYS_OPEN_FLAG_CLOSE_ON_EXECUTE) != 0) {
        HandleFlags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
    }

    //
    // Finally, create a user mode handle for this socket.
    //

    Status = ObCreateHandle(Process->HandleTable,
                            NewHandle,
                            HandleFlags,
                            &(Parameters->NewSocket));

    if (!KSUCCESS(Status)) {
        goto SysSocketAcceptEnd;
    }

    //
    // Copy the remote path over.
    //

    if (RemotePath != NULL) {
        CopySize = RemotePathSize;
        if (CopySize > Parameters->RemotePathSize) {
            CopySize = Parameters->RemotePathSize;
        }

        if (CopySize != 0) {
            Status = MmCopyToUserMode(Parameters->RemotePath,
                                      RemotePath,
                                      CopySize);
        }

        Parameters->RemotePathSize = RemotePathSize;
        if (!KSUCCESS(Status)) {
            goto SysSocketAcceptEnd;
        }
    }

SysSocketAcceptEnd:
    if (!KSUCCESS(Status)) {
        if (NewHandle != NULL) {
            IoIoHandleReleaseReference(NewHandle);
        }
    }

    //
    // An interrupted socket accept cannot be restarted if a receive timeout
    // has been set.
    //

    if (Status == STATUS_INTERRUPTED) {
        Status = IopConvertInterruptedSocketStatus(IoHandle, 0, FALSE);
    }

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysSocketConnect (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that reaches and and attempts to
    connect with another socket.

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
    PSYSTEM_CALL_SOCKET_CONNECT Parameters;
    PSTR PathCopy;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_CONNECT)SystemCallParameter;
    PathCopy = NULL;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketConnectEnd;
    }

    if (Parameters->RemotePathSize != 0) {
        Status = MmCreateCopyOfUserModeString(Parameters->RemotePath,
                                              Parameters->RemotePathSize,
                                              UNIX_SOCKET_ALLOCATION_TAG,
                                              &PathCopy);

        if (!KSUCCESS(Status)) {
            goto SysSocketConnectEnd;
        }
    }

    Status = IoSocketConnect(FALSE,
                             IoHandle,
                             &(Parameters->Address),
                             PathCopy,
                             Parameters->RemotePathSize);

    if (!KSUCCESS(Status)) {
        goto SysSocketConnectEnd;
    }

SysSocketConnectEnd:
    if (PathCopy != NULL) {
        MmFreePagedPool(PathCopy);
    }

    //
    // An interrupted socket connect cannot be restarted if a send timeout
    // has been set.
    //

    if (Status == STATUS_INTERRUPTED) {
        Status = IopConvertInterruptedSocketStatus(IoHandle, 0, TRUE);
    }

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysSocketPerformIo (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that sends a packet to a specific
    destination or receives data from a destination. Sockets may also use the
    generic perform I/O operations if the identity of the remote address is
    either already known or not needed.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    IO_BUFFER IoBuffer;
    PIO_HANDLE IoHandle;
    SOCKET_IO_PARAMETERS IoParameters;
    PSYSTEM_CALL_SOCKET_PERFORM_IO Parameters;
    BOOL ParametersCopied;
    PKPROCESS Process;
    KSTATUS Status;
    BOOL Write;

    Parameters = (PSYSTEM_CALL_SOCKET_PERFORM_IO)SystemCallParameter;
    ParametersCopied = FALSE;
    Process = PsGetCurrentProcess();

    ASSERT(SYS_WAIT_TIME_INDEFINITE == WAIT_TIME_INDEFINITE);

    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketPerformIoEnd;
    }

    Status = MmCopyFromUserMode(&IoParameters,
                                Parameters->Parameters,
                                sizeof(SOCKET_IO_PARAMETERS));

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformIoEnd;
    }

    ParametersCopied = TRUE;
    IoParameters.BytesCompleted = 0;
    IoParameters.IoFlags &= SYS_IO_FLAG_MASK;
    Status = MmInitializeIoBuffer(&IoBuffer,
                                  Parameters->Buffer,
                                  INVALID_PHYSICAL_ADDRESS,
                                  IoParameters.Size,
                                  0);

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformIoEnd;
    }

    //
    // Non-blocking handles always have a timeout of zero.
    //

    if ((IoHandle->OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
        IoParameters.TimeoutInMilliseconds = 0;
    }

    if ((IoParameters.IoFlags & SYS_IO_FLAG_WRITE) != 0) {
        Status = IoSocketSendData(FALSE, IoHandle, &IoParameters, &IoBuffer);

        //
        // Send a pipe signal if the returning status was "broken pipe".
        //

        if (Status == STATUS_BROKEN_PIPE) {

            ASSERT(Process != PsGetKernelProcess());

            PsSignalProcess(Process, SIGNAL_BROKEN_PIPE, NULL);
        }

    } else {
        Status = IoSocketReceiveData(FALSE, IoHandle, &IoParameters, &IoBuffer);
    }

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformIoEnd;
    }

SysSocketPerformIoEnd:

    //
    // An interrupted socket cannot be restarted if a timeout has been set.
    //

    if (Status == STATUS_INTERRUPTED) {
        Write = ((IoParameters.IoFlags & SYS_IO_FLAG_WRITE) != 0);
        Status = IopConvertInterruptedSocketStatus(IoHandle,
                                                   IoParameters.BytesCompleted,
                                                   Write);
    }

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    //
    // Only copy the parameters out if they were copied in.
    //

    if (ParametersCopied != FALSE) {
        MmCopyToUserMode(Parameters->Parameters,
                         &IoParameters,
                         sizeof(SOCKET_IO_PARAMETERS));
    }

    return Status;
}

INTN
IoSysSocketPerformVectoredIo (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine handles the system call that performs socket I/O using I/O
    vectors.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PIO_BUFFER IoBuffer;
    PIO_HANDLE IoHandle;
    SOCKET_IO_PARAMETERS IoParameters;
    PSYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO Parameters;
    BOOL ParametersCopied;
    PKPROCESS Process;
    KSTATUS Status;
    BOOL Write;

    IoBuffer = NULL;
    Parameters = (PSYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO)SystemCallParameter;
    ParametersCopied = FALSE;
    Process = PsGetCurrentProcess();

    ASSERT(SYS_WAIT_TIME_INDEFINITE == WAIT_TIME_INDEFINITE);

    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketPerformVectoredIoEnd;
    }

    Status = MmCopyFromUserMode(&IoParameters,
                                Parameters->Parameters,
                                sizeof(SOCKET_IO_PARAMETERS));

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformVectoredIoEnd;
    }

    ParametersCopied = TRUE;
    IoParameters.BytesCompleted = 0;
    IoParameters.IoFlags &= SYS_IO_FLAG_MASK;
    Status = MmCreateIoBufferFromVector(Parameters->VectorArray,
                                        FALSE,
                                        Parameters->VectorCount,
                                        &IoBuffer);

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformVectoredIoEnd;
    }

    //
    // Non-blocking handles always have a timeout of zero.
    //

    if ((IoHandle->OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
        IoParameters.TimeoutInMilliseconds = 0;
    }

    if ((IoParameters.IoFlags & SYS_IO_FLAG_WRITE) != 0) {
        Status = IoSocketSendData(FALSE, IoHandle, &IoParameters, IoBuffer);

        //
        // Send a pipe signal if the returning status was "broken pipe".
        //

        if (Status == STATUS_BROKEN_PIPE) {

            ASSERT(Process != PsGetKernelProcess());

            PsSignalProcess(Process, SIGNAL_BROKEN_PIPE, NULL);
        }

    } else {
        Status = IoSocketReceiveData(FALSE, IoHandle, &IoParameters, IoBuffer);
    }

    if (!KSUCCESS(Status)) {
        goto SysSocketPerformVectoredIoEnd;
    }

SysSocketPerformVectoredIoEnd:
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    //
    // An interrupted socket cannot be restarted if a timeout has been set.
    //

    if (Status == STATUS_INTERRUPTED) {
        Write = ((IoParameters.IoFlags & SYS_IO_FLAG_WRITE) != 0);
        Status = IopConvertInterruptedSocketStatus(IoHandle,
                                                   IoParameters.BytesCompleted,
                                                   Write);
    }

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    //
    // Only copy the parameters out if they were copied in.
    //

    if (ParametersCopied != FALSE) {
        MmCopyToUserMode(Parameters->Parameters,
                         &IoParameters,
                         sizeof(SOCKET_IO_PARAMETERS));
    }

    return Status;
}

INTN
IoSysSocketGetSetInformation (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for getting or setting socket
    information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Buffer;
    UINTN CopySize;
    KSTATUS CopyStatus;
    PIO_HANDLE IoHandle;
    PSYSTEM_CALL_SOCKET_GET_SET_INFORMATION Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Buffer = NULL;
    Parameters = (PSYSTEM_CALL_SOCKET_GET_SET_INFORMATION)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketGetSetInformationEnd;
    }

    //
    // Create a paged pool buffer to hold the data.
    //

    CopySize = 0;
    if (Parameters->DataSize != 0) {
        Buffer = MmAllocatePagedPool(Parameters->DataSize,
                                     SOCKET_INFORMATION_ALLOCATION_TAG);

        if (Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SysSocketGetSetInformationEnd;
        }

        CopySize = Parameters->DataSize;

        //
        // Copy the data into the kernel mode buffer.
        //

        Status = MmCopyFromUserMode(Buffer,
                                    Parameters->Data,
                                    Parameters->DataSize);

        if (!KSUCCESS(Status)) {
            goto SysSocketGetSetInformationEnd;
        }
    }

    Status = IoSocketGetSetInformation(IoHandle,
                                       Parameters->InformationType,
                                       Parameters->Option,
                                       Buffer,
                                       &(Parameters->DataSize),
                                       Parameters->Set);

    //
    // Copy the data back into user mode, even on set operations.
    //

    if (CopySize > Parameters->DataSize) {
        CopySize = Parameters->DataSize;
    }

    if (CopySize != 0) {
        CopyStatus = MmCopyToUserMode(Parameters->Data, Buffer, CopySize);
        if ((KSUCCESS(Status)) && (!KSUCCESS(CopyStatus))) {
            Status = CopyStatus;
        }
    }

SysSocketGetSetInformationEnd:
    if (Buffer != NULL) {
        MmFreePagedPool(Buffer);
    }

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

INTN
IoSysSocketShutdown (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for shutting down communication to
    a socket.

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
    PSYSTEM_CALL_SOCKET_SHUTDOWN Parameters;
    PKPROCESS Process;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SOCKET_SHUTDOWN)SystemCallParameter;
    Process = PsGetCurrentProcess();
    IoHandle = ObGetHandleValue(Process->HandleTable, Parameters->Socket, NULL);
    if (IoHandle == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto SysSocketShutdownEnd;
    }

    Status = IoSocketShutdown(IoHandle, Parameters->ShutdownType);

SysSocketShutdownEnd:

    //
    // Release the reference that was added when the handle was looked up.
    //

    if (IoHandle != NULL) {
        IoIoHandleReleaseReference(IoHandle);
    }

    return Status;
}

KSTATUS
IopCreateSocket (
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket.

Arguments:

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where the new file object representing the
        socket will be returned on success.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT NewFileObject;
    PSOCKET_CREATION_PARAMETERS Parameters;
    FILE_PROPERTIES Properties;
    PVOID RootObject;
    PSOCKET Socket;
    KSTATUS Status;
    PKTHREAD Thread;

    Create->Created = FALSE;
    NewFileObject = NULL;
    Parameters = (PSOCKET_CREATION_PARAMETERS)(Create->Context);

    //
    // If there are no parameters, then this file object is being created from
    // a leftover file system entry. Just succeed, but it will never be able to
    // be opened. The common case for this is a request for the file properties.
    //

    if (Parameters == NULL) {

        ASSERT(*FileObject != NULL);

        Status = STATUS_SUCCESS;
        goto CreateSocketEnd;
    }

    Socket = NULL;

    //
    // In cases where a Unix socket is trying to bind to a new path entry,
    // there's already a socket that's been created. Use that one.
    //

    if (Parameters->ExistingSocket != NULL) {
        Socket = Parameters->ExistingSocket;

        ASSERT(Socket->IoState != NULL);

        IoSocketAddReference(Socket);

    //
    // Most of the time, a socket needs to be created.
    //

    } else {
        if (Parameters->Domain == NetDomainLocal) {
            Status = IopCreateUnixSocket(Parameters->Domain,
                                         Parameters->Type,
                                         Parameters->Protocol,
                                         &Socket);

        } else {
            if (IoNetInterfaceInitialized == FALSE) {
                Status = STATUS_NOT_IMPLEMENTED;
                goto CreateSocketEnd;
            }

            Status = IoNetInterface.CreateSocket(Parameters->Domain,
                                                 Parameters->Type,
                                                 Parameters->Protocol,
                                                 &Socket);
        }

        if (!KSUCCESS(Status)) {
            goto CreateSocketEnd;
        }

        IoSocketAddReference(Socket);

        //
        // Fill in the standard parts of the socket structure.
        //

        Socket->Domain = Parameters->Domain;
        Socket->Type = Parameters->Type;
        if (Socket->IoState == NULL) {
            Socket->IoState = IoCreateIoObjectState(FALSE, FALSE);
            if (Socket->IoState == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateSocketEnd;
            }
        }
    }

    //
    // Create or look up a file object for the socket if needed.
    //

    if (*FileObject == NULL) {
        Thread = KeGetCurrentThread();
        RtlZeroMemory(&Properties, sizeof(FILE_PROPERTIES));
        RootObject = ObGetRootObject();
        Properties.DeviceId = OBJECT_MANAGER_DEVICE_ID;
        Properties.FileId = (UINTN)Socket;
        Properties.Type = IoObjectSocket;
        Properties.UserId = Thread->Identity.EffectiveUserId;
        Properties.GroupId = Thread->Identity.EffectiveGroupId;
        Properties.HardLinkCount = 1;
        Properties.Permissions = Create->Permissions;
        KeGetSystemTime(&(Properties.StatusChangeTime));
        RtlCopyMemory(&(Properties.ModifiedTime),
                      &(Properties.StatusChangeTime),
                      sizeof(SYSTEM_TIME));

        RtlCopyMemory(&(Properties.AccessTime),
                      &(Properties.StatusChangeTime),
                      sizeof(SYSTEM_TIME));

        Status = IopCreateOrLookupFileObject(&Properties,
                                             RootObject,
                                             FILE_OBJECT_FLAG_EXTERNAL_IO_STATE,
                                             0,
                                             &NewFileObject,
                                             &(Create->Created));

        if (!KSUCCESS(Status)) {
            goto CreateSocketEnd;
        }

        ASSERT((Create->Created != FALSE) ||
               (Socket == Parameters->ExistingSocket));

        *FileObject = NewFileObject;
    }

    ASSERT(((*FileObject)->IoState == NULL) &&
           (((*FileObject)->Flags & FILE_OBJECT_FLAG_EXTERNAL_IO_STATE) != 0));

    (*FileObject)->IoState = Socket->IoState;
    (*FileObject)->SpecialIo = Socket;
    Status = STATUS_SUCCESS;

CreateSocketEnd:

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

    if (!KSUCCESS(Status)) {
        if ((Socket != NULL) &&
            (*FileObject != NULL) &&
            ((*FileObject)->SpecialIo != Socket)) {

            IoSocketReleaseReference(Socket);
        }

        if (NewFileObject != NULL) {
            IopFileObjectReleaseReference(NewFileObject);
            *FileObject = NULL;
        }
    }

    return Status;
}

KSTATUS
IopPerformSocketIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from or writes to a socket.

Arguments:

    Handle - Supplies a pointer to the socket I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    SOCKET_IO_PARAMETERS IoParameters;
    KSTATUS Status;

    ASSERT(IoContext->IoBuffer != NULL);

    RtlZeroMemory(&IoParameters, sizeof(SOCKET_IO_PARAMETERS));
    IoParameters.Size = IoContext->SizeInBytes;
    IoParameters.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    if (IoContext->Write != FALSE) {
        Status = IoSocketSendData(FALSE,
                                  Handle,
                                  &IoParameters,
                                  IoContext->IoBuffer);

    } else {
        Status = IoSocketReceiveData(FALSE,
                                     Handle,
                                     &IoParameters,
                                     IoContext->IoBuffer);
    }

    IoContext->BytesCompleted = IoParameters.BytesCompleted;
    return Status;
}

KSTATUS
IopOpenSocket (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine opens a socket connection.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the socket being opened.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PSOCKET Socket;

    FileObject = IoHandle->FileObject;

    ASSERT(FileObject->Properties.Type == IoObjectSocket);

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Socket = FileObject->SpecialIo;
    if (Socket == NULL) {
        return STATUS_NOT_READY;
    }

    return STATUS_SUCCESS;
}

KSTATUS
IopCloseSocket (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    IoHandle - Supplies a pointer to the socket handle to close.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT FileObject;
    PSOCKET Socket;
    KSTATUS Status;

    FileObject = IoHandle->FileObject;
    if (FileObject->Properties.Type != IoObjectSocket) {
        Status = STATUS_NOT_A_SOCKET;
        goto CloseSocketEnd;
    }

    if (IoHandle->Access == 0) {
        return STATUS_SUCCESS;
    }

    Socket = FileObject->SpecialIo;
    if (Socket->Domain == NetDomainLocal) {
        Status = IopUnixSocketClose(Socket);

    } else {
        if (IoNetInterfaceInitialized == FALSE) {
            Status = STATUS_NOT_IMPLEMENTED;

        } else {
            Status = IoNetInterface.CloseSocket(Socket);
            if (!KSUCCESS(Status)) {
                goto CloseSocketEnd;
            }
        }
    }

CloseSocketEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopDestroySocket (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine destroys a socket object.

Arguments:

    Socket - Supplies a pointer to the socket to close.

Return Value:

    Status code.

--*/

{

    ASSERT(Socket->ReferenceCount == 0);

    ASSERT(Socket->IoState != NULL);

    IoDestroyIoObjectState(Socket->IoState, FALSE);
    Socket->IoState = NULL;
    if (Socket->Domain == NetDomainLocal) {
        IopDestroyUnixSocket(Socket);

    } else {
        IoNetInterface.DestroySocket(Socket);
    }

    return;
}

KSTATUS
IopConvertInterruptedSocketStatus (
    PIO_HANDLE Handle,
    UINTN BytesComplete,
    BOOL OutputOperation
    )

/*++

Routine Description:

    This routine handles converting an interrupted socket status into the
    appropriate system call return status, taking into account whether or not
    the system call can be restarted.

Arguments:

    Handle - Supplies a pointer to the I/O handle for a socket.

    BytesComplete - Supplies the number of I/O bytes completed during the
        system call.

    OutputOperation - Supplies a boolean indicating if the system call was
        performing a an output socket operation (send/connect) or an input
        operation (receive/accept).

Return Value:

    Returns the status that the interrupted status should be convert to.

--*/

{

    ULONG Mask;
    PSOCKET Socket;
    KSTATUS Status;

    ASSERT(Handle != NULL);

    Socket = NULL;

    //
    // If bytes were actually completed, return success.
    //

    if (BytesComplete != 0) {
        return STATUS_SUCCESS;
    }

    Status = IoGetSocketFromHandle(Handle, &Socket);

    ASSERT(KSUCCESS(Status));

    Mask = SOCKET_FLAG_RECEIVE_TIMEOUT_SET;
    if (OutputOperation != FALSE) {
        Mask = SOCKET_FLAG_SEND_TIMEOUT_SET;
    }

    //
    // If no bytes were completed and a timeout was not set, then the system
    // call can be restarted if the signal handler allows.
    //

    if ((Socket->Flags & Mask) == 0) {
        return STATUS_RESTART_AFTER_SIGNAL;
    }

    return STATUS_INTERRUPTED;
}

