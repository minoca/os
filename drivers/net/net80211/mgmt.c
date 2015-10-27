/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mgmt.c

Abstract:

    This module implements management frame handling functionality for the
    802.11 core wireless networking library.

Author:

    Chris Steven 19-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "net80211.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default amount of time to wait for a reply management frame to
// arrive.
//

#define NET80211_MANAGEMENT_FRAME_TIMEOUT (1 * MILLISECONDS_PER_SECOND)

//
// Define the default number of times to retry a management frame before giving
// up.
//

#define NET80211_MANAGEMENT_RETRY_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context used to joing an basic service set (BSS).

Members:

    Link - Stores a pointer to the link that is trying to join the BSS.

    LinkAddress - Stores a pointer to the link's local address to be used in
        joining the BSS.

    Ssid - Stores the string identifying the BSS to join.

    SsidLength - Stores the length of the BSS identifier string, including the
        NULL terminator.

    Bssid - Stores the MAC address of the BSS's access point (a.k.a the BSSID).

    BeaconInterval - Stores the beacon interval for the BSS to which the
        station is attempting to join.

    Capabilities - Stores the capabilities for the BSS to which the station is
        attempting to join.

    AssociationId = Stores the association ID assigned to the station by the AP.

--*/

typedef struct _NET80211_BSS_CONTEXT {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PSTR Ssid;
    ULONG SsidLength;
    NETWORK_ADDRESS Bssid;
    USHORT BeaconInterval;
    USHORT Capabilities;
    USHORT AssociationId;
    PNET80211_LINK_RATE_INFORMATION RateInformation;
} NET80211_BSS_CONTEXT, *PNET80211_BSS_CONTEXT;

/*++

Structure Description:

    This structure defines a cached management frame.

Members:

    ListEntry - Stores a set of pointers to the next and previous saved
        management frames.

    Buffer - Stores a pointer to the management frame data, including the 802.11
        header.

    BufferSize - Stores the size of the management frame, in bytes.

--*/

typedef struct _NET80211_MANAGEMENT_FRAME {
    LIST_ENTRY ListEntry;
    PVOID Buffer;
    ULONG BufferSize;
} NET80211_MANAGEMENT_FRAME, *PNET80211_MANAGEMENT_FRAME;

/*++

Structure Description:

    This structure defines the frame body used for open system authentication.
    For other types of authentication, other fields may be required.

Members:

    AlgorithmNumber - Stores the algorithm in use for the authentication
        process.

    TransactionSequenceNumber - Stores the sequence number of the
        authentication transaction process.

    StatusCode - Stores the states of the authentication process.

--*/

typedef struct _NET80211_AUTHENTICATION_OPEN_BODY {
    USHORT AlgorithmNumber;
    USHORT TransactionSequenceNumber;
    USHORT StatusCode;
} PACKED NET80211_AUTHENTICATION_OPEN_BODY, *PNET80211_AUTHENTICATION_OPEN_BODY;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
Net80211pJoinBssThread (
    PVOID Parameter
    );

KSTATUS
Net80211pSendProbeRequest (
    PNET80211_BSS_CONTEXT Context,
    ULONG Channel
    );

KSTATUS
Net80211pReceiveProbeResponse (
    PNET80211_BSS_CONTEXT Context,
    ULONG Channel
    );

KSTATUS
Net80211pSendAuthenticationRequest (
    PNET80211_BSS_CONTEXT Context
    );

KSTATUS
Net80211pReceiveAuthenticationResponse (
    PNET80211_BSS_CONTEXT Context
    );

KSTATUS
Net80211pSendAssociationRequest (
    PNET80211_BSS_CONTEXT Context
    );

KSTATUS
Net80211pReceiveAssociationResponse (
    PNET80211_BSS_CONTEXT Context
    );

KSTATUS
Net80211pSendManagementFrame (
    PNET_LINK Link,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNETWORK_ADDRESS Bssid,
    ULONG FrameSubtype,
    PVOID FrameBody,
    ULONG FrameBodySize
    );

KSTATUS
Net80211pReceiveManagementFrame (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    ULONG FrameSubtype,
    PNET80211_MANAGEMENT_FRAME *Frame
    );

PNET80211_BSS_CONTEXT
Net80211pCreateBssContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PSTR Ssid,
    ULONG SsidLength
    );

VOID
Net80211pDestroyBssContext (
    PNET80211_BSS_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Net80211pJoinBss (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PSTR Ssid,
    ULONG SsidLength
    )

/*++

Routine Description:

    This routine attempts to join the link to the basic service set (BSS)
    identified by the given SSID.

Arguments:

    Link - Supplies a pointer to the link that is requesting to join a network.

    LinkAddress - Supplies the link address for the link that wants to join the
         network.

    Ssid - Supplies the SSID of the network to join.

    SsidLength - Supplies the length of the SSID string, including the NULL
        terminator.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_CONTEXT Context;
    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;
    if (Net80211Link->State != Net80211StateStarted) {
        return STATUS_NOT_INITIALIZED;
    }

    if ((Ssid == NULL) && (SsidLength != 0)) {
        return STATUS_INVALID_PARAMETER;
    }

    Context = Net80211pCreateBssContext(Link, LinkAddress, Ssid, SsidLength);
    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto JoinBssEnd;
    }

    Status = PsCreateKernelThread(Net80211pJoinBssThread,
                                  Context,
                                  "Net80211pJoinBssThread");

    if (!KSUCCESS(Status)) {
        goto JoinBssEnd;
    }

JoinBssEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            Net80211pDestroyBssContext(Context);
        }
    }

    return Status;
}

VOID
Net80211pProcessManagementFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes 802.11 management frames.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PNET80211_MANAGEMENT_FRAME Frame;
    ULONG FrameSize;
    ULONG FrameSubtype;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    PNET80211_LINK Net80211Link;
    BOOL SaveAndSignal;

    Net80211Link = Link->DataLinkContext;
    Header = Packet->Buffer + Packet->DataOffset;
    SaveAndSignal = FALSE;
    FrameSubtype = NET80211_GET_FRAME_SUBTYPE(Header);
    switch (FrameSubtype) {
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_RESPONSE:
        if (Net80211Link->State != Net80211StateAssociationSent) {
            break;
        }

        SaveAndSignal = TRUE;
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_RESPONSE:
        if (Net80211Link->State != Net80211StateProbeSent) {
            break;
        }

        SaveAndSignal = TRUE;
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION:
        if (Net80211Link->State != Net80211StateAuthenticationSent) {
            break;
        }

        SaveAndSignal = TRUE;
        break;

    //
    // Ignore packets that are not yet handled.
    //

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_REASSOCIATION_RESPONSE:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_DISASSOCIATION:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_DEAUTHENTICATION:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_BEACON:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_TIMING_ADVERTISEMENT:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ATIM:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ACTION:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ACTION_NO_ACK:
        break;

    //
    // Toss out these request packets until AP mode is supported.
    //

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_REQUEST:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_REASSOCIATION_REQUEST:
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_REQUEST:
    default:
        break;
    }

    //
    // If the frame is to be saved and its arrival is to be signaled for a
    // waiting thread to pick up do that now.
    //

    if (SaveAndSignal != FALSE) {
        FrameSize = Packet->FooterOffset - Packet->DataOffset;
        AllocationSize = FrameSize + sizeof(NET80211_MANAGEMENT_FRAME);
        Frame = MmAllocatePagedPool(AllocationSize, NET80211_ALLOCATION_TAG);
        if (Frame == NULL) {
            goto ProcessManagementFrameEnd;
        }

        Frame->Buffer = (Frame + 1);
        Frame->BufferSize = FrameSize;
        RtlCopyMemory(Frame->Buffer,
                      Packet->Buffer + Packet->DataOffset,
                      FrameSize);

        KeAcquireQueuedLock(Net80211Link->Lock);
        INSERT_BEFORE(&(Frame->ListEntry),
                      &(Net80211Link->ManagementFrameList));

        KeSignalEvent(Net80211Link->ManagementFrameEvent,
                      SignalOptionSignalAll);

        KeReleaseQueuedLock(Net80211Link->Lock);
    }

ProcessManagementFrameEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Net80211pJoinBssThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to join a basic service set (BSS) using the 802.11
    association sequence.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread, in
        this case a pointer to the BSS context.

Return Value:

    Status code.

--*/

{

    ULONG ApIndex;
    UCHAR ApRate;
    ULONG Attempts;
    ULONG Channel;
    PNET80211_BSS_CONTEXT Context;
    ULONG LocalIndex;
    UCHAR LocalRate;
    UCHAR MaxRate;
    PNET80211_LINK Net80211Link;
    BOOL ServiceSetFound;
    KSTATUS Status;

    Context = (PNET80211_BSS_CONTEXT)Parameter;
    Net80211Link = (PNET80211_LINK)Context->Link->DataLinkContext;
    ServiceSetFound = FALSE;

    //
    // TODO: Look for the SSID in the cache of networks collected from beacons.
    //

    //
    // If it was not found in the beacon cache, then send a probe request and
    // wait for a response.
    //

    if (ServiceSetFound == FALSE) {
        for (Channel = 1;
             Channel <= Net80211Link->Properties.MaxChannel;
             Channel += 1) {

            Net80211Link->State = Net80211StateProbeSent;
            Attempts = NET80211_MANAGEMENT_RETRY_COUNT;
            while (Attempts != 0) {
                Attempts -= 1;
                Status = Net80211pSendProbeRequest(Context, Channel);
                if (!KSUCCESS(Status)) {
                    goto JoinBssThreadEnd;
                }

                Status = Net80211pReceiveProbeResponse(Context, Channel);
                if (Status == STATUS_TIMEOUT) {
                    continue;
                }

                if (KSUCCESS(Status)) {
                    ServiceSetFound = TRUE;
                    Net80211Link->State = Net80211StateProbeReceived;
                }

                break;
            }

            if (ServiceSetFound != FALSE) {
                break;
            }
        }
    }

    //
    // If the service set could not be found, then exit.
    //

    if (ServiceSetFound == FALSE) {
        Status = STATUS_UNSUCCESSFUL;
        goto JoinBssThreadEnd;
    }

    //
    // Now that the access point for the BSS is within communication, start the
    // authentication sequence. The authentication request is a unicast packet
    // so the hardware will handle the retransmission process.
    //

    Net80211Link->State = Net80211StateAuthenticationSent;
    Status = Net80211pSendAuthenticationRequest(Context);
    if (!KSUCCESS(Status)) {
        goto JoinBssThreadEnd;
    }

    //
    // Wait for the authentication response.
    //

    Status = Net80211pReceiveAuthenticationResponse(Context);
    if (!KSUCCESS(Status)) {
        goto JoinBssThreadEnd;
    }

    Net80211Link->State = Net80211StateAuthenticated;

    //
    // The link is authentication with the BSS. Attempt to join it via the
    // association sequence. The association request is a unicast packet so the
    // hardware will handle the retransmission process.
    //

    Net80211Link->State = Net80211StateAssociationSent;
    Status = Net80211pSendAssociationRequest(Context);
    if (!KSUCCESS(Status)) {
        goto JoinBssThreadEnd;
    }

    //
    // Wait for the association response.
    //

    Status = Net80211pReceiveAssociationResponse(Context);
    if (!KSUCCESS(Status)) {
        goto JoinBssThreadEnd;
    }

    RtlCopyMemory(&(Net80211Link->Bssid),
                  &Context->Bssid,
                  sizeof(NETWORK_ADDRESS));

    Net80211Link->State = Net80211StateAssociated;

    //
    // Determine the link speed by taking the maximum rate supported by both
    // the local station and the BSS's access point. This is O(N^2), but there
    // are never many rates and an O(N) algorithm would add space complexity
    // due to hashing. This is not a common operation.
    //

    MaxRate = 0;
    for (LocalIndex = 0;
         LocalIndex < Net80211Link->Properties.SupportedRates->Count;
         LocalIndex += 1) {

        LocalRate = Net80211Link->Properties.SupportedRates->Rates[LocalIndex];
        LocalRate &= ~NET80211_RATE_BASIC;
        if (LocalRate <= MaxRate) {
            continue;
        }

        for (ApIndex = 0;
             ApIndex < Context->RateInformation->Count;
             ApIndex += 1) {

            ApRate = Context->RateInformation->Rates[ApIndex];
            ApRate &= ~NET80211_RATE_BASIC;
            if (ApRate == LocalRate) {
                MaxRate = LocalRate;
                break;
            }
        }
    }

    //
    // There are no matching rates. This should really not happen given that
    // APs should not response to probes unless the rates and capabilities are
    // agreeable.
    //

    if (MaxRate == 0) {
        RtlDebugPrint("802.11: Failing to join BSS %s because the AP and "
                      "station have no matching rates.\n",
                      Context->Ssid);

        Status = STATUS_UNSUCCESSFUL;
        goto JoinBssThreadEnd;
    }

    Status = NetStartLink(Context->Link);
    if (!KSUCCESS(Status)) {
        goto JoinBssThreadEnd;
    }

    NetSetLinkState(Context->Link, TRUE, MaxRate * NET80211_RATE_UNIT);

JoinBssThreadEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Joining BSS %s failed with status 0x%08x\n",
                      Context->Ssid,
                      Status);
    }

    Net80211pDestroyBssContext(Context);
    return;
}

KSTATUS
Net80211pSendProbeRequest (
    PNET80211_BSS_CONTEXT Context,
    ULONG Channel
    )

/*++

Routine Description:

    This routine sends an 802.11 management probe request frame on the
    specified channel targeting the SSID stored in the BSS context.

Arguments:

    Context - Supplies a pointer to the current BSS context being used to join
        a BSS.

    Channel - Supplies the channel on which to send the probe request.

Return Value:

    Status code.

--*/

{

    PUCHAR FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    ULONG Index;
    PUCHAR InformationByte;
    PNET80211_LINK Net80211Link;
    PNET80211_LINK_RATE_INFORMATION Rates;
    PNETWORK_ADDRESS SourceAddress;
    ULONG SsidLength;
    KSTATUS Status;

    Net80211Link = Context->Link->DataLinkContext;
    FrameBody = NULL;

    //
    // Determine the size of the probe request packet.
    //

    FrameBodySize = 0;

    //
    // Get the SSID size.
    //

    FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
    SsidLength = Context->SsidLength - 1;
    if (SsidLength > NET80211_SSID_MAX_LENGTH) {
        Status = STATUS_INVALID_PARAMETER;
        goto SendProbeRequestEnd;
    }

    if (Context->Ssid == NULL) {
        SsidLength = 0;
    }

    FrameBodySize += SsidLength;

    //
    // Get the supported rates size.
    //

    Rates = Net80211Link->Properties.SupportedRates;
    FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        FrameBodySize += Rates->Count;

    } else {
        FrameBodySize += NET80211_MAX_SUPPORTED_RATES;
        FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
        FrameBodySize += Rates->Count - NET80211_MAX_SUPPORTED_RATES;
    }

    //
    // Get the DSSS (channel) size.
    //

    FrameBodySize += NET80211_DSSS_SIZE;

    //
    // Allocate a buffer to hold the probe request frame body.
    //

    FrameBody = MmAllocatePagedPool(FrameBodySize, NET80211_ALLOCATION_TAG);
    if (FrameBody == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendProbeRequestEnd;
    }

    //
    // Fill out the frame body. There is a strict order here, so do not
    // rearrange the information elements.
    //

    InformationByte = FrameBody;
    *InformationByte = NET80211_ELEMENT_SSID;
    InformationByte += 1;
    *InformationByte = SsidLength;
    InformationByte += 1;
    if (SsidLength != 0) {
        RtlCopyMemory(InformationByte, Context->Ssid, SsidLength);
        InformationByte += SsidLength;
    }

    *InformationByte = NET80211_ELEMENT_SUPPORTED_RATES;
    InformationByte += 1;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        *InformationByte = Rates->Count;
        InformationByte += 1;
        for (Index = 0; Index < Rates->Count; Index += 1) {
            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }

    } else {
        *InformationByte = NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        for (Index = 0; Index < NET80211_MAX_SUPPORTED_RATES; Index += 1) {
            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }

        *InformationByte = NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES;
        InformationByte += 1;
        *InformationByte = Rates->Count - NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        for (Index = NET80211_MAX_SUPPORTED_RATES;
             Index < Rates->Count;
             Index += 1) {

            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }
    }

    *InformationByte = NET80211_ELEMENT_DSSS;
    InformationByte += 1;
    *InformationByte = 1;
    InformationByte += 1;
    *InformationByte = (UCHAR)Channel;
    InformationByte += 1;

    ASSERT(FrameBodySize == (InformationByte - FrameBody));

    //
    // Set the channel to send the packet over.
    //

    Status = Net80211Link->Properties.Interface.SetChannel(
                                        Net80211Link->Properties.DriverContext,
                                        Channel);

    if (!KSUCCESS(Status)) {
        goto SendProbeRequestEnd;
    }

    //
    // Send the management frame down the lower layers.
    //

    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_REQUEST;
    SourceAddress = &(Context->LinkAddress->PhysicalAddress);
    Status = Net80211pSendManagementFrame(Context->Link,
                                          SourceAddress,
                                          NULL,
                                          NULL,
                                          FrameSubtype,
                                          FrameBody,
                                          FrameBodySize);

    if (!KSUCCESS(Status)) {
        goto SendProbeRequestEnd;
    }

SendProbeRequestEnd:
    if (FrameBody != NULL) {
        MmFreePagedPool(FrameBody);
    }

    return Status;
}

KSTATUS
Net80211pReceiveProbeResponse (
    PNET80211_BSS_CONTEXT Context,
    ULONG Channel
    )

/*++

Routine Description:

    This routine receives an 802.11 management probe response frame on the
    specified channel targeting the SSID stored in the BSS context.

Arguments:

    Context - Supplies a pointer to the current BSS context being used to join
        a BSS.

    Channel - Supplies the channel on which to receive the probe request.

Return Value:

    Status code.

--*/

{

    BOOL AcceptedResponse;
    ULONG Attempts;
    USHORT BeaconInterval;
    USHORT Capabilities;
    PUCHAR ElementBytePointer;
    UCHAR ElementId;
    ULONG ElementLength;
    PNET80211_MANAGEMENT_FRAME Frame;
    ULONG FrameSubtype;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    BOOL Match;
    ULONG Offset;
    ULONG ResponseChannel;
    KSTATUS Status;

    //
    // Attempt to receive a probe response. Retry a few times in case an
    // erroneous packet arrives.
    //

    Attempts = NET80211_MANAGEMENT_RETRY_COUNT;
    while (Attempts != 0) {
        Attempts -= 1;
        FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_RESPONSE;
        Status = Net80211pReceiveManagementFrame(Context->Link,
                                                 Context->LinkAddress,
                                                 FrameSubtype,
                                                 &Frame);

        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // OK! Parse the response. It should at least have a timestamp, beacon
        // interval, and capabilities field.
        //

        ElementBytePointer = Frame->Buffer;
        Offset = sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
        if ((Offset +
             NET80211_TIMESTAMP_SIZE +
             NET80211_BEACON_INTERVAL_SIZE +
             NET80211_CAPABILITY_SIZE) > Frame->BufferSize) {

            Status = STATUS_DATA_LENGTH_MISMATCH;
            continue;
        }

        //
        // Skip the timestamp.
        //

        Offset += NET80211_TIMESTAMP_SIZE;

        //
        // Save the beacon internval.
        //

        BeaconInterval = *((PUSHORT)(&(ElementBytePointer[Offset])));
        Offset += NET80211_BEACON_INTERVAL_SIZE;

        //
        // Save the capabilities.
        //

        Capabilities = *((PUSHORT)(&(ElementBytePointer[Offset])));
        Offset += NET80211_CAPABILITY_SIZE;

        //
        // Now look at the information elements.
        //

        AcceptedResponse = TRUE;
        while ((AcceptedResponse != FALSE) && (Offset < Frame->BufferSize)) {
            ElementId = ElementBytePointer[Offset];
            Offset += 1;
            if (Offset >= Frame->BufferSize) {
                Status = STATUS_DATA_LENGTH_MISMATCH;
                break;
            }

            ElementLength = ElementBytePointer[Offset];
            Offset += 1;
            if ((Offset + ElementLength) > Frame->BufferSize) {
                Status = STATUS_DATA_LENGTH_MISMATCH;
                break;
            }

            switch (ElementId) {

            //
            // If the SSID does not match the given SSID, then it is a response
            // from the wrong SSID.
            //

            case NET80211_ELEMENT_SSID:
                if (ElementLength != (Context->SsidLength - 1)) {
                    AcceptedResponse = FALSE;
                    break;
                }

                Match = RtlCompareMemory(&(ElementBytePointer[Offset]),
                                         Context->Ssid,
                                         ElementLength);

                if (Match == FALSE) {
                    AcceptedResponse = FALSE;
                }

                break;

            case NET80211_ELEMENT_DSSS:
                ResponseChannel = (ULONG)ElementBytePointer[Offset];
                if (ResponseChannel != Channel) {
                    RtlDebugPrint("802.11: Received probe response from "
                                  "unexpected channel %d. Expected %d.\n",
                                  ResponseChannel,
                                  Channel);

                    AcceptedResponse = FALSE;
                    Status = STATUS_UNEXPECTED_TYPE;
                }

                break;

            case NET80211_ELEMENT_SUPPORTED_RATES:
            case NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES:
                break;

            default:
                break;
            }

            Offset += ElementLength;
        }

        if (AcceptedResponse != FALSE) {

            ASSERT(KSUCCESS(Status));

            Header = Frame->Buffer;
            Context->Bssid.Network = SocketNetworkPhysical80211;
            RtlCopyMemory(Context->Bssid.Address,
                          Header->SourceAddress,
                          NET80211_ADDRESS_SIZE);

            Context->BeaconInterval = BeaconInterval;
            Context->Capabilities = Capabilities;
            break;
        }
    }

    return Status;
}

KSTATUS
Net80211pSendAuthenticationRequest (
    PNET80211_BSS_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends an 802.11 management authentication frame to the AP
    indicated by the given BSS context.

Arguments:

    Context - Supplies a pointer to the context in use to join a BSS.

Return Value:

    Status code.

--*/

{

    PNET80211_AUTHENTICATION_OPEN_BODY FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    PNETWORK_ADDRESS SourceAddress;
    KSTATUS Status;

    //
    // The body is the size of the standard 802.11 authentication body when
    // using the open system authentication algorithm.
    //

    FrameBodySize = sizeof(NET80211_AUTHENTICATION_OPEN_BODY);

    //
    // Allocate a buffer to hold the authentication request frame body.
    //

    FrameBody = MmAllocatePagedPool(FrameBodySize, NET80211_ALLOCATION_TAG);
    if (FrameBody == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendAuthenticationEnd;
    }

    //
    // Fill out the authentication body.
    //

    FrameBody->AlgorithmNumber = NET80211_AUTHENTICATION_ALGORITHM_OPEN;
    FrameBody->TransactionSequenceNumber =
                               NET80211_AUTHENTICATION_REQUEST_SEQUENCE_NUMBER;

    FrameBody->StatusCode = NET80211_STATUS_CODE_SUCCESS;

    //
    // Send the authentication frame off. The destination address and BSSID
    // should match.
    //

    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION;
    SourceAddress = &(Context->LinkAddress->PhysicalAddress);
    Status = Net80211pSendManagementFrame(Context->Link,
                                          SourceAddress,
                                          &(Context->Bssid),
                                          &(Context->Bssid),
                                          FrameSubtype,
                                          FrameBody,
                                          FrameBodySize);

    if (!KSUCCESS(Status)) {
        goto SendAuthenticationEnd;
    }

SendAuthenticationEnd:
    if (FrameBody != NULL) {
        MmFreePagedPool(FrameBody);
    }

    return Status;
}

KSTATUS
Net80211pReceiveAuthenticationResponse (
    PNET80211_BSS_CONTEXT Context
    )

/*++

Routine Description:

    This routine receives an authentication response frame. It is expected to
    be sent from the BSSID stored in the BSS context.

Arguments:

    Context - Supplies a pointer to the context in use to join a BSS.

Return Value:

    Status code.

--*/

{

    ULONG Attempts;
    PNET80211_AUTHENTICATION_OPEN_BODY Body;
    PNET80211_MANAGEMENT_FRAME Frame;
    ULONG FrameSubtype;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    BOOL Match;
    KSTATUS Status;

    //
    // Attempt to receive a management frame from the access point. Try a few
    // times in case there are unwanted packets sitting in list of received
    // packets.
    //

    Attempts = NET80211_MANAGEMENT_RETRY_COUNT;
    while (Attempts != 0) {
        Attempts -= 1;
        FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION;
        Status = Net80211pReceiveManagementFrame(Context->Link,
                                                 Context->LinkAddress,
                                                 FrameSubtype,
                                                 &Frame);

        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Make sure the this frame was sent from the AP of the BSS.
        //

        Header = Frame->Buffer;
        Match = RtlCompareMemory(Header->SourceAddress,
                                 Context->Bssid.Address,
                                 NET80211_ADDRESS_SIZE);

        if (Match == FALSE) {
            Status = STATUS_INVALID_ADDRESS;
            continue;
        }

        //
        // Make sure it is large enough to hold the authentication body.
        //

        if (Frame->BufferSize <
            (sizeof(NET80211_MANAGEMENT_FRAME_HEADER) +
             sizeof(NET80211_AUTHENTICATION_OPEN_BODY))) {

            Status = STATUS_DATA_LENGTH_MISMATCH;
            continue;
        }

        //
        // The authentication response has a very fixed frame body.
        //

        Body = Frame->Buffer + sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
        if (Body->AlgorithmNumber != NET80211_AUTHENTICATION_ALGORITHM_OPEN) {
            RtlDebugPrint("802.11: Unexpected algorithm type %d. Expected "
                          "%d.\n",
                          Body->AlgorithmNumber,
                          NET80211_AUTHENTICATION_ALGORITHM_OPEN);

            continue;
        }

        if (Body->TransactionSequenceNumber !=
            NET80211_AUTHENTICATION_RESPONSE_SEQUENCE_NUMBER) {

            RtlDebugPrint("802.11: Unexpected authentication transaction "
                          "sequence number 0x%04x. Expected 0x%04x.\n",
                          Body->TransactionSequenceNumber,
                          NET80211_AUTHENTICATION_RESPONSE_SEQUENCE_NUMBER);

            continue;
        }

        if (Body->StatusCode != NET80211_STATUS_CODE_SUCCESS) {
            RtlDebugPrint("802.11: Authentication failed with status %d\n",
                          Body->StatusCode);

            Status = STATUS_UNSUCCESSFUL;
            break;
        }

        ASSERT(KSUCCESS(Status));

        break;
    }

    return Status;
}

KSTATUS
Net80211pSendAssociationRequest (
    PNET80211_BSS_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends an 802.11 management association request frame to the
    access point stores in the BSS context.

Arguments:

    Context - Supplies a pointer to the context in use to join a BSS.

Return Value:

    Status code.

--*/

{

    PUCHAR FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    ULONG Index;
    PUCHAR InformationByte;
    PNET80211_LINK Net80211Link;
    PNET80211_LINK_RATE_INFORMATION Rates;
    PNETWORK_ADDRESS SourceAddress;
    ULONG SsidLength;
    KSTATUS Status;

    Net80211Link = Context->Link->DataLinkContext;
    FrameBody = NULL;

    ASSERT((Context->Ssid != NULL) && (Context->SsidLength > 1));

    //
    // Determine the size of the probe response packet.
    //

    FrameBodySize = 0;

    //
    // Account for the capability size.
    //

    FrameBodySize += NET80211_CAPABILITY_SIZE;

    //
    // Account for the listen interval.
    //

    FrameBodySize += NET80211_LISTEN_INTERVAL_SIZE;

    //
    // Get the SSID size.
    //

    FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
    SsidLength = Context->SsidLength - 1;
    if (SsidLength > 32) {
        Status = STATUS_INVALID_PARAMETER;
        goto SendAssociationRequestEnd;
    }

    FrameBodySize += SsidLength;

    //
    // Get the supported rates size.
    //

    Rates = Net80211Link->Properties.SupportedRates;
    FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        FrameBodySize += Rates->Count;

    } else {
        FrameBodySize += NET80211_MAX_SUPPORTED_RATES;
        FrameBodySize += NET80211_BASE_ELEMENT_SIZE;
        FrameBodySize += Rates->Count - NET80211_MAX_SUPPORTED_RATES;
    }

    //
    // Allocate a buffer to hold the assocation request frame body.
    //

    FrameBody = MmAllocatePagedPool(FrameBodySize, NET80211_ALLOCATION_TAG);
    if (FrameBody == NULL) {
        goto SendAssociationRequestEnd;
    }

    //
    // Fill out the frame body. There is a strict order here, so do not
    // rearrage the information elements.
    //

    InformationByte = FrameBody;
    *((PUSHORT)InformationByte) = Net80211Link->Properties.Capabilities;
    InformationByte += NET80211_CAPABILITY_SIZE;

    //
    // TODO: Implement a non-zero 802.11 listen interval for power save mode.
    //

    *((PUSHORT)InformationByte) = 0;
    InformationByte += NET80211_LISTEN_INTERVAL_SIZE;
    *InformationByte = NET80211_ELEMENT_SSID;
    InformationByte += 1;
    *InformationByte = SsidLength;
    InformationByte += 1;
    RtlCopyMemory(InformationByte, Context->Ssid, SsidLength);
    InformationByte += SsidLength;
    *InformationByte = NET80211_ELEMENT_SUPPORTED_RATES;
    InformationByte += 1;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        *InformationByte = Rates->Count;
        InformationByte += 1;
        for (Index = 0; Index < Rates->Count; Index += 1) {
            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }

    } else {
        *InformationByte = NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        for (Index = 0; Index < NET80211_MAX_SUPPORTED_RATES; Index += 1) {
            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }

        *InformationByte = NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES;
        InformationByte += 1;
        *InformationByte = Rates->Count - NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        for (Index = NET80211_MAX_SUPPORTED_RATES;
             Index < Rates->Count;
             Index += 1) {

            *InformationByte = Rates->Rates[Index];
            InformationByte += 1;
        }
    }

    ASSERT(FrameBodySize == (InformationByte - FrameBody));

    //
    // Send the management frame down the lower layers.
    //

    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_REQUEST;
    SourceAddress = &(Context->LinkAddress->PhysicalAddress);
    Status = Net80211pSendManagementFrame(Context->Link,
                                          SourceAddress,
                                          &(Context->Bssid),
                                          &(Context->Bssid),
                                          FrameSubtype,
                                          FrameBody,
                                          FrameBodySize);

    if (!KSUCCESS(Status)) {
        goto SendAssociationRequestEnd;
    }

SendAssociationRequestEnd:
    if (FrameBody != NULL) {
        MmFreePagedPool(FrameBody);
    }

    return Status;
}

KSTATUS
Net80211pReceiveAssociationResponse (
    PNET80211_BSS_CONTEXT Context
    )

/*++

Routine Description:

    This routine receives an 802.11 management association response frame from
    the access point stores in the BSS context.

Arguments:

    Context - Supplies a pointer to the context in use to join a BSS.

Return Value:

    Status code.

--*/

{

    BOOL AcceptedResponse;
    PVOID Allocation;
    ULONG AllocationSize;
    USHORT AssociationId;
    ULONG Attempts;
    USHORT Capabilities;
    PUCHAR ElementBytePointer;
    UCHAR ElementId;
    ULONG ElementLength;
    ULONG ExtendedRateCount;
    ULONG ExtendedRateOffset;
    PNET80211_MANAGEMENT_FRAME Frame;
    USHORT FrameStatus;
    ULONG FrameSubtype;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    ULONG Index;
    BOOL Match;
    ULONG Offset;
    ULONG RateCount;
    ULONG RateOffset;
    KSTATUS Status;
    ULONG TotalRateCount;

    //
    // Attempt to receive a management frame from the access point. Retry a few
    // times in case a bad packet comes in.
    //

    Attempts = NET80211_MANAGEMENT_RETRY_COUNT;
    while (Attempts != 0) {
        Attempts -= 1;
        FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_RESPONSE;
        Status = Net80211pReceiveManagementFrame(Context->Link,
                                                 Context->LinkAddress,
                                                 FrameSubtype,
                                                 &Frame);

        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Make sure the this frame was sent from the destination.
        //

        Header = Frame->Buffer;
        Match = RtlCompareMemory(Header->SourceAddress,
                                 Context->Bssid.Address,
                                 NET80211_ADDRESS_SIZE);

        if (Match == FALSE) {
            Status = STATUS_INVALID_ADDRESS;
            continue;
        }

        //
        // OK! Parse the response. There should at least be capabilities, a
        // status code and the AID.
        //

        ElementBytePointer = Frame->Buffer;
        Offset = sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
        if ((Offset +
             NET80211_CAPABILITY_SIZE +
             NET80211_STATUS_CODE_SIZE +
             NET80211_ASSOCIATION_ID_SIZE) > Frame->BufferSize) {

            Status = STATUS_DATA_LENGTH_MISMATCH;
            continue;
        }

        //
        // Save the capabilities.
        //

        Capabilities = *((PUSHORT)&(ElementBytePointer[Offset]));
        Offset += NET80211_CAPABILITY_SIZE;

        //
        // Check the frame status.
        //

        FrameStatus = *((PUSHORT)(&(ElementBytePointer[Offset])));
        if (FrameStatus != NET80211_STATUS_CODE_SUCCESS) {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }

        Offset += NET80211_STATUS_CODE_SIZE;

        //
        // Save the association ID.
        //

        AssociationId = *((PUSHORT)&(ElementBytePointer[Offset]));
        Offset += NET80211_ASSOCIATION_ID_SIZE;

        //
        // Now look at the supplied elements.
        //

        RateCount = 0;
        RateOffset = 0;
        ExtendedRateCount = 0;
        ExtendedRateOffset = 0;
        AcceptedResponse = TRUE;
        while ((AcceptedResponse != FALSE) && (Offset < Frame->BufferSize)) {
            ElementId = ElementBytePointer[Offset];
            Offset += 1;
            if (Offset >= Frame->BufferSize) {
                Status = STATUS_DATA_LENGTH_MISMATCH;
                break;
            }

            ElementLength = ElementBytePointer[Offset];
            Offset += 1;
            if (Offset + ElementLength > Frame->BufferSize) {
                break;
            }

            switch (ElementId) {
            case NET80211_ELEMENT_SUPPORTED_RATES:
                RateCount = ElementLength;
                RateOffset = Offset;
                break;

            case NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES:
                ExtendedRateCount = ElementLength;
                ExtendedRateOffset = Offset;
                break;

            case NET80211_ELEMENT_EDCA:
                break;

            default:
                break;
            }

            Offset += ElementLength;
        }

        if (AcceptedResponse != FALSE) {

            ASSERT(KSUCCESS(Status));

            //
            // Save the supported rates into the BSS context so that the
            // maximum link speed can be determined.
            //

            TotalRateCount = RateCount + ExtendedRateCount;
            if (TotalRateCount != 0) {
                if ((Context->RateInformation != NULL) &&
                    (Context->RateInformation->Count < TotalRateCount)) {

                    MmFreePagedPool(Context->RateInformation);
                    Context->RateInformation = NULL;
                }

                if (Context->RateInformation == NULL) {
                    AllocationSize = sizeof(NET80211_LINK_RATE_INFORMATION) +
                                     (TotalRateCount * sizeof(UCHAR));

                    Allocation = MmAllocatePagedPool(AllocationSize,
                                                     NET80211_ALLOCATION_TAG);

                    if (Allocation == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    }

                    Context->RateInformation = Allocation;
                    Context->RateInformation->Rates =
                           Allocation + sizeof(NET80211_LINK_RATE_INFORMATION);
                }

                Context->RateInformation->Count = TotalRateCount;
                for (Index = 0; Index < RateCount; Index += 1) {
                    Context->RateInformation->Rates[Index] =
                                                ElementBytePointer[RateOffset];

                    RateOffset += 1;
                }

                for (Index = RateCount; Index < TotalRateCount; Index += 1) {
                    Context->RateInformation->Rates[Index] =
                                        ElementBytePointer[ExtendedRateOffset];

                    ExtendedRateOffset += 1;
                }
            }

            Context->Capabilities = Capabilities;
            Context->AssociationId = AssociationId;
            break;
        }
    }

    return Status;
}

KSTATUS
Net80211pSendManagementFrame (
    PNET_LINK Link,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNETWORK_ADDRESS Bssid,
    ULONG FrameSubtype,
    PVOID FrameBody,
    ULONG FrameBodySize
    )

/*++

Routine Description:

    This routine sends an 802.11 management frame with the given data and
    subtype out over the link.

Arguments:

    Link - Supplies a pointer to the link on which to send the management frame.

    SourceAddress - Supplies a pointer to the source address for the managment
        frame.

    DestinationAddress - Supplies a pointer to an optional destination address
        for the management frame. Supply NULL to indicate the broadcast address.

    Bssid - Supplies a pointer to an optional BSSID for the management frame.
        Supply NULL to indicate the wildcard BSSID.

    FrameSubtype - Supplies the management frame subtype for the packet.

    FrameBody - Supplies a pointer to the body data to be sent within the frame.

    FrameBodySize - Supplies the size of the body to send in the frame.

Return Value:

    Status code.

--*/

{

    PVOID DriverContext;
    ULONG Flags;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    ULONG Index;
    PNET_PACKET_BUFFER Packet;
    LIST_ENTRY PacketListHead;
    KSTATUS Status;

    //
    // Allocate a network packet to send down to the lower layers.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(sizeof(NET80211_MANAGEMENT_FRAME_HEADER),
                               FrameBodySize,
                               0,
                               Link,
                               Flags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SendManagementFrameEnd;
    }

    //
    // Copy the data to the newly allocated network packet.
    //

    RtlCopyMemory(Packet->Buffer + Packet->DataOffset,
                  FrameBody,
                  FrameBodySize);

    //
    // Move the offset backwards and fill in the 802.11 management frame header.
    //

    Packet->DataOffset -= sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
    Header = Packet->Buffer + Packet->DataOffset;
    Header->FrameControl = (NET80211_FRAME_CONTROL_PROTOCOL_VERSION <<
                            NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT) |
                           (NET80211_FRAME_TYPE_MANAGEMENT <<
                            NET80211_FRAME_CONTROL_TYPE_SHIFT) |
                           (FrameSubtype <<
                            NET80211_FRAME_CONTROL_SUBTYPE_SHIFT);

    //
    // The hardware handles the duration.
    //

    Header->Duration = 0;

    //
    // Initialize the header's addresses. If the destination or BSSID are NULL,
    // the the broadcast address is to be set.
    //

    if (DestinationAddress != NULL) {
        RtlCopyMemory(Header->DestinationAddress,
                      DestinationAddress->Address,
                      NET80211_ADDRESS_SIZE);

    } else {
        for (Index = 0; Index < NET80211_ADDRESS_SIZE; Index += 1) {
            Header->DestinationAddress[Index] = 0xFF;
        }
    }

    RtlCopyMemory(Header->SourceAddress,
                  SourceAddress->Address,
                  NET80211_ADDRESS_SIZE);

    if (Bssid != NULL) {
        RtlCopyMemory(Header->Bssid, Bssid->Address, NET80211_ADDRESS_SIZE);

    } else {
        for (Index = 0; Index < NET80211_ADDRESS_SIZE; Index += 1) {
            Header->Bssid[Index] = 0xFF;
        }
    }

    //
    // The header gets the next sequence number for the link. This is only 1
    // fragment, so that remains 0.
    //

    Header->SequenceControl = Net80211pGetSequenceNumber(Link);
    Header->SequenceControl <<= NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT;

    //
    // Send the packet off.
    //

    INITIALIZE_LIST_HEAD(&PacketListHead);
    INSERT_BEFORE(&(Packet->ListEntry), &PacketListHead);
    DriverContext = Link->Properties.DriverContext;
    Status = Link->Properties.Interface.Send(DriverContext, &PacketListHead);
    if (!KSUCCESS(Status)) {
        goto SendManagementFrameEnd;
    }

SendManagementFrameEnd:
    if (!KSUCCESS(Status)) {
        if (Packet != NULL) {
            NetFreeBuffer(Packet);
        }
    }

    return Status;
}

KSTATUS
Net80211pReceiveManagementFrame (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    ULONG FrameSubtype,
    PNET80211_MANAGEMENT_FRAME *Frame
    )

/*++

Routine Description:

    This routine receives one management frame for the given link and returns
    it.

Arguments:

    Link - Supplies a pointer to the network link on which to receive a
        management frame.

    LinkAddress - Supplies a pointer to the link address that is expected to
        receive the frame.

    FrameSubtype - Supplies the expected subtype of the management frame.

    Frame - Supplies a pointer that receives a pointer to the received
        management frame.

Return Value:

    Status code.

--*/

{

    PNET80211_MANAGEMENT_FRAME FirstFrame;
    PLIST_ENTRY FirstFrameEntry;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    BOOL Match;
    PNET80211_LINK Net80211Link;
    ULONG ReceivedSubtype;
    KSTATUS Status;

    FirstFrame = NULL;
    Net80211Link = Link->DataLinkContext;

    //
    // Wait for the event to signal that a management frame arrived.
    //

    Status = KeWaitForEvent(Net80211Link->ManagementFrameEvent,
                            FALSE,
                            NET80211_MANAGEMENT_FRAME_TIMEOUT);

    if (Status == STATUS_TIMEOUT) {
        goto ReceiveManagementFrameEnd;
    }

    //
    // There should be at least one management frame on the list. Pick it off
    // and return it.
    //

    KeAcquireQueuedLock(Net80211Link->Lock);

    ASSERT(LIST_EMPTY(&(Net80211Link->ManagementFrameList)) == FALSE);

    FirstFrameEntry = Net80211Link->ManagementFrameList.Next;
    LIST_REMOVE(FirstFrameEntry);

    //
    // If the list is now empty, unsignal the event so the next request to
    // receive a frame waits first.
    //

    if (LIST_EMPTY(&(Net80211Link->ManagementFrameList)) != FALSE) {
        KeSignalEvent(Net80211Link->ManagementFrameEvent, SignalOptionUnsignal);
    }

    KeReleaseQueuedLock(Net80211Link->Lock);
    FirstFrame = LIST_VALUE(FirstFrameEntry,
                            NET80211_MANAGEMENT_FRAME,
                            ListEntry);

    //
    // Perform some common validation on the frame.
    //

    if (FirstFrame->BufferSize < sizeof(NET80211_MANAGEMENT_FRAME_HEADER)) {
        RtlDebugPrint("802.11: Skipping management frame as it was too "
                      "small to contain header. Frame was size %d, "
                      "expected at least %d bytes.\n",
                      FirstFrame->BufferSize,
                      sizeof(NET80211_MANAGEMENT_FRAME_HEADER));

        Status = STATUS_BUFFER_TOO_SMALL;
        goto ReceiveManagementFrameEnd;
    }

    //
    // Make sure it is the write management subtype.
    //

    Header = FirstFrame->Buffer;

    ASSERT(NET80211_GET_FRAME_TYPE(Header) == NET80211_FRAME_TYPE_MANAGEMENT);

    ReceivedSubtype = NET80211_GET_FRAME_SUBTYPE(Header);
    if (ReceivedSubtype != FrameSubtype) {
        RtlDebugPrint("802.11: Skipping management frame as it wasn't of "
                      "type %d, but it was of type %d.\n",
                      FrameSubtype,
                      ReceivedSubtype);

        Status = STATUS_UNEXPECTED_TYPE;
        goto ReceiveManagementFrameEnd;
    }

    //
    // Make sure the destination address matches.
    //

    Match = RtlCompareMemory(Header->DestinationAddress,
                             LinkAddress->PhysicalAddress.Address,
                             NET80211_ADDRESS_SIZE);

    if (Match == FALSE) {
        RtlDebugPrint("802.11: Skipping management frame with wrong "
                      "destination address.\n");

        Status = STATUS_INVALID_ADDRESS;
        goto ReceiveManagementFrameEnd;
    }

ReceiveManagementFrameEnd:
    if (!KSUCCESS(Status)) {
        if (FirstFrame != NULL) {
            MmFreePagedPool(FirstFrame);
            FirstFrame = NULL;
        }
    }

    *Frame = FirstFrame;
    return Status;
}

PNET80211_BSS_CONTEXT
Net80211pCreateBssContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PSTR Ssid,
    ULONG SsidLength
    )

/*++

Routine Description:

    This routine creates a BSS context to be used to join the BSS indicated by
    the given SSID.

Arguments:

    Link - Supplies a pointer to the link that is requesting to join a network.

    LinkAddress - Supplies the link address for the link that wants to join the
         network.

    Ssid - Supplies the SSID of the network to join.

    SsidLength - Supplies the length of the SSID string, including the NULL
        terminator.

Return Value:

    Returns a pointer to the created BSS context on success or NULL on failure.

--*/

{

    ULONG AllocationSize;
    PNET80211_BSS_CONTEXT Context;

    AllocationSize = sizeof(NET80211_BSS_CONTEXT) + SsidLength;
    Context = MmAllocatePagedPool(AllocationSize, NET80211_ALLOCATION_TAG);
    if (Context == NULL) {
        goto CreateBssContextEnd;
    }

    RtlZeroMemory(Context, sizeof(NET80211_BSS_CONTEXT));
    NetLinkAddReference(Link);
    Context->Link = Link;
    Context->LinkAddress = LinkAddress;
    if (Ssid != NULL) {
        Context->Ssid = (PSTR)(Context + 1);
        Context->SsidLength = SsidLength;
        RtlCopyMemory(Context->Ssid, Ssid, SsidLength);
    }

CreateBssContextEnd:
    return Context;
}

VOID
Net80211pDestroyBssContext (
    PNET80211_BSS_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given BSS context.

Arguments:

    Context - Supplies a pointer to a BSS context.

Return Value:

    None.

--*/

{

    if (Context->RateInformation != NULL) {
        MmFreePagedPool(Context->RateInformation);
    }

    NetLinkReleaseReference(Context->Link);
    MmFreePagedPool(Context);
    return;
}

