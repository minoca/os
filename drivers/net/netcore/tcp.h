/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tcp.h

Abstract:

    This header contains internal definitions for the TCP implementation.

Author:

    Evan Green 10-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro evaluates whether two sequence numbers are in descending order,
// taking wrapping into account.
//

#define TCP_SEQUENCE_GREATER_THAN(_Sequence1, _Sequence2) \
    (((LONG)((_Sequence1) - (_Sequence2))) > 0)

//
// This macro evaluates whether two sequence numbers are in ascending order,
// taking wrapping into account.
//

#define TCP_SEQUENCE_LESS_THAN(_Sequence1, _Sequence2) \
    (((LONG)((_Sequence1) - (_Sequence2))) < 0)

//
// This macro updates the socket's retry expiration end time and increments the
// retry wait period.
//

#define TCP_UPDATE_RETRY_TIME(_Socket)                                        \
    (_Socket)->RetryTime = KeGetRecentTimeCounter();                          \
    (_Socket)->RetryTime += KeConvertMicrosecondsToTimeTicks(                 \
                                               (_Socket)->RetryWaitPeriod *   \
                                               MICROSECONDS_PER_MILLISECOND); \
                                                                              \
    (_Socket)->RetryWaitPeriod *= 2;

//
// This macro sets the default timeout expiration time in the socket.
//

#define TCP_SET_DEFAULT_TIMEOUT(_Socket)                     \
    (_Socket)->TimeoutEnd = KeGetRecentTimeCounter() +       \
                            (HlQueryTimeCounterFrequency() * \
                             TCP_DEFAULT_TIMEOUT);

//
// This macro determines whether or not the TCP state is a SYN retry state.
//

#define TCP_IS_SYN_RETRY_STATE(_TcpState)  \
    (((_TcpState) == TcpStateSynSent) ||   \
     ((_TcpState) == TcpStateSynReceived))

//
// This macro determins whether or not the TCP state is a FIN retry state.
//

#define TCP_IS_FIN_RETRY_STATE(_TcpState)      \
    (((_TcpState) == TcpStateFinWait1) ||      \
     ((_TcpState) == TcpStateClosing) ||       \
     ((_TcpState) == TcpStateLastAcknowledge))

//
// This macro determines whether or not the TCP state is a keep alive state.
//

#define TCP_IS_KEEP_ALIVE_STATE(_TcpState)   \
    (((_TcpState) == TcpStateEstablished) || \
     ((_TcpState) == TcpStateFinWait2) ||    \
     ((_TcpState) == TcpStateCloseWait))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by the TCP socket protocol.
//

#define TCP_ALLOCATION_TAG 0x21706354 // '!pcT'

//
// Define the default maximum segment size, in bytes.
//

#define TCP_DEFAULT_MAX_SEGMENT_SIZE 576

//
// Define the initial default round trip time, in milliseconds.
//

#define TCP_DEFAULT_ROUND_TRIP_TIME MILLISECONDS_PER_SECOND
#define TCP_ROUND_TRIP_TIMEOUT_FACTOR 2

//
// Define the numerator and denominator for the fraction of the new round trip
// sample that is added to the estimate. The spec indicates that this should be
// somewhere betwen 0.1 and 0.2. Using a denominator that's a power of 2 means
// the compiler can optimize this to a shift.
//

#define TCP_ROUND_TRIP_SAMPLE_NUMERATOR 2
#define TCP_ROUND_TRIP_SAMPLE_DENOMINATOR 16

//
// Define TCP's periodic timer interval, in microseconds.
//

#define TCP_TIMER_PERIOD (250 * MICROSECONDS_PER_MILLISECOND)

//
// Define the length in seconds of the default timeout. This is used as a
// timeout in the time-wait state and when waiting for a SYN or FIN to be
// acknowledges.
//

#define TCP_DEFAULT_TIMEOUT 60

//
// Define the amount of time to wait (in milliseconds) before resending any
// packet, whether it be a zero window probe, SYN, or FIN.
//

#define TCP_INITIAL_RETRY_WAIT_PERIOD 500

//
// Define the maximum amount of time to wait (in milliseconds) before sending
// a packet just to probe for a non-zero window size.
//

#define TCP_WINDOW_WAIT_PERIOD_MAX (120 * MILLISECONDS_PER_SECOND)

//
// Define the number of duplicate ACKs that must come in to signal packet loss.
//

#define TCP_DUPLICATE_ACK_THRESHOLD 3

//
// Define the default receive minimum size, in bytes.
//

#define TCP_DEFAULT_RECEIVE_MINIMUM 1

//
// Define the default send buffer size.
//

#define TCP_DEFAULT_SEND_BUFFER_SIZE (16 * _1KB)

//
// Define the default send minimum size, in bytes.
//

#define TCP_DEFAULT_SEND_MINIMUM 1

//
// Define the default window size.
//

#define TCP_DEFAULT_WINDOW_SIZE (64 * _1KB)

//
// Define the default window scale.
//

#define TCP_DEFAULT_WINDOW_SCALE 8

//
// Define the maximum window size.
//

#define TCP_MAXIMUM_WINDOW_SIZE (_1GB - 1)

//
// Define the mask for the TCP window.
//

#define TCP_WINDOW_MASK MAX_USHORT

//
// Define the minimum window size.
//

#define TCP_MINIMUM_WINDOW_SIZE 256

//
// Define the maximum window scale. A maximum window scale of 14, prevents the
// window from being greater than or equal to 1GB, giving sequence numbers
// enough space to avoid ambiguity between old and new data.
//

#define TCP_MAXIMUM_WINDOW_SCALE 14

//
// Define how often packets are retransmitted, in microseconds.
//

#define TCP_TRANSMIT_RETRY_INTERVAL MICROSECONDS_PER_SECOND

//
// Define how many times a packet is resent before the worst is assumed.
//

#define TCP_RETRANSMIT_COUNT 10

//
// Define the time, in seconds, to wait after a connection goes idle before
// sending the first keep alive probe.
//

#define TCP_DEFAULT_KEEP_ALIVE_TIMEOUT 3600

//
// Define the time, in seconds, between sending keep alive messages on an idle
// connection.
//

#define TCP_DEFAULT_KEEP_ALIVE_PERIOD 60

//
// Define the number of keep alive probes to be sent before the connection is
// reset.
//

#define TCP_DEFAULT_KEEP_ALIVE_PROBE_LIMIT 5

//
// Define TCP header flags.
//

#define TCP_HEADER_FLAG_FIN         0x01
#define TCP_HEADER_FLAG_SYN         0x02
#define TCP_HEADER_FLAG_RESET       0x04
#define TCP_HEADER_FLAG_PUSH        0x08
#define TCP_HEADER_FLAG_ACKNOWLEDGE 0x10
#define TCP_HEADER_FLAG_URGENT      0x20

//
// The keep alive flag is not a real TCP header flag.
//

#define TCP_HEADER_FLAG_KEEP_ALIVE  0x80

#define TCP_HEADER_LENGTH_MASK 0xF0
#define TCP_HEADER_LENGTH_SHIFT 4

//
// Define the TCP option types.
//

#define TCP_OPTION_END                  0
#define TCP_OPTION_NOP                  1
#define TCP_OPTION_MAXIMUM_SEGMENT_SIZE 2
#define TCP_OPTION_WINDOW_SCALE         3

//
// Define TCP option sizes.
//

#define TCP_OPTION_NOP_SIZE 1
#define TCP_OPTION_MSS_SIZE 4
#define TCP_OPTION_WINDOW_SCALE_SIZE 3

//
// Define the TCP receive segment flags. The first six bits matche up with the
// TCP header flags.
//

#define TCP_RECEIVE_SEGMENT_FLAG_FIN TCP_HEADER_FLAG_FIN
#define TCP_RECEIVE_SEGMENT_FLAG_SYN TCP_HEADER_FLAG_SYN
#define TCP_RECEIVE_SEGMENT_FLAG_RESET TCP_HEADER_FLAG_RESET
#define TCP_RECEIVE_SEGMENT_FLAG_PUSH TCP_HEADER_FLAG_PUSH
#define TCP_RECEIVE_SEGMENT_FLAG_ACKNOWLEDGE TCP_HEADER_FLAG_ACKNOWLEDGE
#define TCP_RECEIVE_SEGMENT_FLAG_URGENT TCP_HEADER_FLAG_URGENT

#define TCP_RECEIVE_SEGMENT_HEADER_FLAG_MASK \
    (TCP_RECEIVE_SEGMENT_FLAG_FIN |                \
     TCP_RECEIVE_SEGMENT_FLAG_SYN |                \
     TCP_RECEIVE_SEGMENT_FLAG_RESET |              \
     TCP_RECEIVE_SEGMENT_FLAG_PUSH |               \
     TCP_RECEIVE_SEGMENT_FLAG_ACKNOWLEDGE)

//
// Define the TCP send segment flags. The first six bits matche up with the TCP
// header flags.
//

#define TCP_SEND_SEGMENT_FLAG_FIN TCP_HEADER_FLAG_FIN
#define TCP_SEND_SEGMENT_FLAG_SYN TCP_HEADER_FLAG_SYN
#define TCP_SEND_SEGMENT_FLAG_RESET TCP_HEADER_FLAG_RESET
#define TCP_SEND_SEGMENT_FLAG_PUSH TCP_HEADER_FLAG_PUSH
#define TCP_SEND_SEGMENT_FLAG_ACKNOWLEDGE TCP_HEADER_FLAG_ACKNOWLEDGE
#define TCP_SEND_SEGMENT_FLAG_URGENT TCP_HEADER_FLAG_URGENT

#define TCP_SEND_SEGMENT_HEADER_FLAG_MASK \
    (TCP_SEND_SEGMENT_FLAG_FIN |                \
     TCP_SEND_SEGMENT_FLAG_SYN |                \
     TCP_SEND_SEGMENT_FLAG_RESET |              \
     TCP_SEND_SEGMENT_FLAG_PUSH |               \
     TCP_SEND_SEGMENT_FLAG_ACKNOWLEDGE |        \
     TCP_SEND_SEGMENT_FLAG_URGENT)

//
// Define the TCP socket flags.
//

#define TCP_SOCKET_FLAG_RECEIVE_FINAL_SEQUENCE_VALID 0x00000001
#define TCP_SOCKET_FLAG_SEND_FINAL_SEQUENCE_VALID    0x00000002
#define TCP_SOCKET_FLAG_SEND_FIN_WITH_DATA           0x00000004
#define TCP_SOCKET_FLAG_SEND_ACKNOWLEDGE             0x00000008
#define TCP_SOCKET_FLAG_CONNECTION_RESET             0x00000010
#define TCP_SOCKET_FLAG_IN_FAST_RECOVERY             0x00000020
#define TCP_SOCKET_FLAG_LINGER_ENABLED               0x00000040
#define TCP_SOCKET_FLAG_KEEP_ALIVE                   0x00000080
#define TCP_SOCKET_FLAG_URGENT_INLINE                0x00000100
#define TCP_SOCKET_FLAG_RECEIVE_MISSING_SEGMENTS     0x00000200
#define TCP_SOCKET_FLAG_NO_DELAY                     0x00000400
#define TCP_SOCKET_FLAG_WINDOW_SCALING               0x00000800
#define TCP_SOCKET_FLAG_CONNECT_INTERRUPTED          0x00001000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the ioctl numbers that can be sent to a TCP socket. These have to
// match with the values in the C library header <sys/ioctl.h>.
//

typedef enum _TCP_USER_CONTROL_CODE {
    TcpUserControlAtUrgentMark = 0x7300,
    TcpUserControlGetInputQueueSize = 0x741B,
} TCP_USER_CONTROL_CODE, *PTCP_USER_CONTROL_CODE;

//
// Define the various TCP connection states.
//
// Invalid - The socket should never be in this state.
//
// Initialized - This is a brand new socket that is neither listening nor
//     connected.
//
// Listening - Represents waiting for a connection request from any remote host.
//
// SynSent - Represents waiting for a matched connection request after having
//     sent a connection request.
//
// SynReceived - Represents waiting for a confirmation connection request
//     acknowledgment after having both received and sent a connection request.
//
// Established - Represents an open connection, data can be both sent and
//     received.
//
// FinWait1 - Represents waiting for a connection termination request from the
//     remote host, or an acknowledgment of the connection termition request
//     previously sent.
//
// FinWait2 - Represents waiting for a connection termination request from the
//     remote host.
//
// CloseWait - Represents waiting for a connection termination request from the
//     local user.
//
// Closing - Represents waiting for a connection termination request
//     acknowledgment from the remote host.
//
// LastAcknowledge - Represents waiting for an acknowledgment of the connection
//     termination request previously sent to the remote host (which includes
//     an acknowledgment of its connection termination request.
//
// TimeWait - Represents waiting for enough time to pass to be sure the remote
//     host received the acnkowledgment of its connection termination request.
//     This prevents a stray FIN+ACK still stuck in the network from ruining
//     the next connection to use this host/port combination when it arrives.
//
// Closed - Represents a completely shut down connection.
//

typedef enum _TCP_STATE {
    TcpStateInvalid,
    TcpStateInitialized,
    TcpStateListening,
    TcpStateSynSent,
    TcpStateSynReceived,
    TcpStateEstablished,
    TcpStateFinWait1,
    TcpStateFinWait2,
    TcpStateCloseWait,
    TcpStateClosing,
    TcpStateLastAcknowledge,
    TcpStateTimeWait,
    TcpStateClosed
} TCP_STATE, *PTCP_STATE;

/*++

Structure Description:

    This structure defines a TCP data socket.

Members:

    NetSocket - Stores the common core networking parameters.

    ListEntry - Stores pointers to the previous and next sockets on the global
        list.

    State - Stores the connection state of the socket.

    PreviousState - Stores the previous state of the socket, to debug where
        transitions are coming from.

    Flags - Stores a bitmask of TCP flags. See TCP_SOCKET_FLAG_* for
        definitions.

    TimerReferenceCount - Supplies the reference count on the global TCP timer.
        If this value is non-zero, there is a single reference on the global
        TCP timer.

    SendInitialSequence - Stores the random offset that the sequence numbers
        started at for this socket.

    SendUnacknowledgedSequence - Stores the first unacknowledged sequence
        number, representing data that was sent but not acknowledged.

    SendNextBufferSequence - Stores the sequence number of the next byte
        accepted into the send buffer.

    SendNextNetworkSequence - Stores the sequence number of the next byte to
        be sent out of the network. This may be different than the next buffer
        sequence if data has been accepted into the send buffer but not yet
        actually sent out on the wire.

    SendMaxSegmentSize - Stores the maximum segment size for outgoing packets.

    SendWindowScale - Stores the number of bits to shift the window size left
        by for incoming packets.

    SendWindowSize - Stores the size of the window of data that can be sent out
        to the remote host.

    SendWindowUpdateSequence - Stores the sequence number of the packet when the
        send window was last updated. This is used to prevent old packets from
        updating the send window.

    SendWindowUpdateAcknowledge - Stores the acnknowledge number of the most
        recent packet used to update the send window. This is used to prevent
        old packets from updating the send window.

    SendBufferTotalSize - Stores the total size in the send buffer, in bytes.

    SendBufferFreeSize - Stores the number of free bytes size in the send
        buffer.

    SendFinalSequence - Stores the outgoing sequence number of the sent or
        soon-to-be-sent FIN.

    PreviousAcknowledgeNumber - Stores the most recently received acknowledge
        number.

    DuplicateAcknowledgeCount - Stores the number of duplicate acknowledges
        that have come in. A value of 1 means two packets with the same
        acknowledge number have come in.

    ReceiveWindowTotalSize - Stores the total size of the local receive window,
        in bytes.

    ReceiveWindowFreeSize - Stores the current size of the local receive window,
        in bytes.

    ReceiveWindowScale - Stores the number of bits by which the window size
        must be shifted before being put into the header.

    ReceiveMinimum - Stores the minimum number of bytes that must be received
        before this socket becomes readable.

    ReceiveInitialSequence - Stores the random initial sequence number
        provided by the remote host.

    ReceiveUnreadSequence - Stores the sequence number of the first unread
        byte. This should be at the head of the received segment list.

    ReceiveNextSequence - Stores the next sequence number expected to be
        received from the remote host (the value to fill in the acknowledgment
        number).

    ReceiveFinalSequence - Stores the sequence number in which the FIN was
        sent.

    ReceiveSegmentOffset - Stores the offset in bytes into the first segment
        where the next user receive call will read from.

    ReceiveMaxSegmentSize - Stores the maximum segment size of packets received
        by the TCP socket.

    Lock - Store a pointer to a queued lock used to synchronize access to
        various parts of the structure.

    ReceivedSegmentList - Stores the head of the list of received segments that
        have not yet been read by the user. This list contains objects of type
        TCP_RECEIVED_SEGMENT, and is in order by sequence number.

    OutgoingSegmentList - Stores the list of segments that have either not yet
        been sent or have been sent but not acknowledged.

    FreeSegmentList - Stores the head of the list of segments that can be
        reused for send and receive.

    IncomingConnectionList - Stores the head of the list of incoming
        connections. This list only applies to a listening socket.

    IncomingConnectionCount - Stores the number of elements that are on the
        incoming connection list.

    SlowStartThreshold - Stores the threshold value for the congestion window.
        If the congestion window size is less than or equal to this value, then
        Slow Start is used. Otherwise, Congestion Avoidance is used.

    CongestionWindowSize - Stores the current size of the congestion window.

    FastRecoveryEndSequence - Stores the sequence number that when acknowledged
        will transition congestion control out of Fast Recovery back into
        Congestion Avoidance mode.

    RoundTripTime - Stores the latest estimate for the round trip time.

    TimeoutEnd - Stores the ending time, in time counter ticks, of the current
        timeout period. Depending on the state this could be the time-wait
        timeout, the SYN resend timeout, or the packet retransmit timeout.
        These three uses are mutually exclusive, so the timeout end can be
        safely shared.

    RetryTime - Stores the time, in time counter ticks, when the socket will
        retry sending a packet. Depending on the state, this could be a probe
        despite a zero window size, a resend of the SYN packet, or a resend of
        the FIN packet. These there uses are mutually exclusive, so the retry
        time can be safely shared.

    KeepAliveTime - Stores the time, in time counter ticks, when the socket
        will probe the remote host with a keep alive message.

    KeepAliveTimeout - Stores the time, in seconds, to wait after the
        connection goes idle before sending a keep alive probe.

    KeepAlivePeriod - Stores the time, in seconds, between sending keep alive
        probes on an idle connection.

    KeepAliveProbeLimit - Stores the number of keep alive probes to send before
        resetting the connection.

    KeepAliveProbeCount - Stores the current number of keep alive probes that
        have been sent without reply.

    RetryWaitPeriod - Stores the time in milliseconds for the socket to wait
        until it sends its next retry packet. This could be a probe on zero
        window, another SYN packet, or another FIN packet. These there uses are
        mutually exclusive, so the period can be safely shared.

    LingerTimeout - Stores the time, in milliseconds, that the socket will wait
        for all the data to be sent on close before forcefully closing the
        connection.

    SendTimeout - Stores the maximum time, in milliseconds, for the socket to
        wait until send buffer space becomes available.

    ReceiveTimeout - Stores the maximum time, in milliseconds, for the socket
        to wait until data is available to receive.

    ShutdownTypes - Stores a mask of the shutdown types that have occurred.
        See SOCKET_SHUTDOWN_* definitions.

    OutOfBandData - Stores a single urgent byte, or -1 if the urgent data is
        not valid.

    SegmentAllocationSize - Stores the allocation size for each of the send and
        receive TCP segments, including enough size for the header and data.

--*/

typedef struct _TCP_SOCKET {
    NET_SOCKET NetSocket;
    LIST_ENTRY ListEntry;
    TCP_STATE State;
    TCP_STATE PreviousState;
    ULONG Flags;
    LONG TimerReferenceCount;
    ULONG SendInitialSequence;
    ULONG SendUnacknowledgedSequence;
    ULONG SendNextBufferSequence;
    ULONG SendNextNetworkSequence;
    ULONG SendMaxSegmentSize;
    ULONG SendWindowScale;
    ULONG SendWindowSize;
    ULONG SendWindowUpdateSequence;
    ULONG SendWindowUpdateAcknowledge;
    ULONG SendBufferTotalSize;
    ULONG SendBufferFreeSize;
    ULONG SendFinalSequence;
    ULONG PreviousAcknowledgeNumber;
    ULONG DuplicateAcknowledgeCount;
    ULONG ReceiveWindowTotalSize;
    ULONG ReceiveWindowFreeSize;
    ULONG ReceiveWindowScale;
    ULONG ReceiveMinimum;
    ULONG ReceiveInitialSequence;
    ULONG ReceiveUnreadSequence;
    ULONG ReceiveNextSequence;
    ULONG ReceiveFinalSequence;
    ULONG ReceiveSegmentOffset;
    ULONG ReceiveMaxSegmentSize;
    PQUEUED_LOCK Lock;
    LIST_ENTRY ReceivedSegmentList;
    LIST_ENTRY OutgoingSegmentList;
    LIST_ENTRY FreeSegmentList;
    LIST_ENTRY IncomingConnectionList;
    ULONG IncomingConnectionCount;
    ULONG SlowStartThreshold;
    ULONG CongestionWindowSize;
    ULONG FastRecoveryEndSequence;
    ULONGLONG RoundTripTime;
    ULONGLONG TimeoutEnd;
    ULONGLONG RetryTime;
    ULONGLONG KeepAliveTime;
    ULONG KeepAliveTimeout;
    ULONG KeepAlivePeriod;
    ULONG KeepAliveProbeLimit;
    ULONG KeepAliveProbeCount;
    ULONG RetryWaitPeriod;
    ULONG LingerTimeout;
    ULONG SendTimeout;
    ULONG ReceiveTimeout;
    ULONG ShutdownTypes;
    LONG OutOfBandData;
    ULONG SegmentAllocationSize;
} TCP_SOCKET, *PTCP_SOCKET;

/*++

Structure Description:

    This structure stores information about an incoming TCP connection.

Members:

    ListEntry - Stores pointers to the next and previous incoming connections.

    IoHandle - Stores a pointer to the I/O handle for the connection.

--*/

typedef struct _TCP_INCOMING_CONNECTION {
    LIST_ENTRY ListEntry;
    PIO_HANDLE IoHandle;
} TCP_INCOMING_CONNECTION, *PTCP_INCOMING_CONNECTION;

/*++

Structure Description:

    This structure stores information common to all TCP segment types.

Members:

    ListEntry - Stores a pointer to the next and previous segments.

--*/

typedef struct _TCP_SEGMENT_HEADER {
    LIST_ENTRY ListEntry;
} TCP_SEGMENT_HEADER, *PTCP_SEGMENT_HEADER;

/*++

Structure Description:

    This structure stores information about a received segment. The data comes
    after this structure.

Members:

    Header - Stores information common to all TCP segment types.

    SequenceNumber - Stores the byte offset into the stream where this buffer
        belongs.

    Length - Stores the length of the data, in bytes.

    NextSequence - Stores the sequence number after this segment. Nearly all
        of the time this is the same as SequenceNumber + Length, but in the
        extremely rare cases where an out-of-band byte was pulled out, the
        length will be one shy of the next sequence.

    Flags - Stores a bitmaks of flags for the incoming TCP segment. See TCP_
        RECEIVE_SEGMENT_FLAG_* for definitions;

--*/

typedef struct _TCP_RECEIVED_SEGMENT {
    TCP_SEGMENT_HEADER Header;
    ULONG SequenceNumber;
    ULONG Length;
    ULONG NextSequence;
    ULONG Flags;
} TCP_RECEIVED_SEGMENT, *PTCP_RECEIVED_SEGMENT;

/*++

Structure Description:

    This structure stores information about an outgoing TCP segment. The data
    comes immediately after this structure.

Members:

    Header - Stores information common to all TCP segment types.

    SequenceNumber - Stores the byte offset into the stream where this buffer
        belongs.

    LastSendTime - Stores the performance counter value the last time this
        packet was sent.

    TimeoutInterval - Stores the number of time counter ticks from the last send
        time when this packet is considered timed out and needs to be resent or
        otherwise acted on.

    SendAttemptCount - Stores the number of times this packet has been sent off
        without getting acknowledged.

    Length - Stores the length of the data, in bytes.

    Offset - Stores the offset in bytes from the beginning of the segment to
        resend due to a partial ACK.

    Flags - Stores a bitmask of flags for the outgoing TCP segment. See
        TCP_SEND_SEGMENT_FLAG_* for definitions.

--*/

typedef struct _TCP_SEND_SEGMENT {
    TCP_SEGMENT_HEADER Header;
    ULONG SequenceNumber;
    ULONGLONG LastSendTime;
    ULONGLONG TimeoutInterval;
    ULONG SendAttemptCount;
    ULONG Length;
    ULONG Offset;
    ULONG Flags;
} TCP_SEND_SEGMENT, *PTCP_SEND_SEGMENT;

/*++

Structure Description:

    This structure defines a TCP packet protocol header.

Members:

    SourcePort - Stores the source port number of the packet.

    DestinationPort - Stores the port number of this packet's destination.

    SequenceNumber - Stores the position of this data within the stream.

    AcknowledgmentNumber - Stores the next sequence number that the sender
        expects to receive. This field is only valid if the ACK flag is on,
        which it always is once a connection is established.

    HeaderLength - Stores the length of the header, in 32-bit words.

    Flags - Stores a bitfield of flags used to relay control information
        between two peers.

    WindowSize - Stores the size of the advertised window of data the socket
        can receive from the other host.

    Checksum - Stores the checksum of the header and data.

    NonUrgentOffset - Stores the offset within the data where the non-urgent
        data begins. This field is only used if the urgent flag is set. RFC793
        is inconsistent as to whether this field points to the last urgent
        octet or the first non-urgent octet. RFC1122 attempted to clarify this
        as the last urgent octet, but all of today's implementations
        maintained the opposite semantics. Stick with tradition to be
        consistent with everyone else. RFC6093 sums this all up.

--*/

typedef struct _TCP_HEADER {
    USHORT SourcePort;
    USHORT DestinationPort;
    ULONG SequenceNumber;
    ULONG AcknowledgmentNumber;
    UCHAR HeaderLength;
    UCHAR Flags;
    USHORT WindowSize;
    USHORT Checksum;
    USHORT NonUrgentOffset;
} PACKED TCP_HEADER, *PTCP_HEADER;

//
// -------------------------------------------------------------------- Globals
//

extern BOOL NetTcpDebugPrintCongestionControl;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
NetpTcpPrintSocketEndpoints (
    PTCP_SOCKET Socket,
    BOOL Transmit
    );

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

VOID
NetpTcpRetransmit (
    PTCP_SOCKET Socket
    );

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

//
// Congestion control routines
//

VOID
NetpTcpCongestionInitializeSocket (
    PTCP_SOCKET Socket
    );

/*++

Routine Description:

    This routine initializes the congestion control portion of the TCP socket.

Arguments:

    Socket - Supplies a pointer to the socket to initialize.

Return Value:

    None.

--*/

VOID
NetpTcpCongestionConnectionEstablished (
    PTCP_SOCKET Socket
    );

/*++

Routine Description:

    This routine is called when a socket moves to the Established state.

Arguments:

    Socket - Supplies a pointer to the socket to initialize.

Return Value:

    None.

--*/

ULONG
NetpTcpGetSendWindowSize (
    PTCP_SOCKET Socket
    );

/*++

Routine Description:

    This routine determines the current available window of data that can be
    sent, taking into account both the receiver's window and the congestion
    window.

Arguments:

    Socket - Supplies a pointer to the socket whose send window should be
        returned.

Return Value:

    Returns one beyond the highest sequence number that can currently be sent.

--*/

VOID
NetpTcpCongestionAcknowledgeReceived (
    PTCP_SOCKET Socket,
    ULONG AcknowledgeNumber
    );

/*++

Routine Description:

    This routine is called when an acknowledge (duplicate or not) comes in.
    This routine assumes the socket lock is already held.

Arguments:

    Socket - Supplies a pointer to the socket that just got an acknowledge.

    AcknowledgeNumber - Supplies the acknowledge number that came in.

Return Value:

    None.

--*/

VOID
NetpTcpProcessNewRoundTripTimeSample (
    PTCP_SOCKET Socket,
    ULONGLONG RoundTripTicks
    );

/*++

Routine Description:

    This routine is called when a new round trip time sample arrives.

Arguments:

    Socket - Supplies a pointer to the socket.

    RoundTripTicks - Supplies the most recent sample of round trip time, in
        time counter ticks.

Return Value:

    None.

--*/

VOID
NetpTcpGetTransmitTimeoutInterval (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    );

/*++

Routine Description:

    This routine sets the timeout duration for a transmitted packet.

Arguments:

    Socket - Supplies a pointer to the socket.

    Segment - Supplies a pointer to the segment whose timeout interval needs to
        be set.

Return Value:

    None. Upon completion, the segment's timeout interval is expected to be
    filled in by this routine.

--*/

VOID
NetpTcpTransmissionTimeout (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    );

/*++

Routine Description:

    This routine is called when an acknowledge is not received for a sent
    packet in a timely manner (the packet timed out).

Arguments:

    Socket - Supplies a pointer to the socket.

    Segment - Supplies a pointer to the segment that timed out.

Return Value:

    None.

--*/

