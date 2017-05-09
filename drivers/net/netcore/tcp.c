/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tcp.c

Abstract:

    This module implements the Transmission Control Protocol.

Author:

    Evan Green 10-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Protocol drivers are supposed to be able to stand on their own (ie be able to
// be implemented outside the core net library). For the builtin ones, avoid
// including netcore.h, but still redefine those functions that would otherwise
// generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip4.h>
#include "tcp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TCP_TIMER_MAX_REFERENCE 0x10000000

#define TCP_POLL_EVENT_IO               \
    (POLL_EVENT_IN | POLL_EVENT_OUT |   \
     POLL_EVENT_IN_HIGH_PRIORITY | POLL_EVENT_OUT_HIGH_PRIORITY)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TCP_TIMER_STATE {
    TcpTimerNotQueued,
    TcpTimerQueued,
} TPC_TIMER_STATE, *PTCP_TIMER_STATE;

/*++

Structure Description:

    This structure defines a TCP socket option.

Members:

    InformationType - Stores the information type for the socket option.

    Option - Stores the type-specific option identifier.

    Size - Stores the size of the option value, in bytes.

    SetAllowed - Stores a boolean indicating whether or not the option is
        allowed to be set.

--*/

typedef struct _TCP_SOCKET_OPTION {
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN Option;
    UINTN Size;
    BOOL SetAllowed;
} TCP_SOCKET_OPTION, *PTCP_SOCKET_OPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpTcpCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetpTcpDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpTcpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpTcpListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpTcpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpTcpAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpTcpClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpTcpShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpTcpSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpTcpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpTcpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpTcpReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpTcpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpTcpUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

VOID
NetpTcpWorkerThread (
    PVOID Parameter
    );

VOID
NetpTcpProcessPacket (
    PTCP_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    );

VOID
NetpTcpHandleUnconnectedPacket (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    );

VOID
NetpTcpFillOutHeader (
    PTCP_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    ULONG SequenceNumber,
    USHORT ExtraFlags,
    ULONG OptionsLength,
    USHORT NonUrgentOffset,
    ULONG DataLength
    );

USHORT
NetpTcpChecksumData (
    PVOID Data,
    ULONG DataLength,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

BOOL
NetpTcpIsReceiveSegmentAcceptable (
    PTCP_SOCKET Socket,
    ULONG SequenceNumber,
    ULONG SegmentLength
    );

KSTATUS
NetpTcpProcessAcknowledge (
    PTCP_SOCKET Socket,
    ULONG AcknowledgeNumber,
    ULONG SequenceNumber,
    ULONG DataLength,
    USHORT WindowSize
    );

VOID
NetpTcpProcessPacketOptions (
    PTCP_SOCKET Socket,
    PTCP_HEADER Header,
    PNET_PACKET_BUFFER Packet
    );

VOID
NetpTcpSendControlPacket (
    PTCP_SOCKET Socket,
    ULONG Flags
    );

VOID
NetpTcpProcessReceivedDataSegment (
    PTCP_SOCKET Socket,
    ULONG SequenceNumber,
    PVOID Buffer,
    ULONG Length,
    PTCP_HEADER Header
    );

KSTATUS
NetpTcpInsertReceivedDataSegment (
    PTCP_SOCKET Socket,
    PTCP_RECEIVED_SEGMENT PreviousSegment,
    PTCP_RECEIVED_SEGMENT NextSegment,
    PTCP_HEADER Header,
    PVOID *Buffer,
    PULONG SequenceNumber,
    PULONG Length,
    PBOOL InsertedSegment
    );

VOID
NetpTcpSendPendingSegments (
    PTCP_SOCKET Socket,
    PULONGLONG CurrentTime
    );

KSTATUS
NetpTcpSendSegment (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    );

PNET_PACKET_BUFFER
NetpTcpCreatePacket (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    );

VOID
NetpTcpFreeSentSegments (
    PTCP_SOCKET Socket,
    PULONGLONG CurrentTime
    );

VOID
NetpTcpFreeSocketDataBuffers (
    PTCP_SOCKET Socket
    );

VOID
NetpTcpShutdownUnlocked (
    PTCP_SOCKET TcpSocket,
    ULONG ShutdownType
    );

VOID
NetpTcpShutdownTransmit (
    PTCP_SOCKET TcpSocket
    );

VOID
NetpTcpShutdownReceive (
    PTCP_SOCKET TcpSocket,
    PBOOL ResetSent
    );

KSTATUS
NetpTcpCloseOutSocket (
    PTCP_SOCKET Socket,
    BOOL InsideWorker
    );

VOID
NetpTcpHandleIncomingConnection (
    PTCP_SOCKET ListeningSocket,
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    );

VOID
NetpTcpSetState (
    PTCP_SOCKET Socket,
    TCP_STATE NewState
    );

KSTATUS
NetpTcpSendSyn (
    PTCP_SOCKET Socket,
    BOOL WithAcknowledge
    );

VOID
NetpTcpTimerAddReference (
    PTCP_SOCKET Socket
    );

ULONG
NetpTcpTimerReleaseReference (
    PTCP_SOCKET Socket
    );

VOID
NetpTcpQueueTcpTimer (
    VOID
    );

VOID
NetpTcpArmKeepAliveTimer (
    ULONGLONG DueTime
    );

KSTATUS
NetpTcpReceiveOutOfBandData (
    BOOL FromKernelMode,
    PTCP_SOCKET TcpSocket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

PTCP_SEGMENT_HEADER
NetpTcpAllocateSegment (
    PTCP_SOCKET Socket,
    ULONG AllocationSize
    );

VOID
NetpTcpFreeSegment (
    PTCP_SOCKET Socket,
    PTCP_SEGMENT_HEADER Segment
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the global TCP timer.
//

PKTIMER NetTcpTimer;
ULONGLONG NetTcpTimerPeriod;
volatile ULONG NetTcpTimerReferenceCount;
volatile ULONG NetTcpTimerState = TcpTimerNotQueued;

//
// Store a pointer to the global TCP keep alive timer.
//

PQUEUED_LOCK NetTcpKeepAliveTimerLock;
PKTIMER NetTcpKeepAliveTimer;

//
// Store the global list of sockets.
//

LIST_ENTRY NetTcpSocketList;
PQUEUED_LOCK NetTcpSocketListLock;

//
// Store the TCP debug flags, which print out a bunch more information.
//

BOOL NetTcpDebugPrintAllPackets = FALSE;
BOOL NetTcpDebugPrintSequenceNumbers = FALSE;
BOOL NetTcpDebugPrintCongestionControl = FALSE;

//
// This flag changes the behavior of the debug spew, turning on printing of
// local addresses.
//

BOOL NetTcpDebugPrintLocalAddress = FALSE;

NET_PROTOCOL_ENTRY NetTcpProtocol = {
    {NULL, NULL},
    NetSocketStream,
    SOCKET_INTERNET_PROTOCOL_TCP,
    NET_PROTOCOL_FLAG_UNICAST_ONLY | NET_PROTOCOL_FLAG_CONNECTION_BASED,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpTcpCreateSocket,
        NetpTcpDestroySocket,
        NetpTcpBindToAddress,
        NetpTcpListen,
        NetpTcpAccept,
        NetpTcpConnect,
        NetpTcpClose,
        NetpTcpShutdown,
        NetpTcpSend,
        NetpTcpProcessReceivedData,
        NetpTcpProcessReceivedSocketData,
        NetpTcpReceive,
        NetpTcpGetSetInformation,
        NetpTcpUserControl
    }
};

TCP_SOCKET_OPTION NetTcpSocketOptions[] = {
    {
        SocketInformationBasic,
        SocketBasicOptionLinger,
        sizeof(SOCKET_LINGER),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionSendBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionSendMinimum,
        sizeof(ULONG),
        FALSE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionSendTimeout,
        sizeof(SOCKET_TIME),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveMinimum,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveTimeout,
        sizeof(SOCKET_TIME),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionAcceptConnections,
        sizeof(ULONG),
        FALSE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionKeepAlive,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionInlineOutOfBand,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationTcp,
        SocketTcpOptionNoDelay,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationTcp,
        SocketTcpOptionKeepAliveTimeout,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationTcp,
        SocketTcpOptionKeepAlivePeriod,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationTcp,
        SocketTcpOptionKeepAliveProbeLimit,
        sizeof(ULONG),
        TRUE
    },
};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpTcpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for TCP sockets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Allow debugging to get more verbose, but leave it alone if some
    // developer has already turned it on.
    //

    if (NetTcpDebugPrintAllPackets == FALSE) {
        NetTcpDebugPrintAllPackets = NetGetGlobalDebugFlag();
    }

    if (NetTcpDebugPrintSequenceNumbers == FALSE) {
        NetTcpDebugPrintSequenceNumbers = NetGetGlobalDebugFlag();
    }

    INITIALIZE_LIST_HEAD(&NetTcpSocketList);

    //
    // Create the global periodic timer and list lock.
    //

    ASSERT(NetTcpSocketListLock == NULL);

    NetTcpSocketListLock = KeCreateQueuedLock();
    if (NetTcpSocketListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpInitializeEnd;
    }

    ASSERT(NetTcpTimer == NULL);

    NetTcpTimer = KeCreateTimer(TCP_ALLOCATION_TAG);
    if (NetTcpTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpInitializeEnd;
    }

    ASSERT(NetTcpKeepAliveTimerLock == NULL);

    NetTcpKeepAliveTimerLock = KeCreateQueuedLock();
    if (NetTcpKeepAliveTimerLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpInitializeEnd;
    }

    NetTcpTimerPeriod = KeConvertMicrosecondsToTimeTicks(TCP_TIMER_PERIOD);

    ASSERT(NetTcpKeepAliveTimer == NULL);

    NetTcpKeepAliveTimer = KeCreateTimer(TCP_ALLOCATION_TAG);
    if (NetTcpKeepAliveTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpInitializeEnd;
    }

    //
    // Create the worker thread.
    //

    Status = PsCreateKernelThread(NetpTcpWorkerThread, NULL, "TcpWorkerThread");
    if (!KSUCCESS(Status)) {
        goto TcpInitializeEnd;
    }

    //
    // Register the TCP socket handlers with the core networking library.
    //

    Status = NetRegisterProtocol(&NetTcpProtocol, NULL);
    if (!KSUCCESS(Status)) {
        goto TcpInitializeEnd;
    }

TcpInitializeEnd:
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        if (NetTcpSocketListLock != NULL) {
            KeDestroyQueuedLock(NetTcpSocketListLock);
            NetTcpSocketListLock = NULL;
        }

        if (NetTcpTimer != NULL) {
            KeDestroyTimer(NetTcpTimer);
            NetTcpTimer = NULL;
        }

        if (NetTcpKeepAliveTimerLock != NULL) {
            KeDestroyQueuedLock(NetTcpKeepAliveTimerLock);
            NetTcpKeepAliveTimerLock = NULL;
        }

        if (NetTcpKeepAliveTimer != NULL) {
            KeDestroyTimer(NetTcpKeepAliveTimer);
            NetTcpKeepAliveTimer = NULL;
        }
    }

    return;
}

KSTATUS
NetpTcpCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The protocol
    driver is responsible for allocating the structure (with additional length
    for any of its context). The core networking library will fill in the
    common header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The core network library will fill in the standard socket
        structure after this routine returns. In phase 1, this will contain
        a pointer to the socket allocated during phase 0.

    Phase - Supplies the socket creation phase. Phase 0 is the allocation phase
        and phase 1 is the advanced initialization phase, which is invoked
        after net core is done filling out common portions of the socket
        structure.

Return Value:

    Status code.

--*/

{

    PIO_OBJECT_STATE IoState;
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;

    ASSERT(ProtocolEntry->Type == NetSocketStream);
    ASSERT((ProtocolEntry->ParentProtocolNumber ==
            SOCKET_INTERNET_PROTOCOL_TCP) &&
           (NetworkProtocol == ProtocolEntry->ParentProtocolNumber));

    //
    // TCP only operates in phase 0.
    //

    if (Phase != 0) {
        return STATUS_SUCCESS;
    }

    IoState = NULL;
    TcpSocket = MmAllocatePagedPool(sizeof(TCP_SOCKET), TCP_ALLOCATION_TAG);
    if (TcpSocket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpCreateSocketEnd;
    }

    RtlZeroMemory(TcpSocket, sizeof(TCP_SOCKET));
    TcpSocket->NetSocket.KernelSocket.Protocol = NetworkProtocol;
    TcpSocket->NetSocket.KernelSocket.ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(TcpSocket->ReceivedSegmentList));
    INITIALIZE_LIST_HEAD(&(TcpSocket->OutgoingSegmentList));
    INITIALIZE_LIST_HEAD(&(TcpSocket->FreeSegmentList));
    INITIALIZE_LIST_HEAD(&(TcpSocket->IncomingConnectionList));
    NetpTcpSetState(TcpSocket, TcpStateInitialized);
    TcpSocket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
    TcpSocket->ReceiveWindowTotalSize = TCP_DEFAULT_WINDOW_SIZE;
    TcpSocket->ReceiveWindowFreeSize = TcpSocket->ReceiveWindowTotalSize;
    TcpSocket->ReceiveWindowScale = TCP_DEFAULT_WINDOW_SCALE;
    TcpSocket->ReceiveTimeout = WAIT_TIME_INDEFINITE;
    TcpSocket->ReceiveMinimum = TCP_DEFAULT_RECEIVE_MINIMUM;
    TcpSocket->SendBufferTotalSize = TCP_DEFAULT_SEND_BUFFER_SIZE;
    TcpSocket->SendBufferFreeSize = TcpSocket->SendBufferTotalSize;
    TcpSocket->SendInitialSequence = (ULONG)HlQueryTimeCounter();
    TcpSocket->SendUnacknowledgedSequence = TcpSocket->SendInitialSequence;
    TcpSocket->SendNextBufferSequence = TcpSocket->SendInitialSequence;
    TcpSocket->SendNextNetworkSequence = TcpSocket->SendInitialSequence;
    TcpSocket->SendTimeout = WAIT_TIME_INDEFINITE;
    TcpSocket->KeepAliveTimeout = TCP_DEFAULT_KEEP_ALIVE_TIMEOUT;
    TcpSocket->KeepAlivePeriod = TCP_DEFAULT_KEEP_ALIVE_PERIOD;
    TcpSocket->KeepAliveProbeLimit = TCP_DEFAULT_KEEP_ALIVE_PROBE_LIMIT;
    TcpSocket->OutOfBandData = -1;
    TcpSocket->Lock = KeCreateQueuedLock();
    if (TcpSocket->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpCreateSocketEnd;
    }

    IoState = IoCreateIoObjectState(TRUE, FALSE);
    if (IoState == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpCreateSocketEnd;
    }

    ASSERT(TcpSocket->Flags == 0);

    NetpTcpCongestionInitializeSocket(TcpSocket);

    //
    // Start by assuming the remote supports the desired options.
    //

    TcpSocket->Flags |= TCP_SOCKET_FLAG_WINDOW_SCALING;

    //
    // Initialize the socket on the lower layers.
    //

    PacketSizeInformation = &(TcpSocket->NetSocket.PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = MAX_ULONG;
    Status = NetworkEntry->Interface.InitializeSocket(ProtocolEntry,
                                                      NetworkEntry,
                                                      NetworkProtocol,
                                                      &(TcpSocket->NetSocket));

    if (!KSUCCESS(Status)) {
        goto TcpCreateSocketEnd;
    }

    //
    // TCP has no maximum packet limit as the header does not store a length.
    // The maximum packet size, calculated by the lower layers, should have
    // enough room for a TCP header and a one byte of data.
    //

    ASSERT((PacketSizeInformation->MaxPacketSize -
            PacketSizeInformation->HeaderSize -
            PacketSizeInformation->FooterSize) > sizeof(TCP_HEADER));

    //
    // Add the TCP header size to the protocol header size.
    //

    PacketSizeInformation->HeaderSize += sizeof(TCP_HEADER);

    ASSERT(TcpSocket->NetSocket.KernelSocket.IoState == NULL);

    TcpSocket->NetSocket.KernelSocket.IoState = IoState;
    KeAcquireQueuedLock(NetTcpSocketListLock);
    INSERT_BEFORE(&(TcpSocket->ListEntry), &NetTcpSocketList);
    KeReleaseQueuedLock(NetTcpSocketListLock);
    Status = STATUS_SUCCESS;

TcpCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (TcpSocket != NULL) {
            if (TcpSocket->Lock != NULL) {
                KeDestroyQueuedLock(TcpSocket->Lock);
            }

            MmFreePagedPool(TcpSocket);
            TcpSocket = NULL;
        }

        if (IoState != NULL) {
            IoDestroyIoObjectState(IoState, FALSE);
        }
    }

    if (TcpSocket != NULL) {
        *NewSocket = &(TcpSocket->NetSocket);

    } else {
        *NewSocket = NULL;
    }

    return Status;
}

VOID
NetpTcpDestroySocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel and core networking library's knowledge of
    this structure.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The core networking
        library will have already destroyed any resources inside the common
        header, the protocol should not reach through any pointers inside the
        socket header except the protocol and network entries.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

{

    PTCP_SOCKET TcpSocket;

    TcpSocket = (PTCP_SOCKET)Socket;

    ASSERT(TcpSocket->State == TcpStateClosed);
    ASSERT(TcpSocket->ListEntry.Next == NULL);
    ASSERT(LIST_EMPTY(&(TcpSocket->ReceivedSegmentList)) != FALSE);
    ASSERT(LIST_EMPTY(&(TcpSocket->OutgoingSegmentList)) != FALSE);
    ASSERT(TcpSocket->TimerReferenceCount == 0);

    if (Socket->Network->Interface.DestroySocket != NULL) {
        Socket->Network->Interface.DestroySocket(Socket);
    }

    KeDestroyQueuedLock(TcpSocket->Lock);
    TcpSocket->Lock = NULL;
    TcpSocket->State = TcpStateInvalid;
    MmFreePagedPool(TcpSocket);
    return;
}

KSTATUS
NetpTcpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.
    Usually this is a no-op for the protocol, it's simply responsible for
    passing the request down to the network layer.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Currently only IPv4 addresses are supported.
    //

    if (Address->Domain != NetDomainIp4) {
        Status = STATUS_NOT_SUPPORTED;
        goto TcpBindToAddressEnd;
    }

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.BindToAddress(Socket, Link, Address, 0);
    if (!KSUCCESS(Status)) {
        goto TcpBindToAddressEnd;
    }

TcpBindToAddressEnd:
    return Status;
}

KSTATUS
NetpTcpListen (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing clients to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listning.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PTCP_SOCKET TcpSocket;

    TcpSocket = (PTCP_SOCKET)Socket;
    KeAcquireQueuedLock(TcpSocket->Lock);

    ASSERT(TcpSocket->NetSocket.BindingType != SocketBindingInvalid);

    Status = STATUS_SUCCESS;
    if (TcpSocket->State != TcpStateListening) {
        if (TcpSocket->State != TcpStateInitialized) {
            Status = STATUS_INVALID_PARAMETER;
            goto TcpListenEnd;
        }

        NetpTcpSetState(TcpSocket, TcpStateListening);

        //
        // Begin listening for incoming connection requests.
        //

        Status = Socket->Network->Interface.Listen(Socket);
        if (!KSUCCESS(Status)) {
            goto TcpListenEnd;
        }
    }

TcpListenEnd:
    KeReleaseQueuedLock(TcpSocket->Lock);
    return Status;
}

KSTATUS
NetpTcpAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
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

Return Value:

    Status code.

--*/

{

    PTCP_INCOMING_CONNECTION IncomingConnection;
    PIO_OBJECT_STATE IoState;
    PIO_HANDLE NewHandle;
    PTCP_SOCKET NewTcpSocket;
    ULONG OpenFlags;
    ULONG ReturnedEvents;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;
    ULONG Timeout;

    TcpSocket = (PTCP_SOCKET)Socket;

    //
    // The socket has to be listening first in order to accept connections.
    //

    if (TcpSocket->State != TcpStateListening) {
        NewHandle = NULL;
        Status = STATUS_INVALID_PARAMETER;
        goto TcpAcceptEnd;
    }

    Timeout = WAIT_TIME_INDEFINITE;
    OpenFlags = IoGetIoHandleOpenFlags(Socket->KernelSocket.IoHandle);
    if ((OpenFlags & OPEN_FLAG_NON_BLOCKING) != 0) {
        Timeout = 0;
    }

    //
    // Loop trying to get a solid established connection.
    //

    while (TRUE) {
        IncomingConnection = NULL;
        NewHandle = NULL;
        NewTcpSocket = NULL;
        IoState = TcpSocket->NetSocket.KernelSocket.IoState;

        //
        // Loop competing with other accepts trying to get an incoming
        // connection structure.
        //

        while (TRUE) {
            Status = IoWaitForIoObjectState(IoState,
                                            POLL_EVENT_IN,
                                            TRUE,
                                            Timeout,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                if (Status == STATUS_TIMEOUT) {
                    Status = STATUS_OPERATION_WOULD_BLOCK;
                }

                goto TcpAcceptEnd;
            }

            if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
                if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                    Status = STATUS_NO_NETWORK_CONNECTION;

                } else {
                    Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
                    if (KSUCCESS(Status)) {
                        Status = STATUS_DEVICE_IO_ERROR;
                    }
                }

                goto TcpAcceptEnd;
            }

            KeAcquireQueuedLock(TcpSocket->Lock);
            if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
                KeReleaseQueuedLock(TcpSocket->Lock);
                Status = STATUS_CONNECTION_CLOSED;
                goto TcpAcceptEnd;
            }

            if (TcpSocket->IncomingConnectionCount != 0) {

                ASSERT(LIST_EMPTY(&(TcpSocket->IncomingConnectionList)) ==
                       FALSE);

                IncomingConnection =
                             LIST_VALUE(TcpSocket->IncomingConnectionList.Next,
                                        TCP_INCOMING_CONNECTION,
                                        ListEntry);

                LIST_REMOVE(&(IncomingConnection->ListEntry));
                TcpSocket->IncomingConnectionCount -= 1;
            }

            if (TcpSocket->IncomingConnectionCount == 0) {

                //
                // If the incoming connection count is zero, then there should
                // be nothing on that list.
                //

                ASSERT(LIST_EMPTY(&(TcpSocket->IncomingConnectionList)) !=
                       FALSE);

                IoSetIoObjectState(IoState, POLL_EVENT_IN, FALSE);
            }

            KeReleaseQueuedLock(TcpSocket->Lock);
            if (IncomingConnection != NULL) {
                NewHandle = IncomingConnection->IoHandle;
                MmFreePagedPool(IncomingConnection);
                Status = STATUS_SUCCESS;
                break;
            }
        }

        ASSERT(NewHandle != NULL);

        Status = IoGetSocketFromHandle(NewHandle, (PVOID)&NewTcpSocket);
        if (!KSUCCESS(Status)) {
            goto TcpAcceptEnd;
        }

        //
        // Wait indefinitely for the connection to be established. If there is
        // any error (including timeouts), the new socket will be closed.
        //

        IoState = NewTcpSocket->NetSocket.KernelSocket.IoState;
        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_OUT,
                                        TRUE,
                                        WAIT_TIME_INDEFINITE,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto TcpAcceptEnd;
        }

        //
        // If there were no errors and the socket is in an expected state,
        // then successfully return.
        //

        if ((ReturnedEvents & POLL_ERROR_EVENTS) == 0) {
            if ((NewTcpSocket->State == TcpStateEstablished) ||
                (NewTcpSocket->State == TcpStateCloseWait)) {

                if (RemoteAddress != NULL) {
                    RtlCopyMemory(RemoteAddress,
                                  &(NewTcpSocket->NetSocket.RemoteAddress),
                                  sizeof(NETWORK_ADDRESS));
                }

                break;
            }

        //
        // If there were errors, then only quit the accept if the network
        // was disconnected. Otherwise try to get another connection.
        //

        } else {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;
                break;
            }
        }

        //
        // Destroy the new socket before trying to get another connection.
        //

        IoClose(NewHandle);
    }

TcpAcceptEnd:
    if (!KSUCCESS(Status)) {
        if (NewHandle != NULL) {
            IoClose(NewHandle);
            NewHandle = NULL;
        }
    }

    *NewConnectionSocket = NewHandle;
    return Status;
}

KSTATUS
NetpTcpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

Return Value:

    Status code.

--*/

{

    BOOL Connected;
    BOOL LockHeld;
    ULONG ReturnedEvents;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;

    Connected = FALSE;
    TcpSocket = (PTCP_SOCKET)Socket;
    KeAcquireQueuedLock(TcpSocket->Lock);
    LockHeld = TRUE;
    if (TcpSocket->State != TcpStateInitialized) {

        //
        // If a previous connect was not interrupted, then not being in the
        // initialized state is fatal.
        //

        if ((TcpSocket->Flags & TCP_SOCKET_FLAG_CONNECT_INTERRUPTED) == 0) {
            if ((TcpSocket->State == TcpStateSynSent) ||
                (TcpSocket->State == TcpStateSynReceived)) {

                Status = STATUS_ALREADY_INITIALIZED;

            } else {
                Status = STATUS_CONNECTION_EXISTS;
            }

            goto TcpConnectEnd;

        //
        // Otherwise note that the socket has already been connected to the
        // network layer and move on.
        //

        } else {
            Connected = TRUE;
        }
    }

    //
    // Unset the interrupted flag before giving the connect another shot.
    //

    TcpSocket->Flags &= ~TCP_SOCKET_FLAG_CONNECT_INTERRUPTED;

    //
    // Pass the request down to the network layer.
    //

    if (Connected == FALSE) {
        Status = Socket->Network->Interface.Connect(Socket, Address);
        if (!KSUCCESS(Status)) {
            goto TcpConnectEnd;
        }

        Connected = TRUE;

        //
        // Put the socket in the SYN sent state. This will fire off a SYN.
        //

        NetpTcpSetState(TcpSocket, TcpStateSynSent);
    }

    KeReleaseQueuedLock(TcpSocket->Lock);
    LockHeld = FALSE;

    //
    // Wait indefinitely for the connection to be established. The internal
    // SYN retry mechanisms will timeout and signal the events if the other
    // side isn't there.
    //

    Status = IoWaitForIoObjectState(Socket->KernelSocket.IoState,
                                    POLL_EVENT_OUT,
                                    TRUE,
                                    WAIT_TIME_INDEFINITE,
                                    &ReturnedEvents);

    if (!KSUCCESS(Status)) {
        goto TcpConnectEnd;
    }

    //
    // An event was signalled. If it was an error, then plan to fail the
    // connect.
    //

    if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
        if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
            Status = STATUS_NO_NETWORK_CONNECTION;

        } else {
            Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
            if (KSUCCESS(Status)) {
                Status = STATUS_DEVICE_IO_ERROR;
            }
        }

    //
    // If there was not an error then the connection should either be
    // established or in the close-wait state (the remote side may have
    // quickly sent a SYN and then a FIN).
    //

    } else if ((TcpSocket->State != TcpStateEstablished) &&
               (TcpSocket->State != TcpStateCloseWait)) {

        Status = STATUS_CONNECTION_RESET;
    }

TcpConnectEnd:

    //
    // If the connect was attempted but failed for a reason other than a
    // timeout or that the wait was interrupted, stop the socket in its tracks.
    // When interrupted, the connect is meant to continue in the background,
    // but record the interruption in case the system call gets restarted. On
    // timeout, the mechanism that determined the timeout handled the
    // appropriate clean up of the socket (i.e. disconnect and reinitialize).
    //

    if (!KSUCCESS(Status) &&
        (Connected != FALSE) &&
        (Status != STATUS_TIMEOUT)) {

        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(TcpSocket->Lock);
            LockHeld = TRUE;
        }

        if (Status == STATUS_INTERRUPTED) {
            TcpSocket->Flags |= TCP_SOCKET_FLAG_CONNECT_INTERRUPTED;

        } else {
            NetpTcpCloseOutSocket(TcpSocket, FALSE);
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(TcpSocket->Lock);
    }

    return Status;
}

KSTATUS
NetpTcpClose (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    Socket - Supplies a pointer to the socket to shut down.

Return Value:

    Status code.

--*/

{

    BOOL CloseOutSocket;
    PIO_OBJECT_STATE IoState;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;

    CloseOutSocket = FALSE;
    Status = STATUS_SUCCESS;
    TcpSocket = (PTCP_SOCKET)Socket;
    IoState = TcpSocket->NetSocket.KernelSocket.IoState;
    RtlAtomicOr32(&(TcpSocket->ShutdownTypes),
                  SOCKET_SHUTDOWN_READ | SOCKET_SHUTDOWN_WRITE);

    KeAcquireQueuedLock(TcpSocket->Lock);
    switch (TcpSocket->State) {
    case TcpStateInitialized:
    case TcpStateClosed:
        CloseOutSocket = TRUE;
        break;

    //
    // When awaiting a FIN, if this side is about to drop some received
    // packets due to this close, a RST should be sent.
    //

    case TcpStateFinWait1:
    case TcpStateFinWait2:
        NetpTcpShutdownUnlocked(TcpSocket, TcpSocket->ShutdownTypes);
        break;

    //
    // For many states, do nothing.
    //

    case TcpStateClosing:
    case TcpStateLastAcknowledge:
    case TcpStateTimeWait:
        break;

    //
    // For the Listening and Syn-Sent states, clean up the socket straight
    // away.
    //

    case TcpStateListening:
    case TcpStateSynSent:
        CloseOutSocket = TRUE;
        break;

    //
    // In the states with active connections, send a FIN segment (or at
    // least queue that one needs to be sent. If, however, this side has not
    // read everything it received, skip the FIN and just send a RST.
    //

    case TcpStateSynReceived:
    case TcpStateEstablished:
    case TcpStateCloseWait:
        NetpTcpShutdownUnlocked(TcpSocket, TcpSocket->ShutdownTypes);
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_CONFIGURATION;
    }

    //
    // Potentially destroy the socket right now.
    //

    if (CloseOutSocket != FALSE) {
        Status = NetpTcpCloseOutSocket(TcpSocket, FALSE);

        ASSERT(TcpSocket->NetSocket.KernelSocket.ReferenceCount >= 1);

        KeReleaseQueuedLock(TcpSocket->Lock);

    } else {

        //
        // Handle the socket lingering option if it is enabled.
        //

        if ((TcpSocket->Flags & TCP_SOCKET_FLAG_LINGER_ENABLED) != 0) {

            //
            // If the linger timeout is set to zero, then perform an abortive
            // close by reseting and then closing.
            //

            if (TcpSocket->LingerTimeout == 0) {
                NetpTcpSendControlPacket(TcpSocket, TCP_HEADER_FLAG_RESET);
                TcpSocket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                Status = NetpTcpCloseOutSocket(TcpSocket, FALSE);
                KeReleaseQueuedLock(TcpSocket->Lock);

            //
            // Otherwise wait for the linger timeout.
            //

            } else {
                KeReleaseQueuedLock(TcpSocket->Lock);
                Status = IoWaitForIoObjectState(IoState,
                                                POLL_EVENT_OUT,
                                                TRUE,
                                                TcpSocket->LingerTimeout,
                                                NULL);

                //
                // If the wait failed or the error event was signaled, rather
                // than the out event, then the socket needs to be abortively
                // closed if it isn't already.
                //

                if (!KSUCCESS(Status) ||
                    ((IoState->Events & POLL_ERROR_EVENTS) != 0)) {

                    KeAcquireQueuedLock(TcpSocket->Lock);
                    if (TcpSocket->State != TcpStateClosed) {
                        NetpTcpSendControlPacket(TcpSocket,
                                                 TCP_HEADER_FLAG_RESET);

                        TcpSocket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                        Status = NetpTcpCloseOutSocket(TcpSocket, FALSE);
                    }

                    KeReleaseQueuedLock(TcpSocket->Lock);
                }
            }

        //
        // Otherwise just release the lock and let the close continue on.
        //

        } else {
            KeReleaseQueuedLock(TcpSocket->Lock);
        }
    }

    return Status;
}

KSTATUS
NetpTcpShutdown (
    PNET_SOCKET Socket,
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

    PTCP_SOCKET TcpSocket;

    //
    // Shutdown is not supported unless the socket is connected.
    //

    if (Socket->RemoteAddress.Domain == NetDomainInvalid) {
        return STATUS_NOT_CONNECTED;
    }

    TcpSocket = (PTCP_SOCKET)Socket;
    RtlAtomicOr32(&(TcpSocket->ShutdownTypes), ShutdownType);

    //
    // As long as a shutdown type was provided, take action unless only read is
    // meant to be shut down. Shutting down read may result in a RST if not
    // all the data in the socket's receive list has been read, but the caller
    // may still want to write.
    //

    if ((ShutdownType != 0) && (ShutdownType != SOCKET_SHUTDOWN_READ)) {
        KeAcquireQueuedLock(TcpSocket->Lock);
        NetpTcpShutdownUnlocked(TcpSocket, ShutdownType);
        KeReleaseQueuedLock(TcpSocket->Lock);
    }

    return STATUS_SUCCESS;
}

KSTATUS
NetpTcpSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network using a
    specific protocol.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the socket I/O parameters. This will
        always be a kernel mode pointer.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG AvailableSize;
    UINTN BytesComplete;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG Flags;
    PIO_OBJECT_STATE IoState;
    PTCP_SEND_SEGMENT LastSegment;
    ULONG LastSegmentLength;
    BOOL LockHeld;
    PTCP_SEND_SEGMENT NewSegment;
    BOOL OutgoingSegmentListWasEmpty;
    BOOL PushNeeded;
    ULONG RequiredOpening;
    ULONG ReturnedEvents;
    ULONG SegmentSize;
    UINTN Size;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    BytesComplete = 0;
    EndTime = 0;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    LockHeld = FALSE;
    NewSegment = NULL;
    OutgoingSegmentListWasEmpty = FALSE;
    PushNeeded = TRUE;
    TcpSocket = (PTCP_SOCKET)Socket;
    TimeCounterFrequency = 0;
    IoState = TcpSocket->NetSocket.KernelSocket.IoState;
    if (TcpSocket->State < TcpStateEstablished) {
        Status = STATUS_BROKEN_PIPE;
        goto TcpSendEnd;
    }

    if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_WRITE) != 0) {
        Status = STATUS_BROKEN_PIPE;
        goto TcpSendEnd;
    }

    if ((TcpSocket->State != TcpStateEstablished) &&
        (TcpSocket->State != TcpStateCloseWait)) {

        Status = STATUS_BROKEN_PIPE;
        goto TcpSendEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto TcpSendEnd;
    }

    Size = Parameters->Size;
    if (Size == 0) {
        Status = STATUS_SUCCESS;
        goto TcpSendEnd;
    }

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    Timeout = Parameters->TimeoutInMilliseconds;
    if (Timeout > TcpSocket->SendTimeout) {
        Timeout = TcpSocket->SendTimeout;
    }

    if ((Timeout != 0) && (Timeout != WAIT_TIME_INDEFINITE)) {
        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                                       Timeout * MICROSECONDS_PER_MILLISECOND);

        TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    //
    // First look to see if this data can be at least partially glommed on to
    // the last packet.
    //

    while (TRUE) {
        if (Timeout == 0) {
            WaitTime = 0;

        } else if (Timeout != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_OUT,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto TcpSendEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            goto TcpSendEnd;
        }

        KeAcquireQueuedLock(TcpSocket->Lock);
        LockHeld = TRUE;

        //
        // If the user called shutdown and is now trying to write, that's a
        // dufus maneuver.
        //

        if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_WRITE) != 0) {
            Status = STATUS_BROKEN_PIPE;
            goto TcpSendEnd;
        }

        //
        // Watch out for the connection shutting down.
        //

        if ((TcpSocket->State != TcpStateEstablished) &&
            (TcpSocket->State != TcpStateCloseWait)) {

            if ((TcpSocket->Flags & TCP_SOCKET_FLAG_CONNECTION_RESET) != 0) {
                Status = STATUS_CONNECTION_RESET;

            } else {
                Status = STATUS_BROKEN_PIPE;
            }

            goto TcpSendEnd;
        }

        if (TcpSocket->SendBufferFreeSize == 0) {
            IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);
            KeReleaseQueuedLock(TcpSocket->Lock);
            LockHeld = FALSE;
            NetpTcpSendPendingSegments(TcpSocket, NULL);
            continue;
        }

        //
        // If the list of things to send is empty, a new packet will definitely
        // need to be created.
        //

        if (LIST_EMPTY(&(TcpSocket->OutgoingSegmentList)) != FALSE) {
            break;
        }

        //
        // If the last packet has already been sent off or is jam packed, then
        // forget it, make a new packet.
        //

        LastSegment = LIST_VALUE(TcpSocket->OutgoingSegmentList.Previous,
                                 TCP_SEND_SEGMENT,
                                 Header.ListEntry);

        LastSegmentLength = LastSegment->Length - LastSegment->Offset;
        if ((LastSegment->SendAttemptCount != 0) ||
            (LastSegmentLength == TcpSocket->SendMaxSegmentSize)) {

            break;
        }

        //
        // Create a new segment to replace this last one. This size starts out
        // at the maximum segment size, and is taken down by the actual size
        // of the data, as well as the size of the send buffer.
        //

        SegmentSize = TcpSocket->SendMaxSegmentSize;
        if (SegmentSize > (LastSegmentLength + Size)) {
            SegmentSize = LastSegmentLength + Size;
        }

        AvailableSize = TcpSocket->SendBufferFreeSize + LastSegmentLength;
        if (SegmentSize > AvailableSize) {
            SegmentSize = AvailableSize;
        }

        AllocationSize = sizeof(TCP_SEND_SEGMENT) + SegmentSize;
        NewSegment = (PTCP_SEND_SEGMENT)NetpTcpAllocateSegment(TcpSocket,
                                                               AllocationSize);

        if (NewSegment == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto TcpSendEnd;
        }

        //
        // Copy the old last segment plus part of the new data.
        //

        RtlCopyMemory(NewSegment + 1,
                      (PUCHAR)(LastSegment + 1) + LastSegment->Offset,
                      LastSegmentLength);

        Status = MmCopyIoBufferData(
                                  IoBuffer,
                                  (PUCHAR)(NewSegment + 1) + LastSegmentLength,
                                  BytesComplete,
                                  SegmentSize - LastSegmentLength,
                                  FALSE);

        if (!KSUCCESS(Status)) {
            NetpTcpFreeSegment(TcpSocket, (PTCP_SEGMENT_HEADER)NewSegment);
            goto TcpSendEnd;
        }

        NewSegment->SequenceNumber = LastSegment->SequenceNumber +
                                     LastSegment->Offset;

        NewSegment->LastSendTime = 0;
        NewSegment->Length = SegmentSize;
        NewSegment->Offset = 0;
        NewSegment->SendAttemptCount = 0;
        NewSegment->TimeoutInterval = 0;
        NewSegment->Flags = LastSegment->Flags;

        //
        // If all the new data fit into this existing segment, then add the
        // push flag.
        //

        ASSERT((SegmentSize - LastSegment->Length) <= Size);

        if ((SegmentSize - LastSegment->Length) == Size) {
            NewSegment->Flags |= TCP_SEND_SEGMENT_FLAG_PUSH;
            PushNeeded = FALSE;

        //
        // Otherwise remove the push flag from this segment as there is more
        // data to send.
        //

        } else {
            NewSegment->Flags &= ~TCP_SEND_SEGMENT_FLAG_PUSH;
        }

        //
        // Replace the last segment with this one, and move the counters
        // forward.
        //

        INSERT_AFTER(&(NewSegment->Header.ListEntry),
                     &(LastSegment->Header.ListEntry));

        LIST_REMOVE(&(LastSegment->Header.ListEntry));
        BytesComplete += SegmentSize - LastSegment->Length;
        TcpSocket->SendBufferFreeSize -= BytesComplete;

        ASSERT(TcpSocket->SendNextBufferSequence ==
                            LastSegment->SequenceNumber + LastSegment->Length);

        TcpSocket->SendNextBufferSequence =
                                      NewSegment->SequenceNumber + SegmentSize;

        NetpTcpFreeSegment(TcpSocket, &(LastSegment->Header));
        break;
    }

    //
    // Loop creating packets.
    //

    while (BytesComplete < Size) {
        if (LockHeld == FALSE) {
            KeAcquireQueuedLock(TcpSocket->Lock);
            LockHeld = TRUE;
        }

        //
        // Watch out for the connection shutting down.
        //

        if ((TcpSocket->State != TcpStateEstablished) &&
            (TcpSocket->State != TcpStateCloseWait)) {

            if ((TcpSocket->Flags & TCP_SOCKET_FLAG_CONNECTION_RESET) != 0) {
                Status = STATUS_CONNECTION_RESET;

            } else {
                Status = STATUS_BROKEN_PIPE;
            }

            BytesComplete = 0;
            goto TcpSendEnd;
        }

        //
        // If there's no room to add anything reasonable to the send buffer,
        // try to send what's there, and then block and try again.
        //

        RequiredOpening = TcpSocket->SendMaxSegmentSize;
        if (RequiredOpening > (Size - BytesComplete)) {
            RequiredOpening = Size - BytesComplete;
        }

        if (TcpSocket->SendBufferFreeSize < RequiredOpening) {
            IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);
            OutgoingSegmentListWasEmpty = FALSE;
            NetpTcpSendPendingSegments(TcpSocket, NULL);
            KeReleaseQueuedLock(TcpSocket->Lock);
            LockHeld = FALSE;
            if (Timeout == 0) {
                WaitTime = 0;

            } else if (Timeout != WAIT_TIME_INDEFINITE) {
                CurrentTime = KeGetRecentTimeCounter();
                WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                           TimeCounterFrequency;

            } else {
                WaitTime = WAIT_TIME_INDEFINITE;
            }

            Status = IoWaitForIoObjectState(IoState,
                                            POLL_EVENT_OUT,
                                            TRUE,
                                            WaitTime,
                                            &ReturnedEvents);

            if (!KSUCCESS(Status)) {
                goto TcpSendEnd;
            }

            if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
                if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                    Status = STATUS_NO_NETWORK_CONNECTION;

                } else {
                    Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
                    if (KSUCCESS(Status)) {
                        Status = STATUS_DEVICE_IO_ERROR;
                    }
                }

                BytesComplete = 0;
                goto TcpSendEnd;
            }

            continue;
        }

        ASSERT(KeIsQueuedLockHeld(TcpSocket->Lock) != FALSE);

        //
        // Create a new segment.
        //

        SegmentSize = RequiredOpening;
        AllocationSize = sizeof(TCP_SEND_SEGMENT) + SegmentSize;
        NewSegment = (PTCP_SEND_SEGMENT)NetpTcpAllocateSegment(TcpSocket,
                                                               AllocationSize);

        if (NewSegment == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto TcpSendEnd;
        }

        //
        // Copy the new data in.
        //

        Status = MmCopyIoBufferData(IoBuffer,
                                    NewSegment + 1,
                                    BytesComplete,
                                    SegmentSize,
                                    FALSE);

        if (!KSUCCESS(Status)) {
            NetpTcpFreeSegment(TcpSocket, (PTCP_SEGMENT_HEADER)NewSegment);
            goto TcpSendEnd;
        }

        NewSegment->SequenceNumber = TcpSocket->SendNextBufferSequence;
        NewSegment->LastSendTime = 0;
        NewSegment->Length = SegmentSize;
        NewSegment->Offset = 0;
        NewSegment->SendAttemptCount = 0;
        NewSegment->TimeoutInterval = 0;
        NewSegment->Flags = 0;

        //
        // Add this to the list, and move the counters forward.
        //

        if (LIST_EMPTY(&(TcpSocket->OutgoingSegmentList)) != FALSE) {
            OutgoingSegmentListWasEmpty = TRUE;
            NetpTcpTimerAddReference(TcpSocket);
        }

        INSERT_BEFORE(&(NewSegment->Header.ListEntry),
                      &(TcpSocket->OutgoingSegmentList));

        BytesComplete += SegmentSize;
        TcpSocket->SendBufferFreeSize -= SegmentSize;
        TcpSocket->SendNextBufferSequence = NewSegment->SequenceNumber +
                                            SegmentSize;
    }

    //
    // If a push is still needed then add the flag to the last segment.
    //

    if (PushNeeded != FALSE) {

        ASSERT(NewSegment != NULL);
        ASSERT(TcpSocket->OutgoingSegmentList.Previous ==
               &(NewSegment->Header.ListEntry));

        ASSERT(BytesComplete == Size);

        NewSegment->Flags |= TCP_SEND_SEGMENT_FLAG_PUSH;
    }

    //
    // If the outgoing segment list was empty, then send the data immediately.
    // The timer to coalesce future sends should already be running.
    //

    if ((OutgoingSegmentListWasEmpty != FALSE) ||
        ((TcpSocket->Flags & TCP_SOCKET_FLAG_NO_DELAY) != 0)) {

        NetpTcpSendPendingSegments(TcpSocket, NULL);
    }

    //
    // Unsignal the write event if there is no more space.
    //

    if (TcpSocket->SendBufferFreeSize == 0) {
        IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);
    }

    Status = STATUS_SUCCESS;

TcpSendEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(TcpSocket->Lock);
    }

    //
    // If any bytes were written, then consider this a success.
    //

    if (BytesComplete != 0) {
        Status = STATUS_SUCCESS;

    } else if ((Status == STATUS_BROKEN_PIPE) &&
               ((Flags & SOCKET_IO_NO_SIGNAL) != 0)) {

        Status = STATUS_BROKEN_PIPE_SILENT;
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

VOID
NetpTcpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    USHORT Checksum;
    PTCP_HEADER Header;
    ULONG HeaderLength;
    ULONG Length;
    PNET_PACKET_BUFFER Packet;
    ULONG RelativeAcknowledgeNumber;
    ULONG RelativeSequenceNumber;
    PNET_SOCKET Socket;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;
    ULONG WindowSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Validate the packet is at least as long as the header plus its options.
    //

    Packet = ReceiveContext->Packet;
    Length = Packet->FooterOffset - Packet->DataOffset;
    if (Length < sizeof(TCP_HEADER)) {
        RtlDebugPrint("TCP: Skipping packet shorter than length of TCP "
                      "Header. Length = %d\n",
                      Length);

        return;
    }

    Header = (PTCP_HEADER)(Packet->Buffer + Packet->DataOffset);
    HeaderLength = ((Header->HeaderLength & TCP_HEADER_LENGTH_MASK) >>
                    TCP_HEADER_LENGTH_SHIFT) * sizeof(ULONG);

    if (HeaderLength < sizeof(TCP_HEADER)) {
        RtlDebugPrint("TCP: Malformed packet has header length %d less than "
                      "minimum 20.\n",
                      HeaderLength);

        return;
    }

    if (Length < HeaderLength) {
        RtlDebugPrint("TCP: Skipping packet whose length %d is less than the "
                      "header length %d.\n",
                      Length,
                      HeaderLength);

        return;
    }

    Packet->DataOffset += HeaderLength;

    //
    // Fill out the source and destination ports and look for an eligible socket
    // before doing any more work.
    //

    ReceiveContext->Source->Port = NETWORK_TO_CPU16(Header->SourcePort);
    ReceiveContext->Destination->Port =
                                     NETWORK_TO_CPU16(Header->DestinationPort);

    Socket = NULL;
    Status = NetFindSocket(ReceiveContext, &Socket);
    if (!KSUCCESS(Status)) {

        ASSERT(Status != STATUS_MORE_PROCESSING_REQUIRED);

        NetpTcpHandleUnconnectedPacket(ReceiveContext, Header);
        return;
    }

    TcpSocket = (PTCP_SOCKET)Socket;

    //
    // Ensure the checksum comes out correctly. Skip this if checksum was
    // offloaded and it was valid.
    //

    if (((Packet->Flags & NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD) == 0) ||
        ((Packet->Flags & NET_PACKET_FLAG_TCP_CHECKSUM_FAILED) != 0)) {

        Checksum = NetpTcpChecksumData(Header,
                                       Length,
                                       ReceiveContext->Source,
                                       ReceiveContext->Destination);

        if (Checksum != 0) {
            RtlDebugPrint("TCP ignoring packet with bad checksum 0x%04x headed "
                          "for port %d from port %d.\n",
                          Checksum,
                          ReceiveContext->Destination->Port,
                          ReceiveContext->Source->Port);

            return;
        }
    }

    //
    // This is a valid TCP packet. Handle it.
    //

    KeAcquireQueuedLock(TcpSocket->Lock);

    //
    // Print this packet if debugging is enabled.
    //

    if (NetTcpDebugPrintAllPackets != FALSE) {
        NetpTcpPrintSocketEndpoints(TcpSocket, FALSE);
        RtlDebugPrint(" RX [");
        if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {
            RtlDebugPrint("FIN ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
            RtlDebugPrint("SYN ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
            RtlDebugPrint("RST ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_PUSH) != 0) {
            RtlDebugPrint("PSH ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_URGENT) != 0) {
            RtlDebugPrint("URG");
        }

        RelativeAcknowledgeNumber = 0;
        if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
            RtlDebugPrint("ACK");
            RelativeAcknowledgeNumber =
                               NETWORK_TO_CPU32(Header->AcknowledgmentNumber) -
                               TcpSocket->SendInitialSequence;
        }

        RelativeSequenceNumber = 0;
        if (TcpSocket->ReceiveInitialSequence != 0) {
            RelativeSequenceNumber = NETWORK_TO_CPU32(Header->SequenceNumber) -
                                     TcpSocket->ReceiveInitialSequence;
        }

        WindowSize = NETWORK_TO_CPU16(Header->WindowSize) <<
                     TcpSocket->SendWindowScale;

        RtlDebugPrint("] \n    Seq=%d Ack=%d Win=%d Len=%d\n",
                      RelativeSequenceNumber,
                      RelativeAcknowledgeNumber,
                      WindowSize,
                      Length - HeaderLength);
    }

    NetpTcpProcessPacket(TcpSocket, ReceiveContext, Header);
    KeReleaseQueuedLock(TcpSocket->Lock);

    //
    // Release the reference on the socket added by the find socket call.
    //

    IoSocketReleaseReference(&(Socket->KernelSocket));
    return;
}

KSTATUS
NetpTcpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called for a particular socket to process a received packet
    that was sent to it.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    Status code.

--*/

{

    //
    // This packet processing routine is used by the network core for multicast
    // packets. Since TCP is a connection based stream protocol, multicast
    // packets should not be arriving here.
    //

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpTcpReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket on a
    particular protocol.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to receive data from.

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

    BOOL Break;
    UINTN BytesComplete;
    ULONG BytesThisRound;
    PLIST_ENTRY CurrentEntry;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG ExpectedSequence;
    ULONG FirstFlags;
    ULONG Flags;
    PIO_OBJECT_STATE IoState;
    BOOL LockHeld;
    ULONG MaxSegmentSize;
    ULONG OriginalFreeSize;
    ULONG ReturnedEvents;
    PTCP_RECEIVED_SEGMENT Segment;
    BOOL SegmentMissing;
    ULONG SegmentOffset;
    ULONG SegmentSize;
    UINTN Size;
    PULONG SocketFlags;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    TcpSocket = (PTCP_SOCKET)Socket;
    EndTime = 0;
    FirstFlags = 0;
    Flags = Parameters->SocketIoFlags;
    IoState = TcpSocket->NetSocket.KernelSocket.IoState;
    BytesComplete = 0;
    LockHeld = FALSE;
    Size = Parameters->Size;
    TimeCounterFrequency = 0;
    Timeout = Parameters->TimeoutInMilliseconds;

    //
    // The socket needs to be connected before receiving data.
    //

    if (TcpSocket->State < TcpStateEstablished) {
        Status = STATUS_NOT_CONNECTED;
        goto TcpReceiveEnd;
    }

    //
    // If the user called shutdown and is now trying to read, exit immediately.
    //

    if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
        Status = STATUS_END_OF_FILE;
        goto TcpReceiveEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto TcpReceiveEnd;
    }

    //
    // Potentially receive out of band data. TCP really wasn't designed for
    // this, but this implementation maintains parity with others out there
    // that do the same thing.
    //

    if ((Flags & SOCKET_IO_OUT_OF_BAND) != 0) {
        Status = NetpTcpReceiveOutOfBandData(FromKernelMode,
                                             TcpSocket,
                                             Parameters,
                                             IoBuffer);

        goto TcpReceiveEnd;
    }

    Parameters->SocketIoFlags = 0;

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    if (Timeout > TcpSocket->ReceiveTimeout) {
        Timeout = TcpSocket->ReceiveTimeout;
    }

    if ((Timeout != 0) && (Timeout != WAIT_TIME_INDEFINITE)) {
        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                                       Timeout * MICROSECONDS_PER_MILLISECOND);

        TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    //
    // Optimistically start out trying to get all the data requested.
    //

    Break = FALSE;
    if ((Flags & SOCKET_IO_PEEK) != 0) {
        Break = TRUE;
    }

    do {
        SegmentMissing = FALSE;
        if (Timeout == 0) {
            WaitTime = 0;

        } else if (Timeout != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto TcpReceiveEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            BytesComplete = 0;
            goto TcpReceiveEnd;
        }

        KeAcquireQueuedLock(TcpSocket->Lock);
        LockHeld = TRUE;
        if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
            Status = STATUS_END_OF_FILE;
            goto TcpReceiveEnd;
        }

        OriginalFreeSize = TcpSocket->ReceiveWindowFreeSize;
        CurrentEntry = TcpSocket->ReceivedSegmentList.Next;
        ExpectedSequence = TcpSocket->ReceiveUnreadSequence;
        SegmentOffset = TcpSocket->ReceiveSegmentOffset;
        while ((BytesComplete != Size) &&
               (CurrentEntry != &(TcpSocket->ReceivedSegmentList))) {

            Segment = LIST_VALUE(CurrentEntry,
                                 TCP_RECEIVED_SEGMENT,
                                 Header.ListEntry);

            CurrentEntry = CurrentEntry->Next;

            ASSERT(SegmentOffset < Segment->Length);

            //
            // If this segment is not the next segment, then a segment is
            // missing.
            //

            if (Segment->SequenceNumber != ExpectedSequence) {

                ASSERT(TCP_SEQUENCE_GREATER_THAN(Segment->SequenceNumber,
                                                 ExpectedSequence));

                SegmentMissing = TRUE;
                break;
            }

            //
            // Don't cross over urgent flag changes. The zero check is okay
            // because ACK should always be set.
            //

            if (FirstFlags == 0) {
                FirstFlags = Segment->Flags;

            } else if (((FirstFlags ^ Segment->Flags) &
                        TCP_RECEIVE_SEGMENT_FLAG_URGENT) != 0) {

                if (BytesComplete != 0) {
                    break;

                //
                // Sure the urgent flags changed, but the user didn't get
                // anything, so keep going. This happens if the user starts
                // reading at a zero-length segment.
                //

                } else {
                    FirstFlags = Segment->Flags;
                }
            }

            //
            // Determine how many bytes to copy from this segment.
            //

            SegmentSize = Segment->Length - SegmentOffset;
            BytesThisRound = SegmentSize;
            if (BytesThisRound > (Size - BytesComplete)) {
                BytesThisRound = Size - BytesComplete;
            }

            //
            // Copy the data from the segment into the buffer.
            //

            Status = MmCopyIoBufferData(IoBuffer,
                                        (PUCHAR)(Segment + 1) + SegmentOffset,
                                        BytesComplete,
                                        BytesThisRound,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto TcpReceiveEnd;
            }

            //
            // Unless the "wait for everything" flag was set, the user got
            // something, and can break out. The push flag is essentially
            // ignored.
            //

            if ((Flags & SOCKET_IO_WAIT_ALL) == 0) {
                Break = TRUE;
            }

            //
            // If the entire remainder of the segment was copied, then remove
            // and free that segment.
            //

            if (BytesThisRound == SegmentSize) {
                SegmentOffset = 0;

                //
                // The next thing to read better be just after this
                // segment. A failure here indicates bad receive buffering
                // (e.g. saving duplicate packets into the buffer).
                //

                ASSERT(ExpectedSequence == Segment->SequenceNumber);

                ExpectedSequence = Segment->NextSequence;
                if ((Flags & SOCKET_IO_PEEK) == 0) {
                    LIST_REMOVE(&(Segment->Header.ListEntry));

                    //
                    // The buffer is being freed, so up the receive window to
                    // allow the remote host to send more data.
                    //

                    TcpSocket->ReceiveWindowFreeSize += Segment->Length;
                    if (TcpSocket->ReceiveWindowFreeSize >
                        TcpSocket->ReceiveWindowTotalSize) {

                        TcpSocket->ReceiveWindowFreeSize =
                                             TcpSocket->ReceiveWindowTotalSize;
                    }

                    NetpTcpFreeSegment(TcpSocket, &(Segment->Header));
                }

            //
            // Only a portion of the segment was consumed, so just increase the
            // offset.
            //

            } else {

                ASSERT((BytesComplete + BytesThisRound) == Size);

                SegmentOffset += BytesThisRound;
            }

            BytesComplete += BytesThisRound;
        }

        //
        // Advance the current segment offset and sequence. Also send an ACK if
        // space was tight and enough for another segment became available.
        //

        if ((Flags & SOCKET_IO_PEEK) == 0) {
            TcpSocket->ReceiveSegmentOffset = SegmentOffset;

            ASSERT((ExpectedSequence == TcpSocket->ReceiveUnreadSequence) ||
                   (TCP_SEQUENCE_GREATER_THAN(ExpectedSequence,
                                           TcpSocket->ReceiveUnreadSequence)));

            TcpSocket->ReceiveUnreadSequence = ExpectedSequence;

            //
            // If there is enough free space for a new segment, consider
            // sending a window update. If the original free window size could
            // not hold a max packet then immediately alert the remote side
            // that there is space. Otherwise if there is space for only 1
            // packet, it is still expected to come in from the remote side,
            // but set the timer to send a window update in case the packet is
            // lost and so that the toggle will trigger an immediate ACK if it
            // does arrive.
            //

            MaxSegmentSize = TcpSocket->ReceiveMaxSegmentSize;
            if (TcpSocket->ReceiveWindowFreeSize >= MaxSegmentSize) {
                if (OriginalFreeSize < MaxSegmentSize) {
                    if ((TcpSocket->Flags &
                         TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) != 0) {

                        TcpSocket->Flags &= ~TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
                        NetpTcpTimerReleaseReference(TcpSocket);
                    }

                    NetpTcpSendControlPacket(TcpSocket, 0);

                } else if (OriginalFreeSize < (2 * MaxSegmentSize)) {
                    if ((TcpSocket->Flags &
                         TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0) {

                        TcpSocket->Flags |= TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
                        NetpTcpTimerAddReference(TcpSocket);
                    }
                }
            }
        }

        //
        // If the received segment list is now empty, or a segment is missing,
        // unsignal the receive ready event.
        //

        if ((SegmentMissing != FALSE) ||
            (LIST_EMPTY(&(TcpSocket->ReceivedSegmentList)))) {

            //
            // Watch out for the socket closing down.
            //

            if (TcpSocket->State != TcpStateEstablished) {

                ASSERT(TcpSocket->State > TcpStateEstablished);

                //
                // A reset connection fails as soon as it's known.
                //

                SocketFlags = &(TcpSocket->Flags);
                if ((*SocketFlags & TCP_SOCKET_FLAG_CONNECTION_RESET) != 0) {
                    Status = STATUS_CONNECTION_RESET;
                    BytesComplete = 0;

                //
                // Otherwise, the request was not at all satisfied, and no more
                // data is coming in.
                //

                } else {
                    Status = STATUS_END_OF_FILE;
                }

                goto TcpReceiveEnd;
            }

            IoSetIoObjectState(IoState, POLL_EVENT_IN, FALSE);
        }

        KeReleaseQueuedLock(TcpSocket->Lock);
        LockHeld = FALSE;

    } while ((Break == FALSE) && (BytesComplete != Size));

    Status = STATUS_SUCCESS;

TcpReceiveEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(TcpSocket->Lock);
    }

    //
    // If any bytes were read, then consider this a success.
    //

    if (BytesComplete != 0) {
        Status = STATUS_SUCCESS;
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

KSTATUS
NetpTcpGetSetInformation (
    PNET_SOCKET Socket,
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

    STATUS_NOT_HANDLED if the protocol does not override the default behavior
        for a basic socket option.

--*/

{

    SOCKET_BASIC_OPTION BasicOption;
    ULONG BooleanOption;
    ULONG Count;
    ULONGLONG DueTime;
    ULONG Index;
    ULONG KeepAliveOption;
    ULONG LeadingZeros;
    ULONG LingerMilliseconds;
    PSOCKET_LINGER LingerOption;
    SOCKET_LINGER LingerOptionBuffer;
    ULONG LingerSeconds;
    LONGLONG Milliseconds;
    ULONG SizeDelta;
    ULONG SizeOption;
    PSOCKET_TIME SocketTime;
    SOCKET_TIME SocketTimeBuffer;
    PVOID Source;
    KSTATUS Status;
    SOCKET_TCP_OPTION TcpOption;
    PTCP_SOCKET TcpSocket;
    PTCP_SOCKET_OPTION TcpSocketOption;
    PULONG TcpTimeout;
    ULONG WindowScale;
    ULONG WindowSize;

    TcpSocket = (PTCP_SOCKET)Socket;
    if ((InformationType != SocketInformationBasic) &&
        (InformationType != SocketInformationTcp)) {

        Status = STATUS_NOT_SUPPORTED;
        goto TcpGetSetInformationEnd;
    }

    //
    // Search to see if the socket option is supported by the TCP protocol.
    //

    Count = sizeof(NetTcpSocketOptions) / sizeof(NetTcpSocketOptions[0]);
    for (Index = 0; Index < Count; Index += 1) {
        TcpSocketOption = &(NetTcpSocketOptions[Index]);
        if ((TcpSocketOption->InformationType == InformationType) &&
            (TcpSocketOption->Option == Option)) {

            break;
        }
    }

    if (Index == Count) {
        if (InformationType == SocketInformationBasic) {
            Status = STATUS_NOT_HANDLED;

        } else {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        }

        goto TcpGetSetInformationEnd;
    }

    //
    // Handle failure cases common to all options.
    //

    if (Set != FALSE) {
        if (TcpSocketOption->SetAllowed == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            goto TcpGetSetInformationEnd;
        }

        if (*DataSize < TcpSocketOption->Size) {
            *DataSize = TcpSocketOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto TcpGetSetInformationEnd;
        }
    }

    //
    // Parse the socket option to actually get or set the TCP socket
    // information.
    //

    Source = NULL;
    Status = STATUS_SUCCESS;
    if (InformationType == SocketInformationBasic) {
        BasicOption = (SOCKET_BASIC_OPTION)Option;
        switch (BasicOption) {
        case SocketBasicOptionLinger:
            if (Set != FALSE) {
                LingerOption = (PSOCKET_LINGER)Data;
                LingerSeconds = LingerOption->LingerTimeout;
                if (LingerSeconds > SOCKET_OPTION_MAX_ULONG) {
                    LingerSeconds = SOCKET_OPTION_MAX_ULONG;
                }

                LingerMilliseconds = LingerSeconds * MILLISECONDS_PER_SECOND;
                if (LingerMilliseconds < LingerSeconds) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                KeAcquireQueuedLock(TcpSocket->Lock);
                TcpSocket->LingerTimeout = LingerMilliseconds;
                if (LingerOption->LingerEnabled != FALSE) {
                    TcpSocket->Flags |= TCP_SOCKET_FLAG_LINGER_ENABLED;

                } else {
                    TcpSocket->Flags &= ~TCP_SOCKET_FLAG_LINGER_ENABLED;
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                LingerOptionBuffer.LingerEnabled = FALSE;
                KeAcquireQueuedLock(TcpSocket->Lock);
                if ((TcpSocket->Flags & TCP_SOCKET_FLAG_LINGER_ENABLED) != 0) {
                    LingerOptionBuffer.LingerEnabled = TRUE;
                }

                LingerOptionBuffer.LingerTimeout = TcpSocket->LingerTimeout /
                                                   MILLISECONDS_PER_SECOND;

                KeReleaseQueuedLock(TcpSocket->Lock);
                Source = &LingerOptionBuffer;
            }

            break;

        case SocketBasicOptionSendBufferSize:
            if (Set != FALSE) {
                SizeOption = *((PULONG)Data);
                if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                    SizeOption = SOCKET_OPTION_MAX_ULONG;
                }

                KeAcquireQueuedLock(TcpSocket->Lock);

                //
                // Don't let the send buffer size get smaller than the max
                // packet size.
                //

                if (SizeOption < TcpSocket->SendMaxSegmentSize) {
                    SizeOption = TcpSocket->SendMaxSegmentSize;
                }

                //
                // If the send buffer is getting bigger, the difference needs
                // to be added as free space.
                //

                if (TcpSocket->SendBufferTotalSize < SizeOption) {
                    SizeDelta = SizeOption - TcpSocket->SendBufferTotalSize;
                    TcpSocket->SendBufferTotalSize = SizeOption;
                    TcpSocket->SendBufferFreeSize += SizeDelta;

                //
                // If the send buffer is shrinking, only decrease the free size
                // if it is bigger than the new total. The code that releases
                // buffer space makes sure the free size is below the total.
                //

                } else {
                    TcpSocket->SendBufferTotalSize = SizeOption;
                    if (TcpSocket->SendBufferFreeSize > SizeOption) {
                        TcpSocket->SendBufferFreeSize = SizeOption;
                    }
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                SizeOption = TcpSocket->SendBufferTotalSize;
                Source = &SizeOption;
            }

            break;

        case SocketBasicOptionSendMinimum:

            ASSERT(Set == FALSE);

            SizeOption = TCP_DEFAULT_SEND_MINIMUM;
            Source = &SizeOption;
            break;

        case SocketBasicOptionSendTimeout:
        case SocketBasicOptionReceiveTimeout:
            if (BasicOption == SocketBasicOptionSendTimeout) {
                TcpTimeout = &(TcpSocket->SendTimeout);

            } else {
                TcpTimeout = &(TcpSocket->ReceiveTimeout);
            }

            if (Set != FALSE) {
                SocketTime = (PSOCKET_TIME)Data;
                if (SocketTime->Seconds < 0) {
                    Status = STATUS_DOMAIN_ERROR;
                    break;
                }

                Milliseconds = SocketTime->Seconds * MILLISECONDS_PER_SECOND;
                if (Milliseconds < SocketTime->Seconds) {
                    Status = STATUS_DOMAIN_ERROR;
                    break;
                }

                Milliseconds += SocketTime->Microseconds /
                                MICROSECONDS_PER_MILLISECOND;

                if ((Milliseconds < 0) || (Milliseconds > MAX_LONG)) {
                    Status = STATUS_DOMAIN_ERROR;
                    break;
                }

                *TcpTimeout = (ULONG)(LONG)Milliseconds;

            } else {
                Source = &SocketTimeBuffer;
                if (*TcpTimeout == WAIT_TIME_INDEFINITE) {
                    SocketTimeBuffer.Seconds = 0;
                    SocketTimeBuffer.Microseconds = 0;

                } else {
                    SocketTimeBuffer.Seconds =
                                         *TcpTimeout / MILLISECONDS_PER_SECOND;

                    SocketTimeBuffer.Microseconds =
                                      (*TcpTimeout % MILLISECONDS_PER_SECOND) *
                                      MICROSECONDS_PER_MILLISECOND;
                }
            }

            break;

        case SocketBasicOptionReceiveBufferSize:
            if (Set != FALSE) {
                SizeOption = *((PULONG)Data);
                if ((SizeOption > TCP_MAXIMUM_WINDOW_SIZE) ||
                    (SizeOption < TCP_MINIMUM_WINDOW_SIZE)) {

                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                ASSERT(SizeOption <= SOCKET_OPTION_MAX_ULONG);

                KeAcquireQueuedLock(TcpSocket->Lock);

                //
                // If the socket is in the initialized or listening state, then
                // its receive window maybe changed along with the scale.
                //

                if ((TcpSocket->State == TcpStateInitialized) ||
                    (TcpSocket->State == TcpStateListening)) {

                    ASSERT(TcpSocket->ReceiveWindowTotalSize ==
                           TcpSocket->ReceiveWindowFreeSize);

                    ASSERT((TcpSocket->Flags &
                           TCP_SOCKET_FLAG_WINDOW_SCALING) != 0);

                    //
                    // If the upper word is not zero, count the leading zeros
                    // to figure out a good scale. Getting as close as possible
                    // to the requested window.
                    //

                    if ((SizeOption & ~TCP_WINDOW_MASK) != 0) {
                        LeadingZeros = RtlCountLeadingZeros32(SizeOption);
                        WindowScale = (sizeof(USHORT) * BITS_PER_BYTE) -
                                      LeadingZeros;

                        TcpSocket->ReceiveWindowScale = WindowScale;
                        WindowSize = (SizeOption >> WindowScale);

                        ASSERT(WindowSize != 0);

                        WindowSize <<= WindowScale;
                        TcpSocket->ReceiveWindowTotalSize = WindowSize;
                        TcpSocket->ReceiveWindowFreeSize = WindowSize;

                        ASSERT(WindowScale <= TCP_MAXIMUM_WINDOW_SCALE);

                    //
                    // Otherwise no scaling is necessary.
                    //

                    } else {
                        TcpSocket->ReceiveWindowScale = 0;
                        TcpSocket->ReceiveWindowTotalSize = SizeOption;
                        TcpSocket->ReceiveWindowFreeSize = SizeOption;
                    }

                //
                // Otherwise updates to the window size are bounded by the
                // scale that was communicated to the remote side when the SYN
                // was sent. If the requested size is too large or too small
                // for the current scale, return failure.
                //

                } else {
                    WindowSize = SizeOption >> TcpSocket->ReceiveWindowScale;
                    if ((WindowSize == 0) ||
                        ((WindowSize & ~TCP_WINDOW_MASK) != 0)) {

                        Status = STATUS_INVALID_PARAMETER;

                    } else {

                        //
                        // If the receive window is getting bigger, the
                        // difference needs to be added as free space.
                        //

                        if (TcpSocket->ReceiveWindowTotalSize < SizeOption) {
                            SizeDelta = SizeOption -
                                        TcpSocket->ReceiveWindowTotalSize;

                            TcpSocket->ReceiveWindowTotalSize = SizeOption;
                            TcpSocket->ReceiveWindowFreeSize += SizeDelta;

                        //
                        // If the receive window is shrinking, only decrease
                        // the free size if it is bigger than the new total.
                        // The receive code makes sure that buffer space
                        // reclaim doesn't set the free space above the total.
                        //

                        } else {
                            TcpSocket->ReceiveWindowTotalSize = SizeOption;
                            if (TcpSocket->ReceiveWindowFreeSize > SizeOption) {
                                TcpSocket->ReceiveWindowFreeSize = SizeOption;
                            }
                        }
                    }
                }

                //
                // Make sure the receive minimum is up to date.
                //

                if (TcpSocket->ReceiveMinimum >
                    TcpSocket->ReceiveWindowTotalSize) {

                    TcpSocket->ReceiveMinimum =
                                             TcpSocket->ReceiveWindowTotalSize;
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                SizeOption = TcpSocket->ReceiveWindowTotalSize;
                Source = &SizeOption;
            }

            break;

        case SocketBasicOptionReceiveMinimum:
            if (Set != FALSE) {
                SizeOption = *((PULONG)Data);
                if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                    SizeOption = SOCKET_OPTION_MAX_ULONG;
                }

                KeAcquireQueuedLock(TcpSocket->Lock);
                if (SizeOption > TcpSocket->ReceiveWindowTotalSize) {
                    SizeOption = TcpSocket->ReceiveWindowTotalSize;
                }

                TcpSocket->ReceiveMinimum = SizeOption;
                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                SizeOption = TcpSocket->ReceiveMinimum;
                Source = &SizeOption;
            }

            break;

        case SocketBasicOptionAcceptConnections:

            ASSERT(Set == FALSE);

            Source = &BooleanOption;
            BooleanOption = FALSE;
            if (TcpSocket->State == TcpStateListening) {
                BooleanOption = TRUE;
            }

            break;

        case SocketBasicOptionKeepAlive:
            if (Set != FALSE) {
                BooleanOption = *((PULONG)Data);
                KeAcquireQueuedLock(TcpSocket->Lock);
                if (BooleanOption != FALSE) {

                    //
                    // If keep alive is being enabled and the socket is in a
                    // keep alive state, then arm the timer.
                    //

                    if ((TcpSocket->Flags & TCP_SOCKET_FLAG_KEEP_ALIVE) == 0) {
                        if (TCP_IS_KEEP_ALIVE_STATE(TcpSocket->State)) {
                            DueTime = KeGetRecentTimeCounter();
                            DueTime += KeConvertMicrosecondsToTimeTicks(
                                                  TcpSocket->KeepAliveTimeout *
                                                  MICROSECONDS_PER_SECOND);

                            TcpSocket->KeepAliveTime = DueTime;
                            TcpSocket->KeepAliveProbeCount = 0;
                            NetpTcpArmKeepAliveTimer(DueTime);
                        }

                        TcpSocket->Flags |= TCP_SOCKET_FLAG_KEEP_ALIVE;
                    }

                } else {
                    TcpSocket->Flags &= ~TCP_SOCKET_FLAG_KEEP_ALIVE;
                    TcpSocket->KeepAliveTime = 0;
                    TcpSocket->KeepAliveProbeCount = 0;
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                Source = &BooleanOption;
                BooleanOption = FALSE;
                if ((TcpSocket->Flags & TCP_SOCKET_FLAG_KEEP_ALIVE) != 0) {
                    BooleanOption = TRUE;
                }
            }

            break;

        case SocketBasicOptionInlineOutOfBand:
            if (Set != FALSE) {
                BooleanOption = *((PULONG)Data);
                KeAcquireQueuedLock(TcpSocket->Lock);
                if (BooleanOption != FALSE) {
                    TcpSocket->Flags |= TCP_SOCKET_FLAG_URGENT_INLINE;

                } else {
                    TcpSocket->Flags &= ~TCP_SOCKET_FLAG_URGENT_INLINE;
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                Source = &BooleanOption;
                BooleanOption = FALSE;
                if ((TcpSocket->Flags & TCP_SOCKET_FLAG_URGENT_INLINE) != 0) {
                    BooleanOption = TRUE;
                }
            }

            break;

        default:

            ASSERT(FALSE);

            Status = STATUS_NOT_HANDLED;
            break;
        }

    } else {

        ASSERT(InformationType == SocketInformationTcp);

        TcpOption = (SOCKET_TCP_OPTION)Option;
        switch (TcpOption) {
        case SocketTcpOptionNoDelay:
            if (Set != FALSE) {
                BooleanOption = *((PULONG)Data);
                KeAcquireQueuedLock(TcpSocket->Lock);
                TcpSocket->Flags &= ~TCP_SOCKET_FLAG_NO_DELAY;
                if (BooleanOption != FALSE) {
                    TcpSocket->Flags |= TCP_SOCKET_FLAG_NO_DELAY;
                }

                KeReleaseQueuedLock(TcpSocket->Lock);

            } else {
                Source = &BooleanOption;
                BooleanOption = FALSE;
                if ((TcpSocket->Flags & TCP_SOCKET_FLAG_NO_DELAY) != 0) {
                    BooleanOption = TRUE;
                }
            }

            break;

        case SocketTcpOptionKeepAliveTimeout:
            if (Set != FALSE) {
                KeepAliveOption = *((PULONG)Data);
                if (KeepAliveOption > SOCKET_OPTION_MAX_ULONG) {
                    KeepAliveOption = SOCKET_OPTION_MAX_ULONG;
                }

                TcpSocket->KeepAliveTimeout = KeepAliveOption;

            } else {
                Source = &KeepAliveOption;
                KeepAliveOption = TcpSocket->KeepAliveTimeout;
            }

            break;

        case SocketTcpOptionKeepAlivePeriod:
            if (Set != FALSE) {
                KeepAliveOption = *((PULONG)Data);
                if (KeepAliveOption > SOCKET_OPTION_MAX_ULONG) {
                    KeepAliveOption = SOCKET_OPTION_MAX_ULONG;
                }

                TcpSocket->KeepAlivePeriod = KeepAliveOption;

            } else {
                Source = &KeepAliveOption;
                KeepAliveOption = TcpSocket->KeepAlivePeriod;
            }

            break;

        case SocketTcpOptionKeepAliveProbeLimit:
            if (Set != FALSE) {
                KeepAliveOption = *((PULONG)Data);
                if (KeepAliveOption > SOCKET_OPTION_MAX_ULONG) {
                    KeepAliveOption = SOCKET_OPTION_MAX_ULONG;
                }

                TcpSocket->KeepAliveProbeLimit = KeepAliveOption;

            } else {
                Source = &KeepAliveOption;
                KeepAliveOption = TcpSocket->KeepAliveProbeLimit;
            }

            break;

        default:

            ASSERT(FALSE);

            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto TcpGetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and only
    // return the required size on set requests.
    //

    if (*DataSize > TcpSocketOption->Size) {
        *DataSize = TcpSocketOption->Size;
    }

    //
    // For get requests, copy the gathered information to the supplied data
    // buffer.
    //

    if (Set == FALSE) {

        ASSERT(Source != NULL);

        RtlCopyMemory(Data, Source, *DataSize);

        //
        // If the copy truncated the data, report that the given buffer was too
        // small. The caller can choose to ignore this if the truncated data is
        // enough.
        //

        if (*DataSize < TcpSocketOption->Size) {
            *DataSize = TcpSocketOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto TcpGetSetInformationEnd;
        }
    }

TcpGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpTcpUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests destined for a socket.

Arguments:

    Socket - Supplies a pointer to the socket.

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

    PVOID Buffer;
    UINTN BufferSize;
    ULONG Integer;
    PTCP_RECEIVED_SEGMENT Segment;
    KSTATUS Status;
    PTCP_SOCKET TcpSocket;

    Status = STATUS_SUCCESS;
    TcpSocket = (PTCP_SOCKET)Socket;
    KeAcquireQueuedLock(TcpSocket->Lock);
    switch (CodeNumber) {

    //
    // Determine if the next segment has the urgent flag set.
    //

    case TcpUserControlAtUrgentMark:
        Integer = FALSE;
        if (LIST_EMPTY(&(TcpSocket->ReceivedSegmentList)) == FALSE) {
            Segment = LIST_VALUE(TcpSocket->ReceivedSegmentList.Next,
                                 TCP_RECEIVED_SEGMENT,
                                 Header.ListEntry);

            if ((Segment->Flags & TCP_RECEIVE_SEGMENT_FLAG_URGENT) != 0) {

                //
                // TCP urgent packets are only 1 byte in length. If they were
                // more, then this code would need to check the offset to see
                // if the next receive is at the beginning of this segment.
                //

                ASSERT(Segment->Length <= 1);

                Integer = TRUE;
            }
        }

        Buffer = &Integer;
        BufferSize = sizeof(ULONG);
        break;

    case TcpUserControlGetInputQueueSize:
        if (TcpSocket->State == TcpStateListening) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Integer = TcpSocket->ReceiveWindowTotalSize -
                  TcpSocket->ReceiveWindowFreeSize;

        Buffer = &Integer;
        BufferSize = sizeof(ULONG);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    KeReleaseQueuedLock(TcpSocket->Lock);

    //
    // Copy the gathered data on success.
    //

    if (KSUCCESS(Status)) {
        if (ContextBufferSize < BufferSize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto UserControlEnd;
        }

        if (FromKernelMode != FALSE) {
            RtlCopyMemory(ContextBuffer, Buffer, BufferSize);

        } else {
            Status = MmCopyToUserMode(ContextBuffer, Buffer, BufferSize);
            if (!KSUCCESS(Status)) {
                goto UserControlEnd;
            }
        }
    }

UserControlEnd:
    return Status;
}

VOID
NetpTcpPrintSocketEndpoints (
    PTCP_SOCKET Socket,
    BOOL Transmit
    )

/*++

Routine Description:

    This routine prints the socket local and remote addresses.

Arguments:

    Socket - Supplies a pointer to the socket whose addresses should be printed.

    Transmit - Supplies a boolean indicating if the print is requested for a
        transmit (TRUE) or receive (FALSE).

Return Value:

    None.

--*/

{

    ULONGLONG Milliseconds;

    Milliseconds = (HlQueryTimeCounter() * MILLISECONDS_PER_SECOND) /
                   HlQueryTimeCounterFrequency();

    RtlDebugPrint("TCP %I64dms: ", Milliseconds);
    if (Transmit != FALSE) {
        if (NetTcpDebugPrintLocalAddress != FALSE) {
            NetDebugPrintAddress(&(Socket->NetSocket.LocalSendAddress));
            RtlDebugPrint(" to ");
        }
    }

    NetDebugPrintAddress(&(Socket->NetSocket.RemoteAddress));
    if (Transmit == FALSE) {
        if (NetTcpDebugPrintLocalAddress != FALSE) {
            RtlDebugPrint(" to ");
            NetDebugPrintAddress(&(Socket->NetSocket.LocalReceiveAddress));
        }
    }

    return;
}

VOID
NetpTcpRetransmit (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine immediately transmits the oldest pending packet. This routine
    assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket whose segment should be
        retransmitted.

Return Value:

    None.

--*/

{

    PTCP_SEND_SEGMENT Segment;

    if (LIST_EMPTY(&(Socket->OutgoingSegmentList)) != FALSE) {
        return;
    }

    Segment = LIST_VALUE(Socket->OutgoingSegmentList.Next,
                         TCP_SEND_SEGMENT,
                         Header.ListEntry);

    NetpTcpSendSegment(Socket, Segment);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpTcpWorkerThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements periodic maintenance work required by TCP.

Arguments:

    Parameter - Supplies an unused parameter.

Return Value:

    None. Unless TCP is altogether shut down, this thread never exits.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTCP_SOCKET CurrentSocket;
    ULONGLONG CurrentTime;
    PULONG Flags;
    PIO_OBJECT_STATE IoState;
    ULONGLONG KeepAliveTime;
    BOOL KeepAliveTimeout;
    PSOCKET KernelSocket;
    BOOL LinkUp;
    ULONGLONG RecentTime;
    PVOID SignalingObject;
    PVOID WaitObjectArray[2];
    BOOL WithAcknowledge;

    ASSERT(2 < BUILTIN_WAIT_BLOCK_ENTRY_COUNT);

    KeepAliveTime = MAX_ULONGLONG;
    WaitObjectArray[0] = NetTcpTimer;
    WaitObjectArray[1] = NetTcpKeepAliveTimer;
    while ((NetTcpTimer != NULL) && (NetTcpKeepAliveTimer != NULL)) {

        //
        // Sleep until the periodic or keep alive timer fires again.
        //

        ObWaitOnObjects(WaitObjectArray,
                        2,
                        0,
                        WAIT_TIME_INDEFINITE,
                        NULL,
                        &SignalingObject);

        //
        // If the keep alive timer signaled, then check the keep alive states.
        //

        KeepAliveTimeout = FALSE;
        if (SignalingObject == NetTcpKeepAliveTimer) {
            KeepAliveTimeout = TRUE;
            KeepAliveTime = MAX_ULONGLONG;
            KeSignalTimer(NetTcpKeepAliveTimer, SignalOptionUnsignal);

        //
        // If the TCP timer signaled, determine whether or not work needs to be
        // done. Start by setting the timer state to "not queued" as it just
        // expired. Next, check the timer reference count. If there are no
        // references, then no sockets need processing. If there are
        // references, then at least one socket is hanging around. Attempt to
        // queue the timer for the next round of work.
        //
        // Sockets may be racing to increment the timer reference count from 0
        // to 1 and queue the timer. The timer state variable synchronizes
        // this. If the increment comes before the setting of the state to
        // "not queued", the socket will see that the timer is already queued
        // and the worker will see the timer reference and requeue the timer.
        // If the increment comes after the setting of the state to "not
        // queued" but before the worker checks the reference count, then the
        // worker and socket will race to requeue the timer by performing
        // atomic compare-exchanges on the timer state. If the increment comes
        // after the setting of the state to "not queued" and after the worker
        // sees the reference as 0, then the socket is free and clear to win
        // the compare-exchange and queue the timer.
        //

        } else {

            ASSERT(SignalingObject == NetTcpTimer);

            KeSignalTimer(NetTcpTimer, SignalOptionUnsignal);
            RtlAtomicExchange32(&NetTcpTimerState, TcpTimerNotQueued);
            if (NetTcpTimerReferenceCount == 0) {
                continue;
            }

            NetpTcpQueueTcpTimer();
        }

        //
        // Loop through every socket.
        //

        CurrentTime = 0;
        KeAcquireQueuedLock(NetTcpSocketListLock);
        CurrentEntry = NetTcpSocketList.Next;
        while (CurrentEntry != &NetTcpSocketList) {
            CurrentSocket = LIST_VALUE(CurrentEntry, TCP_SOCKET, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            KernelSocket = &(CurrentSocket->NetSocket.KernelSocket);

            ASSERT(KernelSocket->ReferenceCount >= 1);

            //
            // Check the link state for all bound sockets. If the link is down,
            // then close the socket.
            //

            if (CurrentSocket->NetSocket.Link != NULL) {
                NetGetLinkState(CurrentSocket->NetSocket.Link, &LinkUp, NULL);
                if (LinkUp == FALSE) {
                    IoSocketAddReference(KernelSocket);
                    KeAcquireQueuedLock(CurrentSocket->Lock);
                    NetpTcpCloseOutSocket(CurrentSocket, TRUE);
                    KeReleaseQueuedLock(CurrentSocket->Lock);
                    IoSocketReleaseReference(KernelSocket);
                    continue;
                }
            }

            //
            // If the socket is not waiting on anything, move on. Manipulation
            // of any of these criteria require manipulating the TCP timer
            // reference count.
            //

            Flags = &(CurrentSocket->Flags);
            if ((LIST_EMPTY(&(CurrentSocket->OutgoingSegmentList))) &&
                ((*Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0) &&
                (((*Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) == 0) ||
                 ((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) != 0)) &&
                (CurrentSocket->State != TcpStateTimeWait) &&
                (TCP_IS_SYN_RETRY_STATE(CurrentSocket->State) == FALSE) &&
                (((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) != 0) ||
                 (TCP_IS_FIN_RETRY_STATE(CurrentSocket->State) == FALSE)) &&
                ((KeepAliveTimeout == FALSE) ||
                 ((*Flags & TCP_SOCKET_FLAG_KEEP_ALIVE) == 0) ||
                 (TCP_IS_KEEP_ALIVE_STATE(CurrentSocket->State) == FALSE))) {

                continue;
            }

            IoSocketAddReference(KernelSocket);
            KeAcquireQueuedLock(CurrentSocket->Lock);
            NetpTcpSendPendingSegments(CurrentSocket, &CurrentTime);

            //
            // If the media was disconnected, close out the socket and move on.
            //

            IoState = CurrentSocket->NetSocket.KernelSocket.IoState;
            if ((IoState->Events & POLL_EVENT_DISCONNECTED) != 0) {
                NetpTcpCloseOutSocket(CurrentSocket, TRUE);
                KeReleaseQueuedLock(CurrentSocket->Lock);
                IoSocketReleaseReference(KernelSocket);
                continue;
            }

            //
            // If the socket is in the time wait state and the timer has
            // expired then close out the socket.
            //

            if (CurrentSocket->State == TcpStateTimeWait) {
                if (KeGetRecentTimeCounter() > CurrentSocket->TimeoutEnd) {

                    ASSERT(CurrentSocket->TimeoutEnd != 0);

                    if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                        RtlDebugPrint("TCP: Time-wait finished.\n");
                    }

                    NetpTcpCloseOutSocket(CurrentSocket, TRUE);
                }

            //
            // If the socket is waiting for a SYN to be ACK'd, then resend the
            // SYN if the retry has been reached. If the timeout has been
            // reached then send a reset and signal the error event to wake up
            // connect or accept.
            //

            } else if (TCP_IS_SYN_RETRY_STATE(CurrentSocket->State)) {
                RecentTime = KeGetRecentTimeCounter();
                if (RecentTime > CurrentSocket->TimeoutEnd) {
                    NetpTcpSendControlPacket(CurrentSocket,
                                             TCP_HEADER_FLAG_RESET);

                    NET_SOCKET_SET_LAST_ERROR(&(CurrentSocket->NetSocket),
                                              STATUS_TIMEOUT);

                    IoSetIoObjectState(IoState, POLL_EVENT_ERROR, TRUE);
                    NetpTcpSetState(CurrentSocket, TcpStateInitialized);

                } else if (RecentTime >= CurrentSocket->RetryTime) {
                    WithAcknowledge = FALSE;
                    if (CurrentSocket->State == TcpStateSynReceived) {
                        WithAcknowledge = TRUE;
                    }

                    NetpTcpSendSyn(CurrentSocket, WithAcknowledge);
                    TCP_UPDATE_RETRY_TIME(CurrentSocket);
                }

            //
            // If the socket is waiting for a FIN to be ACK'd, then resend the
            // FIN if the retry time has been reached. If the timeout has
            // expired, send a reset and close the socket.
            //

            } else if (((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) &&
                       TCP_IS_FIN_RETRY_STATE(CurrentSocket->State)) {

                RecentTime = KeGetRecentTimeCounter();
                if (RecentTime > CurrentSocket->TimeoutEnd) {
                    NetpTcpSendControlPacket(CurrentSocket,
                                             TCP_HEADER_FLAG_RESET);

                    *Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                    NET_SOCKET_SET_LAST_ERROR(&(CurrentSocket->NetSocket),
                                              STATUS_DESTINATION_UNREACHABLE);

                    IoSetIoObjectState(IoState, POLL_EVENT_ERROR, TRUE);
                    NetpTcpCloseOutSocket(CurrentSocket, TRUE);

                } else if (RecentTime >= CurrentSocket->RetryTime) {
                    NetpTcpSendControlPacket(CurrentSocket,
                                             TCP_HEADER_FLAG_FIN);

                    TCP_UPDATE_RETRY_TIME(CurrentSocket);
                }

            //
            // If the socket is in the keep alive state and the keep alive
            // timer woke up the thread, then check on that timeout.
            //

            } else if ((KeepAliveTimeout != FALSE) &&
                       ((*Flags & TCP_SOCKET_FLAG_KEEP_ALIVE) != 0) &&
                       TCP_IS_KEEP_ALIVE_STATE(CurrentSocket->State)) {

                //
                // If too many probes have been sent without a response then
                // this socket is dead. Be nice, send a reset and then close it
                // out.
                //

                if (CurrentSocket->KeepAliveProbeCount >
                    CurrentSocket->KeepAliveProbeLimit) {

                    NetpTcpSendControlPacket(CurrentSocket,
                                             TCP_HEADER_FLAG_RESET);

                    *Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                    NET_SOCKET_SET_LAST_ERROR(&(CurrentSocket->NetSocket),
                                              STATUS_DESTINATION_UNREACHABLE);

                    IoSetIoObjectState(IoState, POLL_EVENT_ERROR, TRUE);
                    NetpTcpCloseOutSocket(CurrentSocket, TRUE);

                //
                // Otherwise, if the keep alive time has been reached, then
                // send another ping and then re-arm the keep alive time.
                //

                } else {
                    RecentTime = KeGetRecentTimeCounter();
                    if (RecentTime >= CurrentSocket->KeepAliveTime) {
                        NetpTcpSendControlPacket(CurrentSocket,
                                                 TCP_HEADER_FLAG_KEEP_ALIVE);

                        CurrentSocket->KeepAliveProbeCount += 1;
                        CurrentSocket->KeepAliveTime = RecentTime;
                        CurrentSocket->KeepAliveTime +=
                                               CurrentSocket->KeepAlivePeriod *
                                               HlQueryTimeCounterFrequency();
                    }

                    if (CurrentSocket->KeepAliveTime < KeepAliveTime) {
                        KeepAliveTime = CurrentSocket->KeepAliveTime;
                    }
                }
            }

            //
            // If an acknowledge needs to be sent and it wasn't already sent
            // above, then send just an acknowledge along.
            //

            if ((*Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) != 0) {
                *Flags &= ~TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
                NetpTcpTimerReleaseReference(CurrentSocket);
                NetpTcpSendControlPacket(CurrentSocket, 0);
            }

            KeReleaseQueuedLock(CurrentSocket->Lock);
            IoSocketReleaseReference(KernelSocket);
        }

        KeReleaseQueuedLock(NetTcpSocketListLock);

        //
        // If the keep alive timer needs to be re-armed, then do so with the
        // next lowest time.
        //

        if ((KeepAliveTimeout != FALSE) && (KeepAliveTime != MAX_ULONGLONG)) {
            NetpTcpArmKeepAliveTimer(KeepAliveTime);
        }
    }

    return;
}

VOID
NetpTcpProcessPacket (
    PTCP_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    )

/*++

Routine Description:

    This routine is called to process a valid received packet. This routine
    assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the TCP socket.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

    Header - Supplies a pointer to the TCP header.

Return Value:

    None.

--*/

{

    ULONG AcknowledgeNumber;
    ULONGLONG DueTime;
    PIO_OBJECT_STATE IoState;
    PNET_PACKET_BUFFER Packet;
    ULONG RemoteFinalSequence;
    ULONG RemoteSequence;
    ULONG ResetFlags;
    ULONG ResetSequenceNumber;
    BOOL SegmentAcceptable;
    PVOID SegmentData;
    ULONG SegmentLength;
    KSTATUS Status;
    BOOL SynHandled;

    ASSERT(Socket->NetSocket.KernelSocket.ReferenceCount >= 1);

    Packet = ReceiveContext->Packet;
    IoState = Socket->NetSocket.KernelSocket.IoState;
    SynHandled = FALSE;

    //
    // The socket might have been found during a connect operation that
    // subsequently timed out and put the state back to reset. In this case
    // the socket is now locally bound. Drop the packet since there's no
    // remote address set up and the connect operation was given up on.
    //

    if (Socket->State == TcpStateInitialized) {
        return;
    }

    RemoteSequence = NETWORK_TO_CPU32(Header->SequenceNumber);
    AcknowledgeNumber = NETWORK_TO_CPU32(Header->AcknowledgmentNumber);

    //
    // If the socket is closed, then anything other than a reset generates a
    // reset packet. For the reset, use the acknowledge number supplied by the
    // incoming packet if the flag is set.
    //

    if (Socket->State == TcpStateClosed) {
        if ((Header->Flags & TCP_HEADER_FLAG_RESET) == 0) {

            //
            // Always send a reset.
            //

            ResetFlags = TCP_HEADER_FLAG_RESET;

            //
            // If an ACK was received, the acknoledgement number is used as the
            // sequence number and no ACK is sent.
            //

            if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
                ResetFlags |= TCP_HEADER_FLAG_ACKNOWLEDGE;
                ResetSequenceNumber = AcknowledgeNumber;

            //
            // Otherwise the sequence number is zero and an ACK is sent with
            // the sender's sequence number plus length as the acknowledgement
            // number.
            //

            } else {
                ResetSequenceNumber = 0;
                SegmentLength = Packet->FooterOffset - Packet->DataOffset;

                //
                // Remember to count SYNs and FINs as part of the data length.
                //

                if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
                    SegmentLength += 1;
                }

                if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {
                    SegmentLength += 1;
                }

                Socket->ReceiveNextSequence = RemoteSequence + SegmentLength;
            }

            Socket->SendUnacknowledgedSequence = ResetSequenceNumber;
            NetpTcpSendControlPacket(Socket, ResetFlags);
        }

        return;
    }

    //
    // The socket should only be inactive in the closed or initialized states.
    // If it's in any other state, then there is likely a bug in the state
    // machine. Deactivation should coincide with destroying a socket's list of
    // received packets. As it would be bad to add a new packet to that list,
    // assert that the socket is active.
    //

    ASSERT((Socket->NetSocket.Flags & NET_SOCKET_FLAG_ACTIVE) != 0);

    //
    // Perform special handling for a listening socket.
    //

    if (Socket->State == TcpStateListening) {

        //
        // Incoming resets should be ignored, just return.
        //

        if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
            return;
        }

        //
        // It's too early for any acknowledgements, send a reset if one is
        // found.
        //

        if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
            ResetSequenceNumber = AcknowledgeNumber;
            ResetFlags = TCP_HEADER_FLAG_RESET | TCP_HEADER_FLAG_ACKNOWLEDGE;
            Socket->SendUnacknowledgedSequence = ResetSequenceNumber;
            NetpTcpSendControlPacket(Socket, ResetFlags);
            return;
        }

        //
        // Check for a SYN, someone wants to connect!
        //

        if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
            NetpTcpHandleIncomingConnection(Socket, ReceiveContext, Header);
        }

        return;

    //
    // Perform special handling for the Syn-sent state.
    //

    } else if (Socket->State == TcpStateSynSent) {
        if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {

            //
            // Check the acknowledge number, it had better match the initial
            // sequence number. If it doesn't, send a reset (unless this packet
            // already is a reset).
            //

            if (AcknowledgeNumber != Socket->SendNextNetworkSequence) {
                if ((Header->Flags & TCP_HEADER_FLAG_RESET) == 0) {
                    ResetSequenceNumber = AcknowledgeNumber;
                    ResetFlags = TCP_HEADER_FLAG_RESET |
                                 TCP_HEADER_FLAG_ACKNOWLEDGE;

                    Socket->SendUnacknowledgedSequence = ResetSequenceNumber;
                    NetpTcpSendControlPacket(Socket, ResetFlags);
                    Socket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                    NET_SOCKET_SET_LAST_ERROR(&(Socket->NetSocket),
                                              STATUS_CONNECTION_RESET);

                    NetpTcpCloseOutSocket(Socket, FALSE);
                }

                return;
            }

            //
            // Update the unacknowledged sequence number directly because an
            // acknowledge may be sent directly below. Because this was done,
            // the window update also needs to be done explicitly.
            //

            Socket->SendUnacknowledgedSequence = AcknowledgeNumber;
            Socket->SendWindowSize = NETWORK_TO_CPU16(Header->WindowSize) <<
                                     Socket->SendWindowScale;

            Socket->SendWindowUpdateSequence = RemoteSequence;
            Socket->SendWindowUpdateAcknowledge = AcknowledgeNumber;
        }

        //
        // In the Syn-sent state, a reset is only valid if an ACK is present
        // and it acknowledges the the sent SYN. Abort the connection if this
        // is the case. Otherwise drop the packet.
        //

        if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
            if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {

                ASSERT(AcknowledgeNumber == Socket->SendNextNetworkSequence);

                Socket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
                NET_SOCKET_SET_LAST_ERROR(&(Socket->NetSocket),
                                          STATUS_CONNECTION_RESET);

                NetpTcpCloseOutSocket(Socket, FALSE);
            }

            return;

        //
        // The ACK bit is either not there or is valid. Check for the SYN bit.
        // Initialize the remote sequence number variables if so.
        //

        } else if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
            Socket->ReceiveInitialSequence = RemoteSequence;
            Socket->ReceiveNextSequence = RemoteSequence + 1;
            Socket->ReceiveUnreadSequence = Socket->ReceiveNextSequence;

            //
            // Process the options to get the max segment size and window scale
            // that likely came with the SYN.
            //

            NetpTcpProcessPacketOptions(Socket, Header, Packet);

            //
            // If the local unacknowledged number is not equal to the initial
            // sequence, then a SYN was sent and acknowledged. Move directly to
            // the established state in this case and send an ACK. Send an ACK
            // directly to expedite this critical phase (at the expense of not
            // coalescing this ACK with pending data).
            //

            if (Socket->SendUnacknowledgedSequence !=
                Socket->SendInitialSequence) {

                NetpTcpSetState(Socket, TcpStateEstablished);
                NetpTcpSendControlPacket(Socket, 0);

            //
            // The remote host isn't ACKing the SYN, it just happened to send
            // its own at the same time. Send a SYN+ACK and move to the syn-
            // received state.
            //

            } else {
                NetpTcpSetState(Socket, TcpStateSynReceived);
                return;
            }

            SynHandled = TRUE;

        //
        // If neither the FIN nor RESET flags were set, drop the packet.
        //

        } else {
            return;
        }
    }

    //
    // Perform general processing for all states. Check to see if the sequence
    // number is acceptable.
    //

    SegmentLength = Packet->FooterOffset - Packet->DataOffset;
    SegmentData = Packet->Buffer + Packet->DataOffset;
    SegmentAcceptable = NetpTcpIsReceiveSegmentAcceptable(Socket,
                                                          RemoteSequence,
                                                          SegmentLength);

    //
    // If the segment is not acceptable at all, send an ACK, unless the reset
    // bit is set, in which case the packet is dropped.
    //

    if ((SegmentAcceptable == FALSE) && (SynHandled == FALSE)) {
        if ((Header->Flags & TCP_HEADER_FLAG_RESET) == 0) {
            if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0) {
                Socket->Flags |= TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
                NetpTcpTimerAddReference(Socket);
            }
        }

        return;
    }

    //
    // Next up, check the reset bit. If it is set, close the connection. The
    // exception in the TCP specification is if the socket is in the
    // Syn-received state and was previously in the Listening state. If that's
    // the case, then the socket should return the listening state. Note,
    // however, that incoming connections spawn a new socket. So, even in this
    // case, close out the socket; there is another socket still listening.
    //

    if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
        Socket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
        NET_SOCKET_SET_LAST_ERROR(&(Socket->NetSocket),
                                  STATUS_CONNECTION_RESET);

        NetpTcpCloseOutSocket(Socket, FALSE);
        return;
    }

    //
    // Check the SYN bit, which should really not be set at this point. If it
    // is, send a reset and close the connection. Note that if the SYN were not
    // in the valid window this code would not be reached, an ACK would be
    // sent instead above. Send a reset, flush all queues, and close out the
    // socket.
    //

    if (((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) && (SynHandled == FALSE)) {
        ResetFlags = TCP_HEADER_FLAG_RESET | TCP_HEADER_FLAG_ACKNOWLEDGE;
        NetpTcpSendControlPacket(Socket, ResetFlags);
        Socket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
        NET_SOCKET_SET_LAST_ERROR(&(Socket->NetSocket),
                                  STATUS_CONNECTION_RESET);

        NetpTcpCloseOutSocket(Socket, FALSE);
        return;
    }

    //
    // If the ACK bit is not set here, drop the packet and return.
    //

    if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) == 0) {
        return;
    }

    //
    // The ACK bit is definitely sent, process the acknowledge number. If this
    // fails, it is because the socket was closed via reset or the last ACK was
    // received. Exit. Don't touch the socket again.
    //

    Status = NetpTcpProcessAcknowledge(Socket,
                                       AcknowledgeNumber,
                                       RemoteSequence,
                                       SegmentLength,
                                       Header->WindowSize);

    if (!KSUCCESS(Status)) {

        ASSERT((Status == STATUS_CONNECTION_CLOSED) ||
               (Status == STATUS_CONNECTION_RESET));

        return;
    }

    //
    // If the acknowledge was not enough to bring the SYN-Received state
    // forward to the established state, then the connection was reset.
    //

    ASSERT(Socket->State != TcpStateSynReceived);

    //
    // At last, process any received data.
    //

    if ((Socket->State == TcpStateEstablished) ||
        (Socket->State == TcpStateFinWait1) ||
        (Socket->State == TcpStateFinWait2)) {

        NetpTcpProcessReceivedDataSegment(Socket,
                                          RemoteSequence,
                                          SegmentData,
                                          SegmentLength,
                                          Header);
    }

    //
    // Finally, take a look at the FIN bit.
    //

    if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {

        //
        // Don't process the FIN bit if the state is Closed, Listening, or
        // Syn-Sent, as the incoming sequence number cannot be validated in
        // these states.
        //

        if ((Socket->State == TcpStateClosed) ||
            (Socket->State == TcpStateListening) ||
            (Socket->State == TcpStateSynSent)) {

            return;
        }

        //
        // Calculate the final remote sequence.
        //

        RemoteFinalSequence = RemoteSequence + SegmentLength;

        //
        // The final sequence number has been received. Save it. Don't move the
        // state machine forward just yet; all the data needs to be received
        // first. Also, don't give the remote side a second chance at sending
        // the final sequence. If the remote is being a good citizen then it
        // should match the recorded final sequence.
        //

        if ((Socket->Flags & TCP_SOCKET_FLAG_RECEIVE_FINAL_SEQUENCE_VALID) ==
            0) {

            Socket->ReceiveFinalSequence = RemoteFinalSequence;
            Socket->Flags |= TCP_SOCKET_FLAG_RECEIVE_FINAL_SEQUENCE_VALID;

        } else if ((NetTcpDebugPrintSequenceNumbers != FALSE) &&
                   (Socket->ReceiveFinalSequence != RemoteFinalSequence)) {

            NetpTcpPrintSocketEndpoints(Socket, FALSE);
            RtlDebugPrint(" RX second FIN segment sequence %d. Expected %d.\n",
                          (RemoteFinalSequence -
                           Socket->ReceiveInitialSequence),
                          (Socket->ReceiveFinalSequence -
                           Socket->ReceiveInitialSequence));
        }
    }

    //
    // If a FIN has been received and all the data up to that FIN has been
    // received, then it's time to acknowledge the FIN and move the state
    // machine. This also handles the case of a second FIN.
    //

    if (((Socket->Flags & TCP_SOCKET_FLAG_RECEIVE_FINAL_SEQUENCE_VALID) != 0) &&
        (Socket->ReceiveNextSequence >= Socket->ReceiveFinalSequence)) {

        //
        // This is the first time the FIN has been seen. Step over it and
        // release anybody waiting to read as there's no more data coming in.
        //

        if (Socket->ReceiveNextSequence == Socket->ReceiveFinalSequence) {
            Socket->ReceiveNextSequence += 1;
            IoSetIoObjectState(IoState,
                               POLL_EVENT_IN | POLL_EVENT_IN_HIGH_PRIORITY,
                               TRUE);

            //
            // From the established state, enter the close-wait state. Note
            // that if the socket was in the SYN-Received state when the packet
            // arrived that it transitioned to the established state when the
            // ACK was processed.
            //

            if (Socket->State == TcpStateEstablished) {
                NetpTcpSetState(Socket, TcpStateCloseWait);

            //
            // If a FIN was received but the state is still Fin-Wait-1, then
            // the remote side started closing the connection but hasn't seen
            // the sent FIN yet. Move to the closing state.
            //

            } else if (Socket->State == TcpStateFinWait1) {
                NetpTcpSetState(Socket, TcpStateClosing);

            //
            // In the Fin-Wait-2 state, enter the time-wait state.
            //

            } else if (Socket->State == TcpStateFinWait2) {
                NetpTcpSetState(Socket, TcpStateTimeWait);

            //
            // Other states are not expected to received the first FIN.
            //

            } else {
                if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                    NetpTcpPrintSocketEndpoints(Socket, FALSE);
                    RtlDebugPrint(" RX unexpected FIN in state %d.\n",
                                  Socket->State);
                }

                return;
            }

        //
        // If the FIN has already been received and acknowledged and another
        // FIN has come in then process it. This ignores non-FIN packets.
        //

        } else if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {

            //
            // In the time-wait state, restart the timer.
            //

            if (Socket->State == TcpStateTimeWait) {
                TCP_SET_DEFAULT_TIMEOUT(Socket);

            //
            // Both the closing state and last acknowledge state are waiting on
            // an ACK for the sent FIN. If the other side sends a FIN (without
            // the correct ACK), just reset the FIN resend retry and timeout.
            // At least it is still responding.
            //

            } else if ((Socket->State == TcpStateClosing) ||
                       (Socket->State == TcpStateLastAcknowledge)) {

                Socket->RetryTime = 0;
                Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
                TCP_UPDATE_RETRY_TIME(Socket);
                TCP_SET_DEFAULT_TIMEOUT(Socket);

            //
            // The close-wait state could also get a second FIN, but there is
            // nothing to do other than ACK it.
            //

            } else if (Socket->State != TcpStateCloseWait) {
                if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                    NetpTcpPrintSocketEndpoints(Socket, FALSE);
                    RtlDebugPrint(" RX unexpected FIN in state %d.\n",
                                  Socket->State);
                }

                return;
            }

        //
        // Drop packets received after the FIN that do not contain a FIN.
        //

        } else {
            return;
        }

        //
        // Schedule an ACK to respond to the FIN.
        //

        if ((Socket->State & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0) {
            Socket->Flags |= TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
            NetpTcpTimerAddReference(Socket);
        }
    }

    //
    // If the socket is in a keep alive state then update the keep alive time
    // and arm the keep alive timer. The remote side is still alive!
    //

    if (((Socket->Flags & TCP_SOCKET_FLAG_KEEP_ALIVE) != 0) &&
        (TCP_IS_KEEP_ALIVE_STATE(Socket->State) != FALSE)) {

        DueTime = KeGetRecentTimeCounter();
        DueTime += KeConvertMicrosecondsToTimeTicks(Socket->KeepAliveTimeout *
                                                    MICROSECONDS_PER_SECOND);

        Socket->KeepAliveTime = DueTime;
        Socket->KeepAliveProbeCount = 0;
        NetpTcpArmKeepAliveTimer(DueTime);
    }

    return;
}

VOID
NetpTcpHandleUnconnectedPacket (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    )

/*++

Routine Description:

    This routine is called to handle an invalid received packet that is not
    part of any connection.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

    Header - Supplies a pointer to the TCP header.

Return Value:

    None.

--*/

{

    BOOL LockHeld;
    ULONGLONG Milliseconds;
    PIO_HANDLE NewIoHandle;
    PTCP_SOCKET NewTcpSocket;
    ULONG ResetAcknowledgeNumber;
    ULONG ResetFlags;
    ULONG ResetSequenceNumber;
    ULONG SegmentLength;
    KSTATUS Status;

    ASSERT(ReceiveContext->Link != NULL);

    LockHeld = FALSE;

    //
    // Notify the debugger of this rogue packet.
    //

    if (NetTcpDebugPrintAllPackets != FALSE) {
        Milliseconds = (HlQueryTimeCounter() * MILLISECONDS_PER_SECOND) /
                       HlQueryTimeCounterFrequency();

        RtlDebugPrint("TCP %I64dms: ", Milliseconds);
        NetDebugPrintAddress(ReceiveContext->Source);
        if (NetTcpDebugPrintLocalAddress != FALSE) {
            RtlDebugPrint(" to ");
            NetDebugPrintAddress(ReceiveContext->Destination);
        }
    }

    //
    // Do nothing if this is a reset packet.
    //

    if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
        if (NetTcpDebugPrintAllPackets != FALSE) {
            RtlDebugPrint(" TCP RST packet from port %d for port %d has no "
                          "socket, ignoring packet.\n",
                          ReceiveContext->Source->Port,
                          ReceiveContext->Destination->Port);
        }

        return;
    }

    //
    // Otherwise, send a reset back to the sender.
    //

    if (NetTcpDebugPrintAllPackets != FALSE) {
        RtlDebugPrint(" TCP packet from port %d for port %d has no socket, "
                      "sending reset.\n",
                      ReceiveContext->Source->Port,
                      ReceiveContext->Destination->Port);
    }

    //
    // Always send a reset.
    //

    ResetFlags = TCP_HEADER_FLAG_RESET;

    //
    // If an ACK was received, the acknoledgement number is used as the
    // sequence number and no ACK is sent.
    //

    if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
        ResetFlags |= TCP_HEADER_FLAG_ACKNOWLEDGE;
        ResetSequenceNumber = NETWORK_TO_CPU32(Header->AcknowledgmentNumber);
        ResetAcknowledgeNumber = 0;

    //
    // Otherwise the sequence number is zero and an ACK is sent with
    // the sender's sequence number plus length as the acknowledgement
    // number.
    //

    } else {
        ResetSequenceNumber = 0;
        SegmentLength = ReceiveContext->Packet->FooterOffset -
                        ReceiveContext->Packet->DataOffset;

        //
        // Remember to count SYNs and FINs as part of the data length.
        //

        if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
            SegmentLength += 1;
        }

        if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {
            SegmentLength += 1;
        }

        ResetAcknowledgeNumber = NETWORK_TO_CPU32(Header->SequenceNumber) +
                                 SegmentLength;
    }

    //
    // Create a socket that will be used to send this transmission.
    //

    ASSERT(ReceiveContext->Source->Domain ==
           ReceiveContext->Destination->Domain);

    Status = IoSocketCreate(ReceiveContext->Destination->Domain,
                            NetSocketStream,
                            SOCKET_INTERNET_PROTOCOL_TCP,
                            0,
                            &NewIoHandle);

    if (!KSUCCESS(Status)) {
        goto TcpHandleUnconnectedPacketEnd;
    }

    Status = IoGetSocketFromHandle(NewIoHandle, (PVOID)&NewTcpSocket);
    if (!KSUCCESS(Status)) {
        goto TcpHandleUnconnectedPacketEnd;
    }

    KeAcquireQueuedLock(NewTcpSocket->Lock);
    LockHeld = TRUE;

    //
    // Bind the new socket to the destination (local) address. In most cases
    // this should not conflict with an existing socket's binding to a local
    // address. The system only ended up here because no suitable socket was
    // found to handle the packet. If the bind ends up failing, tough luck. The
    // packet gets dropped without a response.
    //

    ASSERT(NewTcpSocket->NetSocket.Network == ReceiveContext->Network);

    Status = NewTcpSocket->NetSocket.Network->Interface.BindToAddress(
                                                  &(NewTcpSocket->NetSocket),
                                                  ReceiveContext->Link,
                                                  ReceiveContext->Destination,
                                                  0);

    if (!KSUCCESS(Status)) {
        goto TcpHandleUnconnectedPacketEnd;
    }

    //
    // Connect the new socket to the remote address.
    //

    Status = NewTcpSocket->NetSocket.Network->Interface.Connect(
                                                     (PNET_SOCKET)NewTcpSocket,
                                                     ReceiveContext->Source);

    if (!KSUCCESS(Status)) {
        goto TcpHandleUnconnectedPacketEnd;
    }

    //
    // Initialize the correct sequence and acknowledgement numbers and then
    // send the reset.
    //

    NewTcpSocket->SendUnacknowledgedSequence = ResetSequenceNumber;
    NewTcpSocket->ReceiveNextSequence = ResetAcknowledgeNumber;
    NetpTcpSendControlPacket(NewTcpSocket, ResetFlags);

TcpHandleUnconnectedPacketEnd:

    //
    // Always close out the new socket. It should not remain open for
    // transmissions.
    //

    if (NewTcpSocket != NULL) {

        ASSERT(LockHeld != FALSE);

        NetpTcpCloseOutSocket(NewTcpSocket, FALSE);
    }

    if (LockHeld != FALSE) {

        ASSERT(NewTcpSocket != NULL);

        KeReleaseQueuedLock(NewTcpSocket->Lock);
    }

    if (NewIoHandle != INVALID_HANDLE) {
        IoClose(NewIoHandle);
    }

    return;
}

VOID
NetpTcpFillOutHeader (
    PTCP_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    ULONG SequenceNumber,
    USHORT ExtraFlags,
    ULONG OptionsLength,
    USHORT NonUrgentOffset,
    ULONG DataLength
    )

/*++

Routine Description:

    This routine fills out a TCP header that's going to be sent. This routine
    assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the TCP socket owning the connection.

    Packet - Supplies a pointer to the packet for which the header should be
        filled out.

    SequenceNumber - Supplies the sequence number to use for this packet.

    ExtraFlags - Supplies any flags to set in the header. Since the ACK flag is
        always set, it will be set by default unless it is specified in this
        parameter, in which case it won't be. Said differently, the semantics
        for the ACK flag are backwards of all the other flags, for convenience.

    OptionsLength - Supplies the length of any header options (this is usually
        0).

    NonUrgentOffset - Supplies the offset of the non-urgent data. Usually this
        is zero as there is no urgent data.

    DataLength - Supplies the length of the data field.

Return Value:

    None.

--*/

{

    PVOID Buffer;
    PNETWORK_ADDRESS DestinationAddress;
    PTCP_HEADER Header;
    ULONG PacketSize;
    ULONG RelativeAcknowledgeNumber;
    ULONG RelativeSequenceNumber;
    PNETWORK_ADDRESS SourceAddress;
    ULONG WindowSize;

    //
    // Acknowledges come with every header (except the first, but this flag is
    // never going to be set then anyway).
    //

    if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) != 0) {
        Socket->Flags &= ~TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
        NetpTcpTimerReleaseReference(Socket);
    }

    Buffer = Packet->Buffer + Packet->DataOffset;
    Header = (PTCP_HEADER)Buffer;
    SourceAddress = &(Socket->NetSocket.LocalSendAddress);
    DestinationAddress = &(Socket->NetSocket.RemoteAddress);
    Header->SourcePort = CPU_TO_NETWORK16(SourceAddress->Port);
    Header->DestinationPort = CPU_TO_NETWORK16(DestinationAddress->Port);
    Header->SequenceNumber = CPU_TO_NETWORK32(SequenceNumber);
    Header->HeaderLength = ((sizeof(TCP_HEADER) + OptionsLength) >> 2) <<
                           TCP_HEADER_LENGTH_SHIFT;

    Header->Flags = ExtraFlags ^ TCP_HEADER_FLAG_ACKNOWLEDGE;
    if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
        Header->AcknowledgmentNumber =
                                 CPU_TO_NETWORK32(Socket->ReceiveNextSequence);

    } else {
        Header->AcknowledgmentNumber = 0;
    }

    //
    // The window size is the original window size minus the amount of data
    // in the buffer.
    //

    WindowSize = Socket->ReceiveWindowFreeSize >> Socket->ReceiveWindowScale;
    Header->WindowSize = CPU_TO_NETWORK16((USHORT)WindowSize);
    Header->NonUrgentOffset = NonUrgentOffset;
    Header->Checksum = 0;
    PacketSize = sizeof(TCP_HEADER) + OptionsLength + DataLength;
    if ((Socket->NetSocket.Link->Properties.Capabilities &
         NET_LINK_CAPABILITY_TRANSMIT_TCP_CHECKSUM_OFFLOAD) == 0) {

        Header->Checksum = NetpTcpChecksumData(Header,
                                               PacketSize,
                                               SourceAddress,
                                               DestinationAddress);

    } else {
        Packet->Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD;
    }

    //
    // Print this packet if debugging is enabled.
    //

    if (NetTcpDebugPrintAllPackets != FALSE) {
        NetpTcpPrintSocketEndpoints(Socket, TRUE);
        RtlDebugPrint(" TX [");
        if ((Header->Flags & TCP_HEADER_FLAG_FIN) != 0) {
            RtlDebugPrint("FIN ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {
            RtlDebugPrint("SYN ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_RESET) != 0) {
            RtlDebugPrint("RST ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_PUSH) != 0) {
            RtlDebugPrint("PSH ");
        }

        if ((Header->Flags & TCP_HEADER_FLAG_URGENT) != 0) {
            RtlDebugPrint("URG");
        }

        RelativeAcknowledgeNumber = 0;
        if ((Header->Flags & TCP_HEADER_FLAG_ACKNOWLEDGE) != 0) {
            RtlDebugPrint("ACK");
            RelativeAcknowledgeNumber = Socket->ReceiveNextSequence -
                                        Socket->ReceiveInitialSequence;
        }

        RelativeSequenceNumber = SequenceNumber - Socket->SendInitialSequence;
        RtlDebugPrint("] \n    Seq=%d Ack=%d Win=%d Len=%d\n",
                      RelativeSequenceNumber,
                      RelativeAcknowledgeNumber,
                      WindowSize << Socket->ReceiveWindowScale,
                      DataLength);
    }

    if (NetTcpDebugPrintSequenceNumbers != FALSE) {
        NetpTcpPrintSocketEndpoints(Socket, TRUE);
        RtlDebugPrint(" TX Segment %d, length %d.\n",
                      SequenceNumber - Socket->SendInitialSequence,
                      DataLength);
    }

    return;
}

USHORT
NetpTcpChecksumData (
    PVOID Data,
    ULONG DataLength,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine computes the checksum for a TCP packet and returns it in
    network byte order.

Arguments:

    Data - Supplies a pointer to the beginning of the TCP header.

    DataLength - Supplies the length of the header, options, and data, in bytes.

    SourceAddress - Supplies a pointer to the source address of the packet,
        used to compute the pseudo header.

    DestinationAddress - Supplies a pointer to the destination address of the
        packet, used to compute the pseudo header used in the checksum.

Return Value:

    Returns the checksum for the given packet.

--*/

{

    PUCHAR BytePointer;
    PIP4_ADDRESS Ip4Address;
    PULONG LongPointer;
    ULONG NextValue;
    USHORT ShortOne;
    PUSHORT ShortPointer;
    USHORT ShortTwo;
    ULONG Sum;

    Sum = 0;

    ASSERT(SourceAddress->Domain == NetDomainIp4);
    ASSERT(DestinationAddress->Domain == SourceAddress->Domain);

    Ip4Address = (PIP4_ADDRESS)SourceAddress;
    Sum = Ip4Address->Address;
    Ip4Address = (PIP4_ADDRESS)DestinationAddress;
    Sum += Ip4Address->Address;
    if (Sum < Ip4Address->Address) {
        Sum += 1;
    }

    ASSERT(DataLength < MAX_USHORT);

    NextValue = (RtlByteSwapUshort((USHORT)DataLength) << 16) |
                (SOCKET_INTERNET_PROTOCOL_TCP << 8);

    Sum += NextValue;
    if (Sum < NextValue) {
        Sum += 1;
    }

    LongPointer = (PULONG)Data;
    while (DataLength >= sizeof(ULONG)) {
        NextValue = *LongPointer;
        LongPointer += 1;
        Sum += NextValue;
        if (Sum < NextValue) {
            Sum += 1;
        }

        DataLength -= sizeof(ULONG);
    }

    BytePointer = (PUCHAR)LongPointer;
    if ((DataLength & sizeof(USHORT)) != 0) {
        ShortPointer = (PUSHORT)BytePointer;
        NextValue = (USHORT)*ShortPointer;
        Sum += NextValue;
        if (Sum < NextValue) {
            Sum += 1;
        }

        BytePointer += sizeof(USHORT);
    }

    if ((DataLength & sizeof(UCHAR)) != 0) {
        NextValue = (UCHAR)*BytePointer;
        Sum += NextValue;
        if (Sum < NextValue) {
            Sum += 1;
        }
    }

    //
    // Fold the 32-bit value down to 16-bits.
    //

    ShortOne = (USHORT)Sum;
    ShortTwo = (USHORT)(Sum >> 16);
    ShortTwo += ShortOne;
    if (ShortTwo < ShortOne) {
        ShortTwo += 1;
    }

    return (USHORT)~ShortTwo;
}

BOOL
NetpTcpIsReceiveSegmentAcceptable (
    PTCP_SOCKET Socket,
    ULONG SequenceNumber,
    ULONG SegmentLength
    )

/*++

Routine Description:

    This routine determines if any part of the segment is in the window of
    acceptable sequence numbers for the socket. This routine assumes the socket
    lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket.

    SequenceNumber - Supplies a pointer to the sequence number to query for
        validity.

    SegmentLength - Supplies the number of bytes in the incoming segment.

Return Value:

    TRUE if the segment falls at least partially in the receive window.

    FALSE if the segment is outside the receive window.

--*/

{

    BOOL SegmentAcceptable;
    ULONG SegmentEnd;
    ULONG WindowBegin;
    ULONG WindowEnd;

    SegmentAcceptable = FALSE;
    WindowBegin = Socket->ReceiveNextSequence;
    WindowEnd = WindowBegin + Socket->ReceiveWindowFreeSize;

    //
    // Handle zero-length segments.
    //

    if (SegmentLength == 0) {

        //
        // If the window size is zero, then the sequence number must match the
        // expected number exactly.
        //

        if (Socket->ReceiveWindowFreeSize == 0) {
            if (SequenceNumber == Socket->ReceiveNextSequence) {
                SegmentAcceptable = TRUE;
            }

        //
        // If the window size is valid, then sequence number must be within the
        // window.
        //

        } else if (WindowEnd > WindowBegin) {
            if ((SequenceNumber >= WindowBegin) &&
                (SequenceNumber < WindowEnd)) {

                SegmentAcceptable = TRUE;
            }

        //
        // If the window size is valid, but wrapped, then sequence number must
        // be within the window.
        //

        } else {

            ASSERT(WindowBegin != WindowEnd);

            if ((SequenceNumber >= WindowBegin) ||
                (SequenceNumber < WindowEnd)) {

                SegmentAcceptable = TRUE;
            }
        }

    //
    // If the segment is non-zero, then the segment is valid if the beginning
    // or the end falls within the window.
    //

    } else {
        SegmentAcceptable = FALSE;
        SegmentEnd = SequenceNumber + SegmentLength - 1;

        //
        // It's acceptable if at least one of these conditions is met:
        // 1) The starting sequence number is within the window.
        // 2) The ending sequence number is within the window.
        // Watch out here for the window straddling the rollover.
        //

        if (WindowEnd >= WindowBegin) {
            if ((SequenceNumber >= WindowBegin) &&
                (SequenceNumber < WindowEnd)) {

                SegmentAcceptable = TRUE;
            }

            if ((SegmentEnd >= WindowBegin) && (SegmentEnd < WindowEnd)) {
                SegmentAcceptable = TRUE;
            }

        //
        // Yikes, the window straddles the rollover. Do the same logic as above
        // but a bit more carefully.
        //

        } else {
            if ((SequenceNumber >= WindowBegin) ||
                (SequenceNumber < WindowEnd)) {

                SegmentAcceptable = TRUE;
            }

            if ((SegmentEnd >= WindowBegin) || (SegmentEnd < WindowEnd)) {
                SegmentAcceptable = TRUE;
            }
        }

        if (((SequenceNumber >= Socket->ReceiveNextSequence) &&
             (SequenceNumber < WindowEnd)) ||
            ((SegmentEnd >= Socket->ReceiveNextSequence) &&
             (SegmentEnd < WindowEnd))) {

            SegmentAcceptable = TRUE;
        }

        //
        // If the segment length is non-zero but the window is zero, then this
        // is no good.
        //

        if (Socket->ReceiveWindowFreeSize == 0) {
            SegmentAcceptable = FALSE;
        }
    }

    return SegmentAcceptable;
}

KSTATUS
NetpTcpProcessAcknowledge (
    PTCP_SOCKET Socket,
    ULONG AcknowledgeNumber,
    ULONG SequenceNumber,
    ULONG DataLength,
    USHORT WindowSize
    )

/*++

Routine Description:

    This routine handles the update of TCP state based on the incoming
    acknowledge number. This routine assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket.

    AcknowledgeNumber - Supplies the acknowledge number sent by the remote
        host (after being converted back into CPU endianness).

    SequenceNumber - Supplies the remote sequence number that came along with
        this acknowledge number.

    DataLength - Supplies the number of bytes of data that came along with this
        acknowledge number.

    WindowSize - Supplies the window size that came along with this packet,
        which may or may not get saved as the new send window. This value is
        expected to be straight from the header, in network order.

Return Value:

    Status code.

--*/

{

    BOOL AcknowledgeValid;
    ULONGLONG CurrentTime;
    PIO_OBJECT_STATE IoState;
    ULONG ReceiveWindowEnd;
    ULONG RelativeAcknowledgeNumber;
    ULONG ResetFlags;
    ULONG ScaledWindowSize;
    BOOL UpdateValid;

    ASSERT(Socket->NetSocket.KernelSocket.ReferenceCount >= 1);

    CurrentTime = 0;
    IoState = Socket->NetSocket.KernelSocket.IoState;
    ScaledWindowSize = NETWORK_TO_CPU16(WindowSize) <<
                       Socket->SendWindowScale;

    //
    // If this is the Syn-Received state, then an ACK is what's needed to bring
    // this socket to the established state.
    //

    if (Socket->State == TcpStateSynReceived) {

        //
        // If the acnowledge number is valid, move to the established state. At
        // this point, only a SYN should have been sent.
        //

        ASSERT((Socket->SendUnacknowledgedSequence + 1) ==
               Socket->SendNextNetworkSequence);

        ASSERT(Socket->SendNextNetworkSequence ==
               Socket->SendNextBufferSequence);

        if (AcknowledgeNumber == Socket->SendNextNetworkSequence) {
            NetpTcpSetState(Socket, TcpStateEstablished);

        //
        // The acknowledge number is not valid, send a reset using the
        // acknowledge number as the sequence number.
        //

        } else {
            Socket->SendUnacknowledgedSequence = AcknowledgeNumber;
            ResetFlags = TCP_HEADER_FLAG_RESET | TCP_HEADER_FLAG_ACKNOWLEDGE;
            NetpTcpSendControlPacket(Socket, ResetFlags);
            Socket->Flags |= TCP_SOCKET_FLAG_CONNECTION_RESET;
            NET_SOCKET_SET_LAST_ERROR(&(Socket->NetSocket),
                                      STATUS_CONNECTION_RESET);

            NetpTcpCloseOutSocket(Socket, FALSE);
            return STATUS_CONNECTION_RESET;
        }
    }

    //
    // Determine if the acknowledge number is within the send window. Watch out
    // if the send window is partially wrapped around.
    //

    AcknowledgeValid = FALSE;
    if (Socket->SendNextNetworkSequence >= Socket->SendUnacknowledgedSequence) {
        if ((AcknowledgeNumber >= Socket->SendUnacknowledgedSequence) &&
            (AcknowledgeNumber <= Socket->SendNextNetworkSequence)) {

            AcknowledgeValid = TRUE;
        }

    //
    // The send window is wrapped around.
    //

    } else {
        if ((AcknowledgeNumber >= Socket->SendUnacknowledgedSequence) ||
            (AcknowledgeNumber <= Socket->SendNextNetworkSequence)) {

            AcknowledgeValid = TRUE;
        }
    }

    //
    // If the acknowledge number is valid, then update the window state and
    // list of packets that need acknowledgment.
    //

    if (AcknowledgeValid != FALSE) {
        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            RelativeAcknowledgeNumber = Socket->SendUnacknowledgedSequence -
                                        Socket->SendInitialSequence;

            if (RelativeAcknowledgeNumber !=
                AcknowledgeNumber - Socket->SendInitialSequence) {

                NetpTcpPrintSocketEndpoints(Socket, FALSE);
                RtlDebugPrint(" ACK moved up from %d to %d.\n",
                              RelativeAcknowledgeNumber,
                              AcknowledgeNumber - Socket->SendInitialSequence);
            }
        }

        Socket->SendUnacknowledgedSequence = AcknowledgeNumber;
        ReceiveWindowEnd = Socket->ReceiveNextSequence +
                           Socket->ReceiveWindowFreeSize;

        UpdateValid = FALSE;

        //
        // If the sequence number hasn't moved forward, then the update is good
        // to take. RFC 1122 Section 4.2.2.20 has a correction to RFC 793's
        // rules for taking a window update. The rule is that the update is
        // valid if the sequence numbers are equal and the ACK is greater than
        // or equal than the old ACK. RFC 793 statis that only ACKs greater
        // than the old value are acceptable. So, given that the ACK was
        // validated to fit in the send window above, it does not need to be
        // checked here.
        //

        if (SequenceNumber == Socket->SendWindowUpdateSequence) {
            UpdateValid = TRUE;
        }

        //
        // In the normal window arrangement, take the highest sequence number
        // in the window.
        //

        if (ReceiveWindowEnd > Socket->SendWindowUpdateSequence) {
            if ((SequenceNumber > Socket->SendWindowUpdateSequence) &&
                (SequenceNumber < ReceiveWindowEnd)) {

                UpdateValid = TRUE;
            }

        //
        // The eligible window wraps around, be a bit more careful.
        //

        } else {
            if ((SequenceNumber > Socket->SendWindowUpdateSequence) ||
                (SequenceNumber < ReceiveWindowEnd)) {

                UpdateValid = TRUE;
            }
        }

        //
        // If the remote sequence number or the remote acknowledge
        // number has moved forward from the last time the window was
        // updated, then update the window (and the record of the last
        // time the window was updated). This prevents old reordered
        // segments from updating the window size.
        //

        if (UpdateValid != FALSE) {
            Socket->SendWindowSize = ScaledWindowSize;
            Socket->SendWindowUpdateSequence = SequenceNumber;
            Socket->SendWindowUpdateAcknowledge = AcknowledgeNumber;
            Socket->RetryTime = 0;
            Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
        }

        //
        // Clean up the send buffer based on this new acknowledgment.
        //

        NetpTcpFreeSentSegments(Socket, &CurrentTime);

    //
    // If the ACK is ahead of schedule, take note and send a response.
    //

    } else if (TCP_SEQUENCE_GREATER_THAN(AcknowledgeNumber,
                                         Socket->SendNextNetworkSequence)) {

        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            NetpTcpPrintSocketEndpoints(Socket, FALSE);
            RelativeAcknowledgeNumber = Socket->SendUnacknowledgedSequence -
                                        Socket->SendInitialSequence;

            RtlDebugPrint(" Invalid ACK %d, window was %d size %d.\n",
                          AcknowledgeNumber - Socket->SendInitialSequence,
                          RelativeAcknowledgeNumber,
                          Socket->SendWindowSize);
        }

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0) {
            Socket->Flags |= TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
            NetpTcpTimerAddReference(Socket);
        }

    //
    // This ACK was not valid, take a note of it.
    //

    } else if (Socket->SendUnacknowledgedSequence !=
               Socket->SendNextNetworkSequence) {

        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            NetpTcpPrintSocketEndpoints(Socket, FALSE);
            RelativeAcknowledgeNumber = Socket->SendUnacknowledgedSequence -
                                        Socket->SendInitialSequence;

            RtlDebugPrint(" Invalid ACK %d, window was %d size %d.\n",
                          AcknowledgeNumber - Socket->SendInitialSequence,
                          RelativeAcknowledgeNumber,
                          Socket->SendWindowSize);
        }
    }

    //
    // Check to see if this is a duplicate acknowledgment, excluding any ACKs
    // piggybacking on data, window size updates, and cases where this no data
    // waiting to be sent.
    //

    if ((DataLength == 0) &&
        (AcknowledgeNumber == Socket->PreviousAcknowledgeNumber) &&
        (Socket->SendUnacknowledgedSequence !=
         Socket->SendNextNetworkSequence) &&
        (Socket->SendWindowSize == ScaledWindowSize)) {

        Socket->DuplicateAcknowledgeCount += 1;
        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            RtlDebugPrint("Duplicate ACK #%d for sequence %d.\n",
                          Socket->DuplicateAcknowledgeCount,
                          AcknowledgeNumber - Socket->SendInitialSequence);
        }

    } else {
        Socket->DuplicateAcknowledgeCount = 0;
    }

    //
    // Allow congestion control to process the acknowledgment.
    //

    NetpTcpCongestionAcknowledgeReceived(Socket, AcknowledgeNumber);
    Socket->PreviousAcknowledgeNumber = AcknowledgeNumber;

    //
    // Try to send more data immediately. Do this after the congenstion control
    // has processed the acknowledge number to give it a chance to update the
    // congestion window size.
    //

    if ((AcknowledgeValid != FALSE) && (Socket->SendWindowSize != 0)) {
        NetpTcpSendPendingSegments(Socket, &CurrentTime);
    }

    //
    // If the connection is shutting down and the sent FIN was acknowledged,
    // then advance to the second wait state.
    //

    if (Socket->State == TcpStateFinWait1) {
        if (Socket->SendUnacknowledgedSequence ==
            Socket->SendNextNetworkSequence) {

            ASSERT(Socket->SendNextNetworkSequence ==
                   Socket->SendNextBufferSequence);

            NetpTcpSetState(Socket, TcpStateFinWait2);
        }
    }

    //
    // In FIN wait 2, if the retransmission queue is empty the close can be
    // acknowledged, but the socket isn't destroyed yet.
    //

    if (Socket->State == TcpStateFinWait2) {

        ASSERT(Socket->SendUnacknowledgedSequence ==
               Socket->SendNextNetworkSequence);

        ASSERT(Socket->SendNextNetworkSequence ==
               Socket->SendNextBufferSequence);

        //
        // Release the blocked close call.
        //

        IoSetIoObjectState(IoState, POLL_EVENT_OUT, TRUE);
    }

    //
    // If the connection is closing and the sent FIN was acknowledged, then
    // advance to the time-wait state.
    //

    if (Socket->State == TcpStateClosing) {
        if (Socket->SendUnacknowledgedSequence ==
            Socket->SendNextNetworkSequence) {

            ASSERT(Socket->SendNextNetworkSequence ==
                   Socket->SendNextBufferSequence);

            NetpTcpSetState(Socket, TcpStateTimeWait);
        }
    }

    //
    // If the acknowledge was received for the FIN that was sent, then move
    // directly to the closed state and clean up.
    //

    if (Socket->State == TcpStateLastAcknowledge) {

        ASSERT((Socket->Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) !=
               0);

        if (AcknowledgeNumber == Socket->SendFinalSequence + 1) {
            NetpTcpCloseOutSocket(Socket, FALSE);
            return STATUS_CONNECTION_CLOSED;
        }
    }

    return STATUS_SUCCESS;
}

VOID
NetpTcpProcessPacketOptions (
    PTCP_SOCKET Socket,
    PTCP_HEADER Header,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine is called to process TCP packet options.

Arguments:

    Socket - Supplies a pointer to the TCP socket.

    Header - Supplies a pointer to the TCP header.

    Packet - Supplies a pointer to the received packet information.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

Return Value:

    None.

--*/

{

    ULONG LocalMaxSegmentSize;
    ULONG OptionIndex;
    UCHAR OptionLength;
    PUCHAR Options;
    ULONG OptionsLength;
    UCHAR OptionType;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    BOOL WindowScaleSupported;

    WindowScaleSupported = FALSE;

    //
    // Parse the options in the packet.
    //

    OptionsLength = Packet->DataOffset -
                    ((UINTN)Header - (UINTN)(Packet->Buffer));

    OptionIndex = 0;
    Options = (PUCHAR)(Header + 1);
    while (OptionIndex < OptionsLength) {
        OptionType = Options[OptionIndex];
        OptionIndex += 1;
        if (OptionType == TCP_OPTION_END) {
            break;
        }

        if (OptionType == TCP_OPTION_NOP) {
            continue;
        }

        if (OptionIndex >= OptionsLength) {
            break;
        }

        //
        // The option length accounts for the type and length fields themselves.
        //

        OptionLength = Options[OptionIndex] - 2;
        OptionIndex += 1;
        if (OptionIndex + OptionLength > OptionsLength) {
            break;
        }

        //
        // Watch for the maximum segment size option, but only if the SYN flag
        // is set.
        //

        if (OptionType == TCP_OPTION_MAXIMUM_SEGMENT_SIZE) {
            if (((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) &&
                (OptionLength == 2)) {

                Socket->SendMaxSegmentSize =
                         NETWORK_TO_CPU16(*((PUSHORT)&(Options[OptionIndex])));

                SizeInformation = &(Socket->NetSocket.PacketSizeInformation);
                LocalMaxSegmentSize = SizeInformation->MaxPacketSize -
                                      SizeInformation->HeaderSize -
                                      SizeInformation->FooterSize;

                if (LocalMaxSegmentSize < Socket->SendMaxSegmentSize) {
                    Socket->SendMaxSegmentSize = LocalMaxSegmentSize;
                }
            }

        //
        // Watch for the window scale option, but only if the SYN flag is set.
        //

        } else if (OptionType == TCP_OPTION_WINDOW_SCALE) {
            if (((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) &&
                (OptionLength == 1)) {

                Socket->SendWindowScale = Options[OptionIndex];
                WindowScaleSupported = TRUE;
            }
        }

        //
        // Zoom past the object value.
        //

        OptionIndex += OptionLength;
    }

    if ((Header->Flags & TCP_HEADER_FLAG_SYN) != 0) {

        //
        // Disable window scaling locally if the remote doesn't understand it.
        //

        if (WindowScaleSupported == FALSE) {
            Socket->Flags &= ~TCP_SOCKET_FLAG_WINDOW_SCALING;

            //
            // No data should have been sent yet.
            //

            ASSERT(Socket->ReceiveWindowFreeSize ==
                   Socket->ReceiveWindowTotalSize);

            if (Socket->ReceiveWindowTotalSize > MAX_USHORT) {
                Socket->ReceiveWindowTotalSize = MAX_USHORT;
                Socket->ReceiveWindowFreeSize = MAX_USHORT;
            }

            Socket->ReceiveWindowScale = 0;
        }
    }

    return;
}

VOID
NetpTcpSendControlPacket (
    PTCP_SOCKET Socket,
    ULONG Flags
    )

/*++

Routine Description:

    This routine sends a packet to the remote host that contains no data. This
    routine assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket to send the acnkowledge packet on.

    Flags - Supplies the bitfield of flags to set. The exception is the
        acknowledge flag, which is always set by default, but is cleared if the
        bit is set in this parameter.

Return Value:

    None.

--*/

{

    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    ULONG SequenceNumber;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    KSTATUS Status;

    NET_INITIALIZE_PACKET_LIST(&PacketList);

    //
    // If the socket has no link, then some incoming packet happened to guess
    // an unbound socket. Sometimes this happens if the system resets and
    // re-binds to the same port, and the remote end is left wondering what
    // happened.
    //

    if (Socket->NetSocket.Link == NULL) {
        if ((NetTcpDebugPrintAllPackets != FALSE) ||
            (NetTcpDebugPrintSequenceNumbers != FALSE)) {

            RtlDebugPrint("TCP: Ignoring send on unbound socket.\n");
        }

        return;
    }

    Packet = NULL;
    SizeInformation = &(Socket->NetSocket.PacketSizeInformation);
    Status = NetAllocateBuffer(SizeInformation->HeaderSize,
                               0,
                               SizeInformation->FooterSize,
                               Socket->NetSocket.Link,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto TcpSendControlPacketEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    ASSERT(Packet->DataOffset >= sizeof(TCP_HEADER));

    Packet->DataOffset -= sizeof(TCP_HEADER);

    //
    // A keep alive message is just an ACK with a sequence number one less than
    // the current value.
    //

    SequenceNumber = Socket->SendUnacknowledgedSequence;
    if ((Flags & TCP_HEADER_FLAG_KEEP_ALIVE) != 0) {
        SequenceNumber -= 1;
        Flags &= ~TCP_HEADER_FLAG_KEEP_ALIVE;
    }

    NetpTcpFillOutHeader(Socket, Packet, SequenceNumber, Flags, 0, 0, 0);

    //
    // Send this control packet off down the network.
    //

    Status = Socket->NetSocket.Network->Interface.Send(
                                            &(Socket->NetSocket),
                                            &(Socket->NetSocket.RemoteAddress),
                                            NULL,
                                            &PacketList);

    if (!KSUCCESS(Status)) {
        goto TcpSendControlPacketEnd;
    }

TcpSendControlPacketEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return;
}

VOID
NetpTcpProcessReceivedDataSegment (
    PTCP_SOCKET Socket,
    ULONG SequenceNumber,
    PVOID Buffer,
    ULONG Length,
    PTCP_HEADER Header
    )

/*++

Routine Description:

    This routine processes incoming user data on a TCP socket. This routine
    assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    SequenceNumber - Supplies the sequence number of the first byte of the data.

    Buffer - Supplies a pointer to the user data.

    Length - Supplies the length of the user data buffer.

    Header - Supplies a pointer to the TCP header of the segment.

Return Value:

    None.

--*/

{

    ULONG AvailableBytes;
    PLIST_ENTRY CurrentEntry;
    PTCP_RECEIVED_SEGMENT CurrentSegment;
    BOOL DataMissing;
    BOOL InsertedSegment;
    PIO_OBJECT_STATE IoState;
    ULONG NextSequence;
    PTCP_RECEIVED_SEGMENT PreviousSegment;
    ULONG RemainingLength;
    KSTATUS Status;
    BOOL UpdateReceiveNextSequence;

    IoState = Socket->NetSocket.KernelSocket.IoState;

    //
    // Don't process anything if the window is closed.
    //

    if ((Socket->ReceiveWindowFreeSize == 0) || (Length == 0)) {
        return;
    }

    if (NetTcpDebugPrintSequenceNumbers != FALSE) {
        NetpTcpPrintSocketEndpoints(Socket, FALSE);
        RtlDebugPrint(" RX Segment %d size %d.\n",
                      SequenceNumber - Socket->ReceiveInitialSequence,
                      Length);
    }

    //
    // Loop through every segment to find a segment with a larger sequence than
    // this one. If such a segment is found, then try to fill in the hole
    // with the data from the provided segment. The segment will then shrink
    // and the loop continues until the entire segment has been processed.
    //

    RemainingLength = Length;
    UpdateReceiveNextSequence = FALSE;
    PreviousSegment = NULL;
    CurrentEntry = Socket->ReceivedSegmentList.Next;
    while (CurrentEntry != &(Socket->ReceivedSegmentList)) {
        CurrentSegment = LIST_VALUE(CurrentEntry,
                                    TCP_RECEIVED_SEGMENT,
                                    Header.ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // If the starting sequence of this segment is less than or equal to
        // the beginning of what remains of the received segment, skip it.
        //

        if ((SequenceNumber == CurrentSegment->SequenceNumber) ||
            TCP_SEQUENCE_LESS_THAN(CurrentSegment->SequenceNumber,
                                   SequenceNumber)) {

            PreviousSegment = CurrentSegment;
            continue;
        }

        ASSERT(TCP_SEQUENCE_GREATER_THAN(CurrentSegment->SequenceNumber,
                                         SequenceNumber));

        Status = NetpTcpInsertReceivedDataSegment(Socket,
                                                  PreviousSegment,
                                                  CurrentSegment,
                                                  Header,
                                                  &Buffer,
                                                  &SequenceNumber,
                                                  &RemainingLength,
                                                  &InsertedSegment);

        if (!KSUCCESS(Status)) {
            goto TcpProcessReceivedDataSegmentEnd;
        }

        //
        // Record if something was inserted, indicating that the next sequence
        // may need to be updated.
        //

        if (InsertedSegment != FALSE) {
            UpdateReceiveNextSequence = TRUE;
        }

        //
        // If there is nothing left to insert or no room to insert it, then
        // exit.
        //

        if ((RemainingLength == 0) || (Socket->ReceiveWindowFreeSize == 0)) {
            goto TcpProcessReceivedDataSegmentEnd;
        }

        //
        // The current segment becomes the previous segment as more of the
        // region gets processed.
        //

        PreviousSegment = CurrentSegment;
    }

    //
    // There better be something left to insert and the previous segment should
    // either not exist or be the last segment in the list.
    //

    ASSERT(RemainingLength != 0);
    ASSERT((PreviousSegment == NULL) ||
           (&(PreviousSegment->Header.ListEntry) ==
            Socket->ReceivedSegmentList.Previous));

    Status = NetpTcpInsertReceivedDataSegment(Socket,
                                              PreviousSegment,
                                              NULL,
                                              Header,
                                              &Buffer,
                                              &SequenceNumber,
                                              &RemainingLength,
                                              &InsertedSegment);

    if (!KSUCCESS(Status)) {
        goto TcpProcessReceivedDataSegmentEnd;
    }

    //
    // Record if something was inserted, indicating that the next sequence may
    // need to be updated.
    //

    if (InsertedSegment != FALSE) {
        UpdateReceiveNextSequence = TRUE;
    }

TcpProcessReceivedDataSegmentEnd:

    //
    // Locally record if the socket was missing data and then reset that state.
    // It will be updated below if data is still missing.
    //

    DataMissing = FALSE;
    if ((Socket->Flags & TCP_SOCKET_FLAG_RECEIVE_MISSING_SEGMENTS) != 0) {
        Socket->Flags &= ~TCP_SOCKET_FLAG_RECEIVE_MISSING_SEGMENTS;
        DataMissing = TRUE;
    }

    //
    // If a segment was inserted, then try to update the next expected receive
    // sequence. It must be contiguous from the beginning of the unread data.
    //

    if (UpdateReceiveNextSequence != FALSE) {
        NextSequence = Socket->ReceiveUnreadSequence;
        CurrentEntry = Socket->ReceivedSegmentList.Next;
        while (CurrentEntry != &(Socket->ReceivedSegmentList)) {
            CurrentSegment = LIST_VALUE(CurrentEntry,
                                        TCP_RECEIVED_SEGMENT,
                                        Header.ListEntry);

            if (NextSequence != CurrentSegment->SequenceNumber) {
                Socket->Flags |= TCP_SOCKET_FLAG_RECEIVE_MISSING_SEGMENTS;
                DataMissing = TRUE;

                //
                // It would be bad if there were something in the receive list
                // that's less than the supposed start of the receive buffer.
                //

                ASSERT(TCP_SEQUENCE_GREATER_THAN(CurrentSegment->SequenceNumber,
                                                 NextSequence));

                break;
            }

            NextSequence = CurrentSegment->NextSequence;
            CurrentEntry = CurrentEntry->Next;
        }

        //
        // If the sequence number was updated, then alert any readers if the
        // minimum amount of data has been received.
        //

        if (NextSequence != Socket->ReceiveNextSequence) {
            if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                RtlDebugPrint("Moving RX next up from %d to %d.\n",
                              (Socket->ReceiveNextSequence -
                               Socket->ReceiveInitialSequence),
                              (NextSequence - Socket->ReceiveInitialSequence));
            }

            //
            // Shrink the window now that new contiguous data was received.
            //

            Socket->ReceiveWindowFreeSize -= (NextSequence -
                                              Socket->ReceiveNextSequence);

            AvailableBytes = Socket->ReceiveWindowTotalSize -
                             Socket->ReceiveWindowFreeSize;

            if (AvailableBytes >= Socket->ReceiveMinimum) {
                IoSetIoObjectState(IoState, POLL_EVENT_IN, TRUE);
            }

            Socket->ReceiveNextSequence = NextSequence;
        }
    }

    //
    // Data was sent. Whether or not it's repeated data, an ACK is in order. Do
    // it now that the receive sequence is up to date. But in order to not
    // immediately ACK every packet sent, only ACK every other packet. On the
    // odd packets, set the timer in case another packet does not come through.
    // The exception is if a FIN came in with this data packet and all the
    // expected data has been seen; the caller will handle sending an ACK in
    // response to the FIN. If the received data came with a PUSH, then always
    // acknowledge right away, as there's probably not more data coming.
    //

    if ((DataMissing != FALSE) ||
        ((Header->Flags & TCP_HEADER_FLAG_FIN) == 0) ||
        (Socket->ReceiveNextSequence != (SequenceNumber + RemainingLength))) {

        if ((DataMissing == FALSE) &&
            ((Header->Flags & TCP_HEADER_FLAG_PUSH) == 0) &&
            (Length >= Socket->ReceiveMaxSegmentSize) &&
            ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) == 0)) {

            Socket->Flags |= TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
            NetpTcpTimerAddReference(Socket);

        } else {
            if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) != 0) {
                Socket->Flags &= ~TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
                NetpTcpTimerReleaseReference(Socket);
            }

            NetpTcpSendControlPacket(Socket, 0);
        }
    }

    return;
}

KSTATUS
NetpTcpInsertReceivedDataSegment (
    PTCP_SOCKET Socket,
    PTCP_RECEIVED_SEGMENT PreviousSegment,
    PTCP_RECEIVED_SEGMENT NextSegment,
    PTCP_HEADER Header,
    PVOID *Buffer,
    PULONG SequenceNumber,
    PULONG Length,
    PBOOL InsertedSegment
    )

/*++

Routine Description:

    This routine attempts to insert the given data segment, as defined by the
    sequence number and length, into the socket's list of received data
    segments. The provided region should fit between the given segments or
    overlap with a portion thereof. It may extend beyond the end of the given
    next segment but that portion will be clipped. This routine assumes the
    socket lock is held.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    PreviousSegment - Supplies an optional pointer to the previous segment in
        the socket's list of received segments. This segment may or may not
        overlap with the region, but the region should not extend to cover any
        portion of the sequence before this previous region.

    NextSegment - Supplies an optional pointer to the next segment in the
        socket's list of received segments. This segment may or may not overlap
        with the region, but its beginning sequence number should be greater
        than the inserting region's sequence number.

    Header - Supplies a pointer to the TCP header for this received segment.

    Buffer - Supplies a pointer to the buffer containing the data to insert.
        If this routine reads or skips over any data at the beginning of the
        buffer, it will update the buffer pointer.

    SequenceNumber - Supplies a pointer to the sequence number that starts the
        region to insert. If this routine clips from the beginning of the
        region or inserts any portion of the region, it will move the sequence
        number forward.

    Length - Supplies a pointer to the length of the region to insert. If this
        routine clips from the beginning of the region or inserts data from the
        region, the length will be updated.

    InsertedSegment - Supplies a pointer to a boolean that receives indication
        of whether or not this routine successfully inserted a region.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG ClipAmount;
    ULONG InsertBegin;
    ULONG InsertEnd;
    ULONG InsertLength;
    PTCP_RECEIVED_SEGMENT NewSegment;
    PLIST_ENTRY PreviousListEntry;
    ULONG RelativeSequence;
    ULONG SegmentEnd;
    KSTATUS Status;
    ULONG TailLength;
    PTCP_RECEIVED_SEGMENT TailSegment;
    PVOID TailSource;
    USHORT UrgentPointer;
    ULONG UrgentSequence;

    ASSERT(*Length != 0);
    ASSERT(Socket->ReceiveWindowFreeSize != 0);
    ASSERT((NextSegment == NULL) ||
           (TCP_SEQUENCE_GREATER_THAN(NextSegment->SequenceNumber,
                                      *SequenceNumber)));

    ASSERT((PreviousSegment == NULL) ||
           (PreviousSegment->SequenceNumber == *SequenceNumber) ||
           (TCP_SEQUENCE_LESS_THAN(PreviousSegment->SequenceNumber,
                                   *SequenceNumber)));

    *InsertedSegment = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Start out assuming that all of the received segment can be inserted.
    //

    InsertBegin = *SequenceNumber;
    InsertEnd = *SequenceNumber + *Length;
    InsertLength = *Length;
    UrgentSequence = *SequenceNumber;

    //
    // Clip the segment if it is older than what's already been read.
    //

    if (TCP_SEQUENCE_LESS_THAN(InsertEnd, Socket->ReceiveUnreadSequence)) {
        *SequenceNumber = Socket->ReceiveUnreadSequence;
        *Length = 0;
        goto TcpInsertReceivedDataSegmentEnd;
    }

    if (TCP_SEQUENCE_LESS_THAN(InsertBegin, Socket->ReceiveUnreadSequence)) {
        ClipAmount = Socket->ReceiveUnreadSequence - InsertBegin;
        InsertLength -= ClipAmount;
        *Length -= ClipAmount;
        *Buffer += ClipAmount;
        InsertBegin = Socket->ReceiveUnreadSequence;
    }

    //
    // Process the previous segment if it exists, skipping data in here that is
    // already in the previous segment.
    //

    if (PreviousSegment != NULL) {
        SegmentEnd = PreviousSegment->NextSequence;

        //
        // The next segment should not overlap the previous segment.
        //

        ASSERT((NextSegment == NULL) ||
               (SegmentEnd == NextSegment->SequenceNumber) ||
               (TCP_SEQUENCE_GREATER_THAN(NextSegment->SequenceNumber,
                                          SegmentEnd)));

        //
        // If the previous entry overlaps, then clip the insert region and move
        // the sequence number forward.
        //

        if (TCP_SEQUENCE_GREATER_THAN(SegmentEnd, InsertBegin)) {

            //
            // If the previous segment completely swallows this one, move the
            // sequence number forward and exit.
            //

            if (TCP_SEQUENCE_GREATER_THAN(SegmentEnd, InsertEnd)) {
                if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                    RelativeSequence = PreviousSegment->SequenceNumber -
                                       Socket->ReceiveInitialSequence;

                    RtlDebugPrint("RX %d, %d ignored, swallowed by %d, %d\n",
                                  (InsertBegin -
                                   Socket->ReceiveInitialSequence),
                                  InsertLength,
                                  RelativeSequence,
                                  PreviousSegment->Length);
                }

                ASSERT(*Length == (InsertEnd - InsertBegin));

                *SequenceNumber = InsertEnd;
                *Length = 0;
                goto TcpInsertReceivedDataSegmentEnd;
            }

            if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                RtlDebugPrint("Clipping RX begin from %d up to %d.\n",
                              InsertBegin - Socket->ReceiveInitialSequence,
                              SegmentEnd - Socket->ReceiveInitialSequence);
            }

            ClipAmount = SegmentEnd - InsertBegin;
            *Buffer += ClipAmount;
            InsertLength -= ClipAmount;
            InsertBegin = SegmentEnd;

            //
            // This always moves the sequence number as well.
            //

            *SequenceNumber = SegmentEnd;
            *Length -= ClipAmount;
            if (*Length == 0) {
                goto TcpInsertReceivedDataSegmentEnd;
            }

            ASSERT(InsertBegin != InsertEnd);
        }
    }

    //
    // If the next segment overlaps with the insert region, then clip the end
    // of the insert region. Do not update the sequence number.
    //

    if (NextSegment != NULL) {
        if (TCP_SEQUENCE_GREATER_THAN(InsertEnd, NextSegment->SequenceNumber)) {
            if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                RelativeSequence = NextSegment->SequenceNumber -
                                   Socket->ReceiveInitialSequence;

                SegmentEnd = NextSegment->NextSequence;
                if (TCP_SEQUENCE_GREATER_THAN(SegmentEnd, InsertEnd)) {
                    SegmentEnd = InsertEnd;
                }

                RtlDebugPrint("Clipping RX region %d, %d.\n",
                              RelativeSequence,
                              SegmentEnd - NextSegment->SequenceNumber);
            }

            InsertLength -= InsertEnd - NextSegment->SequenceNumber;
            InsertEnd = NextSegment->SequenceNumber;

            //
            // If this makes the current insert length 0, then exit.
            //

            if (InsertEnd == InsertBegin) {
                goto TcpInsertReceivedDataSegmentEnd;
            }
        }
    }

    ASSERT(InsertBegin != InsertEnd);

    //
    // Clip the incoming segment further by the receive window.
    //

    if (InsertLength > Socket->ReceiveWindowFreeSize) {
        InsertEnd = InsertBegin + Socket->ReceiveWindowFreeSize;
        InsertLength = Socket->ReceiveWindowFreeSize;
    }

    ASSERT(InsertEnd == (InsertBegin + InsertLength));

    if (PreviousSegment != NULL) {
        PreviousListEntry = &(PreviousSegment->Header.ListEntry);

    } else {
        PreviousListEntry = &(Socket->ReceivedSegmentList);
    }

    //
    // Create the new segment.
    //

    AllocationSize = sizeof(TCP_RECEIVED_SEGMENT) + InsertLength;
    NewSegment = (PTCP_RECEIVED_SEGMENT)NetpTcpAllocateSegment(Socket,
                                                               AllocationSize);

    if (NewSegment == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpInsertReceivedDataSegmentEnd;
    }

    NewSegment->SequenceNumber = InsertBegin;
    NewSegment->Length = InsertLength;
    NewSegment->NextSequence = InsertBegin + InsertLength;
    NewSegment->Flags = Header->Flags & TCP_RECEIVE_SEGMENT_HEADER_FLAG_MASK;
    RtlCopyMemory(NewSegment + 1, *Buffer, NewSegment->Length);
    *Buffer += NewSegment->Length;

    //
    // If this segment contains an urgent byte, then the segment will need to
    // be split into two or three segments. This is done so that the receiver
    // doesn't read "through" urgent data. For OOB inline, subsequent read
    // calls should return:
    //     1) Data before urgent byte
    //     2) Urgent byte
    //     3) Data after urgent byte
    //
    // For non-inline urgent data, read calls would return:
    //     1) Data before urgent byte
    //     2) Data after urgent byte
    //

    if ((Header->Flags & TCP_HEADER_FLAG_URGENT) != 0) {

        //
        // The urgent pointer points at the first non-urgent sequence number.
        // So the urgent byte is one back from that.
        //

        UrgentPointer = CPU_TO_NETWORK16(Header->NonUrgentOffset);
        if ((UrgentPointer != 0) && (UrgentPointer != (USHORT)-1)) {
            UrgentSequence += UrgentPointer - 1;
        }

        //
        // If this segment contains the urgent byte, split it.
        //

        if ((!TCP_SEQUENCE_LESS_THAN(UrgentSequence,
                                     NewSegment->SequenceNumber)) &&
            (TCP_SEQUENCE_LESS_THAN(UrgentSequence,
                                    NewSegment->NextSequence))) {

            //
            // The length of the remaining segment is from the non-urgent
            // sequence to the end.
            //

            TailLength = NewSegment->NextSequence - (UrgentSequence + 1);
            if (TailLength != 0) {
                AllocationSize =  sizeof(TCP_RECEIVED_SEGMENT) + TailLength;
                TailSegment = (PTCP_RECEIVED_SEGMENT)NetpTcpAllocateSegment(
                                                               Socket,
                                                               AllocationSize);

                if (TailSegment != NULL) {
                    TailSegment->SequenceNumber = UrgentSequence + 1;
                    TailSegment->Length = TailLength;
                    TailSegment->NextSequence = NewSegment->NextSequence;
                    TailSegment->Flags = NewSegment->Flags;
                    TailSource = NewSegment + 1;
                    TailSource += TailSegment->SequenceNumber -
                                  NewSegment->SequenceNumber;

                    RtlCopyMemory(TailSegment + 1, TailSource, TailLength);
                    INSERT_AFTER(&(NewSegment->Header.ListEntry),
                                 PreviousListEntry);

                //
                // On allocation failure, move the insert length back so that
                // these bytes are essentially unreceived.
                //

                } else {
                    InsertLength -= TailLength;
                }
            }

            //
            // Create a segment to hold the urgent byte. This may actually
            // have a length of zero if OOB data is not inline, but is still
            // important as it contains the up-down transition of the URGENT
            // flag, which breaks up the reader so it doesn't cross urgent
            // boundaries.
            //

            AllocationSize = sizeof(TCP_RECEIVED_SEGMENT) + 1;
            TailSegment = (PTCP_RECEIVED_SEGMENT)NetpTcpAllocateSegment(
                                                               Socket,
                                                               AllocationSize);

            if (TailSegment != NULL) {
                TailSegment->SequenceNumber = UrgentSequence;
                TailSegment->NextSequence = UrgentSequence + 1;
                TailSegment->Length = 0;
                TailSegment->Flags = NewSegment->Flags |
                                     TCP_RECEIVE_SEGMENT_FLAG_URGENT;

                if ((Socket->Flags & TCP_SOCKET_FLAG_URGENT_INLINE) != 0) {
                    TailSegment->Length = 1;
                    TailSource = NewSegment + 1;
                    TailSource += TailSegment->SequenceNumber -
                                  NewSegment->SequenceNumber;

                    RtlCopyMemory(TailSegment + 1, TailSource, 1);
                }

                INSERT_AFTER(&(NewSegment->Header.ListEntry),
                             PreviousListEntry);

            //
            // On allocation failure, move the insert length back past the
            // previous tail and this byte so they seem unreceived.
            //

            } else {
                InsertLength -= TailLength + 1;
            }

            //
            // Clip the first segment, as the urgent byte and data following it
            // are in subsequent segments. If allocations above failed, the
            // data will be resent as the insert length variable was rolled
            // back.
            //

            NewSegment->Length = UrgentSequence - NewSegment->SequenceNumber;
            NewSegment->NextSequence = UrgentSequence;
            IoSetIoObjectState(Socket->NetSocket.KernelSocket.IoState,
                               POLL_EVENT_IN_HIGH_PRIORITY,
                               TRUE);
        }
    }

    //
    // Insert the new segment into the list. It always goes after the previous
    // segment, unless the list is empty.
    //

    INSERT_AFTER(&(NewSegment->Header.ListEntry), PreviousListEntry);
    *InsertedSegment = TRUE;

    //
    // Move the sequence number up to the end of the insertion.
    //

    *SequenceNumber = InsertEnd;
    *Length -= InsertLength;

TcpInsertReceivedDataSegmentEnd:
    return Status;
}

VOID
NetpTcpSendPendingSegments (
    PTCP_SOCKET Socket,
    PULONGLONG CurrentTime
    )

/*++

Routine Description:

    This routine surveys the given socket and depending on what's appropriate
    may send new data out, retransmit unacknowledged data, or neither. This
    routine assumes that the socket's lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    CurrentTime - Supplies an optional time counter value for an approximate
        current time.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTCP_SEND_SEGMENT FirstSegment;
    PULONG Flags;
    BOOL InWindow;
    PTCP_SEND_SEGMENT LastSegment;
    ULONGLONG LocalCurrentTime;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    PTCP_SEND_SEGMENT Segment;
    ULONG SegmentBegin;
    KSTATUS Status;
    ULONG WindowBegin;
    ULONG WindowEnd;
    ULONG WindowSize;

    //
    // The connection may have been reset locally and be waiting on the lock to
    // close out the socket. The close routine releases the socket lock briefly
    // in order to acquire the socket list lock. If this is the case, don't
    // bother to send any more packets.
    //

    if ((Socket->Flags & TCP_SOCKET_FLAG_CONNECTION_RESET) != 0) {
        return;
    }

    if (LIST_EMPTY(&(Socket->OutgoingSegmentList)) != FALSE) {

        //
        // Check to see if the final FIN needs to be sent.
        //

        Flags = &(Socket->Flags);
        if (((*Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) != 0) &&
            ((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) &&
            ((Socket->State == TcpStateEstablished) ||
             (Socket->State == TcpStateCloseWait) ||
             (Socket->State == TcpStateSynReceived))) {

            ASSERT(Socket->SendNextNetworkSequence ==
                   Socket->SendFinalSequence);

            Socket->SendNextNetworkSequence += 1;
            NetpTcpTimerReleaseReference(Socket);
            NetpTcpSendControlPacket(Socket, TCP_HEADER_FLAG_FIN);
            if (Socket->State == TcpStateCloseWait) {
                NetpTcpSetState(Socket, TcpStateLastAcknowledge);

            } else {
                NetpTcpSetState(Socket, TcpStateFinWait1);
            }
        }

        return;
    }

    ASSERT((Socket->State == TcpStateEstablished) ||
           (Socket->State == TcpStateCloseWait) ||
           (Socket->State == TcpStateFinWait1));

    //
    // Determine the sequence numbers that can be sent at this time by getting
    // the window size and last acknowledge number received.
    //

    WindowSize = NetpTcpGetSendWindowSize(Socket);
    if (WindowSize == 0) {
        return;
    }

    WindowBegin = Socket->SendWindowUpdateAcknowledge;
    WindowEnd = WindowBegin + WindowSize;

    //
    // Loop adding as many segments as possible to the packets list.
    //

    LocalCurrentTime = 0;
    if (CurrentTime != NULL) {
        LocalCurrentTime = *CurrentTime;
    }

    FirstSegment = NULL;
    LastSegment = NULL;
    NET_INITIALIZE_PACKET_LIST(&PacketList);
    CurrentEntry = Socket->OutgoingSegmentList.Next;
    while (CurrentEntry != &(Socket->OutgoingSegmentList)) {
        Segment = LIST_VALUE(CurrentEntry, TCP_SEND_SEGMENT, Header.ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Determine if the packet is inside the eligible window. If the
        // segment is inside the window but bigger than the window, that's
        // tough nuggets for the receiver. It's their job to chop it up.
        //

        InWindow = FALSE;
        SegmentBegin = Segment->SequenceNumber + Segment->Offset;
        if (WindowEnd >= WindowBegin) {
            if ((SegmentBegin >= WindowBegin) && (SegmentBegin < WindowEnd)) {
                InWindow = TRUE;
            }

        } else {
            if ((SegmentBegin >= WindowBegin) || (SegmentBegin <= WindowEnd)) {
                InWindow = TRUE;
            }
        }

        //
        // If it's not in the window, stop now.
        //

        if (InWindow == FALSE) {
            break;
        }

        //
        // Check to see if the packet needs to be sent for the first
        // time.
        //

        if (Segment->SendAttemptCount == 0) {

            ASSERT(Segment->Offset == 0);

            Packet = NetpTcpCreatePacket(Socket, Segment);
            if (Packet == NULL) {
                break;
            }

            NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
            if (FirstSegment == NULL) {
                FirstSegment = Segment;
            }

            LastSegment = Segment;

            //
            // Update the next pointer and record the send time.
            //

            Socket->SendNextNetworkSequence = Segment->SequenceNumber +
                                              Segment->Length;

            if ((Segment->Flags & TCP_SEND_SEGMENT_FLAG_FIN) != 0) {
                Socket->SendNextNetworkSequence += 1;
                if (Socket->State == TcpStateCloseWait) {
                    NetpTcpSetState(Socket, TcpStateLastAcknowledge);

                } else {
                    NetpTcpSetState(Socket, TcpStateFinWait1);
                }
            }

            NetpTcpGetTransmitTimeoutInterval(Socket, Segment);
            Segment->SendAttemptCount += 1;

        //
        // This segment has been sent before. Check to see if enough
        // time has gone by without an acknowledge that it needs to be
        // retransmitted.
        //

        } else {
            if (LocalCurrentTime == 0) {
                LocalCurrentTime = HlQueryTimeCounter();
            }

            if (LocalCurrentTime >=
                Segment->LastSendTime + Segment->TimeoutInterval) {

                Packet = NetpTcpCreatePacket(Socket, Segment);
                if (Packet == NULL) {
                    break;
                }

                NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
                if (FirstSegment == NULL) {
                    FirstSegment = Segment;
                }

                LastSegment = Segment;
                NetpTcpTransmissionTimeout(Socket, Segment);
                NetpTcpGetTransmitTimeoutInterval(Socket, Segment);
                Segment->SendAttemptCount += 1;
                break;
            }
        }
    }

    //
    // Exit immediately if there was nothing to send.
    //

    if (NET_PACKET_LIST_EMPTY(&PacketList) != FALSE) {
        Status = STATUS_SUCCESS;
        goto TcpSendPendingSegmentsEnd;
    }

    //
    // Otherwise send off the whole group of packets.
    //

    Status = Socket->NetSocket.Network->Interface.Send(
                                            &(Socket->NetSocket),
                                            &(Socket->NetSocket.RemoteAddress),
                                            NULL,
                                            &PacketList);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("TCP segments failed to send %d.\n", Status);
        goto TcpSendPendingSegmentsEnd;
    }

    //
    // Update all the sent segments' last send time now that they have been
    // sent to the physical layer.
    //

    LocalCurrentTime = HlQueryTimeCounter();
    CurrentEntry = &(FirstSegment->Header.ListEntry);
    while (CurrentEntry != LastSegment->Header.ListEntry.Next) {
        Segment = LIST_VALUE(CurrentEntry, TCP_SEND_SEGMENT, Header.ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Segment->LastSendTime = LocalCurrentTime;
    }

TcpSendPendingSegmentsEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    if (CurrentTime != NULL) {
        *CurrentTime = LocalCurrentTime;
    }

    return;
}

KSTATUS
NetpTcpSendSegment (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    )

/*++

Routine Description:

    This routine transmits the given segment down the wire (unconditionally).
    This routine assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    Segment - Supplies a pointer to the segment to transmit.

Return Value:

    Status code.

--*/

{

    ULONGLONG LastSendTime;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    KSTATUS Status;

    //
    // Create the network packet to send down to the network layer.
    //

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Packet = NetpTcpCreatePacket(Socket, Segment);
    if (Packet == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpSendSegmentEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
    Status = Socket->NetSocket.Network->Interface.Send(
                                            &(Socket->NetSocket),
                                            &(Socket->NetSocket.RemoteAddress),
                                            NULL,
                                            &PacketList);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("TCP segment failed to send %d.\n", Status);
        goto TcpSendSegmentEnd;
    }

    //
    // Update the next pointer and window if this is the first time this packet
    // is being sent.
    //

    if (Segment->SendAttemptCount == 0) {

        ASSERT(Segment->Offset == 0);

        Socket->SendNextNetworkSequence = Segment->SequenceNumber +
                                          Segment->Length;

        if ((Segment->Flags & TCP_SEND_SEGMENT_FLAG_FIN) != 0) {
            Socket->SendNextNetworkSequence += 1;
            if (Socket->State == TcpStateCloseWait) {
                NetpTcpSetState(Socket, TcpStateLastAcknowledge);

            } else {
                NetpTcpSetState(Socket, TcpStateFinWait1);
            }
        }
    }

    LastSendTime = Segment->LastSendTime;
    Segment->LastSendTime = HlQueryTimeCounter();

    //
    // Double the timeout interval only if this retransmission was due to a
    // timeout.
    //

    if (LastSendTime + Segment->TimeoutInterval < Segment->LastSendTime) {
        NetpTcpGetTransmitTimeoutInterval(Socket, Segment);
    }

    Segment->SendAttemptCount += 1;
    Status = STATUS_SUCCESS;

TcpSendSegmentEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

PNET_PACKET_BUFFER
NetpTcpCreatePacket (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    )

/*++

Routine Description:

    This routine creates a network packet for the given TCP segment. It
    allocates a network packet buffer and fills out the TCP header.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    Segment - Supplies a pointer to the segment to use for packet
        initialization.

Return Value:

    Returns a pointer to the newly allocated packet buffer on success, or NULL
    on failure.

--*/

{

    USHORT HeaderFlags;
    PNET_PACKET_BUFFER Packet;
    ULONG SegmentLength;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    KSTATUS Status;

    //
    // Allocate the network buffer.
    //

    SegmentLength = Segment->Length - Segment->Offset;

    ASSERT(SegmentLength != 0);

    Packet = NULL;
    SizeInformation = &(Socket->NetSocket.PacketSizeInformation);
    Status = NetAllocateBuffer(SizeInformation->HeaderSize,
                               SegmentLength,
                               SizeInformation->FooterSize,
                               Socket->NetSocket.Link,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {

        ASSERT(Packet == NULL);

        goto TcpCreatePacketEnd;
    }

    //
    // Convert any flags into header flags. They match up for convenience.
    //

    HeaderFlags = Segment->Flags & TCP_SEND_SEGMENT_HEADER_FLAG_MASK;

    //
    // Copy the segment data over and fill out the TCP header.
    //

    RtlCopyMemory(Packet->Buffer + Packet->DataOffset,
                  (PUCHAR)(Segment + 1) + Segment->Offset,
                  SegmentLength);

    ASSERT(Packet->DataOffset >= sizeof(TCP_HEADER));

    Packet->DataOffset -= sizeof(TCP_HEADER);
    NetpTcpFillOutHeader(Socket,
                         Packet,
                         Segment->SequenceNumber + Segment->Offset,
                         HeaderFlags,
                         0,
                         0,
                         SegmentLength);

TcpCreatePacketEnd:
    return Packet;
}

VOID
NetpTcpFreeSentSegments (
    PTCP_SOCKET Socket,
    PULONGLONG CurrentTime
    )

/*++

Routine Description:

    This routine frees any packets in the send buffer that have been
    acknowledged by the remote host. This routine assumes that the socket is
    already locked.

Arguments:

    Socket - Supplies a pointer to the socket involved.

    CurrentTime - Supplies a pointer to a time counter value for an approximate
        current time. If it is set to 0, it may be updated by this routine.

Return Value:

    None.

--*/

{

    ULONG AcknowledgeNumber;
    PLIST_ENTRY CurrentEntry;
    PIO_OBJECT_STATE IoState;
    PTCP_SEND_SEGMENT Segment;
    ULONG SegmentBegin;
    ULONG SegmentEnd;
    BOOL SignalTransmitReadyEvent;

    SignalTransmitReadyEvent = FALSE;
    IoState = Socket->NetSocket.KernelSocket.IoState;
    AcknowledgeNumber = Socket->SendUnacknowledgedSequence;
    CurrentEntry = Socket->OutgoingSegmentList.Next;
    while (CurrentEntry != &(Socket->OutgoingSegmentList)) {
        Segment = LIST_VALUE(CurrentEntry, TCP_SEND_SEGMENT, Header.ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Figure out if the current acknowledge number is greater than the
        // current segment's ending sequence number.
        //

        SegmentBegin = Segment->SequenceNumber + Segment->Offset;
        SegmentEnd = Segment->SequenceNumber + Segment->Length;
        if ((AcknowledgeNumber == SegmentEnd) ||
            (TCP_SEQUENCE_GREATER_THAN(AcknowledgeNumber, SegmentEnd))) {

            //
            // If the remote host is acknowledging exactly this segment, then
            // let congestion control know that there's a new round trip time
            // in the house.
            //

            if ((AcknowledgeNumber == SegmentEnd) &&
                (Segment->SendAttemptCount == 1)) {

                if (*CurrentTime == 0) {
                    *CurrentTime = HlQueryTimeCounter();
                }

                NetpTcpProcessNewRoundTripTimeSample(
                                          Socket,
                                          *CurrentTime - Segment->LastSendTime);

            }

            if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                NetpTcpPrintSocketEndpoints(Socket, TRUE);
                RtlDebugPrint(
                         " Freeing TX segment %d size %d for ACK %d.\n",
                         Segment->SequenceNumber - Socket->SendInitialSequence,
                         Segment->Length,
                         AcknowledgeNumber - Socket->SendInitialSequence);
            }

            //
            // It would be weird for the remote host to acknowledge a packet not
            // yet sent.
            //

            ASSERT(Segment->SendAttemptCount != 0);

            LIST_REMOVE(&(Segment->Header.ListEntry));
            if (LIST_EMPTY(&(Socket->OutgoingSegmentList)) != FALSE) {
                NetpTcpTimerReleaseReference(Socket);
            }

            Socket->SendBufferFreeSize += Segment->Length;
            if (Socket->SendBufferFreeSize > Socket->SendBufferTotalSize) {
                Socket->SendBufferFreeSize = Socket->SendBufferTotalSize;
            }

            SignalTransmitReadyEvent = TRUE;
            NetpTcpFreeSegment(Socket, &(Segment->Header));

        //
        // If the current acknowledge number is in the middle of the segment,
        // update the offset. Don't touch the send buffer size in this case, as
        // the memory is still allocated.
        //

        } else if (TCP_SEQUENCE_GREATER_THAN(AcknowledgeNumber, SegmentBegin)) {

            //
            // It would be weird for the remote host to acknowledge a segment
            // not yet sent.
            //

            ASSERT(Segment->SendAttemptCount != 0);

            Segment->Offset = AcknowledgeNumber - Segment->SequenceNumber;
            if (NetTcpDebugPrintSequenceNumbers != FALSE) {
                NetpTcpPrintSocketEndpoints(Socket, TRUE);
                RtlDebugPrint(" Partial segment ACK: Segment %d, size %x, new "
                              "offset %x.\n",
                              Segment->SequenceNumber,
                              Segment->Length,
                              Segment->Offset);
            }

            break;

        //
        // This segment is wholly beyond the acknowledge number, so it and all
        // the others must remain.
        //

        } else {
            break;
        }
    }

    //
    // If some packets were freed up, signal the transmit ready event unless
    // the final sequence has been reached.
    //

    if ((SignalTransmitReadyEvent != FALSE) &&
        ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) == 0) &&
        ((Socket->State == TcpStateEstablished) ||
         (Socket->State == TcpStateCloseWait))) {

        IoSetIoObjectState(IoState, POLL_EVENT_OUT, TRUE);
    }

    return;
}

VOID
NetpTcpFreeSocketDataBuffers (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine frees many resources associated with a socket, preparing it
    to be deleted. This routine is usually called when a connection is reset
    or a close has completed. This routine assumes the socket lock is already
    acquired.

Arguments:

    Socket - Supplies a pointer to the socket involved.

Return Value:

    None.

--*/

{

    PTCP_INCOMING_CONNECTION IncomingConnection;
    PTCP_SEND_SEGMENT OutgoingSegment;
    PTCP_RECEIVED_SEGMENT ReceivedSegment;
    PTCP_SEGMENT_HEADER Segment;

    //
    // Loop through all outgoing packets and clean them up.
    //

    while (LIST_EMPTY(&(Socket->OutgoingSegmentList)) == FALSE) {
        OutgoingSegment = LIST_VALUE(Socket->OutgoingSegmentList.Next,
                                     TCP_SEND_SEGMENT,
                                     Header.ListEntry);

        LIST_REMOVE(&(OutgoingSegment->Header.ListEntry));
        if (LIST_EMPTY(&(Socket->OutgoingSegmentList)) != FALSE) {
            NetpTcpTimerReleaseReference(Socket);
        }

        MmFreePagedPool(OutgoingSegment);
    }

    //
    // Loop through all received packets and clean them up too.
    //

    while (LIST_EMPTY(&(Socket->ReceivedSegmentList)) == FALSE) {
        ReceivedSegment = LIST_VALUE(Socket->ReceivedSegmentList.Next,
                                     TCP_RECEIVED_SEGMENT,
                                     Header.ListEntry);

        LIST_REMOVE(&(ReceivedSegment->Header.ListEntry));
        MmFreePagedPool(ReceivedSegment);
    }

    //
    // Release the list of free segments.
    //

    while (LIST_EMPTY(&(Socket->FreeSegmentList)) == FALSE) {
        Segment = LIST_VALUE(Socket->FreeSegmentList.Next,
                             TCP_SEGMENT_HEADER,
                             ListEntry);

        LIST_REMOVE(&(Segment->ListEntry));
        MmFreePagedPool(Segment);
    }

    //
    // Also free any pending incoming connections.
    //

    while (LIST_EMPTY(&(Socket->IncomingConnectionList)) == FALSE) {
        IncomingConnection = LIST_VALUE(Socket->IncomingConnectionList.Next,
                                        TCP_INCOMING_CONNECTION,
                                        ListEntry);

        LIST_REMOVE(&(IncomingConnection->ListEntry));
        Socket->IncomingConnectionCount -= 1;
        IoClose(IncomingConnection->IoHandle);
        MmFreePagedPool(IncomingConnection);
    }

    ASSERT(Socket->IncomingConnectionCount == 0);

    return;
}

VOID
NetpTcpShutdownUnlocked (
    PTCP_SOCKET TcpSocket,
    ULONG ShutdownType
    )

/*++

Routine Description:

    This routine shuts down communication with a given socket based on the
    supplied shutdown state. This routine assumes that the socket lock is
    already held.

Arguments:

    TcpSocket - Supplies a pointer to the TCP socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    None.

--*/

{

    BOOL ResetSent;

    ASSERT(KeIsQueuedLockHeld(TcpSocket->Lock) != FALSE);

    ResetSent = FALSE;
    if ((ShutdownType & SOCKET_SHUTDOWN_READ) != 0) {
        NetpTcpShutdownReceive(TcpSocket, &ResetSent);
    }

    if ((ResetSent == FALSE) &&
        ((ShutdownType & SOCKET_SHUTDOWN_WRITE) != 0)) {

        NetpTcpShutdownTransmit(TcpSocket);
    }

    return;
}

VOID
NetpTcpShutdownTransmit (
    PTCP_SOCKET TcpSocket
    )

/*++

Routine Description:

    This routine shuts down the transmit side of communications, marking the
    last sequence number and sending a FIN if already caught up. This routine
    assumes the TCP socket lock is already held.

Arguments:

    TcpSocket - Supplies a pointer to the socket to shut down.

Return Value:

    None.

--*/

{

    PULONG Flags;
    PIO_OBJECT_STATE IoState;
    PTCP_SEND_SEGMENT LastSegment;
    PLIST_ENTRY OutgoingSegmentList;

    ASSERT((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_WRITE) != 0);
    ASSERT(KeIsQueuedLockHeld(TcpSocket->Lock) != FALSE);

    IoState = TcpSocket->NetSocket.KernelSocket.IoState;
    switch (TcpSocket->State) {

    //
    // Some states don't require a FIN to be sent; either the connection wasn't
    // established enough, or it's already been sent.
    //

    case TcpStateClosed:
    case TcpStateFinWait1:
    case TcpStateFinWait2:
    case TcpStateClosing:
    case TcpStateLastAcknowledge:
    case TcpStateTimeWait:
        break;

    //
    // Some states don't need a FIN, but should have the transmit event
    // signaled for anybody polling on this socket.
    //

    case TcpStateInitialized:
    case TcpStateListening:
    case TcpStateSynSent:
        IoSetIoObjectState(IoState, POLL_EVENT_OUT, TRUE);
        break;

    //
    // In the states with active connections, send a FIN segment (or at
    // least queue that one needs to be sent).
    //

    case TcpStateSynReceived:
    case TcpStateEstablished:
    case TcpStateCloseWait:

        //
        // If the final sequence is yet to be determined, do it now and prepare
        // to send the FIN. Only do this once as the socket is guaranteed to
        // move out of the three above states. Another shutdown attempt should
        // have no effect.
        //

        Flags = &(TcpSocket->Flags);
        if ((*Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) == 0) {

            //
            // Mark the "end of the line" sequence number.
            //

            TcpSocket->SendFinalSequence = TcpSocket->SendNextBufferSequence;
            TcpSocket->SendNextBufferSequence += 1;

            //
            // If the outgoing segment list is not empty and the last segment
            // has not yet been sent, then the FIN can be sent along with it.
            //

            ASSERT((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0);

            OutgoingSegmentList = &(TcpSocket->OutgoingSegmentList);
            if (LIST_EMPTY(OutgoingSegmentList) == FALSE) {
                LastSegment = LIST_VALUE(OutgoingSegmentList->Previous,
                                         TCP_SEND_SEGMENT,
                                         Header.ListEntry);

                if (LastSegment->SendAttemptCount == 0) {
                    LastSegment->Flags |= TCP_SEND_SEGMENT_FLAG_FIN;
                    *Flags |= TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA;
                }
            }

            //
            // Now that the final sequence and how the FIN will be sent have
            // been determined, mark the final sequence valid.
            //

            *Flags |= TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID;

            //
            // No more sends are expected, so unset the transmit ready event,
            // as it gets reused as a "close operation finished" event.
            //

            IoSetIoObjectState(IoState, POLL_EVENT_OUT, FALSE);

            //
            // If the acknowledged data is all caught up, send the FIN right
            // away.
            //

            if (TcpSocket->SendUnacknowledgedSequence ==
                TcpSocket->SendFinalSequence) {

                ASSERT((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0);
                ASSERT((TcpSocket->SendNextNetworkSequence + 1) ==
                        TcpSocket->SendNextBufferSequence);

                TcpSocket->SendNextNetworkSequence += 1;
                NetpTcpSendControlPacket(TcpSocket, TCP_HEADER_FLAG_FIN);
                if (TcpSocket->State == TcpStateCloseWait) {
                    NetpTcpSetState(TcpSocket, TcpStateLastAcknowledge);

                } else {
                    NetpTcpSetState(TcpSocket, TcpStateFinWait1);
                }

            //
            // Otherwise if the FIN cannot be sent with a data packet, add a
            // reference to the TCP timer to make sure it gets sent.
            //

            } else if ((*Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) {
                NetpTcpTimerAddReference(TcpSocket);
            }
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
NetpTcpShutdownReceive (
    PTCP_SOCKET TcpSocket,
    PBOOL ResetSent
    )

/*++

Routine Description:

    This routine shuts down the receive side of communications, doing nothing
    if all the received data has been read or sending a RST if it has not. This
    routine assumes the TCP socket lock is already held.

Arguments:

    TcpSocket - Supplies a pointer to the socket to shut down.

    ResetSent - Supplies a pointer to a boolean that receives status as to
        whether or not a reset was sent.

Return Value:

    None.

--*/

{

    ASSERT((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0);
    ASSERT(KeIsQueuedLockHeld(TcpSocket->Lock) != FALSE);

    *ResetSent = FALSE;
    switch (TcpSocket->State) {

    //
    // There is nothing to do for most states. Either a connection was never
    // initialized by the other side or a FIN has been received from the other
    // side.
    //

    case TcpStateClosed:
    case TcpStateClosing:
    case TcpStateLastAcknowledge:
    case TcpStateTimeWait:
    case TcpStateInitialized:
    case TcpStateListening:
    case TcpStateSynSent:
        break;

    //
    // In the states where packets can come in and a FIN needs to be sent to
    // close the connection, send a RST if not all of the received data has
    // been read.
    //

    case TcpStateFinWait1:
    case TcpStateFinWait2:
    case TcpStateSynReceived:
    case TcpStateEstablished:
    case TcpStateCloseWait:
        if (LIST_EMPTY(&(TcpSocket->ReceivedSegmentList)) == FALSE) {
            NetpTcpSendControlPacket(TcpSocket, TCP_HEADER_FLAG_RESET);
            NetpTcpCloseOutSocket(TcpSocket, FALSE);
            *ResetSent = TRUE;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
NetpTcpCloseOutSocket (
    PTCP_SOCKET Socket,
    BOOL InsideWorker
    )

/*++

Routine Description:

    This routine sets the socket to the closed state. This routine assumes the
    socket lock is already held, and WILL briefly release it unless inside
    the TCP worker thread.

Arguments:

    Socket - Supplies a pointer to the socket to destroy.

    InsideWorker - Supplies a boolean indicating that this routine is being
        called from inside the TCP worker thread and that all locks are already
        held.

Return Value:

    Status code.

--*/

{

    BOOL CloseSocket;
    PIO_OBJECT_STATE IoState;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    CloseSocket = FALSE;
    IoState = Socket->NetSocket.KernelSocket.IoState;
    Status = STATUS_SUCCESS;

    //
    // Check to see if the socket needs to be closed. Be careful as the state
    // can change once the lock is releaed.
    //

    if (Socket->State != TcpStateClosed) {
        CloseSocket = TRUE;
        if (InsideWorker == FALSE) {

            //
            // Release the socket lock to prevent a deadlock (the TCP worker
            // thread acquires the socket list lock, then the socket). This
            // shouldn't be a problem for callers since closing out the socket
            // is pretty much the last thing done to a socket. Then acquire the
            // socket list lock and the socket lock (now in the right order)
            // and remove the socket from being observable by the worker
            // thread.
            //

            KeReleaseQueuedLock(Socket->Lock);
            KeAcquireQueuedLock(NetTcpSocketListLock);
            KeAcquireQueuedLock(Socket->Lock);

            //
            // While the lock was released, the socket may have been closed.
            // Prepare to bail out on the rest of the work.
            //

            if (Socket->State == TcpStateClosed) {
                CloseSocket = FALSE;

            //
            // While the lock is held, remove the socket from the global list.
            //

            } else {
                LIST_REMOVE(&(Socket->ListEntry));
                Socket->ListEntry.Next = NULL;
            }

            KeReleaseQueuedLock(NetTcpSocketListLock);
        }
    }

    //
    // Close out the socket if it was deteremined to not be in the closed state
    // after all the lock ordering checks above.
    //

    if (CloseSocket != FALSE) {

        ASSERT(Socket->State != TcpStateClosed);

        //
        // Be careful as the socket may have been removed from the global list
        // above in the case where the global lock was not held upon entrance
        // into this routine.
        //

        if (Socket->ListEntry.Next != NULL) {

            ASSERT(InsideWorker != FALSE);

            LIST_REMOVE(&(Socket->ListEntry));
            Socket->ListEntry.Next = NULL;
        }

        //
        // Leave the socket lock held to prevent late senders from getting
        // involved, close the socket.
        //

        NetpTcpSetState(Socket, TcpStateClosed);
        Status = Socket->NetSocket.Network->Interface.Close(
                                                         &(Socket->NetSocket));

        //
        // Release the reference taken for the TCP connection, after which the
        // socket can't be touched as it may get destroyed.
        //

        IoSocketReleaseReference(&(Socket->NetSocket.KernelSocket));

    //
    // Just signal the event, the socket's already closed.
    //

    } else {
        IoSetIoObjectState(IoState, TCP_POLL_EVENT_IO, TRUE);
    }

    return Status;
}

VOID
NetpTcpHandleIncomingConnection (
    PTCP_SOCKET ListeningSocket,
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PTCP_HEADER Header
    )

/*++

Routine Description:

    This routine handles an incoming TCP connection on a listening socket. It
    spawns a new TCP socket bound to the remote address, sends the SYN+ACK,
    and adds an entry onto the listening socket's incoming connection list.

Arguments:

    ListeningSocket - Supplies a pointer to the original listening socket that
        just saw an incoming connection request.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses
        for the connection request.

    Header - Supplies a pointer to the valid TCP header containing the SYN.

Return Value:

    None.

--*/

{

    PTCP_INCOMING_CONNECTION IncomingConnection;
    PIO_OBJECT_STATE IoState;
    PNETWORK_ADDRESS LocalAddress;
    BOOL LockHeld;
    ULONG NetSocketFlags;
    ULONG NetworkProtocol;
    PIO_HANDLE NewIoHandle;
    PTCP_SOCKET NewTcpSocket;
    PNETWORK_ADDRESS RemoteAddress;
    ULONG RemoteSequence;
    ULONG ResetFlags;
    ULONG ResetSequenceNumber;
    KSTATUS Status;

    LockHeld = FALSE;
    NewIoHandle = NULL;
    NewTcpSocket = NULL;
    LocalAddress = ReceiveContext->Destination;
    RemoteAddress = ReceiveContext->Source;
    IncomingConnection = MmAllocatePagedPool(sizeof(TCP_INCOMING_CONNECTION),
                                             TCP_ALLOCATION_TAG);

    if (IncomingConnection == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TcpHandleIncomingConnectionEnd;
    }

    RtlZeroMemory(IncomingConnection, sizeof(TCP_INCOMING_CONNECTION));

    //
    // Create a new socket for this connection.
    //

    ASSERT(LocalAddress->Domain == RemoteAddress->Domain);
    ASSERT(ListeningSocket->NetSocket.KernelSocket.Protocol ==
           SOCKET_INTERNET_PROTOCOL_TCP);

    NetworkProtocol = ListeningSocket->NetSocket.KernelSocket.Protocol;
    Status = IoSocketCreate(LocalAddress->Domain,
                            NetSocketStream,
                            NetworkProtocol,
                            0,
                            &NewIoHandle);

    if (!KSUCCESS(Status)) {
        goto TcpHandleIncomingConnectionEnd;
    }

    Status = IoGetSocketFromHandle(NewIoHandle, (PVOID)&NewTcpSocket);
    if (!KSUCCESS(Status)) {
        goto TcpHandleIncomingConnectionEnd;
    }

    //
    // Carry over the net socket flags from the original socket. Also record
    // that this socket was copied from a listener to allow reuse of the local
    // port on bind.
    //

    NetSocketFlags = ListeningSocket->NetSocket.Flags &
                     NET_SOCKET_FLAGS_INHERIT_MASK;

    NetSocketFlags |= NET_SOCKET_FLAG_FORKED_LISTENER;
    RtlAtomicOr32(&(NewTcpSocket->NetSocket.Flags), NetSocketFlags);
    KeAcquireQueuedLock(NewTcpSocket->Lock);
    LockHeld = TRUE;

    //
    // Bind the new socket to the local address.
    //

    Status = NewTcpSocket->NetSocket.Network->Interface.BindToAddress(
                                                    &(NewTcpSocket->NetSocket),
                                                    ReceiveContext->Link,
                                                    LocalAddress,
                                                    0);

    if (!KSUCCESS(Status)) {
        goto TcpHandleIncomingConnectionEnd;
    }

    //
    // Bind the new socket to the remote address.
    //

    Status = NewTcpSocket->NetSocket.Network->Interface.Connect(
                                                    &(NewTcpSocket->NetSocket),
                                                    RemoteAddress);

    if (!KSUCCESS(Status)) {
        goto TcpHandleIncomingConnectionEnd;
    }

    //
    // Inherit configurable options from the listening socket.
    //

    ASSERT(ListeningSocket->SendBufferTotalSize ==
           ListeningSocket->SendBufferFreeSize);

    NewTcpSocket->SendBufferTotalSize = ListeningSocket->SendBufferTotalSize;
    NewTcpSocket->SendBufferFreeSize = ListeningSocket->SendBufferFreeSize;
    NewTcpSocket->SendTimeout = ListeningSocket->SendTimeout;

    ASSERT(ListeningSocket->ReceiveWindowTotalSize ==
           ListeningSocket->ReceiveWindowFreeSize);

    NewTcpSocket->ReceiveWindowTotalSize =
                                       ListeningSocket->ReceiveWindowTotalSize;

    NewTcpSocket->ReceiveWindowFreeSize =
                                        ListeningSocket->ReceiveWindowFreeSize;

    NewTcpSocket->ReceiveWindowScale = ListeningSocket->ReceiveWindowScale;
    NewTcpSocket->ReceiveTimeout = ListeningSocket->ReceiveTimeout;
    NewTcpSocket->ReceiveMinimum = ListeningSocket->ReceiveMinimum;
    if ((ListeningSocket->Flags & TCP_SOCKET_FLAG_LINGER_ENABLED) != 0) {
        NewTcpSocket->Flags |= TCP_SOCKET_FLAG_LINGER_ENABLED;
    }

    NewTcpSocket->LingerTimeout = ListeningSocket->LingerTimeout;

    //
    // Copy any network specific socket options.
    //

    if (NewTcpSocket->NetSocket.Network->Interface.CopyInformation != NULL) {
        Status = NewTcpSocket->NetSocket.Network->Interface.CopyInformation(
                                                &(NewTcpSocket->NetSocket),
                                                &(ListeningSocket->NetSocket));

        if (!KSUCCESS(Status)) {
            goto TcpHandleIncomingConnectionEnd;
        }
    }

    //
    // Re-parse any options coming from the SYN packet and set up the sequence
    // numbers.
    //

    NetpTcpProcessPacketOptions(NewTcpSocket, Header, ReceiveContext->Packet);
    RemoteSequence = NETWORK_TO_CPU32(Header->SequenceNumber);
    NewTcpSocket->ReceiveInitialSequence = RemoteSequence;
    NewTcpSocket->ReceiveNextSequence = RemoteSequence + 1;
    NewTcpSocket->ReceiveUnreadSequence = NewTcpSocket->ReceiveNextSequence;

    //
    // If there are already too many connections queued, send a RESET and kill
    // this one.
    //

    if (ListeningSocket->IncomingConnectionCount >=
        ListeningSocket->NetSocket.MaxIncomingConnections) {

        Status = STATUS_TOO_MANY_CONNECTIONS;
        goto TcpHandleIncomingConnectionEnd;
    }

    //
    // Set the state, which will send out an SYN+ACK and kick off some retries.
    //

    NetpTcpSetState(NewTcpSocket, TcpStateSynReceived);
    IncomingConnection->IoHandle = NewIoHandle;
    ListeningSocket->IncomingConnectionCount += 1;
    INSERT_BEFORE(&(IncomingConnection->ListEntry),
                  &(ListeningSocket->IncomingConnectionList));

    IoState = ListeningSocket->NetSocket.KernelSocket.IoState;
    IoSetIoObjectState(IoState, POLL_EVENT_IN, TRUE);
    Status = STATUS_SUCCESS;

TcpHandleIncomingConnectionEnd:
    if (!KSUCCESS(Status)) {
        if (Status == STATUS_TOO_MANY_CONNECTIONS) {
            ResetSequenceNumber = NETWORK_TO_CPU32(
                                                 Header->AcknowledgmentNumber);

            ResetFlags = TCP_HEADER_FLAG_RESET | TCP_HEADER_FLAG_ACKNOWLEDGE;
            NewTcpSocket->SendUnacknowledgedSequence = ResetSequenceNumber;
            NetpTcpSendControlPacket(NewTcpSocket, ResetFlags);
        }

        if (IncomingConnection != NULL) {
            MmFreePagedPool(IncomingConnection);
        }
    }

    if (LockHeld != FALSE) {

        ASSERT(NewTcpSocket != NULL);

        KeReleaseQueuedLock(NewTcpSocket->Lock);
    }

    //
    // Now that the socket's lock has been released, close the handle.
    //

    if (!KSUCCESS(Status)) {
        if (NewIoHandle != NULL) {
            IoClose(NewIoHandle);
        }
    }

    return;
}

VOID
NetpTcpSetState (
    PTCP_SOCKET Socket,
    TCP_STATE NewState
    )

/*++

Routine Description:

    This routine sets the given TCP socket's state, performing any default
    behavior that should happen once that state is reached.

Arguments:

    Socket - Supplies a pointer to the socket whose state is being updated.

    NewState - Supplies the new TCP state for the socket.

Return Value:

    None.

--*/

{

    ULONG Flags;
    PNET_SOCKET NetSocket;
    TCP_STATE OldState;
    BOOL WithAcknowledge;

    OldState = Socket->State;
    Socket->PreviousState = OldState;
    Socket->State = NewState;

    //
    // Modify the socket based on the new state.
    //

    switch (NewState) {
    case TcpStateInitialized:

        ASSERT((OldState == TcpStateInvalid) ||
               (OldState == TcpStateSynReceived) ||
               (OldState == TcpStateSynSent));

        //
        // When transitioning to the initialized state from the SYN-sent or
        // SYN-received state, disconnect the socket from its remote address
        // and reset the retry values and backtrack on the buffer sequences.
        //

        if ((OldState == TcpStateSynReceived) ||
            (OldState == TcpStateSynSent)) {

            NetSocket = &(Socket->NetSocket);
            NetSocket->Network->Interface.Disconnect(NetSocket);
            Socket->RetryTime = 0;
            Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
            Socket->SendNextBufferSequence = Socket->SendInitialSequence;
            Socket->SendNextNetworkSequence = Socket->SendInitialSequence;
            NetpTcpTimerReleaseReference(Socket);
        }

        break;

    case TcpStateListening:

        ASSERT(OldState == TcpStateInitialized);

        break;

    case TcpStateSynSent:

        ASSERT(OldState == TcpStateInitialized);

        //
        // Fall through to the SYN received state. They are almost identical.
        //

    case TcpStateSynReceived:

        ASSERT((OldState == TcpStateInitialized) ||
               (OldState == TcpStateSynSent));

        if (OldState == TcpStateInitialized) {

            //
            // Make sure that the error event is not signalled. Give the socket
            // a new chance to connect.
            //

            NET_SOCKET_CLEAR_LAST_ERROR(&(Socket->NetSocket));
            IoSetIoObjectState(Socket->NetSocket.KernelSocket.IoState,
                               POLL_EVENT_ERROR,
                               FALSE);

            Socket->SendNextBufferSequence += 1;
            Socket->SendNextNetworkSequence += 1;
            TCP_UPDATE_RETRY_TIME(Socket);
            TCP_SET_DEFAULT_TIMEOUT(Socket);
            NetpTcpTimerAddReference(Socket);
        }

        WithAcknowledge = FALSE;
        if (NewState == TcpStateSynReceived) {
            WithAcknowledge = TRUE;
        }

        NetpTcpSendSyn(Socket, WithAcknowledge);
        break;

    case TcpStateEstablished:

        ASSERT((OldState == TcpStateSynReceived) ||
               (OldState == TcpStateSynSent));

        Socket->RetryTime = 0;
        Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
        NetpTcpTimerReleaseReference(Socket);
        NetpTcpCongestionConnectionEstablished(Socket);
        IoSetIoObjectState(Socket->NetSocket.KernelSocket.IoState,
                           POLL_EVENT_OUT,
                           TRUE);

        break;

    case TcpStateFinWait1:

        ASSERT((OldState == TcpStateSynReceived) ||
               (OldState == TcpStateEstablished));

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) {
            Socket->RetryTime = 0;
            Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
            TCP_UPDATE_RETRY_TIME(Socket);
            TCP_SET_DEFAULT_TIMEOUT(Socket);
            if (OldState == TcpStateEstablished) {
                NetpTcpTimerAddReference(Socket);
            }
        }

        break;

    case TcpStateFinWait2:

        ASSERT(OldState == TcpStateFinWait1);

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) {
            NetpTcpTimerReleaseReference(Socket);
        }

        break;

    //
    // The close-wait state is reached when a FIN is received while in the
    // established state. Not much to do here. Data can still be sent and the
    // socket is just waiting on a local close.
    //

    case TcpStateCloseWait:

        ASSERT(OldState == TcpStateEstablished);

        break;

    //
    // The closing state is still waiting on a FIN to be ACK'd. But since the
    // remote is clearly still alive, reset the retry and timeout. Keep the
    // reference on the timer taken during FIN-Wait1 alive. Keep in mind that
    // this logic is only valid if there isn't more data to send, as evidenced
    // by whether or not the FIN is to be sent with data.
    //

    case TcpStateClosing:

        ASSERT(OldState == TcpStateFinWait1);

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) {
            Socket->RetryTime = 0;
            Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
            TCP_UPDATE_RETRY_TIME(Socket);
            TCP_SET_DEFAULT_TIMEOUT(Socket);
        }

        break;

    //
    // The last acknowledge state is waiting for a FIN to be acknowledged.
    // Reinitialize the retry period for resending the FIN and set the default
    // timeout. The close wait state does not have a reference on the timer, so
    // take a new one. This only applies if there isn't data being sent with
    // the FIN.
    //

    case TcpStateLastAcknowledge:

        ASSERT(OldState == TcpStateCloseWait);

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) {
            Socket->RetryTime = 0;
            Socket->RetryWaitPeriod = TCP_INITIAL_RETRY_WAIT_PERIOD;
            TCP_UPDATE_RETRY_TIME(Socket);
            TCP_SET_DEFAULT_TIMEOUT(Socket);
            NetpTcpTimerAddReference(Socket);
        }

        break;

    //
    // The time wait state just sits around until the timeout expires. Set the
    // default timeout and take a reference on the timer if coming from a state
    // that does not have a reference on the timer.
    //

    case TcpStateTimeWait:

        ASSERT((OldState == TcpStateFinWait1) ||
               (OldState == TcpStateFinWait2) ||
               (OldState == TcpStateClosing));

        RtlAtomicOr32(&(Socket->NetSocket.Flags), NET_SOCKET_FLAG_TIME_WAIT);
        TCP_SET_DEFAULT_TIMEOUT(Socket);
        if ((OldState == TcpStateFinWait2) ||
            ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) != 0)) {

            NetpTcpTimerAddReference(Socket);
        }

        break;

    //
    // The closed state can be reached from just about every other state. If
    // the old state had a reference on the timer, then release that reference.
    //

    case TcpStateClosed:
        if ((TCP_IS_SYN_RETRY_STATE(OldState) != FALSE) ||
            ((TCP_IS_FIN_RETRY_STATE(OldState) != FALSE) &&
             ((Socket->Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0)) ||
            (OldState == TcpStateTimeWait)) {

            NetpTcpTimerReleaseReference(Socket);
        }

        //
        // If a more forceful close arrives after a transmit shutdown, the
        // socket still have a reference on the timer in order to send a FIN
        // once all the data has been sent. That's not going to happen now.
        //

        Flags = Socket->Flags;
        if (((Flags & TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID) != 0) &&
            ((Flags & TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA) == 0) &&
            ((OldState == TcpStateEstablished) ||
             (OldState == TcpStateCloseWait) ||
             (OldState == TcpStateSynReceived))) {

            NetpTcpTimerReleaseReference(Socket);
        }

        if ((Socket->Flags & TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE) != 0) {
            Socket->Flags &= ~TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE;
            NetpTcpTimerReleaseReference(Socket);
        }

        NetpTcpFreeSocketDataBuffers(Socket);
        IoSetIoObjectState(Socket->NetSocket.KernelSocket.IoState,
                           TCP_POLL_EVENT_IO,
                           TRUE);

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
NetpTcpSendSyn (
    PTCP_SOCKET Socket,
    BOOL WithAcknowledge
    )

/*++

Routine Description:

    This routine sends a SYN packet with all the fancy options on it.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    WithAcknowledge - Supplies a boolean indicating whether this should just be
        a plain SYN (FALSE) or a SYN+ACK (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG ControlFlags;
    ULONG DataSize;
    ULONG MaximumSegmentSize;
    PNET_SOCKET NetSocket;
    PNET_PACKET_BUFFER Packet;
    PUCHAR PacketBuffer;
    NET_PACKET_LIST PacketList;
    ULONG SavedWindowScale;
    ULONG SavedWindowSize;
    KSTATUS Status;

    NetSocket = &(Socket->NetSocket);
    NET_INITIALIZE_PACKET_LIST(&PacketList);
    DataSize = TCP_OPTION_MSS_SIZE;
    if ((Socket->Flags & TCP_SOCKET_FLAG_WINDOW_SCALING) != 0) {
        DataSize += TCP_OPTION_WINDOW_SCALE_SIZE + TCP_OPTION_NOP_SIZE;
    }

    //
    // Allocate the SYN packet that will kick things off with the remote host.
    //

    Packet = NULL;
    Status = NetAllocateBuffer(NetSocket->PacketSizeInformation.HeaderSize,
                               DataSize,
                               NetSocket->PacketSizeInformation.FooterSize,
                               NetSocket->Link,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto TcpSendSynEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    //
    // Initialize the options of the SYN packet. The first option will be the
    // Maximum Segment Size.
    //

    PacketBuffer = (PUCHAR)(Packet->Buffer + Packet->DataOffset);
    *PacketBuffer = TCP_OPTION_MAXIMUM_SEGMENT_SIZE;
    PacketBuffer += 1;
    *PacketBuffer = TCP_OPTION_MSS_SIZE;
    PacketBuffer += 1;
    MaximumSegmentSize = NetSocket->PacketSizeInformation.MaxPacketSize -
                         NetSocket->PacketSizeInformation.HeaderSize -
                         NetSocket->PacketSizeInformation.FooterSize;

    if (MaximumSegmentSize > MAX_USHORT) {
        MaximumSegmentSize = MAX_USHORT;
    }

    //
    // Save the maximum segment size for future use.
    //

    Socket->ReceiveMaxSegmentSize = MaximumSegmentSize;
    *((PUSHORT)PacketBuffer) = CPU_TO_NETWORK16(MaximumSegmentSize);
    PacketBuffer += sizeof(USHORT);

    //
    // Add the Window Scale option if the remote supports it.
    //

    if ((Socket->Flags & TCP_SOCKET_FLAG_WINDOW_SCALING) != 0) {
        *PacketBuffer = TCP_OPTION_WINDOW_SCALE;
        PacketBuffer += 1;
        *PacketBuffer = TCP_OPTION_WINDOW_SCALE_SIZE;
        PacketBuffer += 1;
        *PacketBuffer = (UCHAR)(Socket->ReceiveWindowScale);
        PacketBuffer += 1;

        //
        // Add a padding option to get the header length to a multiple of
        // 32-bits (as the header length field can only express such granules).
        //

        *PacketBuffer = TCP_OPTION_NOP;
        PacketBuffer += 1;
    }

    //
    // Add the TCP header and send this packet down the wire. Remember that the
    // semantics of the ACK flag are different for the function below, so by
    // passing it here it's being cleared in the header (making SYN the only
    // flag set in the packet).
    //

    ControlFlags = TCP_HEADER_FLAG_SYN;
    if (WithAcknowledge == FALSE) {
        ControlFlags |= TCP_HEADER_FLAG_ACKNOWLEDGE;
    }

    ASSERT(Packet->DataOffset >= sizeof(TCP_HEADER));

    Packet->DataOffset -= sizeof(TCP_HEADER);

    //
    // The SYN packet's window field should never be scaled. Temporarily
    // disable the receive window scale and cap the size.
    //

    SavedWindowScale = Socket->ReceiveWindowScale;
    Socket->ReceiveWindowScale = 0;
    SavedWindowSize = Socket->ReceiveWindowFreeSize;
    if (Socket->ReceiveWindowFreeSize > MAX_USHORT) {
        Socket->ReceiveWindowFreeSize = MAX_USHORT;
    }

    NetpTcpFillOutHeader(Socket,
                         Packet,
                         Socket->SendInitialSequence,
                         ControlFlags,
                         DataSize,
                         0,
                         0);

    Socket->ReceiveWindowScale = SavedWindowScale;
    Socket->ReceiveWindowFreeSize = SavedWindowSize;
    Status = NetSocket->Network->Interface.Send(NetSocket,
                                                &(NetSocket->RemoteAddress),
                                                NULL,
                                                &PacketList);

    if (!KSUCCESS(Status)) {
        goto TcpSendSynEnd;
    }

TcpSendSynEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

VOID
NetpTcpTimerAddReference (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine increments the reference count on the TCP timer, ensuring that
    it runs.

Arguments:

    Socket - Supplies a pointer to the TCP socket requesting the timer. This
        routine assumes the TCP lock is already held.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    //
    // Increment the reference count in the socket. If it's already got
    // references, no action is needed on the global count.
    //

    Socket->TimerReferenceCount += 1;

    ASSERT((Socket->TimerReferenceCount > 0) &&
           (Socket->TimerReferenceCount < TCP_TIMER_MAX_REFERENCE));

    if (Socket->TimerReferenceCount > 1) {
        return;
    }

    //
    // This is the first reference the socket is taking on the global timer.
    // Incremement the reference count of the global timer, and maybe queue it.
    //

    OldReferenceCount = RtlAtomicAdd32(&NetTcpTimerReferenceCount, 1);

    ASSERT(OldReferenceCount < TCP_TIMER_MAX_REFERENCE);

    if (OldReferenceCount == 0) {
        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            RtlDebugPrint("TCP: Enabled periodic timer.\n");
        }

        NetpTcpQueueTcpTimer();
    }

    return;
}

ULONG
NetpTcpTimerReleaseReference (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine decrements the reference count on the TCP timer, canceling it
    if no one else is using it.

Arguments:

    Socket - Supplies an optional pointer to the socket that is releasing the
        timer reference.

Return Value:

    Returns the old reference count on the TCP timer. The return value should
    only be observed if the socket parameter is NULL.

--*/

{

    ULONG OldReferenceCount;

    if (Socket != NULL) {

        ASSERT((Socket->TimerReferenceCount > 0) &&
               (Socket->TimerReferenceCount < TCP_TIMER_MAX_REFERENCE));

        Socket->TimerReferenceCount -= 1;
        if (Socket->TimerReferenceCount != 0) {
            return Socket->TimerReferenceCount;
        }
    }

    OldReferenceCount = RtlAtomicAdd32(&NetTcpTimerReferenceCount, (ULONG)-1);

    ASSERT((OldReferenceCount != 0) &&
           (OldReferenceCount < TCP_TIMER_MAX_REFERENCE));

    return OldReferenceCount;
}

VOID
NetpTcpQueueTcpTimer (
    VOID
    )

/*++

Routine Description:

    This routine queues the TCP timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONGLONG DueTime;
    ULONG OldState;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Attempt to queue the timer. The TCP worker may race with sockets adding
    // the first reference to the timer and then trying to queue it.
    //

    OldState = RtlAtomicCompareExchange32(&NetTcpTimerState,
                                          TcpTimerQueued,
                                          TcpTimerNotQueued);

    if (OldState == TcpTimerNotQueued) {
        DueTime = KeGetRecentTimeCounter();
        DueTime += NetTcpTimerPeriod;
        Status = KeQueueTimer(NetTcpTimer,
                              TimerQueueSoftWake,
                              DueTime,
                              0,
                              0,
                              NULL);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Error: Failed to queue TCP timer: %d\n", Status);
        }
    }

    return;
}

VOID
NetpTcpArmKeepAliveTimer (
    ULONGLONG DueTime
    )

/*++

Routine Description:

    This routine arms or re-arms the keep alive timer to the given due time if
    it is less than the current due time.

Arguments:

    DueTime - Supplies the value of the time tick counter when the keep alive
        timer should expire.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentDueTime;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // If the timer's current due time is 0 (not queued) or greater than the
    // requested due time, cancel the timer and re-queue it.
    //

    KeAcquireQueuedLock(NetTcpKeepAliveTimerLock);
    CurrentDueTime = KeGetTimerDueTime(NetTcpKeepAliveTimer);
    if ((CurrentDueTime == 0) || (CurrentDueTime > DueTime)) {
        if (NetTcpDebugPrintSequenceNumbers != FALSE) {
            RtlDebugPrint("TCP: Arming keep alive timer.\n");
        }

        KeCancelTimer(NetTcpKeepAliveTimer);
        Status = KeQueueTimer(NetTcpKeepAliveTimer,
                              TimerQueueSoftWake,
                              DueTime,
                              0,
                              0,
                              NULL);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Error: Failed to queue TCP keep alive timer: %d\n",
                          Status);
        }
    }

    KeReleaseQueuedLock(NetTcpKeepAliveTimerLock);
    return;
}

KSTATUS
NetpTcpReceiveOutOfBandData (
    BOOL FromKernelMode,
    PTCP_SOCKET TcpSocket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    TcpSocket - Supplies a pointer to the socket to receive data from.

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

    UINTN BytesComplete;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    PIO_OBJECT_STATE IoState;
    BOOL LockHeld;
    ULONG ReturnedEvents;
    UINTN Size;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    UINTN WaitTime;

    BytesComplete = 0;
    IoState = TcpSocket->NetSocket.KernelSocket.IoState;
    LockHeld = FALSE;
    Parameters->SocketIoFlags = 0;
    Size = Parameters->Size;
    Timeout = Parameters->TimeoutInMilliseconds;

    //
    // If OOB data is sent inline, this is not a valid call.
    //

    if ((TcpSocket->Flags & TCP_SOCKET_FLAG_URGENT_INLINE) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto TcpReceiveOutOfBandDataEnd;
    }

    if (Size == 0) {
        Status = STATUS_SUCCESS;
        goto TcpReceiveOutOfBandDataEnd;
    }

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    if (Timeout > TcpSocket->ReceiveTimeout) {
        Timeout = TcpSocket->ReceiveTimeout;
    }

    EndTime = 0;
    if ((Timeout != 0) && (Timeout != WAIT_TIME_INDEFINITE)) {
        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                                       Timeout * MICROSECONDS_PER_MILLISECOND);

    }

    TimeCounterFrequency = HlQueryTimeCounterFrequency();
    while (TRUE) {
        if (Timeout == 0) {
            WaitTime = 0;

        } else if (Timeout != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_IN_HIGH_PRIORITY,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto TcpReceiveOutOfBandDataEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(&(TcpSocket->NetSocket));
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            goto TcpReceiveOutOfBandDataEnd;
        }

        KeAcquireQueuedLock(TcpSocket->Lock);
        LockHeld = TRUE;
        if ((TcpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
            Status = STATUS_END_OF_FILE;
            goto TcpReceiveOutOfBandDataEnd;
        }

        if (TcpSocket->OutOfBandData != -1) {
            Status = MmCopyIoBufferData(IoBuffer,
                                        &(TcpSocket->OutOfBandData),
                                        BytesComplete,
                                        1,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto TcpReceiveOutOfBandDataEnd;
            }

            TcpSocket->OutOfBandData = -1;
            IoSetIoObjectState(IoState, POLL_EVENT_IN_HIGH_PRIORITY, FALSE);
            BytesComplete = 1;
            Parameters->SocketIoFlags |= SOCKET_IO_OUT_OF_BAND;
            Status = STATUS_SUCCESS;
            break;

        //
        // There seemed to be no out of band data ready.
        //

        } else {

            //
            // Watch out for the socket closing down.
            //

            if (TcpSocket->State != TcpStateEstablished) {

                ASSERT(TcpSocket->State > TcpStateEstablished);

                //
                // A reset connection fails as soon as it's known.
                //

                if ((TcpSocket->Flags &
                     TCP_SOCKET_FLAG_CONNECTION_RESET) != 0) {

                    Status = STATUS_CONNECTION_RESET;

                //
                // Otherwise, the request was not at all satisfied, and no more
                // data is coming in.
                //

                } else {
                    Status = STATUS_END_OF_FILE;
                }

                goto TcpReceiveOutOfBandDataEnd;
            }
        }

        KeReleaseQueuedLock(TcpSocket->Lock);
        LockHeld = FALSE;
    }

TcpReceiveOutOfBandDataEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(TcpSocket->Lock);
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

PTCP_SEGMENT_HEADER
NetpTcpAllocateSegment (
    PTCP_SOCKET Socket,
    ULONG AllocationSize
    )

/*++

Routine Description:

    This routine allocates a TCP segment structure and appended buffer that can
    be used to send or receive data.

Arguments:

    Socket - Supplies a pointer to the TCP socket requesting the segment.

    AllocationSize - Supplies the minimum size of the allocation.

Return Value:

    Returns a pointer to the allocated segment on success or NULL on failure.

--*/

{

    PTCP_SEGMENT_HEADER NewSegment;
    ULONG ReceiveSize;
    ULONG SendSize;

    ASSERT(KeIsQueuedLockHeld(Socket->Lock) != FALSE);

    //
    // If the list of free, reusable segments is empty, then allocate a new
    // segment. Ignore the requested allocation size and just make it as big
    // as the maximum segment, making future reuse possible.
    //

    if (LIST_EMPTY(&(Socket->FreeSegmentList)) != FALSE) {

        //
        // Determine the segment allocation size if it has not already been
        // determined.
        //

        if (Socket->SegmentAllocationSize == 0) {
            ReceiveSize = Socket->ReceiveMaxSegmentSize +
                          sizeof(TCP_RECEIVED_SEGMENT);

            SendSize = Socket->SendMaxSegmentSize + sizeof(TCP_SEND_SEGMENT);
            if (ReceiveSize > SendSize) {
                Socket->SegmentAllocationSize = ReceiveSize;

            } else {
                Socket->SegmentAllocationSize = SendSize;
            }
        }

        NewSegment = MmAllocatePagedPool(Socket->SegmentAllocationSize,
                                         TCP_ALLOCATION_TAG);

    //
    // Otherwise grab the first segment off the list. Temporarily treating it
    // as a received segment.
    //

    } else {
        NewSegment = LIST_VALUE(Socket->FreeSegmentList.Next,
                                TCP_SEGMENT_HEADER,
                                ListEntry);

        LIST_REMOVE(&(NewSegment->ListEntry));
    }

    ASSERT(AllocationSize <= Socket->SegmentAllocationSize);

    return NewSegment;
}

VOID
NetpTcpFreeSegment (
    PTCP_SOCKET Socket,
    PTCP_SEGMENT_HEADER Segment
    )

/*++

Routine Description:

    This routine releases a TCP segment by making it available for reuse by
    future incoming and outgoing packets.

Arguments:

    Socket - Supplies a pointer to the TCP socket that owns the segment.

    Segment - Supplies a pointer to the segment to be released.

Return Value:

    None.

--*/

{

    ASSERT(KeIsQueuedLockHeld(Socket->Lock) != FALSE);

    //
    // Just add it to the list of free segments. The socket should never
    // allocate more segments than can fit in the send and receive windows. It
    // shouldn't get out of hand. Put it at the beginning so it stays hot and
    // is reused next.
    //

    INSERT_AFTER(&(Segment->ListEntry), &(Socket->FreeSegmentList));
    return;
}

