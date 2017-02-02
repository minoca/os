/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    eapol.c

Abstract:

    This module implements support for the Extensible Authentication Protocol
    over LAN, which an authentication procedure for joining a LAN or WLAN.

Author:

    Chris Stevens 29-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (i.e. be
// able to be implemented outside the core net library). For the builtin ones,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET80211_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/net80211.h>
#include <minoca/lib/crypto.h>
#include "eapol.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros return the KCK, KEK, and TK from the given PTK.
//

#define EAPOL_PTK_GET_KCK(_Ptk) &((_Ptk)[0])
#define EAPOL_PTK_GET_KEK(_Ptk) &((_Ptk)[EAPOL_KCK_SIZE])
#define EAPOL_PTK_GET_TK(_Ptk) &((_Ptk)[EAPOL_KCK_SIZE + EAPOL_KEK_SIZE])

//
// This macro returns the TK from the given GTK.
//

#define EAPOL_GTK_GET_TK(_Gtk) &((_Gtk)[0])

//
// ---------------------------------------------------------------- Definitions
//

#define EAPOL_ALLOCATION_TAG 0x21706145 // '!paE'

//
// Define the current EAPOL protocol version.
//

#define EAPOL_PROTOCOL_VERSION 2

//
// Define the EAPOL packet types.
//

#define EAPOL_PACKET_TYPE_KEY_FRAME 3

//
// Define the current EAPOL key frame version.
//

#define EAPOL_KEY_FRAME_VERSION 1

//
// Define the EAPOL key frame descriptor types.
//

#define EAPOL_KEY_DESCRIPTOR_TYPE_RSN 2

//
// Define the bits in the EAPOL key information field.
//

#define EAPOL_KEY_INFORMATION_SMK_MESSAGE        0x2000
#define EAPOL_KEY_INFORMATION_ENCRYPTED_KEY_DATA 0x1000
#define EAPOL_KEY_INFORMATION_REQUEST            0x0800
#define EAPOL_KEY_INFORMATION_ERROR              0x0400
#define EAPOL_KEY_INFORMATION_SECURE             0x0200
#define EAPOL_KEY_INFORMATION_MIC_PRESENT        0x0100
#define EAPOL_KEY_INFORMATION_ACK_REQUIRED       0x0080
#define EAPOL_KEY_INFORMATION_INSTALL            0x0040
#define EAPOL_KEY_INFORMATION_TYPE_MASK          0x0008
#define EAPOL_KEY_INFORMATION_TYPE_SHIFT         3
#define EAPOL_KEY_INFORMATION_GROUP              0
#define EAPOL_KEY_INFORMATION_PAIRWISE           1
#define EAPOL_KEY_INFORMATION_VERSION_MASK       0x0007
#define EAPOL_KEY_INFORMATION_VERSION_SHIFT      0

//
// Define the key information versions.
//

#define EAPOL_KEY_VERSION_ARC4_HMAC_MD5 1
#define EAPOL_KEY_VERSION_NIST_AES_HMAC_SHA1_128 2
#define EAPOL_KEY_VERSION_NIST_AES_AES_128_CMAC 3

//
// Define the mask and values for various message types.
//

#define EAPOL_KEY_INFORMATION_MESSAGE_MASK 0x3FC8
#define EAPOL_KEY_INFORMATION_MESSAGE_1    0x0088
#define EAPOL_KEY_INFORMATION_MESSAGE_2    0x0108
#define EAPOL_KEY_INFORMATION_MESSAGE_3    0x13C8
#define EAPOL_KEY_INFORMATION_MESSAGE_4    0x0308

//
// Define the length of the global key counter, in bytes.
//

#define EAPOL_GLOBAL_KEY_COUNTER_SIZE (256 / BITS_PER_BYTE)

//
// Define the size of a nonce, in bytes.
//

#define EAPOL_NONCE_SIZE 32

//
// Define the key IV size, in bytes.
//

#define EAPOL_KEY_IV_SIZE 16

//
// Define the RSC size, in bytes.
//

#define EAPOL_RSC_SIZE 8

//
// Define the default key MIC size, in bytes. This depends on the AKM being
// used, but all AKMs currently have the same MIC size.
//

#define EAPOL_DEFAULT_KEY_MIC_SIZE 16

//
// Define the size of the pairwise master key (PMK), in bytes.
//

#define EAPOL_PMK_SIZE (256 / BITS_PER_BYTE)

//
// Define the size of the key confirmation key (KCK), in bytes.
//

#define EAPOL_KCK_SIZE (128 / BITS_PER_BYTE)

//
// Define the size of the key encryption key (KEK), in bytes.
//

#define EAPOL_KEK_SIZE (128 / BITS_PER_BYTE)

//
// Define the size of the CCMP temporal key (TK), in bytes.
//

#define EAPOL_CCMP_TK_SIZE (128 / BITS_PER_BYTE)

//
// Define the size, in bytes, of the random number used to seed the global key
// counter.
//

#define EAPOL_RANDOM_NUMBER_SIZE (256 / BITS_PER_BYTE)

//
// Define the expected key data encapsulation (KDE) type.
//

#define EAPOL_KDE_TYPE 0xDD

//
// Define the EAPOL KDE selectors (OUI + data type).
//

#define EAPOL_KDE_SELECTOR_GTK      0x000FAC01
#define EAPOL_KDE_SELECTOR_MAC      0x000FAC03
#define EAPOL_KDE_SELECTOR_PMKID    0x000FAC04
#define EAPOL_KDE_SELECTOR_SMK      0x000FAC05
#define EAPOL_KDE_SELECTOR_NONCE    0x000FAC06
#define EAPOL_KDE_SELECTOR_LIFETIME 0x000FAC07
#define EAPOL_KDE_SELECTOR_ERROR    0x000FAC08
#define EAPOL_KDE_SELECTOR_IGTK     0x000FAC09
#define EAPOL_KDE_SELECTOR_KEY_ID   0x000FAC0A

//
// Define the bits for the KDE GTK entry flags.
//

#define EAPOL_KDE_GTK_FLAG_TRANSMIT     0x04
#define EAPOL_KDE_GTK_FLAG_KEY_ID_MASK  0x03
#define EAPOL_KDE_GTK_FLAG_KEY_ID_SHIFT 0

//
// Define the recommended application text to use when generating the global
// key counter.
//

#define EAPOL_GLOBAL_KEY_COUNTER_APPLICATION_TEXT "Init Counter"
#define EAPOL_GLOBAL_KEY_COUNTER_APPLICATION_TEXT_LENGTH \
    RtlStringLength(EAPOL_GLOBAL_KEY_COUNTER_APPLICATION_TEXT)

//
// Define the required application text to use when generating the pairwise
// transient key.
//

#define EAPOL_PTK_APPLICATION_TEXT "Pairwise key expansion"
#define EAPOL_PTK_APPLICATION_TEXT_LENGTH \
    RtlStringLength(EAPOL_PTK_APPLICATION_TEXT)

//
// Define the initial value for the NIST AES key wrap algorithm.
//

#define EAPOL_NIST_AES_KEY_WRAP_INITIAL_VALUE 0xA6A6A6A6A6A6A6A6

//
// Define the number of steps to perform in the NIST AES key wrap algorithm.
//

#define EAPOL_NIST_AES_KEY_WRAP_STEP_COUNT 6

//
// Define the minimum allowed length of the key data before encryption.
//

#define EAPOL_NIST_AES_MIN_KEY_DATA_LENGTH 16

//
// Define the key data alignment required for NIST AES key wrap encryption.
//

#define EAPOL_NIST_AES_KEY_DATA_ALIGNMENT 8

//
// Define the first padding byte used to align key data for NIST AES key wrap
// encryption.
//

#define EAPOL_NIST_AES_KEY_DATA_PADDING_BYTE 0xDD

//
// Define the size difference between the NIST AES key wrap plaintext and
// cipher text.
//

#define EAPOL_NIST_AES_KEY_DATA_CIPHERTEXT_LENGTH_DELTA 8

//
// Define the size of the data buffer used to generate a PTK using the PRF
// algorithm.
//

#define EAPOL_PTK_DATA_SIZE \
    (NET80211_ADDRESS_SIZE * 2) + (EAPOL_NONCE_SIZE * 2)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EAPOL_MESSAGE_TYPE {
    EapolMessageType1,
    EapolMessageType2,
    EapolMessageType3,
    EapolMessageType4
} EAPOL_MESSAGE_TYPE, *PEAPOL_MESSAGE_TYPE;

/*++

Structure Description:

    This structure defines an EAPOL packet heeader.

Members:

    ProtocolVersion - Stores the current protocol version for the packet.
        Should be set to EAPOL_PROTOCOL_VERSION.

    Type - Stores the packet type. See EAPOL_PACKET_TYPE_*.

    BodyLength - Stores the length of the packet body, in bytes. This does not
        include the length of the packet header.

--*/

typedef struct _EAPOL_PACKET_HEADER {
    UCHAR ProtocolVersion;
    UCHAR Type;
    USHORT BodyLength;
} PACKED EAPOL_PACKET_HEADER, *PEAPOL_PACKET_HEADER;

/*++

Structure Description:

    This structure defines an EAPOL key frame packet. The optional key data
    immediately follows this structure.

Members:

    PacketHeader - Stores the standard EAPOL packet header.

    DescriptorType - Stores the key frame type. See EAPOL_KEY_DESCRIPTOR_TYPE_*
        for definitions.

    KeyInformation - Stores a bitmask of key information flags. See
        EAPOL_KEY_INFORMATION_* for definitions.

    KeyLength - Stores the length of the pairwise temporal key, in bytes.
        Together with the KCK and KEK, the temporal key helps to make up the
        PTK.

    KeyReplayCounter - Stores a sequence number used by the EAPOL protocol to
        detect replayed key frames.

    KeyNonce - Stores an optional nonce value.

    KeyIv - Stores an optional IV to use with the KEK. If used, it is
        initialized using the global key counter.

    KeyRsc - Stores the receive sequence counter (RSC) for the GTK.

    Reserved - Stores a received value.

    KeyMic - Stores an option message integrity check (MIC) of the key frame
        calculated with the key MIC field initialized to 0. The 802.11
        specification indicates that the size of the MIC depends on the
        negotiated AKM, but all known AKMs use a 16-byte MIC.

    KeyDataLength - Stores the length of the optional key data.

--*/

typedef struct _EAPOL_KEY_FRAME {
    EAPOL_PACKET_HEADER PacketHeader;
    UCHAR DescriptorType;
    USHORT KeyInformation;
    USHORT KeyLength;
    ULONGLONG KeyReplayCounter;
    UCHAR KeyNonce[EAPOL_NONCE_SIZE];
    UCHAR KeyIv[EAPOL_KEY_IV_SIZE];
    UCHAR KeyRsc[EAPOL_RSC_SIZE];
    UCHAR Reserved[8];
    UCHAR KeyMic[EAPOL_DEFAULT_KEY_MIC_SIZE];
    USHORT KeyDataLength;
} PACKED EAPOL_KEY_FRAME, *PEAPOL_KEY_FRAME;

/*++

Structure Description:

    This structure define the EAPOL key data encapsulation. The data
    immediately follows this structure.

Members:

    Type - Stores the KDE type. Should be set to EAPOL_KDE_TYPE.

    Length - Stores the length of the data, in bytes. This includes the OUI,
        data type, and data, but not the KDE type and length fields.

    OuiDataType - Stores the combined OUI and data type information. See
        EAPOL_KDE_SELECTOR_* for definitions.

--*/

typedef struct _EAPOL_KDE {
    UCHAR Type;
    UCHAR Length;
    ULONG OuiDataType;
} PACKED EAPOL_KDE, *PEAPOL_KDE;

/*++

Structure Description:

    This structure defines a KDE GTK entry.

Members:

    Flags - Stores a bitmask of flags describing the global transient key.

    Reserved - Stores a reserved byte.

    Gtk - Stores the global transient key.

--*/

typedef struct _EAPOL_KDE_GTK {
    UCHAR Flags;
    UCHAR Reserved;
    UCHAR Gtk[ANYSIZE_ARRAY];
} PACKED EAPOL_KDE_GTK, *PEAPOL_KDE_GTK;

/*++

Structure Description:

    This structure defines the information needed for the two nodes
    participating in an EAPOL exchange.

Members:

    Address - Stores the physical address of the node.

    Rsn - Stores the robust security network (RSN) information for the node.
        For the local station, this must match the data sent by the association
        request. For the remote AP, this must match the data received by the
        beacon or probe response.

    RsnSize - Stores the size of the RSN information, in bytes.

    Nonce - Store the nonce value for the node.

--*/

typedef struct _EAPOL_NODE {
    NETWORK_ADDRESS Address;
    PUCHAR Rsn;
    ULONG RsnSize;
    UCHAR Nonce[EAPOL_NONCE_SIZE];
} EAPOL_NODE, *PEAPOL_NODE;

/*++

Structure Description:

    This structure defines the context of an EAPOL instance.

Members:

    TreeEntry - Stores the red black tree information for this node.

    Mode - Stores the mode for this EAPOL instance.

    ReferenceCount - Stores the reference count of the EAPOL link context.

    NetworkLink - Stores a pointer to the network link associated with this
        EAPOL entry.

    Net80211Link - Stores a pointer to the 802.11 link associated with this
        EAPOL entry.

    Lock - Stores a pointer to a queued lock that protects access to the global
        key counter.

    CompletionRoutine - Stores a pointer to the completion routine.

    CompletionContext - Stores a pointer to the completion context.

    Supplicant - Stores all the node-specific information for the supplicant
        node.

    Authenticator - Stores all the node-speficic information for the
        authenticating node.

    GlobalKeyCounter - Stores the 256-bit global key counter for the link.

    Pmk - Stores the pairwise master key for the link.

    Ptk - Stores the pairwise transient key for the link.

    PtkSize - Stores the size of the PTK, in bytes.

    Gtk - Stores the group temporal key.

    GtkFlags - Stores a bitmask of flags for the GTK. See EAPOL_KDE_GTK_FLAG_*
        for definitions.

    GtkSize - Stores the size of the GTK, in bytes.

    TemporalKeySize - Stores the size of the temporal key, in bytes.

    KeyReplayCounterValid - Stores a boolean indicating whether or not the key
        replay counter is valid. It is not valid on a supplicant until the
        first valid key frame (with a MIC) is received from the authenticator.

    KeyReplayCounter - Stores the next expected key replay counter.

    KeyVersion - Stores the key version indicated by the authenticator in
        message 1.

--*/

typedef struct _EAPOL_CONTEXT {
    RED_BLACK_TREE_NODE TreeEntry;
    EAPOL_MODE Mode;
    volatile ULONG ReferenceCount;
    PNET_LINK NetworkLink;
    PNET80211_LINK Net80211Link;
    PQUEUED_LOCK Lock;
    PEAPOL_COMPLETION_ROUTINE CompletionRoutine;
    PVOID CompletionContext;
    EAPOL_NODE Supplicant;
    EAPOL_NODE Authenticator;
    UCHAR GlobalKeyCounter[EAPOL_GLOBAL_KEY_COUNTER_SIZE];
    UCHAR Pmk[EAPOL_PMK_SIZE];
    PUCHAR Ptk;
    ULONG PtkSize;
    PUCHAR Gtk;
    ULONG GtkFlags;
    ULONG GtkSize;
    ULONG TemporalKeySize;
    BOOL KeyReplayCounterValid;
    ULONGLONG KeyReplayCounter;
    ULONG KeyVersion;
} EAPOL_CONTEXT, *PEAPOL_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Net80211pEapolInitializeLink (
    PNET_LINK Link
    );

VOID
Net80211pEapolDestroyLink (
    PNET_LINK Link
    );

VOID
Net80211pEapolProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

ULONG
Net80211pEapolPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

VOID
Net80211pEapolSupplicantReceiveMessage (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    );

KSTATUS
Net80211pEapolSupplicantSendMessage (
    PEAPOL_CONTEXT Context,
    EAPOL_MESSAGE_TYPE Type,
    ULONGLONG KeyReplayCounter
    );

VOID
Net80211pEapolReadGlobalKeyCounter (
    PEAPOL_CONTEXT Context,
    PUCHAR Buffer,
    ULONG BufferLength
    );

KSTATUS
Net80211pEapolConvertPassphraseToPsk (
    PUCHAR Passphrase,
    ULONG PassphraseLength,
    PUCHAR Ssid,
    ULONG SsidLength,
    PUCHAR Psk,
    ULONG PskLength
    );

KSTATUS
Net80211pGeneratePtk (
    PEAPOL_CONTEXT Context,
    ULONG TemporalKeyLength
    );

KSTATUS
Net80211pEapolEncryptKeyData (
    PEAPOL_CONTEXT Context,
    PUCHAR KeyData,
    ULONG KeyDataLength,
    PUCHAR *EncryptedKeyData,
    PULONG EncryptedKeyDataLength
    );

KSTATUS
Net80211pEapolDecryptKeyData (
    PEAPOL_CONTEXT Context,
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength,
    PUCHAR *KeyData,
    PULONG KeyDataLength
    );

VOID
Net80211pEapolComputeMic (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    );

BOOL
Net80211pEapolValidateMic (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    );

VOID
Net80211pEapolNistAesKeyWrap (
    PUCHAR KeyData,
    ULONG KeyDataLength,
    PUCHAR Key,
    ULONG KeyLength,
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength
    );

KSTATUS
Net80211pEapolNistAesKeyUnwrap (
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength,
    PUCHAR Key,
    ULONG KeyLength,
    PUCHAR KeyData,
    ULONG KeyDataLength
    );

KSTATUS
Net80211pEapolPseudoRandomFunction (
    PUCHAR Key,
    ULONG KeyLength,
    PSTR ApplicationText,
    ULONG ApplicationTextLength,
    PUCHAR Data,
    ULONG DataLength,
    PUCHAR Output,
    ULONG OutputLength
    );

COMPARISON_RESULT
Net80211pEapolCompareMemory (
    PUCHAR FirstAddress,
    PUCHAR SecondAddress,
    ULONG AddressSize
    );

COMPARISON_RESULT
Net80211pEapolCompareContexts (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
Net80211pEapolContextAddReference (
    PEAPOL_CONTEXT Context
    );

VOID
Net80211pEapolContextReleaseReference (
    PEAPOL_CONTEXT Context
    );

VOID
Net80211pEapolDestroyContext (
    PEAPOL_CONTEXT Context
    );

VOID
Net80211pEapolCompleteInstance (
    PEAPOL_CONTEXT Context,
    KSTATUS CompletionStatus
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL Net80211EapolDebug;

//
// Store a global tree of all the contexts that are actively looking for EAPOL
// frames.
//

RED_BLACK_TREE Net80211EapolTree;
PQUEUED_LOCK Net80211EapolTreeLock;

//
// Store the handle to the registered EAPOL network layer.
//

HANDLE Net80211EapolNetworkHandle = INVALID_HANDLE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Net80211pEapolInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for EAPOL packets.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    NET_NETWORK_ENTRY NetworkEntry;
    KSTATUS Status;

    if (Net80211EapolDebug == FALSE) {
        Net80211EapolDebug = NetGetGlobalDebugFlag();
    }

    RtlRedBlackTreeInitialize(&Net80211EapolTree,
                              0,
                              Net80211pEapolCompareContexts);

    Net80211EapolTreeLock = KeCreateQueuedLock();
    if (Net80211EapolTreeLock == FALSE) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EapolInitializeEnd;
    }

    //
    // Register the EAPOL handlers with the core networking library.
    //

    RtlZeroMemory(&NetworkEntry, sizeof(NET_NETWORK_ENTRY));
    NetworkEntry.Domain = NetDomainEapol;
    NetworkEntry.ParentProtocolNumber = EAPOL_PROTOCOL_NUMBER;
    NetworkEntry.Interface.InitializeLink = Net80211pEapolInitializeLink;
    NetworkEntry.Interface.DestroyLink = Net80211pEapolDestroyLink;
    NetworkEntry.Interface.ProcessReceivedData =
                                             Net80211pEapolProcessReceivedData;

    NetworkEntry.Interface.PrintAddress = Net80211pEapolPrintAddress;
    Status = NetRegisterNetworkLayer(&NetworkEntry,
                                     &Net80211EapolNetworkHandle);

    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        goto EapolInitializeEnd;
    }

EapolInitializeEnd:
    return Status;
}

VOID
Net80211pEapolDestroy (
    VOID
    )

/*++

Routine Description:

    This routine tears down support for EAPOL packets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ASSERT(RED_BLACK_TREE_EMPTY(&Net80211EapolTree) != FALSE);

    if (Net80211EapolTreeLock != NULL) {
        KeDestroyQueuedLock(Net80211EapolTreeLock);
        Net80211EapolTreeLock = NULL;
    }

    if (Net80211EapolNetworkHandle != INVALID_HANDLE) {
        NetUnregisterNetworkLayer(Net80211EapolNetworkHandle);
    }

    return;
}

KSTATUS
Net80211pEapolCreateInstance (
    PEAPOL_CREATION_PARAMETERS Parameters,
    PHANDLE EapolHandle
    )

/*++

Routine Description:

    This routine creates an EAPOL instance through which a session's private
    key will be derived. The caller can indicate if it intends to be the
    supplicant or the authenticator in the parameters.

Arguments:

    Parameters - Supplies a pointer to the EAPOL instance creation parameters.

    EapolHandle - Supplies a pointer that receives a handle to the created
        EAPOL instance.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PEAPOL_CONTEXT Context;
    UCHAR Data[NET80211_ADDRESS_SIZE + sizeof(SYSTEM_TIME)];
    UCHAR RandomNumber[EAPOL_RANDOM_NUMBER_SIZE];
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    Context = NULL;
    *EapolHandle = INVALID_HANDLE;

    //
    // Check for valid parameters.
    //

    if ((Parameters->NetworkLink == NULL) ||
        (Parameters->Net80211Link == NULL) ||
        (Parameters->SupplicantAddress == NULL) ||
        (Parameters->AuthenticatorAddress == NULL) ||
        (Parameters->Ssid == NULL) ||
        (Parameters->SsidLength <= 1) ||
        (Parameters->Passphrase == NULL) ||
        (Parameters->PassphraseLength == 0) ||
        (Parameters->SupplicantRsn == NULL) ||
        (Parameters->SupplicantRsnSize == 0) ||
        (Parameters->AuthenticatorRsn == NULL) ||
        (Parameters->AuthenticatorRsnSize == 0) ||
        (Parameters->CompletionRoutine == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateEnd;
    }

    //
    // Allocate a context for this EAPOL instance.
    //

    AllocationSize = sizeof(EAPOL_CONTEXT) +
                     Parameters->SupplicantRsnSize +
                     Parameters->AuthenticatorRsnSize;

    Context = MmAllocatePagedPool(AllocationSize, EAPOL_ALLOCATION_TAG);
    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEnd;
    }

    RtlZeroMemory(Context, sizeof(EAPOL_CONTEXT));
    NetLinkAddReference(Parameters->NetworkLink);
    Net80211LinkAddReference(Parameters->Net80211Link);
    Context->ReferenceCount = 1;
    Context->Mode = Parameters->Mode;
    Context->NetworkLink = Parameters->NetworkLink;
    Context->Net80211Link = Parameters->Net80211Link;
    Context->CompletionRoutine = Parameters->CompletionRoutine;
    Context->CompletionContext = Parameters->CompletionContext;
    Context->Lock = KeCreateQueuedLock();
    if (Context->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEnd;
    }

    //
    // Copy both the supplicant and authenticator addresses to the context.
    //

    RtlCopyMemory(&(Context->Supplicant.Address),
                  Parameters->SupplicantAddress,
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Context->Authenticator.Address),
                  Parameters->AuthenticatorAddress,
                  sizeof(NETWORK_ADDRESS));

    //
    // Copy the RSN information for the supplicant and authenticator to the end
    // of the context.
    //

    Context->Supplicant.Rsn = (PUCHAR)(Context + 1);
    RtlCopyMemory(Context->Supplicant.Rsn,
                  Parameters->SupplicantRsn,
                  Parameters->SupplicantRsnSize);

    Context->Supplicant.RsnSize = Parameters->SupplicantRsnSize;
    Context->Authenticator.Rsn = Context->Supplicant.Rsn +
                                 Context->Supplicant.RsnSize;

    RtlCopyMemory(Context->Authenticator.Rsn,
                  Parameters->AuthenticatorRsn,
                  Parameters->AuthenticatorRsnSize);

    Context->Authenticator.RsnSize = Parameters->AuthenticatorRsnSize;

    //
    // Concatenate the local MAC address with the current time to use as the
    // data portion for global key counter generation.
    //

    if (Context->Mode == EapolModeSupplicant) {

        ASSERT(Context->Supplicant.Address.Domain == NetDomain80211);

        RtlCopyMemory(Data,
                      Context->Supplicant.Address.Address,
                      NET80211_ADDRESS_SIZE);

        Context->KeyReplayCounterValid = FALSE;

    } else {

        ASSERT(Context->Mode == EapolModeAuthenticator);
        ASSERT(Context->Authenticator.Address.Domain == NetDomain80211);

        RtlCopyMemory(Data,
                      Context->Authenticator.Address.Address,
                      NET80211_ADDRESS_SIZE);

        Context->KeyReplayCounterValid = TRUE;
    }

    KeGetSystemTime(&SystemTime);
    RtlCopyMemory(&(Data[NET80211_ADDRESS_SIZE]),
                  &SystemTime,
                  sizeof(SYSTEM_TIME));

    //
    // Generate a random number to use as the key for global key counter
    // generation.
    //

    Status = KeGetRandomBytes(RandomNumber, EAPOL_RANDOM_NUMBER_SIZE);
    if (!KSUCCESS(Status)) {
        goto CreateEnd;
    }

    //
    // Initialize the global key counter for this link.
    //

    Status = Net80211pEapolPseudoRandomFunction(
                              RandomNumber,
                              EAPOL_RANDOM_NUMBER_SIZE,
                              EAPOL_GLOBAL_KEY_COUNTER_APPLICATION_TEXT,
                              EAPOL_GLOBAL_KEY_COUNTER_APPLICATION_TEXT_LENGTH,
                              Data,
                              NET80211_ADDRESS_SIZE + sizeof(SYSTEM_TIME),
                              Context->GlobalKeyCounter,
                              EAPOL_GLOBAL_KEY_COUNTER_SIZE);

    if (!KSUCCESS(Status)) {
        goto CreateEnd;
    }

    //
    // Generate a nonce for the supplicant.
    //

    Net80211pEapolReadGlobalKeyCounter(Context,
                                       Context->Supplicant.Nonce,
                                       EAPOL_NONCE_SIZE);

    //
    // If the passphrase is less than the size of the PMK, then it needs to be
    // converted into the PMK, which is the PSK in this case.
    //

    if (Parameters->PassphraseLength < EAPOL_PMK_SIZE) {
        Status = Net80211pEapolConvertPassphraseToPsk(
                                                  Parameters->Passphrase,
                                                  Parameters->PassphraseLength,
                                                  Parameters->Ssid,
                                                  Parameters->SsidLength,
                                                  Context->Pmk,
                                                  EAPOL_PMK_SIZE);

        if (!KSUCCESS(Status)) {
            goto CreateEnd;
        }

    //
    // Otherwise the given passphrase is the PMK.
    //

    } else {
        RtlCopyMemory(Context->Pmk, Parameters->Passphrase, EAPOL_PMK_SIZE);
    }

    //
    // Insert the EAPOL context into the global tree.
    //

    KeAcquireQueuedLock(Net80211EapolTreeLock);
    RtlRedBlackTreeInsert(&Net80211EapolTree, &(Context->TreeEntry));
    KeReleaseQueuedLock(Net80211EapolTreeLock);
    Status = STATUS_SUCCESS;
    *EapolHandle = Context;

CreateEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            Net80211pEapolContextReleaseReference(Context);
            Context = NULL;
        }
    }

    return Status;
}

VOID
Net80211pEapolDestroyInstance (
    HANDLE EapolHandle
    )

/*++

Routine Description:

    This routine destroys the given EAPOL instance.

Arguments:

    EapolHandle - Supplies the handle to the EAPOL instance to destroy.

Return Value:

    None.

--*/

{

    PEAPOL_CONTEXT Context;

    Context = (PEAPOL_CONTEXT)EapolHandle;

    //
    // Remove the instance's context from the global tree so that it can no
    // longer process packets.
    //

    KeAcquireQueuedLock(Net80211EapolTreeLock);
    if (Context->TreeEntry.Parent != NULL) {
        RtlRedBlackTreeRemove(&Net80211EapolTree, &(Context->TreeEntry));
        Context->TreeEntry.Parent = NULL;
    }

    KeReleaseQueuedLock(Net80211EapolTreeLock);
    Net80211pEapolContextReleaseReference(Context);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Net80211pEapolInitializeLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
Net80211pEapolDestroyLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine allows the network layer to tear down any state before a link
    is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

{

    return;
}

VOID
Net80211pEapolProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link and packet information.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PEAPOL_CONTEXT Context;
    PRED_BLACK_TREE_NODE FoundNode;
    PEAPOL_KEY_FRAME KeyFrame;
    PNET_LINK Link;
    PNET_PACKET_BUFFER Packet;
    USHORT PacketBodyLength;
    EAPOL_CONTEXT SearchEntry;

    //
    // Lookup to see if this is link is registered for an authentication
    // sequence. Take a reference on a found context while holding the lock to
    // guarantee it does not get destroyed while processing the packet.
    //

    Context = NULL;
    Link = ReceiveContext->Link;
    Packet = ReceiveContext->Packet;
    SearchEntry.NetworkLink = Link;
    KeAcquireQueuedLock(Net80211EapolTreeLock);
    FoundNode = RtlRedBlackTreeSearch(&Net80211EapolTree,
                                      &(SearchEntry.TreeEntry));

    if (FoundNode != NULL) {
        Context = RED_BLACK_TREE_VALUE(FoundNode, EAPOL_CONTEXT, TreeEntry);
        Net80211pEapolContextAddReference(Context);
    }

    KeReleaseQueuedLock(Net80211EapolTreeLock);

    //
    // If no context was found, drop the packet.
    //

    if (Context == NULL) {
        if (Net80211EapolDebug != FALSE) {
            RtlDebugPrint("EAPOL: Failed to find entry for link 0x%08x. "
                          "Dropping packet.\n",
                          Link);
        }

        goto ProcessReceivedDataEnd;
    }

    //
    // Validate the packet header.
    //

    KeyFrame = Packet->Buffer + Packet->DataOffset;
    if (KeyFrame->PacketHeader.ProtocolVersion > EAPOL_PROTOCOL_VERSION) {
        RtlDebugPrint("EAPOL: Version mismatch. Received %d, expected %d.\n",
                      KeyFrame->PacketHeader.ProtocolVersion,
                      EAPOL_PROTOCOL_VERSION);

        goto ProcessReceivedDataEnd;
    }

    if (KeyFrame->PacketHeader.Type != EAPOL_PACKET_TYPE_KEY_FRAME) {
        RtlDebugPrint("EAPOL: Unexpected EAPOL packet type %d\n",
                      KeyFrame->PacketHeader.Type);

        goto ProcessReceivedDataEnd;
    }

    PacketBodyLength = NETWORK_TO_CPU16(KeyFrame->PacketHeader.BodyLength);
    if ((PacketBodyLength + sizeof(EAPOL_PACKET_HEADER)) >
        (Packet->FooterOffset - Packet->DataOffset)) {

        RtlDebugPrint("EAPOL: Invalid length %d is bigger than packet data, "
                      "which is only %d bytes.\n",
                      PacketBodyLength + sizeof(EAPOL_PACKET_HEADER),
                      (Packet->FooterOffset - Packet->DataOffset));

        goto ProcessReceivedDataEnd;
    }

    //
    // The packet body should at least be the size of an EAPOL key frame, minus
    // the packet header.
    //

    if (PacketBodyLength <
        (sizeof(EAPOL_KEY_FRAME) - sizeof(EAPOL_PACKET_HEADER))) {

        RtlDebugPrint("EAPOL: Invalid packet length %d that does not at least "
                      "hold a key frame of size %d.\n",
                      PacketBodyLength,
                      sizeof(EAPOL_KEY_FRAME) - sizeof(EAPOL_PACKET_HEADER));

        goto ProcessReceivedDataEnd;
    }

    //
    // EAPOL currnetly supports the 802.11 RSN key descriptor.
    //

    if (KeyFrame->DescriptorType != EAPOL_KEY_DESCRIPTOR_TYPE_RSN) {
        RtlDebugPrint("EAPOL: Unsupported key frame descriptor type %d\n",
                      KeyFrame->DescriptorType);

        goto ProcessReceivedDataEnd;
    }

    //
    // Parse the key frame based on the mode.
    //

    if (Context->Mode == EapolModeSupplicant) {
        Net80211pEapolSupplicantReceiveMessage(Context, KeyFrame);

    } else {
        RtlDebugPrint("EAPOL: Packet arrived for unsupported mode %d.\n",
                      Context->Mode);
    }

ProcessReceivedDataEnd:
    if (Context != NULL) {
        Net80211pEapolContextReleaseReference(Context);
    }

    return;
}

ULONG
Net80211pEapolPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine is called to convert a network address into a string, or
    determine the length of the buffer needed to convert an address into a
    string.

Arguments:

    Address - Supplies an optional pointer to a network address to convert to
        a string.

    Buffer - Supplies an optional pointer where the string representation of
        the address will be returned.

    BufferLength - Supplies the length of the supplied buffer, in bytes.

Return Value:

    Returns the maximum length of any address if no network address is
    supplied.

    Returns the actual length of the network address string if a network address
    was supplied, including the null terminator.

--*/

{

    //
    // There is no such thing as an EAPOL address. The packets destination is
    // determined by the data link layer.
    //

    return 0;
}

VOID
Net80211pEapolSupplicantReceiveMessage (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    )

/*++

Routine Description:

    This routine processes a message key frame received by an EAPOL supplicant.

Arguments:

    Context - Supplies a pointer to the EAPOL supplicant context processing the
        key frame.

    KeyFrame - Supplies a pointer to a key frame sent to the supplicant that
        needs to be processed.

Return Value:

    None.

--*/

{

    BOOL CompleteExchange;
    KSTATUS CompletionStatus;
    PUCHAR EncryptedKeyData;
    USHORT EncryptedKeyDataLength;
    ULONG GtkLength;
    PEAPOL_KDE Kde;
    PEAPOL_KDE_GTK KdeGtk;
    ULONG KdeOuiDataType;
    PUCHAR KeyData;
    ULONG KeyDataLength;
    USHORT KeyInformation;
    USHORT KeyLength;
    ULONGLONG KeyReplayCounter;
    ULONG KeyVersion;
    BOOL Match;
    USHORT MessageType;
    KSTATUS Status;
    BOOL ValidMic;

    ASSERT(Context->Mode == EapolModeSupplicant);

    CompleteExchange = FALSE;
    CompletionStatus = STATUS_SUCCESS;
    KeyData = NULL;

    //
    // Synchronize with other packets arriving for this EAPOL context.
    //

    KeAcquireQueuedLock(Context->Lock);

    //
    // If this context has already been removed from the tree, then a previous
    // packet completed it.
    //

    if (Context->TreeEntry.Parent == NULL) {
        goto SupplicantProcessKeyFrameEnd;
    }

    //
    // Make sure the replay counter has not been used. It should be greater
    // than the current replay counter. The local key replay counter, however,
    // is not valid until a message with a MIC is received.
    //

    KeyReplayCounter = NETWORK_TO_CPU64(KeyFrame->KeyReplayCounter);
    if ((Context->KeyReplayCounterValid != FALSE) &&
        (KeyReplayCounter <= Context->KeyReplayCounter)) {

        RtlDebugPrint("EAPOL: Skipping key frame with old replay counter "
                      "%I64d. Expected %I64d or greater.\n",
                      KeyReplayCounter,
                      Context->KeyReplayCounter);

        goto SupplicantProcessKeyFrameEnd;
    }

    //
    // Act based on the message type. Even though a previous message 1 has been
    // received and replied to with a message 2, if this supplicant receives a
    // message 1 it should go through the same reply process and forget the old
    // message 1 ever arrived. The message # can be determined by the key
    // information.
    //

    KeyInformation = NETWORK_TO_CPU16(KeyFrame->KeyInformation);
    KeyVersion = (KeyInformation & EAPOL_KEY_INFORMATION_VERSION_MASK) >>
                 EAPOL_KEY_INFORMATION_VERSION_SHIFT;

    MessageType = KeyInformation & EAPOL_KEY_INFORMATION_MESSAGE_MASK;
    switch (MessageType) {
    case EAPOL_KEY_INFORMATION_MESSAGE_1:

        //
        // Save the nonce sent from the authenticator.
        //

        RtlCopyMemory(Context->Authenticator.Nonce,
                      KeyFrame->KeyNonce,
                      EAPOL_NONCE_SIZE);

        //
        // Derive the pairwise transient key (PTK) for this link. The length of
        // the temporal key portion of the PTK is indicated by the AP.
        //

        KeyLength = NETWORK_TO_CPU16(KeyFrame->KeyLength);
        Status = Net80211pGeneratePtk(Context, KeyLength);
        if (!KSUCCESS(Status)) {
            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Save the key version for use in sending message 2 and parsing
        // message 3.
        //

        Context->KeyVersion = KeyVersion;

        //
        // Send message 2 back to the authenticator.
        //

        Status = Net80211pEapolSupplicantSendMessage(Context,
                                                     EapolMessageType2,
                                                     KeyReplayCounter);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("EAPOL: Failed to send supplicant message 2: %d.\n",
                          Status);

            goto SupplicantProcessKeyFrameEnd;
        }

        break;

    case EAPOL_KEY_INFORMATION_MESSAGE_3:

        //
        // Validate the version matches that of message 1.
        //

        if (Context->KeyVersion != KeyVersion) {
            RtlDebugPrint("EAPOL: Found unexpected key version in message 3. "
                          "Expected %d, received %d.\n",
                          Context->KeyVersion,
                          KeyVersion);

            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Make sure the authenticator's nonce matches that of message 1.
        //

        Match = RtlCompareMemory(KeyFrame->KeyNonce,
                                 Context->Authenticator.Nonce,
                                 EAPOL_NONCE_SIZE);

        if (Match == FALSE) {
            RtlDebugPrint("EAPOL: Mismatching nonce from authenticator in "
                          "message 3.\n");

            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Decrypt the key data and validate the RSN information for the
        // authenticator.
        //

        EncryptedKeyDataLength = NETWORK_TO_CPU16(KeyFrame->KeyDataLength);
        if (EncryptedKeyDataLength == 0) {
            RtlDebugPrint("EAPOL: Supplicant expected encrypted key data in "
                          "message 3, but found no key data.\n");

            goto SupplicantProcessKeyFrameEnd;
        }

        EncryptedKeyData = (PUCHAR)(KeyFrame + 1);
        Status = Net80211pEapolDecryptKeyData(Context,
                                              EncryptedKeyData,
                                              EncryptedKeyDataLength,
                                              &KeyData,
                                              &KeyDataLength);

        if (!KSUCCESS(Status)) {
            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Compare the decrypted key data with the RSN from the beacon/probe.
        //

        Match = RtlCompareMemory(KeyData,
                                 Context->Authenticator.Rsn,
                                 Context->Authenticator.RsnSize);

        if (Match == FALSE) {
            RtlDebugPrint("EAPOL: Mismatching encrypted RSN in message 3.\n");
            CompleteExchange = TRUE;
            CompletionStatus = STATUS_UNSUCCESSFUL;
            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Validate the MIC. If it is not valid, drop the packet.
        //

        ValidMic = Net80211pEapolValidateMic(Context, KeyFrame);
        if (ValidMic == FALSE) {
            goto SupplicantProcessKeyFrameEnd;
        }

        //
        // Parse the rest of the decrypted key data to see if an GTK was
        // supplied.
        //

        KeyDataLength -= Context->Authenticator.RsnSize;
        Kde = (PEAPOL_KDE)(KeyData + Context->Authenticator.RsnSize);
        if ((KeyDataLength >= sizeof(EAPOL_KDE)) &&
            (Kde->Type == EAPOL_KDE_TYPE)) {

            KdeOuiDataType = NETWORK_TO_CPU32(Kde->OuiDataType);
            switch (KdeOuiDataType) {
            case EAPOL_KDE_SELECTOR_GTK:
                GtkLength = Kde->Length - 6;
                if ((GtkLength > Kde->Length) || (GtkLength == 0)) {
                    break;
                }

                //
                // The length should match the key data length specified in
                // message 1, which was cached in the context.
                //

                if (GtkLength != Context->TemporalKeySize) {
                    break;
                }

                KdeGtk = (PEAPOL_KDE_GTK)(Kde + 1);
                Context->GtkFlags = KdeGtk->Flags;
                Context->Gtk = MmAllocatePagedPool(GtkLength,
                                                   EAPOL_ALLOCATION_TAG);

                if (Context->Gtk == NULL) {
                    goto SupplicantProcessKeyFrameEnd;
                }

                Context->GtkSize = GtkLength;
                RtlCopyMemory(Context->Gtk, KdeGtk->Gtk, GtkLength);
                break;

            default:
                break;
            }
        }

        //
        // The MIC was valid. Update the local key replay counter.
        //

        Context->KeyReplayCounter = KeyReplayCounter;
        Context->KeyReplayCounterValid = TRUE;

        //
        // Send message 4 back to the authenticator.
        //

        Status = Net80211pEapolSupplicantSendMessage(Context,
                                                     EapolMessageType4,
                                                     KeyReplayCounter);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("EAPOL: Failed to send supplicant message 4.\n");
            goto SupplicantProcessKeyFrameEnd;
        }

        CompleteExchange = TRUE;
        CompletionStatus = STATUS_SUCCESS;
        break;

    default:
        RtlDebugPrint("EAPOL: Supplicant received unknown message type "
                      "0x%04x.\n",
                      MessageType);

        goto SupplicantProcessKeyFrameEnd;
    }

SupplicantProcessKeyFrameEnd:

    //
    // In order to not process more packets for a completed context, remove the
    // context from the global tree while the context lock is still held.
    //

    if (CompleteExchange != FALSE) {
        KeAcquireQueuedLock(Net80211EapolTreeLock);
        if (Context->TreeEntry.Parent != NULL) {
            RtlRedBlackTreeRemove(&Net80211EapolTree, &(Context->TreeEntry));
            Context->TreeEntry.Parent = NULL;
        }

        KeReleaseQueuedLock(Net80211EapolTreeLock);
    }

    KeReleaseQueuedLock(Context->Lock);

    //
    // Now that the context lock has been released, call the completion
    // routine if necessary.
    //

    if (CompleteExchange != FALSE) {
        Net80211pEapolCompleteInstance(Context, CompletionStatus);
    }

    if (KeyData != NULL) {
        MmFreePagedPool(KeyData);
    }

    return;
}

KSTATUS
Net80211pEapolSupplicantSendMessage (
    PEAPOL_CONTEXT Context,
    EAPOL_MESSAGE_TYPE Type,
    ULONGLONG KeyReplayCounter
    )

/*++

Routine Description:

    This routine sends a message to the authenticator from the supplicant.

Arguments:

    Context - Supplies a pointer to the EAPOL context sending the message.

    Type - Supplies the message type to send to the authenticator.

    KeyReplayCounter - Supplies the key replay counter for the authenticator's
        message to which this message is replying.

Return Value:

    Status code.

--*/

{

    USHORT BodyLength;
    ULONG Flags;
    PUCHAR KeyData;
    ULONG KeyDataLength;
    PEAPOL_KEY_FRAME KeyFrame;
    USHORT KeyInformation;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    ULONG PacketSize;
    PNET_DATA_LINK_SEND Send;
    KSTATUS Status;

    ASSERT((Type == EapolMessageType2) || (Type == EapolMessageType4));

    KeyDataLength = 0;
    if (Type == EapolMessageType2) {
        KeyDataLength = Context->Supplicant.RsnSize;
    }

    //
    // Allocate a network packet buffer large enough to hold the key frame and
    // key data.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_UNENCRYPTED;

    Packet = NULL;
    PacketSize = sizeof(EAPOL_KEY_FRAME) + KeyDataLength;
    Status = NetAllocateBuffer(0,
                               PacketSize,
                               0,
                               Context->NetworkLink,
                               Flags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SupplicantSendMessageEnd;
    }

    //
    // EAPOL packets may need to be sent while the transmit queue is paused.
    // Force the transmission through.
    //

    Packet->Flags |= NET_PACKET_FLAG_FORCE_TRANSMIT |
                     NET_PACKET_FLAG_UNENCRYPTED;

    //
    // Initialize the key frame.
    //

    KeyFrame = Packet->Buffer + Packet->DataOffset;
    RtlZeroMemory(KeyFrame, sizeof(EAPOL_KEY_FRAME));
    KeyFrame->PacketHeader.ProtocolVersion = EAPOL_PROTOCOL_VERSION;
    KeyFrame->PacketHeader.Type = EAPOL_PACKET_TYPE_KEY_FRAME;
    BodyLength = (USHORT)PacketSize - sizeof(EAPOL_PACKET_HEADER);
    KeyFrame->PacketHeader.BodyLength = CPU_TO_NETWORK16(BodyLength);
    KeyFrame->DescriptorType = EAPOL_KEY_DESCRIPTOR_TYPE_RSN;
    KeyInformation = (Context->KeyVersion <<
                      EAPOL_KEY_INFORMATION_VERSION_SHIFT) |
                     (EAPOL_KEY_INFORMATION_PAIRWISE <<
                      EAPOL_KEY_INFORMATION_TYPE_SHIFT) |
                     EAPOL_KEY_INFORMATION_MIC_PRESENT;

    if (Type == EapolMessageType4) {
        KeyInformation |= EAPOL_KEY_INFORMATION_SECURE;
    }

    KeyFrame->KeyInformation = CPU_TO_NETWORK16(KeyInformation);
    KeyFrame->KeyReplayCounter = CPU_TO_NETWORK64(KeyReplayCounter);

    //
    // Send the supplicant's nonce value to the authenticator so it can
    // generate the PTK.
    //

    if (Type == EapolMessageType2) {
        RtlCopyMemory(KeyFrame->KeyNonce,
                      Context->Supplicant.Nonce,
                      EAPOL_NONCE_SIZE);

        //
        // The key data is the RSNE. Same as the 802.11 (re)assocation request
        // would send.
        //

        ASSERT(KeyDataLength != 0);

        KeyFrame->KeyDataLength = CPU_TO_NETWORK16(KeyDataLength);
        KeyData = (PUCHAR)(KeyFrame + 1);
        RtlCopyMemory(KeyData, Context->Supplicant.Rsn, KeyDataLength);
    }

    //
    // Compute the MIC for the key frame.
    //

    Net80211pEapolComputeMic(Context, KeyFrame);

    //
    // Send the packet down to the data link layer.
    //

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
    Send = Context->NetworkLink->DataLinkEntry->Interface.Send;
    Status = Send(Context->NetworkLink->DataLinkContext,
                  &PacketList,
                  &(Context->Supplicant.Address),
                  &(Context->Authenticator.Address),
                  EAPOL_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto SupplicantSendMessageEnd;
    }

SupplicantSendMessageEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

VOID
Net80211pEapolReadGlobalKeyCounter (
    PEAPOL_CONTEXT Context,
    PUCHAR ReadBuffer,
    ULONG ReadSize
    )

/*++

Routine Description:

    This routine reads a portion of the EAPOL instances's global key counter
    into the given buffer. If the read size is greater than the size of the
    global key counter, then the rest of the buffer will be left untouched.
    Once the global key is read, it is incremented by 1.

Arguments:

    Context - Supplies a pointer to the context of the EAPOL instance whose
        global key counter is to be read and incremented.

    ReadBuffer - Supplies a pointer to a buffer that receives the current
        global key counter value.

    ReadSize - Supplies the number of bytes to read in the given buffer.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Offset;

    if (ReadSize > EAPOL_GLOBAL_KEY_COUNTER_SIZE) {
        ReadSize = EAPOL_GLOBAL_KEY_COUNTER_SIZE;
    }

    Offset = EAPOL_GLOBAL_KEY_COUNTER_SIZE - ReadSize;

    //
    // Copy the lowest N-bytes from the global key counter to the read buffer.
    // The global key counter is a 32-byte big endian value.
    //

    KeAcquireQueuedLock(Context->Lock);
    RtlCopyMemory(ReadBuffer, Context->GlobalKeyCounter + Offset, ReadSize);

    //
    // Increment the key. The key is saved in big endian byte order where the
    // least significant byte is at the end.
    //

    for (Index = EAPOL_GLOBAL_KEY_COUNTER_SIZE; Index > 0; Index += 1) {
        Context->GlobalKeyCounter[Index - 1] += 1;
        if (Context->GlobalKeyCounter[Index - 1] != 0) {
            break;
        }
    }

    KeReleaseQueuedLock(Context->Lock);
    return;
}

KSTATUS
Net80211pEapolConvertPassphraseToPsk (
    PUCHAR Passphrase,
    ULONG PassphraseLength,
    PUCHAR Ssid,
    ULONG SsidLength,
    PUCHAR Psk,
    ULONG PskLength
    )

/*++

Routine Description:

    This routine converts the 8 to 63 character passphrase into a 256-bit PSK
    using the SSID as a salt.

Arguments:

    Passphrase - Supplies an array of ASCII characters used as the passphrase.

    PassphraseLength - Supplies the length of the passphrase.

    Ssid - Supplies the SSID string for the BSS to which the passphrase belongs.

    SsidLength - Supplies the length of the SSID.

    Psk - Supplies a pointer that receives the 256-bit PSK derived from the
        passphrase.

    PskLength - Supplies the length of the desired PSK, in bytes.

Return Value:

    Status code.

--*/

{

    UCHAR Digest[SHA1_HASH_SIZE];
    ULONG HashIndex;
    PUCHAR Message;
    ULONG MessageLength;
    PUCHAR Output;
    PUCHAR PskBuffer;
    ULONG PskIndex;
    ULONG RequiredSha1Count;
    KSTATUS Status;
    ULONG XorIndex;

    PskBuffer = NULL;

    //
    // Allocate a buffer to hold the SSID plus the PSK index. It must be at
    // least the size of a SHA1 hash.
    //

    MessageLength = SsidLength + sizeof(ULONG);
    if (MessageLength < SHA1_HASH_SIZE) {
        MessageLength = SHA1_HASH_SIZE;
    }

    Message = MmAllocatePagedPool(MessageLength, EAPOL_ALLOCATION_TAG);
    if (Message == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ConvertPassphraseToPskEnd;
    }

    //
    // If the given PSK buffer is not big enough to hold the contents of
    // the necessary number of SHA-1 HMAC signatures, then allocate a temporary
    // buffer to hold the output.
    //

    RequiredSha1Count = PskLength / SHA1_HASH_SIZE;
    if ((PskLength % SHA1_HASH_SIZE) != 0) {
        RequiredSha1Count += 1;
        PskBuffer = MmAllocatePagedPool(RequiredSha1Count * SHA1_HASH_SIZE,
                                        EAPOL_ALLOCATION_TAG);

        if (PskBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ConvertPassphraseToPskEnd;
        }

    } else {
        PskBuffer = Psk;
    }

    //
    // Perform as many iterations as necessary to fill the PSK with SHA-1 HMAC
    // signatures.
    //

    Output = PskBuffer;
    for (PskIndex = 1; PskIndex <= RequiredSha1Count; PskIndex += 1) {
        RtlCopyMemory(Message, Ssid, SsidLength);
        *((PULONG)&(Message[SsidLength])) = RtlByteSwapUlong(PskIndex);

        //
        // Compute the first digest and save it to the output.
        //

        CySha1ComputeHmac(Message,
                          SsidLength + sizeof(ULONG),
                          Passphrase,
                          PassphraseLength,
                          Digest);

        RtlCopyMemory(Output, Digest, SHA1_HASH_SIZE);
        RtlCopyMemory(Message, Digest, SHA1_HASH_SIZE);

        //
        // Now compute the rest of the interations reusing each computed digest
        // as the next message.
        //

        for (HashIndex = 1; HashIndex < 4096; HashIndex += 1) {
            CySha1ComputeHmac(Message,
                              SHA1_HASH_SIZE,
                              Passphrase,
                              PassphraseLength,
                              Digest);

            RtlCopyMemory(Message, Digest, SHA1_HASH_SIZE);

            //
            // XOR the total output with the current digest.
            //

            for (XorIndex = 0; XorIndex < SHA1_HASH_SIZE; XorIndex += 1) {
                Output[XorIndex] ^= Digest[XorIndex];
            }
        }

        Output += SHA1_HASH_SIZE;
    }

    //
    // If a temporary buffer was allocated for the PSK, copy the desired bytes
    // back to the supplied buffer.
    //

    if (PskBuffer != Psk) {
        RtlCopyMemory(Psk, PskBuffer, PskLength);
    }

    Status = STATUS_SUCCESS;

ConvertPassphraseToPskEnd:
    if (Message != NULL) {
        MmFreePagedPool(Message);
    }

    if ((PskBuffer != NULL) && (PskBuffer != Psk)) {
        MmFreePagedPool(PskBuffer);
    }

    return Status;
}

KSTATUS
Net80211pGeneratePtk (
    PEAPOL_CONTEXT Context,
    ULONG TemporalKeyLength
    )

/*++

Routine Description:

    This routine generates the pairwise transient key (PTK) for a session
    between a supplicant and an authenticator. It uses the MAC address and
    nonce values stored in the context.

Arguments:

    Context - Supplies a pointer to context of the EAPOL instance for which the
        PTK should be generated.

    TemporalKeyLength - Supplies the length of the temporal key portion of the
        PTK, in bytes.

Return Value:

    Status code.

--*/

{

    PUCHAR CurrentData;
    UCHAR Data[EAPOL_PTK_DATA_SIZE];
    PUCHAR MaxAddress;
    PUCHAR MaxNonce;
    PUCHAR MinAddress;
    PUCHAR MinNonce;
    ULONG PtkSize;
    COMPARISON_RESULT Result;
    KSTATUS Status;

    //
    // Release the existing PTK.
    //

    if (Context->Ptk != NULL) {
        MmFreePagedPool(Context->Ptk);
        Context->Ptk = NULL;
    }

    //
    // Concatenate both MAC addresses and both nonce values from the
    // authenticator and the supplicant in to the data buffer.
    //

    CurrentData = Data;

    //
    // Set the MAC addresses in ascending order.
    //

    MinAddress = (PUCHAR)(Context->Authenticator.Address.Address);
    MaxAddress = (PUCHAR)(Context->Supplicant.Address.Address);
    Result = Net80211pEapolCompareMemory(MinAddress,
                                         MaxAddress,
                                         NET80211_ADDRESS_SIZE);

    if (Result == ComparisonResultDescending) {
        MinAddress = (PUCHAR)(Context->Supplicant.Address.Address);
        MaxAddress = (PUCHAR)(Context->Authenticator.Address.Address);
    }

    RtlCopyMemory(CurrentData, MinAddress, NET80211_ADDRESS_SIZE);
    CurrentData += NET80211_ADDRESS_SIZE;
    RtlCopyMemory(CurrentData, MaxAddress, NET80211_ADDRESS_SIZE);
    CurrentData += NET80211_ADDRESS_SIZE;

    //
    // Set the nonce's in ascending order.
    //

    MinNonce = Context->Authenticator.Nonce;
    MaxNonce = Context->Supplicant.Nonce;
    Result = Net80211pEapolCompareMemory(MinNonce, MaxNonce, EAPOL_NONCE_SIZE);
    if (Result == ComparisonResultDescending) {
        MinNonce = Context->Supplicant.Nonce;
        MaxNonce = Context->Authenticator.Nonce;
    }

    RtlCopyMemory(CurrentData, MinNonce, EAPOL_NONCE_SIZE);
    CurrentData += EAPOL_NONCE_SIZE;
    RtlCopyMemory(CurrentData, MaxNonce, EAPOL_NONCE_SIZE);
    CurrentData += EAPOL_NONCE_SIZE;

    //
    // Allocate a buffer for the PTK, which includes the KCK, KEK, and temporal
    // key.
    //

    PtkSize = EAPOL_KCK_SIZE + EAPOL_KEK_SIZE + TemporalKeyLength;
    Context->Ptk = MmAllocatePagedPool(PtkSize, EAPOL_ALLOCATION_TAG);
    if (Context->Ptk == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GeneratePtkEnd;
    }

    Context->PtkSize = PtkSize;
    Context->TemporalKeySize = TemporalKeyLength;

    //
    // Run the data through the PRF using the PMK as a key.
    //

    Status = Net80211pEapolPseudoRandomFunction(
                                             Context->Pmk,
                                             EAPOL_PMK_SIZE,
                                             EAPOL_PTK_APPLICATION_TEXT,
                                             EAPOL_PTK_APPLICATION_TEXT_LENGTH,
                                             Data,
                                             EAPOL_PTK_DATA_SIZE,
                                             Context->Ptk,
                                             Context->PtkSize);

    if (!KSUCCESS(Status)) {
        goto GeneratePtkEnd;
    }

GeneratePtkEnd:
    if (!KSUCCESS(Status)) {
        if (Context->Ptk != NULL) {
            MmFreePagedPool(Context->Ptk);
            Context->Ptk = NULL;
            Context->PtkSize = 0;
        }
    }

    return Status;
}

KSTATUS
Net80211pEapolEncryptKeyData (
    PEAPOL_CONTEXT Context,
    PUCHAR KeyData,
    ULONG KeyDataLength,
    PUCHAR *EncryptedKeyData,
    PULONG EncryptedKeyDataLength
    )

/*++

Routine Description:

    This routine encrypts the given key data using the appropriate algorithm
    as defined by the key encryption type, which is gathered from the key
    information version.

Arguments:

    Context - Supplies a pointer to the context of the EAPOL instance to which
        the key data belongs.

    KeyData - Supplies a pointer to the plaintext key data to encrypt.

    KeyDataLength - Supplies the length of the key data, in bytes.

    EncryptedKeyData - Supplies a pointer that receives a pointer to a buffer
        containing the encrypted key data. The caller is responsible for
        releasing this resource.

    EncryptedKeyDataLength - Supplies a pointer that receives the length of the
        encrypted key data.

Return Value:

    Status code.

--*/

{

    PUCHAR Ciphertext;
    ULONG CiphertextLength;
    PUCHAR Plaintext;
    ULONG PlaintextLength;
    KSTATUS Status;

    ASSERT((Context->KeyVersion == EAPOL_KEY_VERSION_NIST_AES_HMAC_SHA1_128) ||
           (Context->KeyVersion == EAPOL_KEY_VERSION_NIST_AES_AES_128_CMAC));

    Ciphertext = NULL;
    CiphertextLength = 0;

    //
    // If the key data is less than 16 bytes or not 8-byte aligned, then it
    // needs to be padded.
    //

    PlaintextLength = KeyDataLength;
    if (PlaintextLength < EAPOL_NIST_AES_MIN_KEY_DATA_LENGTH) {
        PlaintextLength = EAPOL_NIST_AES_MIN_KEY_DATA_LENGTH;

    } else if (IS_ALIGNED(PlaintextLength,
                          EAPOL_NIST_AES_KEY_DATA_ALIGNMENT) == FALSE) {

        PlaintextLength = ALIGN_RANGE_UP(PlaintextLength,
                                         EAPOL_NIST_AES_KEY_DATA_ALIGNMENT);
    }

    //
    // If padding is required, allocate a new buffer and pad it.
    //

    if (PlaintextLength != KeyDataLength) {
        Plaintext = MmAllocatePagedPool(PlaintextLength, EAPOL_ALLOCATION_TAG);
        if (Plaintext == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EncryptKeyDataEnd;
        }

        RtlCopyMemory(Plaintext, KeyData, KeyDataLength);
        Plaintext[KeyDataLength] = EAPOL_NIST_AES_KEY_DATA_PADDING_BYTE;
        RtlZeroMemory((Plaintext + KeyDataLength + 1),
                      PlaintextLength - KeyDataLength - 1);

    } else {
        Plaintext = KeyData;
    }

    //
    // Allocate a buffer to hold the encrypted key data. It should be 8 bytes
    // longer than the plaintext.
    //

    CiphertextLength = PlaintextLength +
                       EAPOL_NIST_AES_KEY_DATA_CIPHERTEXT_LENGTH_DELTA;

    Ciphertext = MmAllocatePagedPool(CiphertextLength, EAPOL_ALLOCATION_TAG);
    if (Ciphertext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EncryptKeyDataEnd;
    }

    //
    // Perform the NIST AES key wrap to encrypt the key data.
    //

    Net80211pEapolNistAesKeyWrap(Plaintext,
                                 PlaintextLength,
                                 EAPOL_PTK_GET_KEK(Context->Ptk),
                                 EAPOL_KEK_SIZE,
                                 Ciphertext,
                                 CiphertextLength);

    if (!KSUCCESS(Status)) {
        goto EncryptKeyDataEnd;
    }

EncryptKeyDataEnd:
    if ((Plaintext != KeyData) && (Plaintext != NULL)) {
        MmFreePagedPool(Plaintext);
    }

    if (!KSUCCESS(Status)) {
        if (Ciphertext != NULL) {
            MmFreePagedPool(Ciphertext);
            Ciphertext = NULL;
            CiphertextLength = 0;
        }
    }

    *EncryptedKeyData = Ciphertext;
    *EncryptedKeyDataLength = CiphertextLength;
    return Status;
}

KSTATUS
Net80211pEapolDecryptKeyData (
    PEAPOL_CONTEXT Context,
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength,
    PUCHAR *KeyData,
    PULONG KeyDataLength
    )

/*++

Routine Description:

    This routine decrypts the given key data using the appropriate algorithm
    as defined by the key encryption type, which is gathered from the key
    information version.

Arguments:

    Context - Supplies a pointer to the context of the EAPOL instance to which
        the key data belongs.

    EncryptedKeyData - Supplies a pointer to the ciphertext key data to decrypt.

    EncryptedKeyDataLength - Supplies the length of the encrypted key data, in
        bytes.

    KeyData - Supplies a pointer that receives a pointer to a buffer containing
        the plaintext key data. The caller is responsible for releasing this
        resource.

    KeyDataLength - Supplies a pointer that receives the length of the
        decrypted key data.

Return Value:

    Status code.

--*/

{

    PUCHAR Plaintext;
    ULONG PlaintextLength;
    KSTATUS Status;

    ASSERT((Context->KeyVersion == EAPOL_KEY_VERSION_NIST_AES_HMAC_SHA1_128) ||
           (Context->KeyVersion == EAPOL_KEY_VERSION_NIST_AES_AES_128_CMAC));

    //
    // The final decrypted key data is 8 bytes shorter than the encrypted key
    // data.
    //

    PlaintextLength = EncryptedKeyDataLength -
                      EAPOL_NIST_AES_KEY_DATA_CIPHERTEXT_LENGTH_DELTA;

    Plaintext = MmAllocatePagedPool(PlaintextLength, EAPOL_ALLOCATION_TAG);
    if (Plaintext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DecryptKeyDataEnd;
    }

    Status = Net80211pEapolNistAesKeyUnwrap(EncryptedKeyData,
                                            EncryptedKeyDataLength,
                                            EAPOL_PTK_GET_KEK(Context->Ptk),
                                            EAPOL_KEK_SIZE,
                                            Plaintext,
                                            PlaintextLength);

    if (!KSUCCESS(Status)) {
        goto DecryptKeyDataEnd;
    }

DecryptKeyDataEnd:
    if (!KSUCCESS(Status)) {
        if (Plaintext != NULL) {
            MmFreePagedPool(Plaintext);
            Plaintext = NULL;
        }

        PlaintextLength = 0;
    }

    *KeyData = Plaintext;
    *KeyDataLength = PlaintextLength;
    return Status;
}

VOID
Net80211pEapolComputeMic (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    )

/*++

Routine Description:

    This routine computes the MIC for the given key frame and sets it into the
    key frame's MIC field.

Arguments:

    Context - Supplies a pointer to the EAPOL context generating the key frame.

    KeyFrame - Supplies a pointer to the key frame whose MIC needs to be
        computed.

Return Value:

    None.

--*/

{

    PUCHAR ComputedMic;
    UCHAR Digest[SHA1_HASH_SIZE];
    ULONG KeyFrameLength;

    ASSERT(Context->Ptk != NULL);

    //
    // Compute the MIC. The algorithm depends on the key version.
    //

    KeyFrameLength = sizeof(EAPOL_KEY_FRAME) +
                     NETWORK_TO_CPU16(KeyFrame->KeyDataLength);

    switch (Context->KeyVersion) {
    case EAPOL_KEY_VERSION_NIST_AES_HMAC_SHA1_128:
        CySha1ComputeHmac((PUCHAR)KeyFrame,
                          KeyFrameLength,
                          EAPOL_PTK_GET_KCK(Context->Ptk),
                          EAPOL_KCK_SIZE,
                          Digest);

        ComputedMic = Digest;
        break;

    case EAPOL_KEY_VERSION_NIST_AES_AES_128_CMAC:
    case EAPOL_KEY_VERSION_ARC4_HMAC_MD5:
    default:
        RtlDebugPrint("EAPOL: Unsupported MIC algorithm %d.\n",
                      Context->KeyVersion);

        goto ComputeMicEnd;
    }

    //
    // Save the compute MIC in the key frame.
    //

    RtlCopyMemory(KeyFrame->KeyMic, ComputedMic, EAPOL_DEFAULT_KEY_MIC_SIZE);

ComputeMicEnd:
    return;
}

BOOL
Net80211pEapolValidateMic (
    PEAPOL_CONTEXT Context,
    PEAPOL_KEY_FRAME KeyFrame
    )

/*++

Routine Description:

    This routine validates the MIC in the given key frame. It assumes that the
    key frame does indeed have a MIC to validate.

Arguments:

    Context - Supplies a pointer to the EAPOL context that received the key
        frame.

    KeyFrame - Supplies a pointer to the key frame whose MIC needs to be
        validated.

Return Value:

    Returns TRUE if the MIC is valid or FALSE otherwise.

--*/

{

    PUCHAR ComputedMic;
    UCHAR Digest[SHA1_HASH_SIZE];
    ULONG KeyFrameLength;
    BOOL Match;
    UCHAR SavedMic[EAPOL_DEFAULT_KEY_MIC_SIZE];
    BOOL Valid;

    Valid = FALSE;
    if (Context->Ptk == NULL) {
        RtlDebugPrint("EAPOL: Unexpected key frame MIC for link that has "
                      "no PTK to validate the MIC.\n");

        goto ValidateMicEnd;
    }

    //
    // Save the MIC and zero it in the key frame.
    //

    RtlCopyMemory(SavedMic, KeyFrame->KeyMic, EAPOL_DEFAULT_KEY_MIC_SIZE);
    RtlZeroMemory(KeyFrame->KeyMic, EAPOL_DEFAULT_KEY_MIC_SIZE);

    //
    // Recompute the MIC. The algorithm depends on the key version.
    //

    KeyFrameLength = sizeof(EAPOL_KEY_FRAME) +
                     NETWORK_TO_CPU16(KeyFrame->KeyDataLength);

    switch (Context->KeyVersion) {
    case EAPOL_KEY_VERSION_NIST_AES_HMAC_SHA1_128:
        CySha1ComputeHmac((PUCHAR)KeyFrame,
                          KeyFrameLength,
                          EAPOL_PTK_GET_KCK(Context->Ptk),
                          EAPOL_KCK_SIZE,
                          Digest);

        ComputedMic = Digest;
        break;

    case EAPOL_KEY_VERSION_NIST_AES_AES_128_CMAC:
    case EAPOL_KEY_VERSION_ARC4_HMAC_MD5:
    default:
        RtlDebugPrint("EAPOL: Unsupported MIC algorithm %d.\n",
                      Context->KeyVersion);

        goto ValidateMicEnd;
    }

    //
    // Compare the saved MIC to the computed MIC. The key frame is not
    // valid unless they match.
    //

    Match = RtlCompareMemory(SavedMic, ComputedMic, EAPOL_DEFAULT_KEY_MIC_SIZE);
    if (Match == FALSE) {
        RtlDebugPrint("EAPOL: Invalid MIC received.\n");
        goto ValidateMicEnd;
    }

    Valid = TRUE;

ValidateMicEnd:
    return Valid;
}

VOID
Net80211pEapolNistAesKeyWrap (
    PUCHAR KeyData,
    ULONG KeyDataLength,
    PUCHAR Key,
    ULONG KeyLength,
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength
    )

/*++

Routine Description:

    This routine performs the NIST AES Key Wrap algorithm on the given key data
    using the provided key. The encrypted key data is returned to the caller in
    the encrypted key data buffer.

Arguments:

    KeyData - Supplies a pointer to the key data to wrap.

    KeyDataLength - Supplies the length of the key data. This should be 8-byte
        aligned.

    Key - Supplies a pointer to the key encryption key to use for encrypting
        the key data.

    KeyLength - Supplies the length of the key encryption key.

    EncryptedKeyData - Supplies a pointer to a buffer that receives the wrapped
        key data.

    EncryptedKeyDataLength - Supplies the length of the encrypted key data
        buffer. It must be 8-byte aligned and should be at least 8 bytes larger
        than the key data buffer.

Return Value:

    None.

--*/

{

    ULONGLONG CipherText[2];
    AES_CONTEXT Context;
    ULONG Index;
    PULONGLONG Output;
    ULONGLONG PlainText[2];
    ULONG QuadwordCount;
    ULONGLONG Register;
    ULONG Step;
    ULONGLONG XorValue;

    ASSERT(IS_ALIGNED(KeyDataLength, EAPOL_NIST_AES_KEY_DATA_ALIGNMENT) !=
           FALSE);

    ASSERT((KeyDataLength + EAPOL_NIST_AES_KEY_DATA_CIPHERTEXT_LENGTH_DELTA) ==
           EncryptedKeyDataLength);

    ASSERT(KeyLength == AES_CBC128_KEY_SIZE);

    //
    // Initialize the AES context for codebook encryption.
    //

    CyAesInitialize(&Context, AesModeEcb128, Key, NULL);

    //
    // The algorithm treats the input and output as arrays of 64-bit words.
    // Initialize the register and the output buffer. The register gets the
    // default initial value and the output gets the input values, leaving
    // space for the final register value to fill the first 64-bit word. The
    // output buffer must be 8-byte aligned, but it cannot be assumed that the
    // input key data (that originates from the network packet) is aligned.
    //

    ASSERT(IS_POINTER_ALIGNED(EncryptedKeyData,
                              EAPOL_NIST_AES_KEY_DATA_ALIGNMENT) != FALSE);

    Output = (PULONGLONG)EncryptedKeyData;
    QuadwordCount = KeyDataLength / sizeof(ULONGLONG);
    Register = EAPOL_NIST_AES_KEY_WRAP_INITIAL_VALUE;
    RtlCopyMemory(Output + 1, KeyData, KeyDataLength);

    //
    // The input is wrapped 6 times in order to produce the encrypted key data.
    //

    for (Step = 0; Step < EAPOL_NIST_AES_KEY_WRAP_STEP_COUNT; Step += 1) {
        for (Index = 1; Index <= QuadwordCount; Index += 1) {
            PlainText[0] = Register;
            PlainText[1] = Output[Index];

            //
            // Encrypt this iteration's plaintext.
            //

            CyAesEcbEncrypt(&Context,
                            (PUCHAR)PlainText,
                            (PUCHAR)CipherText,
                            sizeof(PlainText));

            //
            // Treating the result as big-endian, the most significant bits go
            // back into the register and the least significant bits get stored
            // in the output buffer.
            //

            Output[Index] = CipherText[1];
            Register = CipherText[0];
            XorValue = (QuadwordCount * Step) + Index;
            Register ^= CPU_TO_NETWORK64(XorValue);
        }
    }

    Output[0] = Register;
    return;
}

KSTATUS
Net80211pEapolNistAesKeyUnwrap (
    PUCHAR EncryptedKeyData,
    ULONG EncryptedKeyDataLength,
    PUCHAR Key,
    ULONG KeyLength,
    PUCHAR KeyData,
    ULONG KeyDataLength
    )

/*++

Routine Description:

    This routine performs the NIST AES Key Unwrap algorithm on the given
    encrypted key data using the provided key. The decrypted key data is
    returned to the caller in the key data buffer.

Arguments:

    EncryptedKeyData - Supplies a pointer to the encrypted key data to unwrap.

    EncryptedKeyDataLength - Supplies the length of the encrypted key data.
        This should be 8-byte aligned.

    Key - Supplies a pointer to the key encryption key to use for decrypting
        the key data.

    KeyLength - Supplies the length of the key encryption key.

    KeyData - Supplies a pointer to a buffer that receives the unwrapped key
        data.

    KeyDataLength - Supplies the length of the key data buffer. It must be
        8-byte aligned. It should be 8 bytes less than the encrypted key data.

Return Value:

    Status code.

--*/

{

    ULONGLONG Ciphertext[2];
    AES_CONTEXT Context;
    ULONG Index;
    PULONGLONG Output;
    ULONGLONG Plaintext[2];
    ULONG QuadwordCount;
    ULONGLONG Register;
    KSTATUS Status;
    ULONG Step;
    ULONGLONG XorValue;

    ASSERT(IS_ALIGNED(EncryptedKeyDataLength,
                      EAPOL_NIST_AES_KEY_DATA_ALIGNMENT) != FALSE);

    ASSERT((KeyDataLength + EAPOL_NIST_AES_KEY_DATA_CIPHERTEXT_LENGTH_DELTA) ==
           EncryptedKeyDataLength);

    ASSERT(KeyLength == AES_ECB128_KEY_SIZE);

    //
    // Initailize the AES context for codebook decryption.
    //

    CyAesInitialize(&Context, AesModeEcb128, Key, NULL);
    CyAesConvertKeyForDecryption(&Context);

    //
    // The algorithm treats the input and output as arrays of 64-bit words.
    // Initialize the register and the output buffer. The register gets the
    // first 64-bit word and the output gets the remaining input values. The
    // input may not be aligned for quadword access, so be careful.
    //

    ASSERT(IS_POINTER_ALIGNED(KeyData, EAPOL_NIST_AES_KEY_DATA_ALIGNMENT));

    Output = (PULONGLONG)KeyData;
    QuadwordCount = KeyDataLength / sizeof(ULONGLONG);
    RtlCopyMemory(&Register, EncryptedKeyData, sizeof(ULONGLONG));
    RtlCopyMemory(Output, EncryptedKeyData + sizeof(ULONGLONG), KeyDataLength);

    //
    // The input is unwrapped 6 times in order to reproduce the key data.
    //

    for (Step = EAPOL_NIST_AES_KEY_WRAP_STEP_COUNT; Step > 0; Step -= 1) {
        for (Index = QuadwordCount; Index > 0; Index -= 1) {
            XorValue = (QuadwordCount * (Step - 1)) + Index;
            Register ^= CPU_TO_NETWORK64(XorValue);
            Ciphertext[0] = Register;
            Ciphertext[1] = Output[Index - 1];

            //
            // Decrypt this iteration's ciphertext.
            //

            CyAesEcbDecrypt(&Context,
                            (PUCHAR)Ciphertext,
                            (PUCHAR)Plaintext,
                            sizeof(Ciphertext));

            //
            // Treating the result as big-endian, the most significant bits go
            // back into the register and the least significant bits get stored
            // in the output buffer.
            //

            Register = Plaintext[0];
            Output[Index - 1] = Plaintext[1];
        }
    }

    //
    // Check the register for the initial value.
    //

    Status = STATUS_SUCCESS;
    if (Register != EAPOL_NIST_AES_KEY_WRAP_INITIAL_VALUE) {
        RtlDebugPrint("EAPOL: NIST AES key unwrap failed. Found initial value "
                      "0x%I64x, expected 0x%I64x.\n",
                      Register,
                      EAPOL_NIST_AES_KEY_WRAP_INITIAL_VALUE);

        Status = STATUS_UNSUCCESSFUL;
    }

    return Status;
}

KSTATUS
Net80211pEapolPseudoRandomFunction (
    PUCHAR Key,
    ULONG KeyLength,
    PSTR ApplicationText,
    ULONG ApplicationTextLength,
    PUCHAR Data,
    ULONG DataLength,
    PUCHAR Output,
    ULONG OutputLength
    )

/*++

Routine Description:

    This routine converts the key, application text, and data set into a
    pseudorandom number of the specified output length. This algorithm is
    specific to the EAP specification.

Arguments:

    Key - Supplies a pointer to a private key.

    KeyLength - Supplies the length of the private key.

    ApplicationText - Supplies the well-known text to use when calculating
        pseudorandom numbers for different EAP applications.

    ApplicationTextLength - Supplies the length of the application text. This
        should not include the NULL terminator for any strings.

    Data - Supplies a pointer to the data to input into the pseudorandom
        function along with the application text.

    DataLength - Supplies the length of the data.

    Output - Supplies a pointer to the buffer that will receive the output of
        the pseudorandom number computation.

    OutputLength - Supplies the desired length of the output, in bytes.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    PUCHAR Input;
    ULONG InputLength;
    PUCHAR OutputBuffer;
    ULONG OutputOffset;
    ULONG RequiredSha1Count;
    KSTATUS Status;

    //
    // Allocate an input buffer to hold the concatenated strings.
    //

    OutputBuffer = NULL;
    InputLength = ApplicationTextLength + DataLength + 2;
    Input = MmAllocatePagedPool(InputLength, EAPOL_ALLOCATION_TAG);
    if (Input == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PseudoRandomFunctionEnd;
    }

    //
    // If the given output buffer is not big enough to hold the contents of
    // the necessary number of SHA-1 HMAC signatures, then allocate a temporary
    // buffer to hold the output.
    //

    RequiredSha1Count = OutputLength / SHA1_HASH_SIZE;
    if ((OutputLength % SHA1_HASH_SIZE) != 0) {
        RequiredSha1Count += 1;
        OutputBuffer = MmAllocatePagedPool(RequiredSha1Count * SHA1_HASH_SIZE,
                                           EAPOL_ALLOCATION_TAG);

        if (OutputBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PseudoRandomFunctionEnd;
        }

    } else {
        OutputBuffer = Output;
    }

    //
    // Concatenate the application text and the data, leaving a 0 byte
    // inbetween and an initial 0 at the end.
    //

    RtlCopyMemory(Input, ApplicationText, ApplicationTextLength);
    Input[ApplicationTextLength] = 0;
    RtlCopyMemory(&(Input[ApplicationTextLength + 1]), Data, DataLength);
    Input[InputLength - 1] = 0;

    //
    // Repeatedly compute the SHA-1 HMAC signature, changing the input each
    // time, until the desired output length is obtained. The last byte of the
    // input array gets incremented for each iteration.
    //

    OutputOffset = 0;
    for (Index = 0; Index < RequiredSha1Count; Index += 1) {
        CySha1ComputeHmac(Input,
                          InputLength,
                          Key,
                          KeyLength,
                          &(OutputBuffer[OutputOffset]));

        OutputOffset += SHA1_HASH_SIZE;
        Input[InputLength - 1] += 1;
    }

    //
    // Copy the contents back to the original output if it a buffer was
    // allocated to account for overflow.
    //

    if (Output != OutputBuffer) {
        RtlCopyMemory(Output, OutputBuffer, OutputLength);
    }

    Status = STATUS_SUCCESS;

PseudoRandomFunctionEnd:
    if (Input != NULL) {
        MmFreePagedPool(Input);
    }

    if ((OutputBuffer != NULL) && (OutputBuffer != Output)) {
        MmFreePagedPool(OutputBuffer);
    }

    return Status;
}

COMPARISON_RESULT
Net80211pEapolCompareMemory (
    PUCHAR FirstBuffer,
    PUCHAR SecondBuffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine compares two buffers of equal size, treating the first byte as
    the most significant byte (i.e. big endian). This can be used to compare
    MAC address and nonce values.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    BufferSize - Supplies the size of the buffers.

Return Value:

    Same if the two addresses are the same.

    Ascending if the first address is less than the second address.

    Descending if the second address is less than the first address.

--*/

{

    ULONG Index;

    for (Index = 0; Index < BufferSize; Index += 1) {
        if (FirstBuffer[Index] < SecondBuffer[Index]) {
            return ComparisonResultAscending;

        } else if (FirstBuffer[Index] > SecondBuffer[Index]) {
            return ComparisonResultDescending;
        }
    }

    return ComparisonResultSame;
}

COMPARISON_RESULT
Net80211pEapolCompareContexts (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes, in this case two EAPOL
    contexts.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PEAPOL_CONTEXT FirstEntry;
    PEAPOL_CONTEXT SecondEntry;

    FirstEntry = RED_BLACK_TREE_VALUE(FirstNode, EAPOL_CONTEXT, TreeEntry);
    SecondEntry = RED_BLACK_TREE_VALUE(SecondNode, EAPOL_CONTEXT, TreeEntry);
    if (FirstEntry->NetworkLink < SecondEntry->NetworkLink) {
        return ComparisonResultAscending;

    } else if (FirstEntry->NetworkLink > SecondEntry->NetworkLink) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

VOID
Net80211pEapolContextAddReference (
    PEAPOL_CONTEXT Context
    )

/*++

Routine Description:

    This routine increases the reference count on an EAPOL context.

Arguments:

    Context - Supplies a pointer to the EAPOL context whose reference count
        should be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Context->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    return;
}

VOID
Net80211pEapolContextReleaseReference (
    PEAPOL_CONTEXT Context
    )

/*++

Routine Description:

    This routine decreases the reference count of an EAPOL context, and
    destroys the context if the reference count drops to zero.

Arguments:

    Context - Supplies a pointer to the EAPOL context whose reference count
        should be decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Context->ReferenceCount), -1);

    ASSERT(OldReferenceCount != 0);

    if (OldReferenceCount == 1) {
        Net80211pEapolDestroyContext(Context);
    }

    return;
}

VOID
Net80211pEapolDestroyContext (
    PEAPOL_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given EAPOL context, releasing all of its
    resources.

Arguments:

    Context - Supplies a pointer to the EAPOL context to release.

Return Value:

    None.

--*/

{

    if (Context->Lock != NULL) {
        KeDestroyQueuedLock(Context->Lock);
    }

    if (Context->Ptk != NULL) {
        MmFreePagedPool(Context->Ptk);
    }

    if (Context->Gtk != NULL) {
        MmFreePagedPool(Context->Gtk);
    }

    if (Context->NetworkLink != NULL) {
        NetLinkReleaseReference(Context->NetworkLink);
    }

    if (Context->Net80211Link != NULL) {
        Net80211LinkReleaseReference(Context->Net80211Link);
    }

    MmFreePagedPool(Context);
    return;
}

VOID
Net80211pEapolCompleteInstance (
    PEAPOL_CONTEXT Context,
    KSTATUS CompletionStatus
    )

/*++

Routine Description:

    This routine completes an EAPOL instance. If the exchange was successful,
    then this routine sets the acquired keys in the link. The routine always
    notified the creator of the instance via the completion callback.

Arguments:

    Context - Supplies a pointer to the context of the completed EAPOL instance.

    CompletionStatus - Supplies the completion status for the instance.

Return Value:

    None.

--*/

{

    ULONG KeyFlags;
    ULONG KeyId;

    if (!KSUCCESS(CompletionStatus)) {
        goto InstanceCompleteEnd;
    }

    if (Context->Ptk != NULL) {
        KeyFlags = NET80211_KEY_FLAG_CCMP | NET80211_KEY_FLAG_TRANSMIT;
        CompletionStatus = Net80211SetKey(Context->Net80211Link,
                                          EAPOL_PTK_GET_TK(Context->Ptk),
                                          Context->TemporalKeySize,
                                          KeyFlags,
                                          0);

        if (!KSUCCESS(CompletionStatus)) {
            goto InstanceCompleteEnd;
        }
    }

    if (Context->Gtk != NULL) {
        KeyFlags = NET80211_KEY_FLAG_CCMP | NET80211_KEY_FLAG_GLOBAL;
        if ((Context->GtkFlags & EAPOL_KDE_GTK_FLAG_TRANSMIT) != 0) {
            KeyFlags |= NET80211_KEY_FLAG_TRANSMIT;
        }

        KeyId = (Context->GtkFlags & EAPOL_KDE_GTK_FLAG_KEY_ID_MASK) >>
                EAPOL_KDE_GTK_FLAG_KEY_ID_SHIFT;

        CompletionStatus = Net80211SetKey(Context->Net80211Link,
                                          EAPOL_GTK_GET_TK(Context->Gtk),
                                          Context->TemporalKeySize,
                                          KeyFlags,
                                          KeyId);

        if (!KSUCCESS(CompletionStatus)) {
            goto InstanceCompleteEnd;
        }
    }

InstanceCompleteEnd:
    Context->CompletionRoutine(Context->CompletionContext, CompletionStatus);
    return;
}

