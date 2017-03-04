/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define default values for the local station's RSN capabilities.
//

#define NET80211_DEFAULT_RSN_ELEMENT_LENGTH \
    (sizeof(NET80211_DEFAULT_RSN_INFORMATION) - (2 * sizeof(UCHAR)))

#define NET80211_DEFAULT_RSN_CAPABILITIES 0
#define NET80211_DEFAULT_RSN_PAIRWISE_CIPHER_SUITE_COUNT 1
#define NET80211_DEFAULT_RSN_AKM_SUITE_COUNT 1

//
// Define the default RSN group cipher suite. This is
// NET80211_CIPHER_SUITE_CCMP in network byte order.
//

#define NET80211_DEFAULT_RSN_GROUP_CIPHER_SUITE 0x04AC0F00

//
// Define the default RSN pairwise cipher suite. This is
// NET80211_CIPHER_SUITE_CCMP in network byte order.
//

#define NET80211_DEFAULT_RSN_PAIRWISE_CIPHER_SUITE 0x04AC0F00

//
// Define the default RSN AKM cipher suite. This is NET80211_AKM_SUITE_PSK in
// network byte order.
//

#define NET80211_DEFAULT_RSN_AKM_SUITE 0x02AC0F00

//
// Define the time to wait for a state management frame.
//

#define NET80211_STATE_TIMEOUT (2 * MICROSECONDS_PER_SECOND)

//
// Define the time to wait for advanced authentication.
//

#define NET80211_AUTHENTICATION_TIMEOUT (5 * MICROSECONDS_PER_SECOND)

//
// Define the timeout until a BSS entry has expired in microseconds.
//

#define NET80211_BSS_ENTRY_TIMEOUT (10 * MICROSECONDS_PER_SECOND)

//
// Define the pad to subtract from the beacon interval during a background
// scan in order to determine the amount of time to dwell on a channel without
// missing a beacon from the active BSS.
//

#define NET80211_BEACON_INTERVAL_PAD (10 * MICROSECONDS_PER_MILLISECOND)

//
// Define the default amount of time to wait between scanning channels when
// performing a background scan.
//

#define NET80211_BACKGROUND_SCAN_CHANNEL_DELAY \
    (200 * MICROSECONDS_PER_MILLISECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the set of information gathered from a probe
    response management frame or a beacon management frame.

Members:

    Bssid - Stores a pointer to the BSSID, which is always
        NET80211_ADDRESS_SIZE bytes long.

    BeaconInterval - Stores the interval between beacons sent by an AP.

    Capabilities - Stores the 802.11 capabilities of the AP. See
        NET80211_CAPABILITY_FLAG_* for definitions.

    Timestamp - Stores the timestamp from the AP.

    Elements - Stores a pointer to the information elements.

    ElementsSize - Stores the size of the information elements, in bytes.

    Channel - Stores a pointer to the channel element, that indicates the
        channel on which the AP is operating.

    Ssid - Stores a pointer to the SSID element from the AP.

    Rates - Stores a pointer to the supported rates element.

    ExtendedRates - Stores a pointer to the extended supported rates element.

    Rsn - Stores the optional RSN element broadcasted by the AP.

--*/

typedef struct _NET80211_PROBE_RESPONSE {
    PUCHAR Bssid;
    USHORT BeaconInterval;
    USHORT Capabilities;
    ULONGLONG Timestamp;
    PVOID Elements;
    ULONG ElementsSize;
    PVOID Channel;
    PVOID Ssid;
    PVOID Rates;
    PVOID ExtendedRates;
    PVOID Rsn;
} NET80211_PROBE_RESPONSE, *PNET80211_PROBE_RESPONSE;

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

/*++

Structure Description:

    This structure defines the default RSN information used by the 802.11
    networking library.

Members:

    ElementId - Stores the RSN element ID. This should be NET80211_ELEMENT_RSN.

    ElementLength - Stores the length of the RSN information, not including the
        first two bytes.

    RsnVersion - Stores the RSN information version.

    GroupCipherSuite - Stores the group cipher suite.

    PairwiseCipherSuiteCount - Stores the number of pairwise cipher suites that
        follow this field. There should only be 1.

    PairwiseCipherSuite - Stores the only supported pairwise cipher suite.

    AkmSuiteCount - Stores the number of AKM cipher suites that follow this
        field. There should be only 1.

    AkmSuite - Stores the only supported AKM cipher suite.

    RsnCapabilites - Stores the RSN capapbilites for the node.

--*/

typedef struct _NET80211_DEFAULT_RSN_INFORMATION {
    UCHAR ElementId;
    UCHAR ElementLength;
    USHORT RsnVersion;
    ULONG GroupCipherSuite;
    USHORT PairwiseCipherSuiteCount;
    ULONG PairwiseCipherSuite;
    USHORT AkmSuiteCount;
    ULONG AkmSuite;
    USHORT RsnCapabilities;
} PACKED NET80211_DEFAULT_RSN_INFORMATION, *PNET80211_DEFAULT_RSN_INFORMATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
Net80211pSetStateUnlocked (
    PNET80211_LINK Link,
    NET80211_STATE State
    );

VOID
Net80211pScanThread (
    PVOID Parameter
    );

VOID
Net80211pStartProbing (
    PNET80211_LINK Link
    );

VOID
Net80211pStopProbing (
    PNET80211_LINK Link,
    BOOL LockHeld
    );

KSTATUS
Net80211pPrepareForReconnect (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY *Bss
    );

VOID
Net80211pJoinBss (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    );

VOID
Net80211pLeaveBss (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss,
    BOOL SendNotification,
    ULONG Subtype,
    USHORT Reason
    );

KSTATUS
Net80211pSendProbeRequest (
    PNET80211_LINK Link,
    PNET80211_SCAN_STATE Scan
    );

VOID
Net80211pProcessProbeResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendAuthenticationRequest (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    );

VOID
Net80211pProcessAuthenticationResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendAssociationRequest (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    );

VOID
Net80211pProcessAssociationResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendManagementFrame (
    PNET80211_LINK Link,
    PUCHAR DestinationAddress,
    PUCHAR Bssid,
    ULONG FrameSubtype,
    PVOID FrameBody,
    ULONG FrameBodySize
    );

KSTATUS
Net80211pValidateRates (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    );

KSTATUS
Net80211pParseRsnElement (
    PUCHAR Rsn,
    PNET80211_ENCRYPTION Encryption
    );

VOID
Net80211pUpdateBssCache (
    PNET80211_LINK Link,
    PNET80211_PROBE_RESPONSE Response
    );

VOID
Net80211pTrimBssCache (
    PNET80211_LINK Link
    );

PNET80211_BSS_ENTRY
Net80211pCopyBssEntry (
    PNET80211_BSS_ENTRY Bss
    );

PNET80211_BSS_ENTRY
Net80211pCreateBssEntry (
    PUCHAR Bssid
    );

VOID
Net80211pDestroyBssEntry (
    PNET80211_BSS_ENTRY BssEntry
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the default RSN information to send out for association requests.
//

NET80211_DEFAULT_RSN_INFORMATION Net80211DefaultRsnInformation = {
    NET80211_ELEMENT_RSN,
    NET80211_DEFAULT_RSN_ELEMENT_LENGTH,
    NET80211_RSN_VERSION,
    NET80211_DEFAULT_RSN_GROUP_CIPHER_SUITE,
    NET80211_DEFAULT_RSN_PAIRWISE_CIPHER_SUITE_COUNT,
    NET80211_DEFAULT_RSN_PAIRWISE_CIPHER_SUITE,
    NET80211_DEFAULT_RSN_AKM_SUITE_COUNT,
    NET80211_DEFAULT_RSN_AKM_SUITE,
    NET80211_DEFAULT_RSN_CAPABILITIES
};

//
// ------------------------------------------------------------------ Functions
//

VOID
Net80211pProcessManagementFrame (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes 802.11 management frames.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

{

    ULONG FrameSubtype;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;

    Header = Packet->Buffer + Packet->DataOffset;
    FrameSubtype = NET80211_GET_FRAME_SUBTYPE(Header);
    switch (FrameSubtype) {
    case NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_RESPONSE:
        Net80211pProcessProbeResponse(Link, Packet);
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION:
        Net80211pProcessAuthenticationResponse(Link, Packet);
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_RESPONSE:
        Net80211pProcessAssociationResponse(Link, Packet);
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_DISASSOCIATION:
        if ((Link->State != Net80211StateAssociated) &&
            (Link->State != Net80211StateEncrypted)) {

            break;
        }

        Net80211pSetState(Link, Net80211StateAssociating);
        break;

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_DEAUTHENTICATION:
        if ((Link->State != Net80211StateAssociating) &&
            (Link->State != Net80211StateReassociating) &&
            (Link->State != Net80211StateAssociated) &&
            (Link->State != Net80211StateEncrypted)) {

            break;
        }

        Net80211pSetState(Link, Net80211StateAuthenticating);
        break;

    //
    // Ignore packets that are not yet handled.
    //

    case NET80211_MANAGEMENT_FRAME_SUBTYPE_REASSOCIATION_RESPONSE:
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

    return;
}

KSTATUS
Net80211pStartScan (
    PNET80211_LINK Link,
    PNET80211_SCAN_STATE Parameters
    )

/*++

Routine Description:

    This routine starts a scan for one or more BSSs within range of this
    station.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to perform the scan.

    Parameters - Supplies a pointer to a scan state used to initialize the
        scan. This memory will not be referenced after the function returns,
        so this may be a stack allocated structure.

Return Value:

    Status code.

--*/

{

    PNET80211_SCAN_STATE ScanState;
    KSTATUS Status;

    ASSERT(Parameters->SsidLength <= NET80211_MAX_SSID_LENGTH);
    ASSERT(Parameters->PassphraseLength <= NET80211_MAX_PASSPHRASE_LENGTH);

    ScanState = MmAllocatePagedPool(sizeof(NET80211_SCAN_STATE),
                                    NET80211_ALLOCATION_TAG);

    if (ScanState == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto StartScanEnd;
    }

    RtlCopyMemory(ScanState, Parameters, sizeof(NET80211_SCAN_STATE));
    Net80211LinkAddReference(Link);
    ScanState->Link = Link;

    //
    // Kick off a thread to complete the scan.
    //

    Status = PsCreateKernelThread(Net80211pScanThread,
                                  ScanState,
                                  "Net80211ScanThread");

    if (!KSUCCESS(Status)) {
        goto StartScanEnd;
    }

StartScanEnd:
    if (!KSUCCESS(Status)) {
        if (ScanState != NULL) {
            Net80211pSetState(Link, Net80211StateInitialized);
            Net80211LinkReleaseReference(ScanState->Link);
            MmFreePagedPool(ScanState);
        }
    }

    return Status;
}

VOID
Net80211pSetState (
    PNET80211_LINK Link,
    NET80211_STATE State
    )

/*++

Routine Description:

    This routine sets the given link's 802.11 state by alerting the driver of
    the state change and then performing any necessary actions based on the
    state transition.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose state is being updated.

    State - Supplies the state to which the link is transitioning.

Return Value:

    None.

--*/

{

    KeAcquireQueuedLock(Link->Lock);
    Net80211pSetStateUnlocked(Link, State);
    KeReleaseQueuedLock(Link->Lock);
    return;
}

PNET80211_BSS_ENTRY
Net80211pGetBss (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine gets the link's active BSS entry and hands back a pointer with
    a reference to the caller.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose active BSS is to be
        returned.

Return Value:

    Returns a pointer to the active BSS.

--*/

{

    PNET80211_BSS_ENTRY Bss;

    Bss = NULL;
    if (Link->ActiveBss != NULL) {
        KeAcquireQueuedLock(Link->Lock);
        Bss = Link->ActiveBss;
        if (Bss != NULL) {
            Net80211pBssEntryAddReference(Bss);
        }

        KeReleaseQueuedLock(Link->Lock);
    }

    return Bss;
}

VOID
Net80211pBssEntryAddReference (
    PNET80211_BSS_ENTRY BssEntry
    )

/*++

Routine Description:

    This routine increments the reference count of the given BSS entry.

Arguments:

    BssEntry - Supplies a pointer to the BSS entry whose reference count is to
        be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(BssEntry->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
Net80211pBssEntryReleaseReference (
    PNET80211_BSS_ENTRY BssEntry
    )

/*++

Routine Description:

    This routine decrements the reference count of the given BSS entry,
    destroying the entry if there are no more references.

Arguments:

    BssEntry - Supplies a pointer to the BSS entry whose reference count is to
        be decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(BssEntry->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        Net80211pDestroyBssEntry(BssEntry);
    }

    return;
}

KSTATUS
Net80211pQueueStateTransitionTimer (
    PNET80211_LINK Link,
    ULONGLONG Timeout
    )

/*++

Routine Description:

    This routine queues the given network link's state transition timer.

Arguments:

    Link - Supplies a pointer to a 802.11 link.

    Timeout - Supplies the desired timeout in microseconds.

Return Value:

    Status code.

--*/

{

    ULONGLONG DueTime;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    DueTime = KeGetRecentTimeCounter();
    DueTime += KeConvertMicrosecondsToTimeTicks(Timeout);
    Status = KeQueueTimer(Link->StateTimer,
                          TimerQueueSoft,
                          DueTime,
                          0,
                          0,
                          Link->TimeoutDpc);

    if (KSUCCESS(Status)) {
        Link->Flags |= NET80211_LINK_FLAG_TIMER_QUEUED;
    }

    return Status;
}

VOID
Net80211pCancelStateTransitionTimer (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine cancels the given link's state transition timer if it is
    queued.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose state transition timer
        shall be canceled.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    //
    // Cancel the timer if it is queued. Also make sure the DPC is flushed if
    // the timer just expired. The timer may be requeued at any time and a DPC
    // cannot be queued twice.
    //

    if ((Link->Flags & NET80211_LINK_FLAG_TIMER_QUEUED) != 0) {
        Status = KeCancelTimer(Link->StateTimer);
        if (!KSUCCESS(Status)) {
            KeFlushDpc(Link->TimeoutDpc);
        }

        Link->Flags &= ~NET80211_LINK_FLAG_TIMER_QUEUED;
    }

    return;
}

VOID
Net80211pStateTimeoutDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the 802.11 state transition timeout DPC that gets
    called after a remote node does not respond to a management frame.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PNET80211_LINK Net80211Link;

    Net80211Link = (PNET80211_LINK)Dpc->UserData;
    KeQueueWorkItem(Net80211Link->TimeoutWorkItem);
    return;
}

VOID
Net80211pStateTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when an 802.11 state transition
    times out due to a remote node not responding.

Arguments:

    Parameter - Supplies a pointer to the nework link whose 802.11 state
        transition has timed out.

Return Value:

    None.

--*/

{

    PNET80211_LINK Link;

    Link = (PNET80211_LINK)Parameter;

    //
    // If a packet did not arrive to advance the state and cancel the timer,
    // then this really is a timeout. Set the state back to initialized.
    //

    KeAcquireQueuedLock(Link->Lock);
    if ((Link->Flags & NET80211_LINK_FLAG_TIMER_QUEUED) != 0) {
        Link->Flags &= ~NET80211_LINK_FLAG_TIMER_QUEUED;
        Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
    }

    KeReleaseQueuedLock(Link->Lock);
    return;
}

PNET80211_BSS_ENTRY
Net80211pLookupBssEntry (
    PNET80211_LINK Link,
    PUCHAR Bssid
    )

/*++

Routine Description:

    This routine searches the link for a known BSS entry with the given BSSID.
    It does not take a reference on the BSS entry and assumes that the link's
    lock is already held.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to search.

    Bssid - Supplies a pointer to the BSSID for the desired BSS entry.

Return Value:

    Returns a pointer to the matching BSS entry on success, or NULL on failure.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PLIST_ENTRY CurrentEntry;
    BOOL Match;
    PNET80211_BSS_ENTRY MatchedBss;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    MatchedBss = NULL;
    CurrentEntry = Link->BssList.Next;
    while (CurrentEntry != &(Link->BssList)) {
        Bss = LIST_VALUE(CurrentEntry, NET80211_BSS_ENTRY, ListEntry);
        Match = RtlCompareMemory(Bssid,
                                 Bss->State.Bssid,
                                 NET80211_ADDRESS_SIZE);

        if (Match != FALSE) {
            MatchedBss = Bss;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return MatchedBss;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Net80211pSetStateUnlocked (
    PNET80211_LINK Link,
    NET80211_STATE State
    )

/*++

Routine Description:

    This routine sets the given link's 802.11 state by alerting the driver of
    the state change and then performing any necessary actions based on the
    state transition. This routine assumes that the 802.11 link's lock is held.

Arguments:

    Link - Supplies a pointer to the link whose state is being updated.

    State - Supplies the state to which the link is transitioning.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PNET80211_BSS BssState;
    PVOID DeviceContext;
    ULONGLONG LinkSpeed;
    BOOL Notify;
    NET80211_STATE OldState;
    USHORT Reason;
    BOOL SetLinkUp;
    KSTATUS Status;
    ULONG Subtype;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    Bss = Link->ActiveBss;
    OldState = Link->State;

    //
    // State transitions are not allowed from the probing state. Save the
    // transition so it can be replayed later after the link moves out of the
    // probing state.
    //

    if (OldState == Net80211StateProbing) {
        Link->ProbeNextState = State;
        goto SetStateUnlockedEnd;
    }

    //
    // Notify the driver about the state transition first, allowing it to
    // prepare for the type of packets to be sent and received in the new state.
    //

    BssState = NULL;
    if (Bss != NULL) {
        BssState = &(Bss->State);
    }

    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.SetState(DeviceContext,
                                                 State,
                                                 BssState);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Failed to set state %d: %d\n", State, Status);
        goto SetStateUnlockedEnd;
    }

    //
    // Officially update the state.
    //

    Link->State = State;

    //
    // Make sure the state transition timer is canceled.
    //

    Net80211pCancelStateTransitionTimer(Link);

    //
    // Perform the necessary steps according to the state transition.
    //

    SetLinkUp = FALSE;
    switch (State) {
    case Net80211StateAuthenticating:
        switch (OldState) {
        case Net80211StateAssociated:
        case Net80211StateEncrypted:
            Status = Net80211pPrepareForReconnect(Link, &Bss);
            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            //
            // Fall through to send the authentication request.
            //

        case Net80211StateAssociating:
        case Net80211StateReassociating:
        case Net80211StateInitialized:
            Status = Net80211pSendAuthenticationRequest(Link, Bss);
            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            Status = Net80211pQueueStateTransitionTimer(Link,
                                                        NET80211_STATE_TIMEOUT);

            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            break;

        default:
            break;
        }

        break;

    case Net80211StateAssociating:
        switch (OldState) {
        case Net80211StateAssociated:
        case Net80211StateEncrypted:
            Status = Net80211pPrepareForReconnect(Link, &Bss);
            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            //
            // Fall through to send the association request.
            //

        case Net80211StateAuthenticating:

            //
            // Send out an association request and set the timeout.
            //

            Status = Net80211pSendAssociationRequest(Link, Bss);
            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            Status = Net80211pQueueStateTransitionTimer(Link,
                                                        NET80211_STATE_TIMEOUT);

            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            break;

        default:
            break;
        }

        break;

    //
    // In the associated state, if no advanced encryption is involved, the link
    // is ready to start transmitting and receiving data.
    //

    case Net80211StateAssociated:

        ASSERT(Bss != NULL);

        if ((Bss->Encryption.Pairwise == NetworkEncryptionNone) ||
            (Bss->Encryption.Pairwise == NetworkEncryptionWep)) {

            SetLinkUp = TRUE;

        } else {

            //
            // Initialize the encryption authentication process so that it is
            // ready to receive key exchange packets.
            //

            Status = Net80211pInitializeEncryption(Link, Bss);
            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }

            Status = Net80211pQueueStateTransitionTimer(
                                              Link,
                                              NET80211_AUTHENTICATION_TIMEOUT);

            if (!KSUCCESS(Status)) {
                goto SetStateUnlockedEnd;
            }
        }

        break;

    //
    // If advanced encryption was involved, then the link is not ready until
    // the encrypted state is reached.
    //

    case Net80211StateEncrypted:

        ASSERT((Bss->Encryption.Pairwise == NetworkEncryptionWpaPsk) ||
               (Bss->Encryption.Pairwise == NetworkEncryptionWpa2Psk));

        Net80211pDestroyEncryption(Bss);
        SetLinkUp = TRUE;
        break;

    case Net80211StateInitialized:
    case Net80211StateUninitialized:
        switch (OldState) {
        case Net80211StateAssociated:
        case Net80211StateEncrypted:
            Notify = TRUE;
            Subtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_DISASSOCIATION;
            Reason = NET80211_REASON_CODE_DISASSOCIATION_LEAVING;
            break;

        case Net80211StateAssociating:
            Notify = TRUE;
            Subtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_DEAUTHENTICATION;
            Reason = NET80211_REASON_CODE_DEAUTHENTICATION_LEAVING;
            break;

        default:
            Notify = FALSE;
            Subtype = 0;
            Reason = 0;
            break;
        }

        if (Bss != NULL) {
            Net80211pDestroyEncryption(Bss);
            Net80211pLeaveBss(Link, Bss, Notify, Subtype, Reason);
            NetSetLinkState(Link->NetworkLink, FALSE, 0);
        }

        break;

    default:
        break;
    }

    //
    // If requested, fire up the link and get traffic going in the upper layers.
    //

    if (SetLinkUp != FALSE) {
        Net80211pResumeDataFrames(Link);
        LinkSpeed = Bss->State.MaxRate * NET80211_RATE_UNIT;
        NetSetLinkState(Link->NetworkLink, TRUE, LinkSpeed);
    }

SetStateUnlockedEnd:
    return;
}

VOID
Net80211pScanThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the scan thread.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this can it is a pointer to an 802.11 scan state structure.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY ActiveBss;
    PNET80211_BSS_ENTRY BssEntry;
    PLIST_ENTRY CurrentEntry;
    PNET80211_BSS_ENTRY FoundEntry;
    PNET80211_LINK Link;
    BOOL LockHeld;
    BOOL Match;
    LONG MaxRssi;
    PNET80211_SCAN_STATE Scan;
    ULONGLONG ScanDelay;
    ULONG SsidLength;
    KSTATUS Status;

    LockHeld = FALSE;
    Scan = (PNET80211_SCAN_STATE)Parameter;
    Link = Scan->Link;

    //
    // Acquire the link's scan lock to prevent multiple scans from happening
    // simultaneously. This protects the hardware from being set to different
    // channels and protects against a network being joined during a scan.
    //

    KeAcquireQueuedLock(Link->ScanLock);

    //
    // Before pulling in new BSS entries, clean out the old ones.
    //

    Net80211pTrimBssCache(Link);

    //
    // If there is an active BSS, then this is a background scan.
    //

    ActiveBss = Net80211pGetBss(Link);
    if (ActiveBss != NULL) {
        Scan->Flags |= NET80211_SCAN_FLAG_BACKGROUND;
    }

    //
    // If this is a foreground scan, just set the state to probing and start
    // running through the channels.
    //

    if ((Scan->Flags & NET80211_SCAN_FLAG_BACKGROUND) == 0) {
        Net80211pStartProbing(Link);
        ScanDelay = NET80211_DEFAULT_SCAN_DWELL_TIME;

    } else {
        ScanDelay = ActiveBss->State.BeaconInterval * NET80211_TIME_UNIT;
        if (ScanDelay > NET80211_BEACON_INTERVAL_PAD) {
            ScanDelay -= NET80211_BEACON_INTERVAL_PAD;
        }
    }

    //
    // Always start scanning on channel 1.
    //

    Scan->Channel = 1;

    //
    // Search for BSS entries on all channels.
    //

    FoundEntry = NULL;
    while (Scan->Channel < Link->Properties.MaxChannel) {

        //
        // If this a background scan, temporarily set the state to probing to
        // alert that hardware that it's in scan mode.
        //

        if ((Scan->Flags & NET80211_SCAN_FLAG_BACKGROUND) != 0) {
            Net80211pStartProbing(Link);
        }

        //
        // Set the channel to send the packet over.
        //

        Status = Net80211pSetChannel(Link, Scan->Channel);
        if (!KSUCCESS(Status)) {
            goto ScanThreadEnd;
        }

        //
        // Send a probe request over the link, this will look in the
        // current scan state and set the correct channel and BSSID
        // (broadcast or a specific ID).
        //

        Status = Net80211pSendProbeRequest(Link, Scan);
        if (!KSUCCESS(Status)) {
            goto ScanThreadEnd;
        }

        //
        // Give the responses a chance before moving to the next channel.
        //

        KeDelayExecution(FALSE, FALSE, ScanDelay);

        //
        // If this a background scan, set the state back to what it was and
        // continue sending packets for a period.
        //

        if ((Scan->Flags & NET80211_SCAN_FLAG_BACKGROUND) != 0) {
            if (ActiveBss != NULL) {
                Status = Net80211pSetChannel(Link, ActiveBss->State.Channel);
                if (!KSUCCESS(Status)) {
                    goto ScanThreadEnd;
                }
            }

            Net80211pStopProbing(Link, LockHeld);
        }

        //
        // Now that the channel has been probed, search to see if the
        // targeted BSS is in range. This should only be done if a specific
        // BSSID is being probed.
        //

        if (((Scan->Flags & NET80211_SCAN_FLAG_BROADCAST) == 0) &&
            ((Scan->Flags & NET80211_SCAN_FLAG_JOIN) != 0)) {

            KeAcquireQueuedLock(Link->Lock);
            LockHeld = TRUE;
            FoundEntry = Net80211pLookupBssEntry(Link, Scan->Bssid);
            if (FoundEntry != NULL) {
                Status = Net80211pValidateRates(Link, FoundEntry);
                if (!KSUCCESS(Status)) {
                   goto ScanThreadEnd;
                }

                break;
            }

            KeReleaseQueuedLock(Link->Lock);
            LockHeld = FALSE;
        }

        Scan->Channel += 1;

        //
        // When performing background scans, wait a bit before moving to the
        // next channel to allow normal traffic to progress.
        //

        if ((Scan->Flags & NET80211_SCAN_FLAG_BACKGROUND) != 0) {
            KeDelayExecution(FALSE,
                             FALSE,
                             NET80211_BACKGROUND_SCAN_CHANNEL_DELAY);
        }
    }

    //
    // Stop probing if this is not a background scan.
    //

    if ((Scan->Flags & NET80211_SCAN_FLAG_BACKGROUND) == 0) {
        Net80211pStopProbing(Link, LockHeld);
    }

    //
    // If the scan completed and a join is required, then search for the
    // BSS with the most signal strength.
    //

    if (((Scan->Flags & NET80211_SCAN_FLAG_BROADCAST) != 0) &&
        ((Scan->Flags & NET80211_SCAN_FLAG_JOIN) != 0)) {

        ASSERT(Scan->SsidLength != 0);
        ASSERT(FoundEntry == NULL);

        MaxRssi = MIN_LONG;
        KeAcquireQueuedLock(Link->Lock);
        LockHeld = TRUE;
        CurrentEntry = Link->BssList.Next;
        while (CurrentEntry != &(Link->BssList)) {
            BssEntry = LIST_VALUE(CurrentEntry,
                                  NET80211_BSS_ENTRY,
                                  ListEntry);

            CurrentEntry = CurrentEntry->Next;
            SsidLength = NET80211_GET_ELEMENT_LENGTH(BssEntry->Ssid);
            if (SsidLength != Scan->SsidLength) {
                continue;
            }

            Match = RtlCompareMemory(NET80211_GET_ELEMENT_DATA(BssEntry->Ssid),
                                     Scan->Ssid,
                                     Scan->SsidLength);

            if (Match == FALSE) {
                continue;
            }

            //
            // Validate that the BSS and station agree on a basic rate set.
            // Also determine the mode at which it would connect.
            //

            Status = Net80211pValidateRates(Link, BssEntry);
            if (!KSUCCESS(Status)) {
                continue;
            }

            if (BssEntry->State.Rssi >= MaxRssi) {
                MaxRssi = BssEntry->State.Rssi;
                FoundEntry = BssEntry;
            }
        }

        if (FoundEntry == NULL) {
            KeReleaseQueuedLock(Link->Lock);
            LockHeld = FALSE;
        }
    }

    //
    // If an entry was found, join that BSS and start the authentication
    // process.
    //

    if (FoundEntry != NULL) {

        ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

        if (FoundEntry->Encryption.Pairwise != NetworkEncryptionNone) {
            if (Scan->PassphraseLength == 0) {
                Status = STATUS_ACCESS_DENIED;
                goto ScanThreadEnd;
            }

            if (FoundEntry->Encryption.Pairwise != NetworkEncryptionWpa2Psk) {
                Status = STATUS_NOT_SUPPORTED;
                goto ScanThreadEnd;
            }

            RtlCopyMemory(FoundEntry->Passphrase,
                          Scan->Passphrase,
                          Scan->PassphraseLength);

            FoundEntry->PassphraseLength = Scan->PassphraseLength;
        }

        //
        // Leave the active BSS by setting the state back to initialized.
        // Protect against leaving and joining an already active BSS, but still
        // reauthenticate with the active BSS as the scan was issued for some
        // reason (e.g. maybe the connection is half-baked and the user isn't
        // seeing an IP address).
        //

        if (Link->ActiveBss != FoundEntry) {
            Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
            Net80211pJoinBss(Link, FoundEntry);
        }

        Net80211pSetChannel(Link, FoundEntry->State.Channel);
        Net80211pSetStateUnlocked(Link, Net80211StateAuthenticating);
        Status = STATUS_SUCCESS;

    } else if ((Scan->Flags & NET80211_SCAN_FLAG_JOIN) != 0) {
        Status = STATUS_UNSUCCESSFUL;

    } else {
        Status = STATUS_SUCCESS;
    }

ScanThreadEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->Lock);
    }

    KeReleaseQueuedLock(Link->ScanLock);
    if (!KSUCCESS(Status)) {
        Net80211pSetState(Link, Net80211StateInitialized);
    }

    if (Scan->CompletionRoutine != NULL) {
        Scan->CompletionRoutine(Link, Status);
    }

    if (ActiveBss != NULL) {
        Net80211pBssEntryReleaseReference(ActiveBss);
    }

    Net80211LinkReleaseReference(Link);
    MmFreePagedPool(Scan);
    return;
}

VOID
Net80211pStartProbing (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine prepares the given link for a network probe by pausing data
    frames and saving the current state.

Arguments:

    Link - Supplies a pointer to the 802.11 link that is about to start probing
        for networks.

Return Value:

    None.

--*/

{

    PNET80211_BSS BssState;
    PVOID DeviceContext;
    KSTATUS Status;

    KeAcquireQueuedLock(Link->Lock);

    ASSERT(Link->State != Net80211StateProbing);
    ASSERT(Link->ProbePreviousState == Net80211StateInvalid);
    ASSERT(Link->ProbeNextState == Net80211StateInvalid);

    //
    // Set the next state to invalid.
    //

    Link->ProbeNextState = Net80211StateInvalid;

    //
    // When entering the probe state, immediately pause data packet
    // transmission. This must be done before the hardware is notified of the
    // switch.
    //

    Net80211pPauseDataFrames(Link);

    //
    // Notify the hardware about the transition to probing.
    //

    BssState = NULL;
    if (Link->ActiveBss != NULL) {
        BssState = &(Link->ActiveBss->State);
    }

    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.SetState(DeviceContext,
                                                 Net80211StateProbing,
                                                 BssState);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Failed to set state %d: %d\n",
                      Net80211StateProbing,
                      Status);

        goto StartProbingEnd;
    }

    //
    // Save the current state and transition to the probing state.
    //

    Link->ProbePreviousState = Link->State;
    Link->State = Net80211StateProbing;

StartProbingEnd:
    if (!KSUCCESS(Status)) {
        Net80211pResumeDataFrames(Link);
    }

    KeReleaseQueuedLock(Link->Lock);
    return;
}

VOID
Net80211pStopProbing (
    PNET80211_LINK Link,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine takes the given link for out of the probing state, restoring
    the previous state. Keep in mind that an attempt to transition the state
    may have occurred while the link was probing. This routine will replay that
    transition after reverting the previous state.

Arguments:

    Link - Supplies a pointer to the 802.11 link that is exiting the probing
        state.

    LockHeld - Supplies a boolean indicating whether or not the link's lock is
        already held.

Return Value:

    None.

--*/

{

    PNET80211_BSS BssState;
    PVOID DeviceContext;
    KSTATUS Status;

    if (LockHeld == FALSE) {
        KeAcquireQueuedLock(Link->Lock);
    }

    ASSERT(Link->State == Net80211StateProbing);

    //
    // Restore the initial state, notify the hardware of the transition and
    // resume the data frames.
    //

    BssState = NULL;
    if (Link->ActiveBss != NULL) {
        BssState = &(Link->ActiveBss->State);
    }

    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.SetState(DeviceContext,
                                                 Link->ProbePreviousState,
                                                 BssState);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Failed to set state %d: %d\n",
                      Link->ProbePreviousState,
                      Status);

        goto StopProbingEnd;
    }

    Link->State = Link->ProbePreviousState;
    Net80211pResumeDataFrames(Link);

    //
    // If the transition state is not invalid, then the link tried to move to
    // a new state while the probe was active. Replay that transition now that
    // the original state is restored.
    //

    if (Link->ProbeNextState != Net80211StateInvalid) {
        Net80211pSetStateUnlocked(Link, Link->ProbeNextState);
    }

    Link->ProbePreviousState = Net80211StateInvalid;
    Link->ProbeNextState = Net80211StateInvalid;

StopProbingEnd:
    if (LockHeld == FALSE) {
        KeReleaseQueuedLock(Link->Lock);
    }

    return;
}

KSTATUS
Net80211pPrepareForReconnect (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY *Bss
    )

/*++

Routine Description:

    This routine prepares the network link for reconnecting to the given BSS.
    This includes pausing all outgoing data traffic and creating a copy of the
    BSS entry to use for the new association.

Arguments:

    Link - Supplies a pointer to the 802.11 link to be connected to the BSS.

    Bss - Supplies a pointer to a BSS on entry on input that is being left and
        on output, receives a pointer to the copied BSS entry to join.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_ENTRY BssCopy;
    PNET80211_BSS_ENTRY BssOriginal;
    KSTATUS Status;

    BssOriginal = *Bss;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);
    ASSERT(BssOriginal = Link->ActiveBss);

    //
    // Copy the BSS so a fresh state is used for the reconnection. Old
    // encryption keys must be reacquired.
    //

    BssCopy = Net80211pCopyBssEntry(BssOriginal);
    if (BssCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForReconnectEnd;
    }

    //
    // Pause all data frames while the link is attempting to reconnect to the
    // BSS.
    //

    Net80211pPauseDataFrames(Link);

    //
    // Leave the original BSS and join the copy.
    //

    Net80211pLeaveBss(Link, BssOriginal, FALSE, 0, 0);
    Net80211pJoinBss(Link, BssCopy);
    INSERT_BEFORE(&(BssCopy->ListEntry), &(Link->BssList));
    *Bss = BssCopy;
    Status = STATUS_SUCCESS;

PrepareForReconnectEnd:
    return Status;
}

VOID
Net80211pJoinBss (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine joins the given network link to the BSS.

Arguments:

    Link - Supplies a pointer to the 802.11 link that is joining the BSS.

    Bss - Supplies a pointer to the BSS to join.

Return Value:

    None.

--*/

{

    ASSERT(Link->ActiveBss == NULL);
    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    Link->ActiveBss = Bss;
    Net80211pBssEntryAddReference(Bss);
    return;
}

VOID
Net80211pLeaveBss (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss,
    BOOL SendNotification,
    ULONG Subtype,
    USHORT Reason
    )

/*++

Routine Description:

    This routine disconnects the network link from the given BSS. With this
    disconnection, any encryption keys are now invalid. To safely move said
    keys out of the system, this routine removes the BSS from the global list
    and releases the reference taken by the list. The BSS entry should be
    destroyed shortly.

Arguments:

    Link - Supplies a pointer to the 802.11 link that is leaving the BSS.

    Bss - Supplies a pointer to the BSS to leave.

    SendNotification - Supplies a boolean indicating whether or not this
        station should notify the BSS that it is leaving.

    Subtype - Supplies the notification type in the form of a management frame
        subtype. It should either be disassociation or deauthentication.

    Reason - Supplies the reason for leaving. See NET80211_REASON_CODE_* for
        definitions.

Return Value:

    None.

--*/

{

    ASSERT(Link->ActiveBss == Bss);
    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    if (SendNotification != FALSE) {
        Net80211pSendManagementFrame(Link,
                                     Bss->State.Bssid,
                                     Bss->State.Bssid,
                                     Subtype,
                                     &Reason,
                                     sizeof(USHORT));
    }

    Link->ActiveBss = NULL;

    //
    // Remove the BSS from the global list, destroy the reference taken on join
    // and the list's reference. This really just needs to destroy the keys,
    // but while the BSS is on the list and reference are outstanding, the keys
    // may be in use. The best thing to do is destroy the BSS entry.
    //

    LIST_REMOVE(&(Bss->ListEntry));
    Net80211pBssEntryReleaseReference(Bss);
    Net80211pBssEntryReleaseReference(Bss);
    return;
}

KSTATUS
Net80211pSendProbeRequest (
    PNET80211_LINK Link,
    PNET80211_SCAN_STATE Scan
    )

/*++

Routine Description:

    This routine sends an 802.11 management probe request frame based on the
    given scan state.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to send the probe
        request.

    Scan - Supplies a pointer to the scan state that is requesting the probe.

Return Value:

    Status code.

--*/

{

    PUCHAR Bssid;
    PUCHAR DestinationAddress;
    PUCHAR FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    PUCHAR InformationByte;
    PNET80211_RATE_INFORMATION Rates;
    KSTATUS Status;

    FrameBody = NULL;

    //
    // The probe request packed always includes the SSID, supported rates and
    // channel (DSSS).
    //

    ASSERT(Scan->SsidLength <= NET80211_MAX_SSID_LENGTH);

    FrameBodySize = 0;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    FrameBodySize += Scan->SsidLength;

    //
    // Get the supported rates size.
    //

    Rates = Link->Properties.SupportedRates;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    if (Rates->Count > NET80211_MAX_SUPPORTED_RATES) {
        FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    }

    FrameBodySize += Rates->Count;

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
    *InformationByte = Scan->SsidLength;
    InformationByte += 1;
    if (Scan->SsidLength != 0) {
        RtlCopyMemory(InformationByte, Scan->Ssid, Scan->SsidLength);
        InformationByte += Scan->SsidLength;
    }

    *InformationByte = NET80211_ELEMENT_SUPPORTED_RATES;
    InformationByte += 1;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        *InformationByte = Rates->Count;
        InformationByte += 1;
        RtlCopyMemory(InformationByte, Rates->Rate, Rates->Count);
        InformationByte += Rates->Count;

    } else {
        *InformationByte = NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        RtlCopyMemory(InformationByte,
                      Rates->Rate,
                      NET80211_MAX_SUPPORTED_RATES);

        InformationByte += NET80211_MAX_SUPPORTED_RATES;
        *InformationByte = NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES;
        InformationByte += 1;
        *InformationByte = Rates->Count - NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        RtlCopyMemory(InformationByte,
                      &(Rates->Rate[NET80211_MAX_SUPPORTED_RATES]),
                      Rates->Count - NET80211_MAX_SUPPORTED_RATES);

        InformationByte += Rates->Count - NET80211_MAX_SUPPORTED_RATES;
    }

    *InformationByte = NET80211_ELEMENT_DSSS;
    InformationByte += 1;
    *InformationByte = 1;
    InformationByte += 1;
    *InformationByte = (UCHAR)Scan->Channel;
    InformationByte += 1;

    ASSERT(FrameBodySize == (InformationByte - FrameBody));

    //
    // Send the management frame down to the lower layers.
    //

    if ((Scan->Flags & NET80211_SCAN_FLAG_BROADCAST) != 0) {
        Bssid = NULL;
        DestinationAddress = NULL;

    } else {
        Bssid = Scan->Bssid;
        DestinationAddress = Scan->Bssid;
    }

    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_REQUEST;
    Status = Net80211pSendManagementFrame(Link,
                                          DestinationAddress,
                                          Bssid,
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

VOID
Net80211pProcessProbeResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 management probe response frame. It stores
    the information for the transmitting BSS in the BSS cache.

Arguments:

    Link - Supplies a pointer to the 802.11 link that received the probe
        response.

    Packet - Supplies a pointer to the packet to process.

Return Value:

    None.

--*/

{

    UCHAR ElementId;
    ULONG ElementLength;
    ULONG ExpectedFrameSize;
    PUCHAR FrameBody;
    ULONG FrameSize;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    ULONG Offset;
    NET80211_PROBE_RESPONSE Response;
    ULONG Subtype;

    if (Link->State != Net80211StateProbing) {
        goto ProcessProbeResponseEnd;
    }

    Header = Packet->Buffer + Packet->DataOffset;
    Subtype = NET80211_GET_FRAME_SUBTYPE(Header);
    RtlZeroMemory(&Response, sizeof(NET80211_PROBE_RESPONSE));

    ASSERT((Subtype == NET80211_MANAGEMENT_FRAME_SUBTYPE_BEACON) ||
           (Subtype == NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_RESPONSE));

    //
    // Parse the response. It should at least have a timestamp, beacon
    // interval, and capabilities field.
    //

    FrameBody = Packet->Buffer + Packet->DataOffset;
    FrameSize = Packet->FooterOffset - Packet->DataOffset;
    Offset = sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
    ExpectedFrameSize = Offset +
                        NET80211_TIMESTAMP_SIZE +
                        NET80211_BEACON_INTERVAL_SIZE +
                        NET80211_CAPABILITY_SIZE;

    if (ExpectedFrameSize > FrameSize) {
        goto ProcessProbeResponseEnd;
    }

    //
    // Save the timestamp.
    //

    Response.Timestamp = *((PULONGLONG)&(FrameBody[Offset]));
    Offset += NET80211_TIMESTAMP_SIZE;

    //
    // Save the beacon internval.
    //

    Response.BeaconInterval = *((PUSHORT)&(FrameBody[Offset]));
    Offset += NET80211_BEACON_INTERVAL_SIZE;

    //
    // Save the capabilities.
    //

    Response.Capabilities = *((PUSHORT)&(FrameBody[Offset]));
    Offset += NET80211_CAPABILITY_SIZE;

    //
    // Collect the information elements.
    //

    Response.Elements = FrameBody + Offset;
    Response.ElementsSize = FrameSize - Offset;
    while (Offset < FrameSize) {
        if ((Offset + NET80211_ELEMENT_HEADER_SIZE) > FrameSize) {
            goto ProcessProbeResponseEnd;
        }

        ElementId = FrameBody[Offset + NET80211_ELEMENT_ID_OFFSET];
        ElementLength = FrameBody[Offset + NET80211_ELEMENT_LENGTH_OFFSET];
        ExpectedFrameSize = Offset +
                            NET80211_ELEMENT_HEADER_SIZE +
                            ElementLength;

        if (ExpectedFrameSize > FrameSize) {
            goto ProcessProbeResponseEnd;
        }

        switch (ElementId) {
        case NET80211_ELEMENT_SSID:
            Response.Ssid = FrameBody + Offset;
            break;

        case NET80211_ELEMENT_DSSS:
            if (ElementLength == 0) {
                goto ProcessProbeResponseEnd;
            }

            Response.Channel = FrameBody + Offset;
            break;

        case NET80211_ELEMENT_RSN:
            Response.Rsn = FrameBody + Offset;
            break;

        case NET80211_ELEMENT_SUPPORTED_RATES:
            if (ElementLength == 0) {
                goto ProcessProbeResponseEnd;
            }

            Response.Rates = FrameBody + Offset;
            break;

        case NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES:
            if (ElementLength == 0) {
                goto ProcessProbeResponseEnd;
            }

            Response.ExtendedRates = FrameBody + Offset;
            break;

        default:
            break;
        }

        Offset += NET80211_ELEMENT_HEADER_SIZE + ElementLength;
    }

    //
    // Toss out the packet if not all of the expected information is present.
    //

    if ((Response.Rates == NULL) ||
        (Response.Channel == NULL) ||
        (Response.Ssid == NULL)) {

        goto ProcessProbeResponseEnd;
    }

    //
    // Filter out any beacon/probe responses that claim to be open but still
    // include encryption information. Also filter out the opposite where
    // privacy is a required capability, but no encryption information was
    // provided.
    //

    if (Response.Rsn != NULL) {
        if ((Response.Capabilities & NET80211_CAPABILITY_PRIVACY) == 0) {
            RtlDebugPrint("802.11: Found RSN element in probe/beacon that does "
                          "not require privacy.\n");

            goto ProcessProbeResponseEnd;
        }

    } else {
        if ((Response.Capabilities & NET80211_CAPABILITY_PRIVACY) != 0) {
            RtlDebugPrint("802.11: Did not find RSN element in probe/beacon "
                          "that requires privacy.\n");

            goto ProcessProbeResponseEnd;
        }
    }

    //
    // Update the BSS cache with the latest information from this beacon /
    // probe response. The SSID, encryption method, and rates are subject to
    // change for a BSSID.
    //

    Response.Bssid = Header->SourceAddress;
    Net80211pUpdateBssCache(Link, &Response);

ProcessProbeResponseEnd:
    return;
}

KSTATUS
Net80211pSendAuthenticationRequest (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine sends an 802.11 management authentication frame to the AP of
    the given BSS.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to send an
        authentication request.

    Bss - Supplies a pointer to the BSS over which to send the authentication
        frame.

Return Value:

    Status code.

--*/

{

    NET80211_AUTHENTICATION_OPEN_BODY FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    KSTATUS Status;

    //
    // Fill out the authentication body.
    //

    FrameBody.AlgorithmNumber = NET80211_AUTHENTICATION_ALGORITHM_OPEN;
    FrameBody.TransactionSequenceNumber =
                               NET80211_AUTHENTICATION_REQUEST_SEQUENCE_NUMBER;

    FrameBody.StatusCode = NET80211_STATUS_CODE_SUCCESS;

    //
    // Send the authentication frame off. The destination address and BSSID
    // should match.
    //

    FrameBodySize = sizeof(NET80211_AUTHENTICATION_OPEN_BODY);
    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION;
    Status = Net80211pSendManagementFrame(Link,
                                          Bss->State.Bssid,
                                          Bss->State.Bssid,
                                          FrameSubtype,
                                          &FrameBody,
                                          FrameBodySize);

    if (!KSUCCESS(Status)) {
        goto SendAuthenticationEnd;
    }

SendAuthenticationEnd:
    return Status;
}

VOID
Net80211pProcessAuthenticationResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an authentication response frame. It is expected to
    be sent from the BSSID stored in the link's BSS context.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which the authentication
        packet was received.

    Packet - Supplies a pointer to the network packet containing the
        authentication frame.

Return Value:

    None.

--*/

{

    PNET80211_AUTHENTICATION_OPEN_BODY Body;
    PNET80211_BSS_ENTRY Bss;
    ULONG ExpectedFrameSize;
    ULONG FrameSize;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    BOOL Match;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    if (Link->State != Net80211StateAuthenticating) {
        return;
    }

    KeAcquireQueuedLock(Link->Lock);
    if (Link->State != Net80211StateAuthenticating) {
        goto ProcessAuthenticationResponseEnd;
    }

    ASSERT(Link->ActiveBss != NULL);

    Bss = Link->ActiveBss;

    //
    // Make sure the this frame was sent from the AP of the BSS.
    //

    Header = Packet->Buffer + Packet->DataOffset;
    Match = RtlCompareMemory(Header->SourceAddress,
                             Bss->State.Bssid,
                             NET80211_ADDRESS_SIZE);

    if (Match == FALSE) {
        Status = STATUS_INVALID_ADDRESS;
        goto ProcessAuthenticationResponseEnd;
    }

    //
    // Make sure it is large enough to hold the authentication body.
    //

    FrameSize = Packet->FooterOffset - Packet->DataOffset;
    ExpectedFrameSize = sizeof(NET80211_MANAGEMENT_FRAME_HEADER) +
                        sizeof(NET80211_AUTHENTICATION_OPEN_BODY);

    if (FrameSize < ExpectedFrameSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ProcessAuthenticationResponseEnd;
    }

    //
    // The authentication response has a very fixed frame body.
    //

    Body = Packet->Buffer +
           Packet->DataOffset +
           sizeof(NET80211_MANAGEMENT_FRAME_HEADER);

    if (Body->AlgorithmNumber != NET80211_AUTHENTICATION_ALGORITHM_OPEN) {
        RtlDebugPrint("802.11: Unexpected algorithm type %d. Expected %d.\n",
                      Body->AlgorithmNumber,
                      NET80211_AUTHENTICATION_ALGORITHM_OPEN);

        Status = STATUS_NOT_SUPPORTED;
        goto ProcessAuthenticationResponseEnd;
    }

    if (Body->TransactionSequenceNumber !=
        NET80211_AUTHENTICATION_RESPONSE_SEQUENCE_NUMBER) {

        RtlDebugPrint("802.11: Unexpected authentication transaction "
                      "sequence number 0x%04x. Expected 0x%04x.\n",
                      Body->TransactionSequenceNumber,
                      NET80211_AUTHENTICATION_RESPONSE_SEQUENCE_NUMBER);

        Status = STATUS_UNSUCCESSFUL;
        goto ProcessAuthenticationResponseEnd;
    }

    if (Body->StatusCode != NET80211_STATUS_CODE_SUCCESS) {
        RtlDebugPrint("802.11: Authentication failed with status %d\n",
                      Body->StatusCode);

        Status = STATUS_UNSUCCESSFUL;
        goto ProcessAuthenticationResponseEnd;
    }

    Net80211pSetStateUnlocked(Link, Net80211StateAssociating);
    Status = STATUS_SUCCESS;

ProcessAuthenticationResponseEnd:
    if (!KSUCCESS(Status)) {
        Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
    }

    KeReleaseQueuedLock(Link->Lock);
    return;
}

KSTATUS
Net80211pSendAssociationRequest (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine sends an 802.11 management association request frame to the
    to the AP of the given BSS.

Arguments:

    Link - Supplies a pointer to the 802.11 link over which to send the
        association request.

    Bss - Supplies a pointer to the BSS over which to send the association
        request.

Return Value:

    Status code.

--*/

{

    PUCHAR FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    PUCHAR InformationByte;
    PNET80211_RATE_INFORMATION Rates;
    ULONG SsidLength;
    KSTATUS Status;

    ASSERT(Bss != NULL);
    ASSERT(Bss->Ssid != NULL);

    FrameBody = NULL;
    SsidLength = NET80211_GET_ELEMENT_LENGTH(Bss->Ssid);

    //
    // Determine the size of the probe response packet, which always includes
    // the capabilities, listen interval, SSID, and supported rates.
    //

    ASSERT((SsidLength <= NET80211_MAX_SSID_LENGTH) && (SsidLength != 0));

    FrameBodySize = NET80211_CAPABILITY_SIZE + NET80211_LISTEN_INTERVAL_SIZE;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE + SsidLength;

    //
    // Get the supported rates size, including the extended rates if necessary.
    //

    Rates = Link->Properties.SupportedRates;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    if (Rates->Count > NET80211_MAX_SUPPORTED_RATES) {
        FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    }

    FrameBodySize += Rates->Count;

    //
    // Only include the RSN information if advanced encryption is required.
    //

    if ((Bss->Encryption.Pairwise != NetworkEncryptionNone) &&
        (Bss->Encryption.Pairwise != NetworkEncryptionWep)) {

        FrameBodySize += sizeof(NET80211_DEFAULT_RSN_INFORMATION);
    }

    //
    // Allocate a buffer to hold the assocation request frame body.
    //

    FrameBody = MmAllocatePagedPool(FrameBodySize, NET80211_ALLOCATION_TAG);
    if (FrameBody == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendAssociationRequestEnd;
    }

    //
    // Fill out the frame body. There is a strict order here, so do not
    // rearrage the information elements.
    //

    InformationByte = FrameBody;
    *((PUSHORT)InformationByte) = Link->Properties.Net80211Capabilities |
                                  NET80211_CAPABILITY_ESS;

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
    RtlCopyMemory(InformationByte,
                  NET80211_GET_ELEMENT_DATA(Bss->Ssid),
                  SsidLength);

    InformationByte += SsidLength;
    *InformationByte = NET80211_ELEMENT_SUPPORTED_RATES;
    InformationByte += 1;
    if (Rates->Count <= NET80211_MAX_SUPPORTED_RATES) {
        *InformationByte = Rates->Count;
        InformationByte += 1;
        RtlCopyMemory(InformationByte, Rates->Rate, Rates->Count);
        InformationByte += Rates->Count;

    } else {
        *InformationByte = NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        RtlCopyMemory(InformationByte,
                      Rates->Rate,
                      NET80211_MAX_SUPPORTED_RATES);

        InformationByte += NET80211_MAX_SUPPORTED_RATES;
        *InformationByte = NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES;
        InformationByte += 1;
        *InformationByte = Rates->Count - NET80211_MAX_SUPPORTED_RATES;
        InformationByte += 1;
        RtlCopyMemory(InformationByte,
                      &(Rates->Rate[NET80211_MAX_SUPPORTED_RATES]),
                      Rates->Count - NET80211_MAX_SUPPORTED_RATES);

        InformationByte += Rates->Count - NET80211_MAX_SUPPORTED_RATES;
    }

    //
    // Set the RSN information if advanced encryption is required.
    //

    if ((Bss->Encryption.Pairwise != NetworkEncryptionNone) &&
        (Bss->Encryption.Pairwise != NetworkEncryptionWep)) {

        RtlCopyMemory(InformationByte,
                      &Net80211DefaultRsnInformation,
                      sizeof(NET80211_DEFAULT_RSN_INFORMATION));

        InformationByte += sizeof(NET80211_DEFAULT_RSN_INFORMATION);
    }

    ASSERT(FrameBodySize == (InformationByte - FrameBody));

    //
    // Send the management frame down to the lower layers.
    //

    FrameSubtype = NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_REQUEST;
    Status = Net80211pSendManagementFrame(Link,
                                          Bss->State.Bssid,
                                          Bss->State.Bssid,
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

VOID
Net80211pProcessAssociationResponse (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 management association response frame from
    an access point.

Arguments:

    Link - Supplies a pointer to the 802.11 link that received the association
        response.

    Packet - Supplies a pointer to the network packet that contains the
        association response frame.

Return Value:

    None.

--*/

{

    USHORT AssociationId;
    PNET80211_BSS_ENTRY Bss;
    USHORT Capabilities;
    PUCHAR ElementBytePointer;
    UCHAR ElementId;
    ULONG ElementLength;
    ULONG ExpectedFrameSize;
    ULONG ExtendedRateCount;
    PUCHAR ExtendedRates;
    ULONG FrameSize;
    USHORT FrameStatus;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    BOOL Match;
    ULONG Offset;
    ULONG RateCount;
    PUCHAR Rates;
    KSTATUS Status;
    ULONG TotalRateCount;

    Status = STATUS_SUCCESS;
    if (Link->State != Net80211StateAssociating) {
        return;
    }

    KeAcquireQueuedLock(Link->Lock);
    if (Link->State != Net80211StateAssociating) {
        goto ProcessAssociationResponseEnd;
    }

    ASSERT(Link->ActiveBss != NULL);

    Bss = Link->ActiveBss;

    //
    // Make sure the this frame was sent from the destination.
    //

    Header = Packet->Buffer + Packet->DataOffset;
    Match = RtlCompareMemory(Header->SourceAddress,
                             Bss->State.Bssid,
                             NET80211_ADDRESS_SIZE);

    if (Match == FALSE) {
        Status = STATUS_INVALID_ADDRESS;
        goto ProcessAssociationResponseEnd;
    }

    //
    // There should at least be capabilities, a status code and the AID.
    //

    FrameSize = Packet->FooterOffset - Packet->DataOffset;
    Offset = sizeof(NET80211_MANAGEMENT_FRAME_HEADER);
    ExpectedFrameSize = Offset +
                        NET80211_CAPABILITY_SIZE +
                        NET80211_STATUS_CODE_SIZE +
                        NET80211_ASSOCIATION_ID_SIZE;

    if (FrameSize < ExpectedFrameSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ProcessAssociationResponseEnd;
    }

    ElementBytePointer = Packet->Buffer + Packet->DataOffset;

    //
    // Save the capabilities.
    //

    Capabilities = *((PUSHORT)&(ElementBytePointer[Offset]));
    Offset += NET80211_CAPABILITY_SIZE;

    //
    // Don't continue unless the association was a success.
    //

    FrameStatus = *((PUSHORT)(&(ElementBytePointer[Offset])));
    if (FrameStatus != NET80211_STATUS_CODE_SUCCESS) {
        RtlDebugPrint("802.11: Association response failed with status "
                      "0x%04x.\n",
                      FrameStatus);

        Status = STATUS_UNSUCCESSFUL;
        goto ProcessAssociationResponseEnd;
    }

    Offset += NET80211_STATUS_CODE_SIZE;

    //
    // Save the association ID.
    //

    AssociationId = *((PUSHORT)&(ElementBytePointer[Offset]));
    AssociationId &= NET80211_ASSOCIATION_ID_MASK;
    Offset += NET80211_ASSOCIATION_ID_SIZE;

    //
    // Now look at the supplied elements.
    //

    Rates = NULL;
    RateCount = 0;
    ExtendedRates = NULL;
    ExtendedRateCount = 0;
    while (Offset < FrameSize) {
        ElementId = ElementBytePointer[Offset];
        Offset += 1;
        if (Offset >= FrameSize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ProcessAssociationResponseEnd;
        }

        ElementLength = ElementBytePointer[Offset];
        Offset += 1;
        if ((Offset + ElementLength) > FrameSize) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ProcessAssociationResponseEnd;
        }

        switch (ElementId) {
        case NET80211_ELEMENT_SUPPORTED_RATES:
            if (ElementLength == 0) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto ProcessAssociationResponseEnd;
            }

            Rates = ElementBytePointer + Offset;
            RateCount = ElementLength;
            break;

        case NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES:
            if (ElementLength == 0) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto ProcessAssociationResponseEnd;
            }

            ExtendedRates = ElementBytePointer + Offset;
            ExtendedRateCount = ElementLength;
            break;

        default:
            break;
        }

        Offset += ElementLength;
    }

    //
    // If the capabilities or rates have changed from the probe response or
    // beacon, do not proceed with the association. The AP has changed since
    // the association process began. Deauthenticate instead.
    //

    if (Capabilities != Bss->State.Capabilities) {
        Status = STATUS_OPERATION_CANCELLED;
        goto ProcessAssociationResponseEnd;
    }

    TotalRateCount = RateCount + ExtendedRateCount;
    if (TotalRateCount != Bss->State.Rates.Count) {
        Status = STATUS_OPERATION_CANCELLED;
        goto ProcessAssociationResponseEnd;
    }

    //
    // Copy the current rates into the BSS entry.
    //

    RtlCopyMemory(Bss->State.Rates.Rate, Rates, RateCount);
    RtlCopyMemory(Bss->State.Rates.Rate + RateCount,
                  ExtendedRates,
                  ExtendedRateCount);

    Status = Net80211pValidateRates(Link, Bss);
    if (!KSUCCESS(Status)) {
        goto ProcessAssociationResponseEnd;
    }

    Bss->State.AssociationId = AssociationId;
    Net80211pSetStateUnlocked(Link, Net80211StateAssociated);

ProcessAssociationResponseEnd:
    if (!KSUCCESS(Status)) {
        Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
    }

    KeReleaseQueuedLock(Link->Lock);
    return;
}

KSTATUS
Net80211pSendManagementFrame (
    PNET80211_LINK Link,
    PUCHAR DestinationAddress,
    PUCHAR Bssid,
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

    PVOID DeviceContext;
    ULONG Flags;
    PNET80211_MANAGEMENT_FRAME_HEADER Header;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    KSTATUS Status;

    NET_INITIALIZE_PACKET_LIST(&PacketList);

    //
    // Allocate a network packet to send down to the lower layers.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(sizeof(NET80211_MANAGEMENT_FRAME_HEADER),
                               FrameBodySize,
                               0,
                               Link->NetworkLink,
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
                      DestinationAddress,
                      NET80211_ADDRESS_SIZE);

    } else {
        RtlSetMemory(Header->DestinationAddress, 0xFF, NET80211_ADDRESS_SIZE);
    }

    //
    // The source address is always the local link's physical address (i.e. the
    // MAC address).
    //

    RtlCopyMemory(Header->SourceAddress,
                  Link->Properties.PhysicalAddress.Address,
                  NET80211_ADDRESS_SIZE);

    if (Bssid != NULL) {
        RtlCopyMemory(Header->Bssid, Bssid, NET80211_ADDRESS_SIZE);

    } else {
        RtlSetMemory(Header->Bssid, 0xFF, NET80211_ADDRESS_SIZE);
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

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.Send(DeviceContext, &PacketList);
    if (!KSUCCESS(Status)) {
        goto SendManagementFrameEnd;
    }

SendManagementFrameEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

KSTATUS
Net80211pValidateRates (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine validates that the link and BSS share the same basic rates and
    detects the maximum mode for a future connection, storing the result in the
    BSS entry.

Arguments:

    Link - Supplies a pointer to the 802.11 link for which to validate the BSS
        entry's rates.

    Bss - Supplies a pointer to a BSS entry.

Return Value:

    Status code.

--*/

{

    ULONG BssIndex;
    UCHAR BssRate;
    PNET80211_RATE_INFORMATION BssRates;
    UCHAR BssRateValue;
    ULONGLONG LinkSpeed;
    ULONG LocalIndex;
    UCHAR LocalRate;
    PNET80211_RATE_INFORMATION LocalRates;
    UCHAR MaxRate;
    KSTATUS Status;

    BssRates = &(Bss->State.Rates);
    LocalRates = Link->Properties.SupportedRates;

    //
    // Make sure the basic rates are supported. Unfortunately, there is no
    // guarantee about the ordering of the rates. There aren't that many so do
    // not bother sorting.
    //

    MaxRate = 0;
    for (BssIndex = 0; BssIndex < BssRates->Count; BssIndex += 1) {
        BssRate = BssRates->Rate[BssIndex];
        BssRateValue = BssRate & NET80211_RATE_VALUE_MASK;
        if ((BssRate & NET80211_RATE_BASIC) != 0) {
            if (BssRateValue == NET80211_MEMBERSHIP_SELECTOR_HT_PHY) {
                continue;
            }

        } else if (BssRateValue <= MaxRate) {
            continue;
        }

        //
        // Attempt to find the rate in the local supported rates.
        //

        for (LocalIndex = 0; LocalIndex < LocalRates->Count; LocalIndex += 1) {
            LocalRate = LocalRates->Rate[LocalIndex] & NET80211_RATE_VALUE_MASK;
            if (LocalRate == BssRateValue) {
                break;
            }
        }

        //
        // If this is a basic rate and it is not supported locally, then
        // connecting to this BSS is not allowed.
        //

        if (LocalIndex == LocalRates->Count) {
            if ((BssRate & NET80211_RATE_BASIC) != 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto ValidateRatesEnd;
            }

            continue;
        }

        if (BssRateValue > MaxRate) {
            MaxRate = BssRateValue;
        }
    }

    //
    // If no rate could be agreed upon, then fail to connect to the BSS.
    //

    if (MaxRate == 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto ValidateRatesEnd;
    }

    //
    // Fill in the connection mode based on the maximum supported rate.
    //

    Bss->State.MaxRate = MaxRate;
    LinkSpeed = MaxRate * NET80211_RATE_UNIT;
    if (LinkSpeed <= NET80211_MODE_B_MAX_RATE) {
        Bss->State.Mode = Net80211ModeB;

    } else if (LinkSpeed <= NET80211_MODE_G_MAX_RATE) {
        Bss->State.Mode = Net80211ModeG;

    } else {
        Status = STATUS_NOT_SUPPORTED;
        goto ValidateRatesEnd;
    }

    Status = STATUS_SUCCESS;

ValidateRatesEnd:
    return Status;
}

KSTATUS
Net80211pParseRsnElement (
    PUCHAR Rsn,
    PNET80211_ENCRYPTION Encryption
    )

/*++

Routine Description:

    This routine parses the RSN information element in order to detect which
    encryption methods are supported by the BSS to which it belongs.

Arguments:

    Rsn - Supplies a pointer to the RSN element, the first byte of which must
        be the element ID.

    Encryption - Supplies a pointer to an 802.11 encryption structure that
        receives the parsed data from the RSN element.

Return Value:

    Status code.

--*/

{

    NETWORK_ENCRYPTION_TYPE GroupEncryption;
    ULONG Index;
    ULONG Offset;
    NETWORK_ENCRYPTION_TYPE PairwiseEncryption;
    USHORT PmkidCount;
    BOOL PskSupported;
    ULONG RsnLength;
    KSTATUS Status;
    ULONG Suite;
    USHORT SuiteCount;
    PULONG Suites;
    USHORT Version;

    ASSERT(NET80211_GET_ELEMENT_ID(Rsn) == NET80211_ELEMENT_RSN);

    Status = STATUS_SUCCESS;
    Offset = NET80211_ELEMENT_HEADER_SIZE;
    PairwiseEncryption = NetworkEncryptionNone;
    GroupEncryption = NetworkEncryptionNone;
    RsnLength = NET80211_GET_ELEMENT_LENGTH(Rsn);

    //
    // The version field is the only non-optional field.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ParseRsnElementEnd;
    }

    Version = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if (Version != NET80211_RSN_VERSION) {
        RtlDebugPrint("802.11: Unexpected RSN version %d\n", Version);
        Status = STATUS_VERSION_MISMATCH;
        goto ParseRsnElementEnd;
    }

    //
    // Get the optional group suite.
    //

    if ((Offset + sizeof(ULONG)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    Suite = NETWORK_TO_CPU32(*((PULONG)&(Rsn[Offset])));
    Offset += sizeof(ULONG);
    switch (Suite) {
    case NET80211_CIPHER_SUITE_WEP_40:
    case NET80211_CIPHER_SUITE_WEP_104:
        GroupEncryption = NetworkEncryptionWep;
        break;

    case NET80211_CIPHER_SUITE_TKIP:
        GroupEncryption = NetworkEncryptionWpaEap;
        break;

    case NET80211_CIPHER_SUITE_CCMP:
        GroupEncryption = NetworkEncryptionWpa2Eap;
        break;

    case NET80211_CIPHER_SUITE_GROUP_NOT_ALLOWED:
        GroupEncryption = NetworkEncryptionNone;
        break;

    default:
        GroupEncryption = NetworkEncryptionInvalid;
        RtlDebugPrint("802.11: Group cipher suite not supported 0x%08x\n",
                      Suite);

        break;
    }

    if (GroupEncryption == NetworkEncryptionInvalid) {
        Status = STATUS_NOT_SUPPORTED;
        goto ParseRsnElementEnd;
    }

    //
    // Gather the pairwise suites.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    SuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if ((Offset + (SuiteCount * sizeof(ULONG))) > RsnLength) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ParseRsnElementEnd;
    }

    Suites = (PULONG)&(Rsn[Offset]);
    Offset += SuiteCount * sizeof(ULONG);
    for (Index = 0; Index < SuiteCount; Index += 1) {
        Suite = NETWORK_TO_CPU32(Suites[Index]);

        //
        // As soon as CCMP is found, prefer that. None of the others are
        // supported anyway.
        //

        if (Suite == NET80211_CIPHER_SUITE_CCMP) {
            PairwiseEncryption = NetworkEncryptionWpa2Eap;
            break;
        }

        switch (Suite) {
        case NET80211_CIPHER_SUITE_WEP_40:
        case NET80211_CIPHER_SUITE_WEP_104:
            PairwiseEncryption = NetworkEncryptionWep;
            break;

        case NET80211_CIPHER_SUITE_TKIP:
            PairwiseEncryption = NetworkEncryptionWpaEap;
            break;

        case NET80211_CIPHER_SUITE_USE_GROUP_CIPHER:
            PairwiseEncryption = GroupEncryption;
            Encryption->Flags |= NET80211_ENCRYPTION_FLAG_USE_GROUP_CIPHER;
            break;

        default:
            PairwiseEncryption = NetworkEncryptionInvalid;
            RtlDebugPrint("802.11: Pairwise cipher suite not supported "
                          "0x%08x\n",
                          Suite);

            break;
        }
    }

    if (PairwiseEncryption == NetworkEncryptionInvalid) {
        Status = STATUS_NOT_SUPPORTED;
        goto ParseRsnElementEnd;
    }

    //
    // Upgrade the group and pairwise encryption methods from EAP to PSK if
    // PSK is present.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    SuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if ((Offset + (SuiteCount * sizeof(ULONG))) > RsnLength) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ParseRsnElementEnd;
    }

    Suites = (PULONG)&(Rsn[Offset]);
    Offset += SuiteCount * sizeof(ULONG);
    PskSupported = FALSE;
    for (Index = 0; Index < SuiteCount; Index += 1) {
        Suite = NETWORK_TO_CPU32(Suites[Index]);
        if ((Suite == NET80211_AKM_SUITE_PSK) ||
            (Suite == NET80211_AKM_SUITE_PSK_SHA256)) {

            PskSupported = TRUE;
            break;
        }
    }

    if (PskSupported != FALSE) {
        if (GroupEncryption == NetworkEncryptionWpaEap) {
            GroupEncryption = NetworkEncryptionWpaPsk;

        } else if (GroupEncryption == NetworkEncryptionWpa2Eap) {
            GroupEncryption = NetworkEncryptionWpa2Psk;
        }

        if (PairwiseEncryption == NetworkEncryptionWpaEap) {
            PairwiseEncryption = NetworkEncryptionWpaPsk;

        } else if (PairwiseEncryption == NetworkEncryptionWpa2Eap) {
            PairwiseEncryption = NetworkEncryptionWpa2Psk;
        }
    }

    //
    // Skip the RSN capabilities.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    Offset += sizeof(USHORT);

    //
    // Skip the PMKIDs.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    PmkidCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if ((Offset + (PmkidCount * NET80211_RSN_PMKID_LENGTH)) > RsnLength) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ParseRsnElementEnd;
    }

    Offset += PmkidCount * NET80211_RSN_PMKID_LENGTH;

    //
    // Skip the group management suite.
    //

    if ((Offset + sizeof(ULONG)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    Suite = *((PULONG)&(Rsn[Offset]));
    Offset += sizeof(ULONG);

ParseRsnElementEnd:
    Encryption->Pairwise = PairwiseEncryption;
    Encryption->Group = GroupEncryption;
    return Status;
}

VOID
Net80211pUpdateBssCache (
    PNET80211_LINK Link,
    PNET80211_PROBE_RESPONSE Response
    )

/*++

Routine Description:

    This routine updates the given BSS cache based on the data found in a
    beacon or probe response packet.

Arguments:

    Link - Supplies a pointer to the 802.11 link that received the packet.

    Response - Supplies a pointer to a parsed representation of a beacon or
        probe response packet.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    ULONG Channel;
    BOOL DestroyBss;
    BOOL LinkDown;
    BOOL Match;
    PUCHAR NewRsn;
    ULONG NewRsnLength;
    ULONG NewSsidLength;
    ULONG Offset;
    PUCHAR OldRsn;
    ULONG OldRsnLength;
    ULONG OldSsidLength;
    PUCHAR RatesArray;
    KSTATUS Status;
    ULONG TotalRateCount;

    TotalRateCount = NET80211_GET_ELEMENT_LENGTH(Response->Rates);
    if (Response->ExtendedRates != NULL) {
        TotalRateCount += NET80211_GET_ELEMENT_LENGTH(Response->ExtendedRates);
    }

    //
    // First look for an existing BSS entry based on the BSSID. But if no
    // matching BSS entry is found, then create a new one and insert it into
    // the list.
    //

    KeAcquireQueuedLock(Link->Lock);
    Bss = Net80211pLookupBssEntry(Link, Response->Bssid);
    if (Bss == NULL) {
        Bss = Net80211pCreateBssEntry(Response->Bssid);
        if (Bss == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto UpdateBssCacheEnd;
        }

        INSERT_BEFORE(&(Bss->ListEntry), &(Link->BssList));
    }

    //
    // Gather some locals from the response elements.
    //

    Channel = *NET80211_GET_ELEMENT_DATA(Response->Channel);

    //
    // If this is an update for the active BSS, then any changes will cause
    // the link to go down.
    //

    if (Link->ActiveBss == Bss) {
        LinkDown = FALSE;
        NewSsidLength = NET80211_GET_ELEMENT_LENGTH(Response->Ssid);
        OldSsidLength = NET80211_GET_ELEMENT_LENGTH(Bss->Ssid);
        NewRsnLength = 0;
        NewRsn = Response->Rsn;
        if (NewRsn != NULL) {
            NewRsnLength = NET80211_ELEMENT_HEADER_SIZE +
                           NET80211_GET_ELEMENT_LENGTH(Response->Rsn);
        }

        OldRsnLength = 0;
        OldRsn = Bss->Encryption.ApRsn;
        if (OldRsn != NULL) {
            OldRsnLength = NET80211_ELEMENT_HEADER_SIZE +
                           NET80211_GET_ELEMENT_LENGTH(Bss->Encryption.ApRsn);
        }

        ASSERT(NewSsidLength <= NET80211_MAX_SSID_LENGTH);

        if ((Bss->State.BeaconInterval != Response->BeaconInterval) ||
            (Bss->State.Capabilities != Response->Capabilities) ||
            (Bss->State.Channel != Channel) ||
            (Bss->State.Rates.Count != TotalRateCount) ||
            (OldSsidLength != NewSsidLength) ||
            (OldRsnLength != NewRsnLength)) {

            LinkDown = TRUE;
        }

        if (LinkDown == FALSE) {
            Match = RtlCompareMemory(NET80211_GET_ELEMENT_DATA(Bss->Ssid),
                                     NET80211_GET_ELEMENT_DATA(Response->Ssid),
                                     NewSsidLength);

            if (Match == FALSE) {
                LinkDown = TRUE;
            }
        }

        if (LinkDown == FALSE) {
            Match = RtlCompareMemory(OldRsn, NewRsn, NewRsnLength);
            if (Match == FALSE) {
                LinkDown = TRUE;
            }
        }

        if (LinkDown != FALSE) {
            Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
        }
    }

    //
    // Update the BSS entry with the latest information from the AP.
    //

    Bss->State.BeaconInterval = Response->BeaconInterval;
    Bss->State.Capabilities = Response->Capabilities;
    Bss->State.Channel = Channel;
    Bss->State.Timestamp = Response->Timestamp;

    //
    // Allocate a new elements buffer with the newest data. This will include
    // resetting the SSID and AP's RSN pointers.
    //

    if (Response->ElementsSize != 0) {
        if (Bss->Elements != NULL) {
            MmFreePagedPool(Bss->Elements);
            Bss->ElementsSize = 0;
        }

        Bss->Ssid = NULL;
        Bss->Encryption.ApRsn = NULL;
        Bss->Elements = MmAllocatePagedPool(Response->ElementsSize,
                                            NET80211_ALLOCATION_TAG);

        if (Bss->Elements == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto UpdateBssCacheEnd;
        }

        Bss->ElementsSize = Response->ElementsSize;
        RtlCopyMemory(Bss->Elements, Response->Elements, Bss->ElementsSize);
        Offset = Response->Ssid - Response->Elements;
        Bss->Ssid = Bss->Elements + Offset;
        if (Response->Rsn != NULL) {
            Offset = Response->Rsn - Response->Elements;
            Bss->Encryption.ApRsn = Bss->Elements + Offset;

            //
            // Parse the RSN information to determine the encryption algorithms
            // in use by the BSS.
            //

            Status = Net80211pParseRsnElement(Bss->Encryption.ApRsn,
                                              &(Bss->Encryption));

            if (!KSUCCESS(Status)) {
                goto UpdateBssCacheEnd;
            }
        }
    }

    //
    // Gather the rates from the response into one array.
    //

    ASSERT(TotalRateCount != 0);

    RatesArray = Bss->State.Rates.Rate;
    if (Bss->State.Rates.Count < TotalRateCount) {
        if (RatesArray != NULL) {
            MmFreePagedPool(RatesArray);
            Bss->State.Rates.Rate = NULL;
        }

        RatesArray = MmAllocatePagedPool(TotalRateCount * sizeof(UCHAR),
                                         NET80211_ALLOCATION_TAG);

        if (RatesArray == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto UpdateBssCacheEnd;
        }

        Bss->State.Rates.Rate = RatesArray;
    }

    Bss->State.Rates.Count = TotalRateCount;
    RtlCopyMemory(RatesArray,
                  NET80211_GET_ELEMENT_DATA(Response->Rates),
                  NET80211_GET_ELEMENT_LENGTH(Response->Rates));

    if (Response->ExtendedRates != NULL) {
        RtlCopyMemory(RatesArray + NET80211_GET_ELEMENT_LENGTH(Response->Rates),
                      NET80211_GET_ELEMENT_DATA(Response->ExtendedRates),
                      NET80211_GET_ELEMENT_LENGTH(Response->ExtendedRates));
    }

    //
    // Record that this BSS needs to encrypt/decrypt data.
    //

    if (Bss->Encryption.Pairwise != NetworkEncryptionNone) {
        Bss->Flags |= NET80211_BSS_FLAG_ENCRYPT_DATA;
    }

    //
    // For now, the station always advertises the same RSN information. Just
    // point at the global.
    //

    Bss->Encryption.StationRsn = (PUCHAR)&Net80211DefaultRsnInformation;
    Bss->LastUpdated = KeGetRecentTimeCounter();
    Status = STATUS_SUCCESS;

UpdateBssCacheEnd:
    DestroyBss = FALSE;
    if (!KSUCCESS(Status)) {
        if (Bss != NULL) {
            LIST_REMOVE(&(Bss->ListEntry));
            DestroyBss = TRUE;
        }
    }

    KeReleaseQueuedLock(Link->Lock);
    if (DestroyBss != FALSE) {
        Net80211pBssEntryReleaseReference(Bss);
    }

    return;
}

VOID
Net80211pTrimBssCache (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine removes the expired BSS entries from the given link's list.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose old BSS entries are to
        be removed.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PLIST_ENTRY CurrentEntry;
    ULONGLONG CurrentTime;
    ULONGLONG ElapsedTime;
    LIST_ENTRY LocalList;
    ULONGLONG Timeout;

    INITIALIZE_LIST_HEAD(&LocalList);
    CurrentTime = KeGetRecentTimeCounter();
    Timeout = KeConvertMicrosecondsToTimeTicks(NET80211_BSS_ENTRY_TIMEOUT);

    //
    // Go through the list once and find all the expired entries, moving them
    // to a local list.
    //

    KeAcquireQueuedLock(Link->Lock);
    CurrentEntry = Link->BssList.Next;
    while (CurrentEntry != &(Link->BssList)) {
        Bss = LIST_VALUE(CurrentEntry, NET80211_BSS_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Don't trim the active BSS.
        //

        if (Bss == Link->ActiveBss) {
            continue;
        }

        ElapsedTime = CurrentTime - Bss->LastUpdated;
        if (ElapsedTime > Timeout) {
            LIST_REMOVE(&(Bss->ListEntry));
            INSERT_BEFORE(&(Bss->ListEntry), &LocalList);
        }
    }

    KeReleaseQueuedLock(Link->Lock);

    //
    // Run through the local list and release a reference on each.
    //

    while (LIST_EMPTY(&LocalList) == FALSE) {
        Bss = LIST_VALUE(LocalList.Next, NET80211_BSS_ENTRY, ListEntry);
        LIST_REMOVE(&(Bss->ListEntry));
        Net80211pBssEntryReleaseReference(Bss);
    }

    return;
}

PNET80211_BSS_ENTRY
Net80211pCopyBssEntry (
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine creates a copy of the given BSS entry with the encryption keys
    removed.

Arguments:

    Bss - Supplies a pointer to the BSS from a prior connection that the link
        is now trying to reconnect with.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_ENTRY BssCopy;
    ULONG Offset;
    ULONG RatesSize;
    KSTATUS Status;

    BssCopy = NULL;

    //
    // Allocate a copy of the BSS entry, but do not copy any encryption keys as
    // those are associated with a single connection to a BSS.
    //

    BssCopy = Net80211pCreateBssEntry(Bss->State.Bssid);
    if (BssCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CopyBssEntryEnd;
    }

    BssCopy->Flags = Bss->Flags;
    RtlCopyMemory(&(BssCopy->State), &(Bss->State), sizeof(NET80211_BSS));
    RtlCopyMemory(&(BssCopy->Encryption),
                  &(Bss->Encryption),
                  sizeof(NET80211_ENCRYPTION));

    if (Bss->ElementsSize != 0) {
        BssCopy->ElementsSize = Bss->ElementsSize;
        BssCopy->Elements = MmAllocatePagedPool(BssCopy->ElementsSize,
                                                NET80211_ALLOCATION_TAG);

        if (BssCopy->Elements == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyBssEntryEnd;
        }

        RtlCopyMemory(BssCopy->Elements, Bss->Elements, BssCopy->ElementsSize);
        Offset = Bss->Ssid - Bss->Elements;
        BssCopy->Ssid = BssCopy->Elements + Offset;
        if (Bss->Encryption.ApRsn != NULL) {
            Offset = Bss->Encryption.ApRsn - Bss->Elements;
            BssCopy->Encryption.ApRsn = BssCopy->Elements + Offset;
        }
    }

    RtlCopyMemory(BssCopy->Passphrase, Bss->Passphrase, Bss->PassphraseLength);
    BssCopy->PassphraseLength = Bss->PassphraseLength;
    BssCopy->State.Rates.Rate = NULL;
    RtlZeroMemory(BssCopy->Encryption.Keys,
                  sizeof(PNET80211_KEY) * NET80211_MAX_KEY_COUNT);

    ASSERT(BssCopy->Encryption.StationRsn ==
           (PUCHAR)&Net80211DefaultRsnInformation);

    RatesSize = BssCopy->State.Rates.Count * sizeof(UCHAR);
    BssCopy->State.Rates.Rate = MmAllocatePagedPool(RatesSize,
                                                    NET80211_ALLOCATION_TAG);

    if (BssCopy->State.Rates.Rate == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CopyBssEntryEnd;
    }

    RtlCopyMemory(BssCopy->State.Rates.Rate, Bss->State.Rates.Rate, RatesSize);
    Status = STATUS_SUCCESS;

CopyBssEntryEnd:
    if (!KSUCCESS(Status)) {
        if (BssCopy != NULL) {
            Net80211pBssEntryReleaseReference(BssCopy);
            BssCopy = NULL;
        }
    }

    return BssCopy;
}

PNET80211_BSS_ENTRY
Net80211pCreateBssEntry (
    PUCHAR Bssid
    )

/*++

Routine Description:

    This routine creates a BSS entry.

Arguments:

    Bssid - Supplies a pointer to the BSSID.

Return Value:

    Returns a pointer to the newly allocated BSS entry on success or NULL on
    failure.

--*/

{

    PNET80211_BSS_ENTRY Bss;

    Bss = MmAllocatePagedPool(sizeof(NET80211_BSS_ENTRY),
                              NET80211_ALLOCATION_TAG);

    if (Bss == NULL) {
        goto CreateBssEntryEnd;
    }

    RtlZeroMemory(Bss, sizeof(NET80211_BSS_ENTRY));
    Bss->State.Version = NET80211_BSS_VERSION;
    Bss->ReferenceCount = 1;
    Bss->EapolHandle = INVALID_HANDLE;
    RtlCopyMemory(Bss->State.Bssid, Bssid, NET80211_ADDRESS_SIZE);

CreateBssEntryEnd:
    return Bss;
}

VOID
Net80211pDestroyBssEntry (
    PNET80211_BSS_ENTRY BssEntry
    )

/*++

Routine Description:

    This routine destroys the resources for the given BSS entry.

Arguments:

    BssEntry - Supplies a pointer to the BSS entry to destroy.

Return Value:

    None.

--*/

{

    ULONG Index;

    ASSERT((BssEntry->Encryption.StationRsn == NULL) ||
           (BssEntry->Encryption.StationRsn ==
            (PUCHAR)&Net80211DefaultRsnInformation));

    Net80211pDestroyEncryption(BssEntry);
    if (BssEntry->State.Rates.Rate != NULL) {
        MmFreePagedPool(BssEntry->State.Rates.Rate);
    }

    if (BssEntry->Elements != NULL) {
        MmFreePagedPool(BssEntry->Elements);
    }

    for (Index = 0; Index < NET80211_MAX_KEY_COUNT; Index += 1) {
        if (BssEntry->Encryption.Keys[Index] != NULL) {
            Net80211pDestroyKey(BssEntry->Encryption.Keys[Index]);
        }
    }

    MmFreePagedPool(BssEntry);
    return;
}

