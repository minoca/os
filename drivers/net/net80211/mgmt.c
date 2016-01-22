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
// Define the number of times to retry a scan before giving up.
//

#define NET80211_SCAN_RETRY_COUNT 5

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
    PUCHAR Channel;
    PUCHAR Ssid;
    PUCHAR Rates;
    PUCHAR ExtendedRates;
    PUCHAR Rsn;
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
    PNET_LINK Link,
    NET80211_STATE State
    );

VOID
Net80211pScanThread (
    PVOID Parameter
    );

KSTATUS
Net80211pSendProbeRequest (
    PNET_LINK Link,
    PNET80211_SCAN_STATE Scan
    );

VOID
Net80211pProcessProbeResponse (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendAuthenticationRequest (
    PNET_LINK Link
    );

VOID
Net80211pProcessAuthenticationResponse (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendAssociationRequest (
    PNET_LINK Link
    );

VOID
Net80211pProcessAssociationResponse (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pSendManagementFrame (
    PNET_LINK Link,
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
    PNET_LINK Link,
    PNET80211_PROBE_RESPONSE Response
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

    return;
}

KSTATUS
Net80211pStartScan (
    PNET_LINK Link,
    PNET80211_SCAN_STATE Parameters
    )

/*++

Routine Description:

    This routine starts a scan for one or more BSSs within range of this
    station.

Arguments:

    Link - Supplies a pointer to the link on which to perform the scan.

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
    NetLinkAddReference(Link);
    ScanState->Link = Link;

    //
    // Kick off a thread to complete the scan.
    //

    Net80211pSetState(Link, Net80211StateProbing);
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
            NetLinkReleaseReference(ScanState->Link);
            MmFreePagedPool(ScanState);
        }
    }

    return Status;
}

VOID
Net80211pSetState (
    PNET_LINK Link,
    NET80211_STATE State
    )

/*++

Routine Description:

    This routine sets the given link's 802.11 state by alerting the driver of
    the state change and then performing any necessary actions based on the
    state transition.

Arguments:

    Link - Supplies a pointer to the link whose state is being updated.

    State - Supplies the state to which the link is transitioning.

Return Value:

    None.

--*/

{

    PNET80211_LINK Net80211Link;

    Net80211Link = Link->DataLinkContext;
    KeAcquireQueuedLock(Net80211Link->Lock);
    Net80211pSetStateUnlocked(Link, State);
    KeReleaseQueuedLock(Net80211Link->Lock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Net80211pSetStateUnlocked (
    PNET_LINK Link,
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
    PVOID DriverContext;
    PNET80211_LINK Net80211Link;
    BOOL StartLink;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;

    ASSERT(KeIsQueuedLockHeld(Net80211Link->Lock) != FALSE);

    Bss = Net80211Link->ActiveBss;

    //
    // Notify the driver about the state transition first, allowing it to
    // prepare for the type of packets to be sent and received in the new state.
    //

    BssState = NULL;
    if (Bss != NULL) {
        BssState = &(Bss->State);
    }

    DriverContext = Net80211Link->Properties.DriverContext;
    Status = Net80211Link->Properties.Interface.SetState(DriverContext,
                                                         State,
                                                         BssState);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Failed to set state %d: 0x%08x\n",
                      State,
                      Status);

        goto SetStateEnd;
    }

    //
    // Officially update the state.
    //

    Net80211Link->State = State;

    //
    // Perform the necessary steps according to the state transition.
    //

    StartLink = FALSE;
    switch (State) {
    case Net80211StateAuthenticating:
        Status = Net80211pSendAuthenticationRequest(Link);
        if (!KSUCCESS(Status)) {
            goto SetStateEnd;
        }

        break;

    case Net80211StateAssociating:

        ASSERT(Bss != NULL);

        //
        // Initialize the encryption authentication process so that it is ready
        // to receive packets assuming association succeeds.
        //

        Status = Net80211pInitializeEncryption(Link);
        if (!KSUCCESS(Status)) {
            goto SetStateEnd;
        }

        //
        // Send out an association request.
        //

        Status = Net80211pSendAssociationRequest(Link);
        if (!KSUCCESS(Status)) {
            goto SetStateEnd;
        }

        break;

    //
    // In the associated state, if no advanced encryption is involved, the link
    // is ready to start transmitting and receiving data.
    //

    case Net80211StateAssociated:

        ASSERT(Bss != NULL);

        if ((Bss->Encryption.Pairwise == Net80211EncryptionNone) ||
            (Bss->Encryption.Pairwise == Net80211EncryptionWep)) {

            StartLink = TRUE;
        }

        break;

    //
    // If advanced encryption was involved, then the link is not ready until
    // the encrypted state is reached.
    //

    case Net80211StateEncrypted:

        ASSERT((Bss->Encryption.Pairwise == Net80211EncryptionWpaPsk) ||
               (Bss->Encryption.Pairwise == Net80211EncryptionWpa2Psk));

        Net80211pDestroyEncryption(Link);
        StartLink = TRUE;
        break;

    case Net80211StateInitialized:
        if (Bss != NULL) {
            Net80211pDestroyEncryption(Link);
            Net80211Link->ActiveBss = NULL;
            NetSetLinkState(Link, FALSE, 0);
        }

        break;

    default:
        break;
    }

    //
    // If requested, fire up the link and get traffic going in the upper layers.
    //

    if (StartLink != FALSE) {
        Status = NetStartLink(Link);
        if (!KSUCCESS(Status)) {
            goto SetStateEnd;
        }

        NetSetLinkState(Link, TRUE, Bss->State.MaxRate * NET80211_RATE_UNIT);
    }

SetStateEnd:
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

    PNET80211_BSS_ENTRY BssEntry;
    PLIST_ENTRY CurrentEntry;
    PNET80211_BSS_ENTRY FoundEntry;
    PNET_LINK Link;
    BOOL LockHeld;
    BOOL Match;
    ULONG MaxRssi;
    PNET80211_LINK Net80211Link;
    ULONG Retries;
    PNET80211_SCAN_STATE Scan;
    KSTATUS Status;

    LockHeld = FALSE;
    Scan = (PNET80211_SCAN_STATE)Parameter;
    Link = Scan->Link;
    Net80211Link = Link->DataLinkContext;
    Retries = 0;
    while (Retries < NET80211_SCAN_RETRY_COUNT) {

        //
        // Always start scanning on channel 1.
        //

        Scan->Channel = 1;

        //
        // Search for BSS entries on all channels.
        //

        Status = STATUS_UNSUCCESSFUL;
        FoundEntry = NULL;
        while (Scan->Channel < Net80211Link->Properties.MaxChannel) {

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
            // Wait the default dwell time before moving to the next channel.
            //

            KeDelayExecution(FALSE, FALSE, NET80211_DEFAULT_SCAN_DWELL_TIME);

            //
            // Now that the channel has been probed, search to see if the
            // targeted BSS is in range. This should only be done if a specific
            // BSSID is being probed.
            //

            if (((Scan->Flags & NET80211_SCAN_FLAG_BROADCAST) == 0) &&
                ((Scan->Flags & NET80211_SCAN_FLAG_JOIN) != 0)) {

                LockHeld = TRUE;
                KeAcquireQueuedLock(Net80211Link->Lock);
                CurrentEntry = Net80211Link->BssList.Next;
                while (CurrentEntry != &(Net80211Link->BssList)) {
                    BssEntry = LIST_VALUE(CurrentEntry,
                                          NET80211_BSS_ENTRY,
                                          ListEntry);

                    Match = RtlCompareMemory(Scan->Bssid,
                                             BssEntry->State.Bssid,
                                             NET80211_ADDRESS_SIZE);

                    if (Match != FALSE) {
                        FoundEntry = BssEntry;
                        break;
                    }

                    CurrentEntry = CurrentEntry->Next;
                }

                if (FoundEntry != NULL) {
                    Status = Net80211pValidateRates(Net80211Link, FoundEntry);
                    if (!KSUCCESS(Status)) {
                       goto ScanThreadEnd;
                    }

                    break;
                }

                KeReleaseQueuedLock(Net80211Link->Lock);
                LockHeld = FALSE;
            }

            Scan->Channel += 1;
        }

        //
        // If the scan completed and a join is required, then search for the
        // BSS with the most signal strength.
        //

        if (((Scan->Flags & NET80211_SCAN_FLAG_BROADCAST) != 0) &&
            ((Scan->Flags & NET80211_SCAN_FLAG_JOIN) != 0)) {

            ASSERT(Scan->SsidLength != 0);
            ASSERT(FoundEntry == NULL);

            MaxRssi = 0;
            LockHeld = TRUE;
            KeAcquireQueuedLock(Net80211Link->Lock);
            CurrentEntry = Net80211Link->BssList.Next;
            while (CurrentEntry != &(Net80211Link->BssList)) {
                BssEntry = LIST_VALUE(CurrentEntry,
                                      NET80211_BSS_ENTRY,
                                      ListEntry);

                CurrentEntry = CurrentEntry->Next;
                if (BssEntry->SsidLength != Scan->SsidLength) {
                    continue;
                }

                Match = RtlCompareMemory(BssEntry->Ssid,
                                         Scan->Ssid,
                                         Scan->SsidLength);

                if (Match == FALSE) {
                    continue;
                }

                //
                // Validate that the BSS and station agree on a basic rate set.
                // Also determine the mode at which it would connect.
                //

                Status = Net80211pValidateRates(Net80211Link, BssEntry);
                if (!KSUCCESS(Status)) {
                    continue;
                }

                if (BssEntry->State.Rssi >= MaxRssi) {
                    MaxRssi = BssEntry->State.Rssi;
                    FoundEntry = BssEntry;
                }
            }

            if (FoundEntry == NULL) {
                KeReleaseQueuedLock(Net80211Link->Lock);
                LockHeld = FALSE;
            }
        }

        //
        // If an entry was found, make it the active BSS and start the
        // authentication process.
        //

        if (FoundEntry != NULL) {

            ASSERT(KeIsQueuedLockHeld(Net80211Link->Lock) != FALSE);

            if (FoundEntry->Encryption.Pairwise != Net80211EncryptionNone) {
                if (Scan->PassphraseLength == 0) {
                    Status = STATUS_ACCESS_DENIED;
                    break;
                }

                RtlCopyMemory(FoundEntry->Passphrase,
                              Scan->Passphrase,
                              Scan->PassphraseLength);

                FoundEntry->PassphraseLength = Scan->PassphraseLength;
            }

            Net80211Link->ActiveBss = FoundEntry;
            Net80211pSetChannel(Link, FoundEntry->State.Channel);
            Net80211pSetStateUnlocked(Link, Net80211StateAuthenticating);
            Status = STATUS_SUCCESS;
            break;
        }

        Retries += 1;
    }

ScanThreadEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Net80211Link->Lock);
    }

    if (!KSUCCESS(Status)) {
        Net80211pSetState(Link, Net80211StateInitialized);
    }

    NetLinkReleaseReference(Link);
    MmFreePagedPool(Scan);
    return;
}

KSTATUS
Net80211pSendProbeRequest (
    PNET_LINK Link,
    PNET80211_SCAN_STATE Scan
    )

/*++

Routine Description:

    This routine sends an 802.11 management probe request frame based on the
    given scan state.

Arguments:

    Link - Supplies a pointer to the network link on which to send the probe
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
    PNET80211_LINK Net80211Link;
    PNET80211_RATE_INFORMATION Rates;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;
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

    Rates = Net80211Link->Properties.SupportedRates;
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
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 management probe response frame. It stores
    the information for the transmitting BSS in the BSS cache.

Arguments:

    Link - Supplies a pointer to the network link that received the probe
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
    PNET80211_LINK Net80211Link;
    ULONG Offset;
    NET80211_PROBE_RESPONSE Response;
    ULONG Subtype;

    Net80211Link = Link->DataLinkContext;
    if (Net80211Link->State != Net80211StateProbing) {
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
        if ((Response.Capabilities & NET80211_CAPABILITY_FLAG_PRIVACY) == 0) {
            RtlDebugPrint("802.11: Found RSN element in probe/beacon that does "
                          "not require privacy.\n");

            goto ProcessProbeResponseEnd;
        }

    } else {
        if ((Response.Capabilities & NET80211_CAPABILITY_FLAG_PRIVACY) != 0) {
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
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine sends an 802.11 management authentication frame to the AP of
    the link's active BSS.

Arguments:

    Link - Supplies a pointer to the network link on which to send an
        authentication request.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    NET80211_AUTHENTICATION_OPEN_BODY FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;

    ASSERT(KeIsQueuedLockHeld(Net80211Link->Lock) != FALSE);
    ASSERT(Net80211Link->ActiveBss != NULL);

    Bss = Net80211Link->ActiveBss;

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
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an authentication response frame. It is expected to
    be sent from the BSSID stored in the link's BSS context.

Arguments:

    Link - Supplies a pointer to the network link on which the authentication
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
    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    Net80211Link = Link->DataLinkContext;
    if (Net80211Link->State != Net80211StateAuthenticating) {
        return;
    }

    KeAcquireQueuedLock(Net80211Link->Lock);
    if (Net80211Link->State != Net80211StateAuthenticating) {
        goto ProcessAuthenticationResponseEnd;
    }

    ASSERT(Net80211Link->ActiveBss != NULL);

    Bss = Net80211Link->ActiveBss;

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

    KeReleaseQueuedLock(Net80211Link->Lock);
    return;
}

KSTATUS
Net80211pSendAssociationRequest (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine sends an 802.11 management association request frame to the
    to the AP of the link's active BSS.

Arguments:

    Link - Supplies a pointer to the network link over which to send the
        association request.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PUCHAR FrameBody;
    ULONG FrameBodySize;
    ULONG FrameSubtype;
    PUCHAR InformationByte;
    PNET80211_LINK Net80211Link;
    PNET80211_RATE_INFORMATION Rates;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;
    Bss = Net80211Link->ActiveBss;
    FrameBody = NULL;

    ASSERT(KeIsQueuedLockHeld(Net80211Link->Lock) != FALSE);
    ASSERT(Bss != NULL);
    ASSERT(Bss->SsidLength != 0);

    //
    // Determine the size of the probe response packet, which always includes
    // the capabilities, listen interval, SSID, and supported rates.
    //

    ASSERT(Bss->SsidLength <= NET80211_MAX_SSID_LENGTH);

    FrameBodySize = NET80211_CAPABILITY_SIZE + NET80211_LISTEN_INTERVAL_SIZE;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE + Bss->SsidLength;

    //
    // Get the supported rates size, including the extended rates if necessary.
    //

    Rates = Net80211Link->Properties.SupportedRates;
    FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    if (Rates->Count > NET80211_MAX_SUPPORTED_RATES) {
        FrameBodySize += NET80211_ELEMENT_HEADER_SIZE;
    }

    FrameBodySize += Rates->Count;

    //
    // Only include the RSN information if advanced encryption is required.
    //

    if ((Bss->Encryption.Pairwise != Net80211EncryptionNone) &&
        (Bss->Encryption.Pairwise != Net80211EncryptionWep)) {

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
    *((PUSHORT)InformationByte) = Net80211Link->Properties.Capabilities |
                                  NET80211_CAPABILITY_FLAG_ESS;

    InformationByte += NET80211_CAPABILITY_SIZE;

    //
    // TODO: Implement a non-zero 802.11 listen interval for power save mode.
    //

    *((PUSHORT)InformationByte) = 0;
    InformationByte += NET80211_LISTEN_INTERVAL_SIZE;
    *InformationByte = NET80211_ELEMENT_SSID;
    InformationByte += 1;
    *InformationByte = Bss->SsidLength;
    InformationByte += 1;
    RtlCopyMemory(InformationByte, Bss->Ssid, Bss->SsidLength);
    InformationByte += Bss->SsidLength;
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

    if ((Bss->Encryption.Pairwise != Net80211EncryptionNone) &&
        (Bss->Encryption.Pairwise != Net80211EncryptionWep)) {

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
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 management association response frame from
    an access point.

Arguments:

    Link - Supplies a pointer to the network link that received the association
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
    PNET80211_LINK Net80211Link;
    ULONG Offset;
    ULONG RateCount;
    PUCHAR Rates;
    KSTATUS Status;
    ULONG TotalRateCount;

    Status = STATUS_SUCCESS;
    Net80211Link = Link->DataLinkContext;
    if (Net80211Link->State != Net80211StateAssociating) {
        return;
    }

    KeAcquireQueuedLock(Net80211Link->Lock);
    if (Net80211Link->State != Net80211StateAssociating) {
        goto ProcessAssociationResponseEnd;
    }

    ASSERT(Net80211Link->ActiveBss != NULL);

    Bss = Net80211Link->ActiveBss;

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

    Status = Net80211pValidateRates(Net80211Link, Bss);
    if (!KSUCCESS(Status)) {
        goto ProcessAssociationResponseEnd;
    }

    Bss->State.AssociationId = AssociationId;
    Net80211pSetStateUnlocked(Link, Net80211StateAssociated);

ProcessAssociationResponseEnd:
    if (!KSUCCESS(Status)) {
        Net80211pSetStateUnlocked(Link, Net80211StateInitialized);
    }

    KeReleaseQueuedLock(Net80211Link->Lock);
    return;
}

KSTATUS
Net80211pSendManagementFrame (
    PNET_LINK Link,
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

    PVOID DriverContext;
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
    DriverContext = Link->Properties.DriverContext;
    Status = Link->Properties.Interface.Send(DriverContext, &PacketList);
    if (!KSUCCESS(Status)) {
        goto SendManagementFrameEnd;
    }

SendManagementFrameEnd:
    if (!KSUCCESS(Status)) {
        while (NET_PACKET_LIST_EMPTY(&PacketList) == FALSE) {
            Packet = LIST_VALUE(PacketList.Head.Next,
                                NET_PACKET_BUFFER,
                                ListEntry);

            NET_REMOVE_PACKET_FROM_LIST(Packet, &PacketList);
            NetFreeBuffer(Packet);
        }
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

    NET80211_ENCRYPTION_TYPE GroupEncryption;
    ULONG Index;
    ULONG Offset;
    NET80211_ENCRYPTION_TYPE PairwiseEncryption;
    USHORT PmkidCount;
    BOOL PskSupported;
    ULONG RsnLength;
    KSTATUS Status;
    ULONG Suite;
    USHORT SuiteCount;
    USHORT Version;

    ASSERT(NET80211_GET_ELEMENT_ID(Rsn) == NET80211_ELEMENT_RSN);

    Status = STATUS_SUCCESS;
    Offset = NET80211_ELEMENT_HEADER_SIZE;
    PairwiseEncryption = Net80211EncryptionNone;
    GroupEncryption = Net80211EncryptionNone;
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
    // Get the optional group suite. Do not support anything less secure than
    // CCMP (i.e. WPA2-PSK) as the older algorithms have proven to be insecure.
    //

    if ((Offset + sizeof(ULONG)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    Suite = NETWORK_TO_CPU32(*((PULONG)&(Rsn[Offset])));
    Offset += sizeof(ULONG);
    switch (Suite) {
    case NET80211_CIPHER_SUITE_CCMP:
        GroupEncryption = Net80211EncryptionWpa2Psk;
        break;

    default:
        RtlDebugPrint("802.11: Group cipher suite not supported 0x%08x\n",
                      Suite);

        break;
    }

    if (GroupEncryption == Net80211EncryptionNone) {
        Status = STATUS_NOT_SUPPORTED;
        goto ParseRsnElementEnd;
    }

    //
    // Gather the pairwise suites. The BSS must at least support CCMP (i.e.
    // WPA2-PSK), but may support others.
    //

    PairwiseEncryption = Net80211EncryptionNone;
    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    SuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    for (Index = 0; Index < SuiteCount; Index += 1) {
        if ((Offset + sizeof(ULONG)) > RsnLength) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ParseRsnElementEnd;
        }

        Suite = NETWORK_TO_CPU32(*((PULONG)&(Rsn[Offset])));
        Offset += sizeof(ULONG);
        switch (Suite) {
        case NET80211_CIPHER_SUITE_CCMP:
            PairwiseEncryption = Net80211EncryptionWpa2Psk;
            break;

        default:
            RtlDebugPrint("802.11: Pairwise cipher suite not supported "
                          "0x%08x\n",
                          Suite);

            break;
        }
    }

    if (PairwiseEncryption == Net80211EncryptionNone) {
        Status = STATUS_NOT_SUPPORTED;
        goto ParseRsnElementEnd;
    }

    //
    // The PSK authentication and key management (AKM) must be one of the
    // optional AKM suites.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    SuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    PskSupported = FALSE;
    for (Index = 0; Index < SuiteCount; Index += 1) {
        if ((Offset + sizeof(ULONG)) > RsnLength) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ParseRsnElementEnd;
        }

        Suite = *((PULONG)&(Rsn[Offset]));
        Offset += sizeof(ULONG);
        switch (NETWORK_TO_CPU32(Suite)) {
        case NET80211_AKM_SUITE_PSK:
            PskSupported = TRUE;
            break;

        default:
            RtlDebugPrint("802.11: AKM suite not supported 0x%08x\n", Suite);
            break;
        }
    }

    if (PskSupported == FALSE) {
        Status = STATUS_NOT_SUPPORTED;
        goto ParseRsnElementEnd;
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
    for (Index = 0; Index < PmkidCount; Index += 1) {
        if ((Offset + NET80211_RSN_PMKID_LENGTH) > RsnLength) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ParseRsnElementEnd;
        }

        Offset += NET80211_RSN_PMKID_LENGTH;
    }

    //
    // Skip the group management suite, but make sure that it is CCMP if it's
    // present.
    //

    if ((Offset + sizeof(ULONG)) > RsnLength) {
        goto ParseRsnElementEnd;
    }

    Suite = *((PULONG)&(Rsn[Offset]));
    Offset += sizeof(ULONG);
    switch (NETWORK_TO_CPU32(Suite)) {
    case NET80211_CIPHER_SUITE_CCMP:
        break;

    default:
        RtlDebugPrint("802.11: Group cipher suite not supported 0x%08x\n",
                      Suite);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto ParseRsnElementEnd;
    }

ParseRsnElementEnd:
    Encryption->Pairwise = PairwiseEncryption;
    Encryption->Group = GroupEncryption;
    return Status;
}

VOID
Net80211pUpdateBssCache (
    PNET_LINK Link,
    PNET80211_PROBE_RESPONSE Response
    )

/*++

Routine Description:

    This routine updates the given BSS cache based on the data found in a
    beacon or probe response packet.

Arguments:

    Link - Supplies a pointer to the network link that received the packet.

    Response - Supplies a pointer to a parsed representation of a beacon or
        probe response packet.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    ULONG Channel;
    PLIST_ENTRY CurrentEntry;
    BOOL LinkDown;
    BOOL Match;
    PNET80211_LINK Net80211Link;
    PUCHAR NewRsn;
    ULONG NewRsnLength;
    PUCHAR OldRsn;
    ULONG OldRsnLength;
    PUCHAR RatesArray;
    ULONG SsidLength;
    KSTATUS Status;
    ULONG TotalRateCount;

    Net80211Link = Link->DataLinkContext;
    TotalRateCount = NET80211_GET_ELEMENT_LENGTH(Response->Rates);
    if (Response->ExtendedRates != NULL) {
        TotalRateCount += NET80211_GET_ELEMENT_LENGTH(Response->ExtendedRates);
    }

    //
    // First look for an existing BSS entry based on the BSSID.
    //

    KeAcquireQueuedLock(Net80211Link->Lock);
    CurrentEntry = Net80211Link->BssList.Next;
    while (CurrentEntry != &(Net80211Link->BssList)) {
        Bss = LIST_VALUE(CurrentEntry, NET80211_BSS_ENTRY, ListEntry);
        Match = RtlCompareMemory(Response->Bssid,
                                 Bss->State.Bssid,
                                 NET80211_ADDRESS_SIZE);

        if (Match != FALSE) {
            break;
        }

        Bss = NULL;
    }

    //
    // If no matching BSS entry was found, then create a new one and insert it
    // into the list.
    //

    if (Bss == NULL) {
        Bss = MmAllocatePagedPool(sizeof(NET80211_BSS_ENTRY),
                                  NET80211_ALLOCATION_TAG);

        if (Bss == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto UpdateBssCacheEnd;
        }

        RtlZeroMemory(Bss, sizeof(NET80211_BSS_ENTRY));
        Bss->State.Version = NET80211_BSS_VERSION;
        RtlCopyMemory(Bss->State.Bssid, Response->Bssid, NET80211_ADDRESS_SIZE);
        Bss->EapolHandle = INVALID_HANDLE;
        INSERT_BEFORE(&(Bss->ListEntry), &(Net80211Link->BssList));
    }

    //
    // Gather some locals from the response elements.
    //

    Channel = *NET80211_GET_ELEMENT_DATA(Response->Channel);
    SsidLength = NET80211_GET_ELEMENT_LENGTH(Response->Ssid);
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

    ASSERT(SsidLength <= NET80211_MAX_SSID_LENGTH);

    //
    // If this is an update for the active BSS, then any changes will cause
    // the link to go down.
    //

    if (Net80211Link->ActiveBss == Bss) {
        LinkDown = FALSE;
        if ((Bss->State.BeaconInterval != Response->BeaconInterval) ||
            (Bss->State.Capabilities != Response->Capabilities) ||
            (Bss->State.Channel != Channel) ||
            (Bss->State.Rates.Count != TotalRateCount) ||
            (Bss->SsidLength != SsidLength) ||
            (OldRsnLength != NewRsnLength)) {

            LinkDown = TRUE;
        }

        if (LinkDown == FALSE) {
            Match = RtlCompareMemory(Bss->Ssid,
                                     NET80211_GET_ELEMENT_DATA(Response->Ssid),
                                     SsidLength);

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
            NetSetLinkState(Link, FALSE, 0);
        }
    }

    //
    // Update the BSS entry with the latest information from the AP.
    //

    Bss->State.BeaconInterval = Response->BeaconInterval;
    Bss->State.Capabilities = Response->Capabilities;
    Bss->State.Channel = Channel;
    Bss->State.Timestamp = Response->Timestamp;
    Bss->SsidLength = SsidLength;
    RtlCopyMemory(Bss->Ssid,
                  NET80211_GET_ELEMENT_DATA(Response->Ssid),
                  SsidLength);

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
    // Copy the RSN information into the BSS entry.
    //

    if (NewRsnLength != 0) {
        if (OldRsnLength < NewRsnLength) {
            if (OldRsn != NULL) {
                MmFreePagedPool(OldRsn);
                Bss->Encryption.ApRsn = NULL;
            }

            OldRsn = MmAllocatePagedPool(NewRsnLength, NET80211_ALLOCATION_TAG);
            if (OldRsn == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto UpdateBssCacheEnd;
            }

            Bss->Encryption.ApRsn = OldRsn;
        }

        RtlCopyMemory(OldRsn, NewRsn, NewRsnLength);

        //
        // Parse the RSN information to determine the encryption algorithms in
        // use by the BSS.
        //

        Status = Net80211pParseRsnElement(NewRsn, &(Bss->Encryption));
        if (!KSUCCESS(Status)) {
            goto UpdateBssCacheEnd;
        }

    } else if (OldRsnLength != 0) {

        ASSERT((OldRsn != NULL) && (OldRsn == Bss->Encryption.ApRsn));

        MmFreePagedPool(OldRsn);
        Bss->Encryption.ApRsn = NULL;
    }

    //
    // For now, the station always advertises the same RSN information. Just
    // point at the global.
    //

    Bss->Encryption.StationRsn = (PUCHAR)&Net80211DefaultRsnInformation;

UpdateBssCacheEnd:
    if (!KSUCCESS(Status)) {
        if (Bss != NULL) {
            LIST_REMOVE(&(Bss->ListEntry));
            Net80211pDestroyBssEntry(Bss);
        }
    }

    KeReleaseQueuedLock(Net80211Link->Lock);
    return;
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

    ASSERT(BssEntry->EapolHandle == INVALID_HANDLE);

    if (BssEntry->State.Rates.Rate != NULL) {
        MmFreePagedPool(BssEntry->State.Rates.Rate);
    }

    if (BssEntry->Encryption.ApRsn != NULL) {
        MmFreePagedPool(BssEntry->Encryption.ApRsn);
    }

    MmFreePagedPool(BssEntry);
    return;
}

