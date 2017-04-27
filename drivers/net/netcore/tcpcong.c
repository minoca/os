/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tcpcong.c

Abstract:

    This module implements support for TCP congestion control. Specifically
    this module implements the New Reno algorithm, however this set of functions
    could easily be interfaced to include alternate congestion control
    algorithms.

Author:

    Evan Green 15-Apr-2013

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
#include "tcp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

ULONGLONG NetDefaultRoundTripTicks = 0;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpTcpCongestionInitializeSocket (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine initializes the congestion control portion of the TCP socket.

Arguments:

    Socket - Supplies a pointer to the socket to initialize.

Return Value:

    None.

--*/

{

    ULONGLONG Ticks;

    if (NetDefaultRoundTripTicks == 0) {
        Ticks = TCP_DEFAULT_ROUND_TRIP_TIME * MICROSECONDS_PER_MILLISECOND;
        NetDefaultRoundTripTicks = KeConvertMicrosecondsToTimeTicks(Ticks) *
                                   TCP_ROUND_TRIP_SAMPLE_DENOMINATOR;
    }

    ASSERT((Socket->Flags & TCP_SOCKET_FLAG_IN_FAST_RECOVERY) == 0);

    Socket->SlowStartThreshold = MAX_ULONG;
    Socket->CongestionWindowSize = 2 * TCP_DEFAULT_MAX_SEGMENT_SIZE;
    Socket->FastRecoveryEndSequence = 0;
    Socket->RoundTripTime = NetDefaultRoundTripTicks;
    return;
}

VOID
NetpTcpCongestionConnectionEstablished (
    PTCP_SOCKET Socket
    )

/*++

Routine Description:

    This routine is called when a socket moves to the Established state.

Arguments:

    Socket - Supplies a pointer to the socket to initialize.

Return Value:

    None.

--*/

{

    Socket->SlowStartThreshold = Socket->SendWindowSize;
    if (Socket->SendMaxSegmentSize == 0) {
        Socket->SendMaxSegmentSize = TCP_DEFAULT_MAX_SEGMENT_SIZE;
    }

    Socket->CongestionWindowSize = 2 * Socket->SendMaxSegmentSize;
    if (NetTcpDebugPrintCongestionControl != FALSE) {
        NetpTcpPrintSocketEndpoints(Socket, FALSE);
        RtlDebugPrint(" Initial SlowStartThreshold %d, "
                      "CongestionWindowSize %d.\n",
                      Socket->SlowStartThreshold,
                      Socket->CongestionWindowSize);
    }

    return;
}

ULONG
NetpTcpGetSendWindowSize (
    PTCP_SOCKET Socket
    )

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

{

    ULONGLONG DueTime;
    ULONGLONG WaitInMicroseconds;
    ULONG WindowSize;

    WindowSize = Socket->CongestionWindowSize;
    if (Socket->SendWindowSize < WindowSize) {
        WindowSize = Socket->SendWindowSize;
        if (WindowSize == 0) {

            //
            // If this is the first time the window is being seen as zero,
            // start the probe timer.
            //

            if (Socket->RetryTime == 0) {
                WaitInMicroseconds = Socket->RetryWaitPeriod *
                                     MICROSECONDS_PER_MILLISECOND;

                DueTime = KeGetRecentTimeCounter() +
                          KeConvertMicrosecondsToTimeTicks(WaitInMicroseconds);

                Socket->RetryTime =  DueTime;

            //
            // This socket has grown impatient with a zero window size, try
            // sending something to see if an ACK comes back with an updated
            // window size.
            //

            } else if (KeGetRecentTimeCounter() > Socket->RetryTime) {
                Socket->RetryTime = 0;
                WindowSize = Socket->SendMaxSegmentSize;

                //
                // Double the wait period in case nothing comes back.
                //

                Socket->RetryWaitPeriod *= 2;
                if (Socket->RetryWaitPeriod > TCP_WINDOW_WAIT_PERIOD_MAX) {
                    Socket->RetryWaitPeriod = TCP_WINDOW_WAIT_PERIOD_MAX;
                }
            }
        }
    }

    return WindowSize;
}

VOID
NetpTcpCongestionAcknowledgeReceived (
    PTCP_SOCKET Socket,
    ULONG AcknowledgeNumber
    )

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

{

    ULONG Flags;
    ULONG SegmentSize;
    ULONG WindowIncrease;

    //
    // Process an ACK that made progress.
    //

    SegmentSize = Socket->SendMaxSegmentSize;
    if (Socket->DuplicateAcknowledgeCount == 0) {

        //
        // The same ACK can come in multiple times and not get counted as a
        // duplicate. Really only adjust things when new ACKs come in.
        //

        if (AcknowledgeNumber != Socket->PreviousAcknowledgeNumber) {

            //
            // Perform slow start if below the threshold. With slow start,
            // the congestion window is increased 1 Maximum Segment Size for
            // every new ACK received. Thus it is really exponentially
            // increasing.
            //

            Flags = Socket->Flags;
            if (Socket->CongestionWindowSize <= Socket->SlowStartThreshold) {
                Socket->CongestionWindowSize += SegmentSize;
                if (NetTcpDebugPrintCongestionControl != FALSE) {
                    NetpTcpPrintSocketEndpoints(Socket, FALSE);
                    RtlDebugPrint(" SlowStart Window up by %d to %d.\n",
                                  SegmentSize,
                                  Socket->CongestionWindowSize);
                }

            //
            // Perform fast recovery if enabled.
            //

            } else if ((Flags & TCP_SOCKET_FLAG_IN_FAST_RECOVERY) != 0) {

                //
                // If the acknowledge number is greater than the highest
                // sequence number in flight when the old packet was lost, then
                // go back to regular congestion avoidance mode.
                //

                if ((AcknowledgeNumber == Socket->FastRecoveryEndSequence) ||
                    (TCP_SEQUENCE_GREATER_THAN(AcknowledgeNumber,
                     Socket->FastRecoveryEndSequence))) {

                    Socket->Flags &= ~TCP_SOCKET_FLAG_IN_FAST_RECOVERY;
                    Socket->CongestionWindowSize = Socket->SlowStartThreshold;
                    if (NetTcpDebugPrintCongestionControl != FALSE) {
                        NetpTcpPrintSocketEndpoints(Socket, FALSE);
                        RtlDebugPrint(" Exit FastRecovery: Window %d\n",
                                      Socket->CongestionWindowSize);
                    }
                }

                //
                // If the socket is still in fast recovery mode, then only
                // partial progress was made. The acknowledge number must point
                // to the next hole, so send that off right away.
                //

                if (((Socket->Flags & TCP_SOCKET_FLAG_IN_FAST_RECOVERY) != 0) &&
                    (Socket->SendWindowSize != 0)) {

                    NetpTcpRetransmit(Socket);
                }

            //
            // Perform congestion avoidance.
            //

            } else {
                WindowIncrease = SegmentSize * SegmentSize /
                                 Socket->CongestionWindowSize;

                if (WindowIncrease == 0) {
                    WindowIncrease = 1;
                }

                Socket->CongestionWindowSize += WindowIncrease;
                if (NetTcpDebugPrintCongestionControl != FALSE) {
                    NetpTcpPrintSocketEndpoints(Socket, FALSE);
                    RtlDebugPrint(" CongestionAvoid Window up by %d to %d.\n",
                                  WindowIncrease,
                                  Socket->CongestionWindowSize);
                }
            }
        }

    //
    // Process a duplicate ACK.
    //

    } else if (Socket->DuplicateAcknowledgeCount >=
               TCP_DUPLICATE_ACK_THRESHOLD) {

        //
        // Cut the window if this just crossed the "packet loss" threshold.
        //

        if (Socket->DuplicateAcknowledgeCount == TCP_DUPLICATE_ACK_THRESHOLD) {

            //
            // Set the slow start threshold to half the congestion window. The
            // congestion window is also halved, but three segment sizes are
            // added to it to represent the packets after the hole that are
            // presumably buffered on the other side. This is called "inflating"
            // the window.
            //

            Socket->SlowStartThreshold = Socket->CongestionWindowSize / 2;
            Socket->CongestionWindowSize = (Socket->CongestionWindowSize / 2) +
                                   (TCP_DUPLICATE_ACK_THRESHOLD * SegmentSize);

            Socket->Flags |= TCP_SOCKET_FLAG_IN_FAST_RECOVERY;
            Socket->FastRecoveryEndSequence = Socket->SendNextNetworkSequence;
            if (NetTcpDebugPrintCongestionControl != FALSE) {
                NetpTcpPrintSocketEndpoints(Socket, FALSE);
                RtlDebugPrint(" Entering FastRecovery. SlowStartThreshold %d, "
                              "Window %d, FastRecoveryEnd %x\n",
                              Socket->SlowStartThreshold,
                              Socket->CongestionWindowSize,
                              Socket->FastRecoveryEndSequence);
            }

        //
        // Process additional duplicate ACKs coming in after the window was cut.
        // Inflate the window to represent those packets sequentially after the
        // missing packet that are buffered up in the receiver.
        //

        } else {
            Socket->CongestionWindowSize += SegmentSize;
            if (NetTcpDebugPrintCongestionControl != FALSE) {
                NetpTcpPrintSocketEndpoints(Socket, FALSE);
                RtlDebugPrint(" FastRecovery ACK #%d. Window %d\n",
                              Socket->DuplicateAcknowledgeCount,
                              Socket->CongestionWindowSize);
            }
        }

        //
        // Fast retransmit the packet that's missing.
        //

        if (Socket->SendWindowSize != 0) {
            NetpTcpRetransmit(Socket);
        }
    }

    return;
}

VOID
NetpTcpProcessNewRoundTripTimeSample (
    PTCP_SOCKET Socket,
    ULONGLONG RoundTripTicks
    )

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

{

    ULONGLONG NewMilliseconds;
    ULONGLONG NewRoundTripTime;
    ULONGLONG SampleMilliseconds;
    ULONGLONG TimeCounterFrequency;

    //
    // The new round trip time is equal to A * NewSample + (1 - A) * OldValue,
    // basically a weighted average. The A part is split into a numerator and
    // denominator, and the result is stored multiplied by the denominator. So
    // the calculation is:
    // (Numerator * New) + (((Denominator - Numerator) * Original) /
    //                      Denominator).
    //

    NewRoundTripTime = (RoundTripTicks * TCP_ROUND_TRIP_SAMPLE_NUMERATOR) +
                       ((Socket->RoundTripTime *
                         (TCP_ROUND_TRIP_SAMPLE_DENOMINATOR -
                          TCP_ROUND_TRIP_SAMPLE_NUMERATOR)) /
                         TCP_ROUND_TRIP_SAMPLE_DENOMINATOR);

    Socket->RoundTripTime = NewRoundTripTime;
    if (NetTcpDebugPrintCongestionControl != FALSE) {
        TimeCounterFrequency = HlQueryTimeCounterFrequency();
        SampleMilliseconds = (RoundTripTicks * MILLISECONDS_PER_SECOND) /
                             TimeCounterFrequency;

        NewMilliseconds = ((NewRoundTripTime *
                            MILLISECONDS_PER_SECOND) /
                           TCP_ROUND_TRIP_SAMPLE_DENOMINATOR) /
                          TimeCounterFrequency;

        NetpTcpPrintSocketEndpoints(Socket, TRUE);
        RtlDebugPrint(" Round trip sample %I64dms, new estimate %I64dms.\n",
                      SampleMilliseconds,
                      NewMilliseconds);
    }

    return;
}

VOID
NetpTcpGetTransmitTimeoutInterval (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    )

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

{

    ULONGLONG Milliseconds;
    ULONGLONG NewTimeoutInterval;

    //
    // If this is the first time this is being sent, then set the timeout to
    // a couple round trip times.
    //

    if (Segment->SendAttemptCount == 0) {

        ASSERT(Segment->TimeoutInterval == 0);
        ASSERT(Socket->RoundTripTime != 0);

        Segment->TimeoutInterval = (Socket->RoundTripTime *
                                    TCP_ROUND_TRIP_TIMEOUT_FACTOR) /
                                   TCP_ROUND_TRIP_SAMPLE_DENOMINATOR;

    //
    // This packet is going around again, bump up the previous timeout
    // interval.
    //

    } else {

        ASSERT(Segment->TimeoutInterval != 0);

        NewTimeoutInterval =
                     Segment->TimeoutInterval * TCP_ROUND_TRIP_TIMEOUT_FACTOR;

        //
        // This assert catches both a zero timeout interval (which is not
        // valid) and a timeout that just overflowed during the multiply.
        //

        ASSERT(NewTimeoutInterval > Segment->TimeoutInterval);

        Segment->TimeoutInterval = NewTimeoutInterval;
    }

    if (NetTcpDebugPrintCongestionControl != FALSE) {
        Milliseconds = Segment->TimeoutInterval * MILLISECONDS_PER_SECOND /
                       HlQueryTimeCounterFrequency();

        RtlDebugPrint("TCP: Packet timeout %I64dms.\n", Milliseconds);
    }

    return;
}

VOID
NetpTcpTransmissionTimeout (
    PTCP_SOCKET Socket,
    PTCP_SEND_SEGMENT Segment
    )

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

{

    ULONG RelativeSequenceNumber;
    ULONGLONG SentTime;
    ULONGLONG TimeoutTime;

    //
    // Set the slow start threshold to half of what the congestion window was
    // before the loss. Move all the way back to slow start for a loss.
    //

    Socket->SlowStartThreshold = Socket->CongestionWindowSize / 2;
    Socket->CongestionWindowSize = Socket->SendMaxSegmentSize;
    if (NetTcpDebugPrintCongestionControl != FALSE) {
        NetpTcpPrintSocketEndpoints(Socket, TRUE);
        RelativeSequenceNumber = Segment->SequenceNumber -
                                 Socket->SendInitialSequence;

        SentTime = (Segment->LastSendTime * MILLISECONDS_PER_SECOND) /
                   HlQueryTimeCounterFrequency();

        TimeoutTime = (Segment->TimeoutInterval * MILLISECONDS_PER_SECOND) /
                       HlQueryTimeCounterFrequency();

        RtlDebugPrint(" Timeout on Seq %d sent %I64dms timeout %I64dms, New "
                      "SlowStartThreshold %d, CWindow %d.\n",
                      RelativeSequenceNumber,
                      SentTime,
                      TimeoutTime,
                      Socket->SlowStartThreshold,
                      Socket->CongestionWindowSize);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

