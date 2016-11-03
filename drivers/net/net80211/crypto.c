/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crypto.c

Abstract:

    This module implements cryptographic functionality for the 802.11 core
    wireless networking library.

Author:

    Chris Stevens 5-Nov-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "net80211.h"
#include "eapol.h"
#include <minoca/lib/crypto.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default key ID to use for transmitting data.
//

#define NET80211_DEFAULT_ENCRYPTION_KEY 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Net80211pCcmEncrypt (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    );

KSTATUS
Net80211pCcmDecrypt (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    );

VOID
Net80211pCcmComputeAuthenticationField (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    );

VOID
Net80211pEapolCompletionRoutine (
    PVOID Context,
    KSTATUS Status
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

NET80211_API
KSTATUS
Net80211SetKey (
    PNET80211_LINK Link,
    PUCHAR KeyValue,
    ULONG KeyLength,
    ULONG KeyFlags,
    ULONG KeyId
    )

/*++

Routine Description:

    This routine sets the given key into the given network link. The 802.11
    networking library makes a local copy of all parameters.

Arguments:

    Link - Supplies a pointer to the networking link to which the keys should
        be added.

    KeyValue - Supplies a pointer to the key value.

    KeyLength - Supplies the length of the key value, in bytes.

    KeyFlags - Supplies a bitmask of flags to describe the key. See
        NET80211_KEY_FLAG_* for definitions.

    KeyId - Supplies the ID of the key negotiated between this station and its
        peers and/or access point.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PNET80211_ENCRYPTION Encryption;
    PNET80211_KEY Key;
    PNET80211_KEY OldKey;
    KSTATUS Status;

    OldKey = NULL;

    //
    // Make sure the key ID is valid and supported. The CCMP header only has
    // two bits for the key ID.
    //

    if (KeyId >= NET80211_MAX_KEY_COUNT) {
        return STATUS_INVALID_PARAMETER;
    }

    AllocationSize = sizeof(NET80211_KEY) + KeyLength - ANYSIZE_ARRAY;
    Key = MmAllocatePagedPool(AllocationSize, NET80211_ALLOCATION_TAG);
    if (Key == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SetKeyEnd;
    }

    RtlZeroMemory(Key, AllocationSize);
    Key->Flags = KeyFlags;
    Key->Id = KeyId;
    Key->Length = KeyLength;
    RtlCopyMemory(Key->Value, KeyValue, KeyLength);

    //
    // Update the pointer in the array of keys for the active BSS.
    //

    KeAcquireQueuedLock(Link->Lock);
    if ((Link->ActiveBss == NULL) ||
        (Link->State != Net80211StateAssociated)) {

        Status = STATUS_NOT_READY;

    } else {
        Encryption = &(Link->ActiveBss->Encryption);
        OldKey = Encryption->Keys[KeyId];
        Encryption->Keys[KeyId] = Key;

        //
        // Update the key indices if this is a group key.
        //

        if ((KeyFlags & NET80211_KEY_FLAG_GLOBAL) != 0) {
            Encryption->GroupKeyIndex = KeyId;
            if ((Encryption->Flags &
                 NET80211_ENCRYPTION_FLAG_USE_GROUP_CIPHER) != 0) {

                 Encryption->PairwiseKeyIndex = KeyId;
            }
        }

        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(Link->Lock);

SetKeyEnd:
    if (!KSUCCESS(Status)) {
        if (Key != NULL) {
            Net80211pDestroyKey(Key);
        }
    }

    if (OldKey != NULL) {
        Net80211pDestroyKey(OldKey);
    }

    return Status;
}

VOID
Net80211pDestroyKey (
    PNET80211_KEY Key
    )

/*++

Routine Description:

    This routine destroys the given 802.11 encryption key.

Arguments:

    Key - Supplies a pointer to the key to be destroyed.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Key->Value, Key->Length);
    MmFreePagedPool(Key);
    return;
}

KSTATUS
Net80211pInitializeEncryption (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine initializes the 802.11 core to handle the completion of an
    advanced encryption handshake.

Arguments:

    Link - Supplies a pointer to the 802.11 link establishing an ecrypted
        connection.

    Bss - Supplies a pointer to the BSS on which the encryption handshake will
        take place.

Return Value:

    Status code.

--*/

{

    ULONG ApRsnSize;
    NETWORK_ADDRESS AuthenticatorAddress;
    EAPOL_CREATION_PARAMETERS Parameters;
    ULONG StationRsnSize;
    KSTATUS Status;

    ASSERT(Bss != NULL);

    //
    // The BSS is good to go if there is alreayd an EAPOL instance associated
    // with it.
    //

    if (Bss->EapolHandle != INVALID_HANDLE) {
        return STATUS_SUCCESS;
    }

    //
    // If there is no encryption required by the BSS or it is using the basic
    // authentication built into 802.11, there is no work to be done.
    //

    if ((Bss->Encryption.Pairwise == NetworkEncryptionNone) ||
        (Bss->Encryption.Pairwise == NetworkEncryptionWep)) {

        return STATUS_SUCCESS;
    }

    //
    // Set both the pairwise and group key indices to the default.
    //

    Bss->Encryption.PairwiseKeyIndex = NET80211_DEFAULT_ENCRYPTION_KEY;
    Bss->Encryption.GroupKeyIndex = NET80211_DEFAULT_ENCRYPTION_KEY;

    //
    // Otherwise, EAPOL must be invoked in order to derive the PTK.
    //

    ASSERT(Bss->Encryption.Pairwise == NetworkEncryptionWpa2Psk);

    RtlZeroMemory(&AuthenticatorAddress, sizeof(NETWORK_ADDRESS));
    AuthenticatorAddress.Domain = NetDomain80211;
    RtlCopyMemory(AuthenticatorAddress.Address,
                  Bss->State.Bssid,
                  NET80211_ADDRESS_SIZE);

    RtlZeroMemory(&Parameters, sizeof(EAPOL_CREATION_PARAMETERS));
    Parameters.Mode = EapolModeSupplicant;
    Parameters.NetworkLink = Link->NetworkLink;
    Parameters.Net80211Link = Link;
    Parameters.SupplicantAddress = &(Link->Properties.PhysicalAddress);
    Parameters.AuthenticatorAddress = &AuthenticatorAddress;
    Parameters.Ssid = NET80211_GET_ELEMENT_DATA(Bss->Ssid);
    Parameters.SsidLength = NET80211_GET_ELEMENT_LENGTH(Bss->Ssid);
    Parameters.Passphrase = Bss->Passphrase;
    Parameters.PassphraseLength = Bss->PassphraseLength;
    Parameters.SupplicantRsn = Bss->Encryption.StationRsn;
    StationRsnSize = NET80211_GET_ELEMENT_LENGTH(Bss->Encryption.StationRsn) +
                     NET80211_ELEMENT_HEADER_SIZE;

    Parameters.SupplicantRsnSize = StationRsnSize;
    Parameters.AuthenticatorRsn = Bss->Encryption.ApRsn;
    ApRsnSize = NET80211_GET_ELEMENT_LENGTH(Bss->Encryption.ApRsn) +
                NET80211_ELEMENT_HEADER_SIZE;

    Parameters.AuthenticatorRsnSize = ApRsnSize;
    Parameters.CompletionRoutine = Net80211pEapolCompletionRoutine;
    Parameters.CompletionContext = Link;
    Status = Net80211pEapolCreateInstance(&Parameters, &(Bss->EapolHandle));
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID
Net80211pDestroyEncryption (
    PNET80211_BSS_ENTRY Bss
    )

/*++

Routine Description:

    This routine destroys the context used to handle encryption initialization.
    It is not necessary to keep this context once the encrypted state is
    reached.

Arguments:

    Bss - Supplies a pointer to the BSS on which encryption initialization took
        place.

Return Value:

    None.

--*/

{

    ASSERT(Bss != NULL);

    if (Bss->EapolHandle == INVALID_HANDLE) {
        return;
    }

    Net80211pEapolDestroyInstance(Bss->EapolHandle);
    Bss->EapolHandle = INVALID_HANDLE;
    return;
}

KSTATUS
Net80211pEncryptPacket (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine encrypts the given network packet's plaintext data. The
    supplied packet buffer is modified directly and should already include the
    full MPDU (i.e. the 802.11 headers should be present).

Arguments:

    Link - Supplies a pointer to the 802.11 network link that owns the packet.

    Bss - Supplies a pointer to the BSS over which this packet should be sent.

    Packet - Supplies a pointer to the packet to encrypt.

Return Value:

    Status code.

--*/

{

    NET80211_AAD Aad;
    PUCHAR AuthField;
    NET80211_CCM_NONCE CcmNonce;
    PNET80211_CCMP_HEADER CcmpHeader;
    PNET80211_DATA_FRAME_HEADER DataHeader;
    ULONG Index;
    PNET80211_KEY Key;
    ULONG KeyId;
    PUCHAR Message;
    ULONG MessageLength;
    PNET80211_DATA_FRAME_HEADER MovedDataHeader;
    ULONGLONG PacketNumber;
    PUCHAR PacketNumberArray;
    KSTATUS Status;

    //
    // Use the pairwise key by default.
    //

    KeyId = Bss->Encryption.PairwiseKeyIndex;
    Key = Bss->Encryption.Keys[KeyId];
    if ((Key == NULL) || ((Key->Flags & NET80211_KEY_FLAG_TRANSMIT) == 0)) {
        RtlDebugPrint("802.11: Failed to find valid key for transmit.\n");
        Status = STATUS_INVALID_CONFIGURATION;
        goto EncryptPacketEnd;
    }

    //
    // The start of the packet's valid data should point to the 802.11 header.
    //

    DataHeader = Packet->Buffer + Packet->DataOffset;
    AuthField = Packet->Buffer + Packet->FooterOffset;
    Message = (PUCHAR)(DataHeader + 1);
    MessageLength = AuthField - Message;
    Packet->FooterOffset += NET80211_CCMP_MIC_SIZE;

    //
    // Get a new packet number for the temporal key. The first 48-bits cannot
    // wrap. It's time to get a new temporal key if they do.
    //

    PacketNumber = RtlAtomicAdd64(&(Key->PacketNumber), 1);

    ASSERT(PacketNumber < 0x1000000000000);

    //
    // Construct the AAD based on the 802.11 header.
    //

    Aad.FrameControl = DataHeader->FrameControl &
                       NET80211_AAD_FRAME_CONTROL_DEFAULT_MASK;

    RtlCopyMemory(Aad.Address1,
                  DataHeader->ReceiverAddress,
                  NET80211_ADDRESS_SIZE * 3);

    Aad.SequenceControl = DataHeader->SequenceControl &
                          NET80211_AAD_SEQUENCE_CONTROL_MASK;

    //
    // Construct the CCM nonce. This is based on the packet number, which must
    // be stored in big-endian byte order.
    //

    CcmNonce.Flags = 0;
    RtlCopyMemory(CcmNonce.Address2, Aad.Address2, NET80211_ADDRESS_SIZE);
    PacketNumberArray = (PUCHAR)&PacketNumber;
    for (Index = 0; Index < NET80211_CCMP_PACKET_NUMBER_SIZE; Index += 1) {
        CcmNonce.PacketNumber[Index] =
               PacketNumberArray[NET80211_CCMP_PACKET_NUMBER_SIZE - 1 - Index];
    }

    //
    // Perform the CCM originator processing to produce the cipher text.
    //

    Status = Net80211pCcmEncrypt(Key,
                                 Message,
                                 MessageLength,
                                 (PUCHAR)&Aad,
                                 sizeof(NET80211_AAD),
                                 (PUCHAR)&CcmNonce,
                                 sizeof(NET80211_CCM_NONCE),
                                 AuthField,
                                 NET80211_CCMP_MIC_SIZE,
                                 NET80211_CCMP_LENGTH_FIELD_SIZE);

    if (!KSUCCESS(Status)) {
        goto EncryptPacketEnd;
    }

    //
    // Build the finalized encrypted packet. First move the 802.11 header
    // forward to make space for the CCMP header.
    //

    Packet->DataOffset -= sizeof(NET80211_CCMP_HEADER);
    MovedDataHeader = Packet->Buffer + Packet->DataOffset;
    RtlCopyMemory(MovedDataHeader,
                  DataHeader,
                  sizeof(NET80211_DATA_FRAME_HEADER));

    //
    // Construct the CCMP header using key ID 0.
    //

    CcmpHeader = (PNET80211_CCMP_HEADER)(MovedDataHeader + 1);
    CcmpHeader->Reserved = 0;
    CcmpHeader->Flags = NET80211_CCMP_FLAG_EXT_IV |
                        ((KeyId << NET80211_CCMP_FLAG_KEY_ID_SHIFT) &
                         NET80211_CCMP_FLAG_KEY_ID_MASK);

    NET80211_SET_CCMP_HEADER_PACKET_NUMBER(CcmpHeader, PacketNumber);

    //
    // The plaintext was encrypted in place and is right where it should be and
    // the MIC was placed where it should be in the footer. This packet is good
    // to go!
    //

EncryptPacketEnd:
    return Status;
}

KSTATUS
Net80211pDecryptPacket (
    PNET80211_LINK Link,
    PNET80211_BSS_ENTRY Bss,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine decrypts the given network packet's ciphertext. The supplied
    packet buffer is modified directly and should contain the full encrypted
    MPDU, including the 802.11 headers.

Arguments:

    Link - Supplies a pointer to the 802.11 network link that owns the packet.

    Bss - Supplies a pointer to the BSS over which this packet was received.

    Packet - Supplies a pointer to the packet to decrypt.

Return Value:

    Status code.

--*/

{

    NET80211_AAD Aad;
    PUCHAR AuthField;
    NET80211_CCM_NONCE CcmNonce;
    PNET80211_CCMP_HEADER CcmpHeader;
    PNET80211_DATA_FRAME_HEADER DataHeader;
    ULONG Index;
    PNET80211_KEY Key;
    ULONG KeyId;
    PUCHAR Message;
    ULONG MessageLength;
    ULONGLONG PacketNumber;
    PUCHAR PacketNumberArray;
    KSTATUS Status;

    //
    // The start of the packet's valid data should point to the 802.11 header.
    //

    DataHeader = Packet->Buffer + Packet->DataOffset;
    CcmpHeader = (PNET80211_CCMP_HEADER)(DataHeader + 1);
    Message = (PUCHAR)(CcmpHeader + 1);
    AuthField = Packet->Buffer + Packet->FooterOffset - NET80211_CCMP_MIC_SIZE;
    MessageLength = AuthField - Message;

    //
    // Get the correct key to use for the decryption.
    //

    KeyId = (CcmpHeader->Flags & NET80211_CCMP_FLAG_KEY_ID_MASK) >>
            NET80211_CCMP_FLAG_KEY_ID_SHIFT;

    Key = Bss->Encryption.Keys[KeyId];
    if (Key == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto DecryptPacketEnd;
    }

    //
    // Construct the AAD based on the 802.11 header.
    //

    Aad.FrameControl = DataHeader->FrameControl &
                       NET80211_AAD_FRAME_CONTROL_DEFAULT_MASK;

    RtlCopyMemory(Aad.Address1,
                  DataHeader->ReceiverAddress,
                  NET80211_ADDRESS_SIZE * 3);

    Aad.SequenceControl = DataHeader->SequenceControl &
                          NET80211_AAD_SEQUENCE_CONTROL_MASK;

    //
    // Construct the CCM nonce. This is based on the packet number retrieved
    // from the CCMP header.
    //

    CcmNonce.Flags = 0;
    RtlCopyMemory(CcmNonce.Address2, Aad.Address2, NET80211_ADDRESS_SIZE);
    NET80211_GET_CCMP_HEADER_PACKET_NUMBER(CcmpHeader, PacketNumber);
    PacketNumberArray = (PUCHAR)&PacketNumber;
    for (Index = 0; Index < NET80211_CCMP_PACKET_NUMBER_SIZE; Index += 1) {
        CcmNonce.PacketNumber[Index] =
               PacketNumberArray[NET80211_CCMP_PACKET_NUMBER_SIZE - 1 - Index];
    }

    //
    // Perform the CCM originator processing to produce the cipher text.
    //

    Status = Net80211pCcmDecrypt(Key,
                                 Message,
                                 MessageLength,
                                 (PUCHAR)&Aad,
                                 sizeof(NET80211_AAD),
                                 (PUCHAR)&CcmNonce,
                                 sizeof(NET80211_CCM_NONCE),
                                 AuthField,
                                 NET80211_CCMP_MIC_SIZE,
                                 NET80211_CCMP_LENGTH_FIELD_SIZE);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: Failed to decrypt packet 0x%08x for link "
                      "0x%08x.\n",
                      Packet,
                      Link);

        goto DecryptPacketEnd;
    }

    //
    // Compare the packet number to the replay counter and toss the packet if
    // if it's number is too low.
    //

    if (PacketNumber <= Key->ReplayCounter) {
        Status = STATUS_TOO_LATE;
        goto DecryptPacketEnd;
    }

    Key->ReplayCounter = PacketNumber;

    //
    // Move past both the encryption header and the data header. Fully
    // recreating a decrypted packet is not useful unless tooling requires it.
    //

    Packet->DataOffset += sizeof(NET80211_DATA_FRAME_HEADER) +
                          sizeof(NET80211_CCMP_HEADER);

    Packet->FooterOffset -= NET80211_CCMP_MIC_SIZE;

DecryptPacketEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Net80211pCcmEncrypt (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    )

/*++

Routine Description:

    This routine performs CCM originator processing on the given plaintext
    message, updating it in place with the corresponding encrypted text.

Arguments:

    Key - Supplies a pointer to the temporal key to use for CCM encryption.

    Message - Supplies a pointer to the plaintext message that is to be
        encrypted in place.

    MessageLength - Supplies the length of the plaintext message, in bytes.

    Aad - Supplies a pointer to the additional authentication data (AAD).

    AadLength - Supplies the length of the AAD, in bytes.

    Nonce - Supplies a pointer to the nonce value to use for CCM encryption.

    NonceLength - Supplies the length of the nonce value, in bytes.

    AuthenticationField - Supplies a pointer to a buffer that is to receive
        the authentication bytes for the CCM encryption (i.e. the MIC).

    AuthenticationFieldSize - Supplies the length of the authentication field
        in bytes.

    LengthFieldSize - Supplies the size, in bytes, of the CCM length field.

Return Value:

    Status code.

--*/

{

    AES_CONTEXT AesContext;
    UCHAR AesInitializationVector[AES_INITIALIZATION_VECTOR_SIZE];
    ULONG BlockCount;
    UCHAR BlockIn[AES_BLOCK_SIZE];
    ULONG BytesRemaining;
    ULONG Index;

    ASSERT(NonceLength >= (AES_BLOCK_SIZE - 1 - LengthFieldSize));

    //
    // Compute the authentication field and store it in the input block, as it
    // used as the first input for the encryption process.
    //

    Net80211pCcmComputeAuthenticationField(Key,
                                           Message,
                                           MessageLength,
                                           Aad,
                                           AadLength,
                                           Nonce,
                                           NonceLength,
                                           BlockIn,
                                           AuthenticationFieldSize,
                                           LengthFieldSize);

    //
    // Initialize the AES context in counter mode.
    //

    AesInitializationVector[0] = LengthFieldSize - 1;
    RtlCopyMemory(AesInitializationVector + 1,
                  Nonce,
                  (AES_BLOCK_SIZE - 1 - LengthFieldSize));

    for (Index = AES_INITIALIZATION_VECTOR_SIZE - LengthFieldSize;
         Index < AES_INITIALIZATION_VECTOR_SIZE;
         Index += 1) {

        AesInitializationVector[Index] = 0;
    }

    CyAesInitialize(&AesContext,
                    AesModeCtr128,
                    Key->Value,
                    AesInitializationVector);

    //
    // The first block encrypts the authentication field to get the
    // authentication value.
    //

    CyAesCtrEncrypt(&AesContext, BlockIn, BlockIn, AES_BLOCK_SIZE);
    RtlCopyMemory(AuthenticationField, BlockIn, AuthenticationFieldSize);

    //
    // The message is now encrypted with the rest of the counter sequence.
    // Because of how the counter algorithm works, this can actually be done
    // in-place.
    //

    BlockCount = MessageLength / AES_BLOCK_SIZE;
    BytesRemaining = MessageLength % AES_BLOCK_SIZE;
    CyAesCtrEncrypt(&AesContext, Message, Message, BlockCount * AES_BLOCK_SIZE);

    //
    // If there are leftover bytes, then copy them to a local block, perform
    // the encryption and then copy back the ciphertext.
    //

    if (BytesRemaining != 0) {
        RtlCopyMemory(BlockIn,
                      Message + (BlockCount * AES_BLOCK_SIZE),
                      BytesRemaining);

        CyAesCtrEncrypt(&AesContext, BlockIn, BlockIn, AES_BLOCK_SIZE);
        RtlCopyMemory(Message + (BlockCount * AES_BLOCK_SIZE),
                      BlockIn,
                      BytesRemaining);
    }

    return STATUS_SUCCESS;
}

KSTATUS
Net80211pCcmDecrypt (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    )

/*++

Routine Description:

    This routine performs CCM recipient processing on the given ciphertext
    message, updating it in place with the corresponding decrypted text.

Arguments:

    Key - Supplies a pointer to the temporal key to use for CCM decryption.

    Message - Supplies a pointer to the encrypted message that is to be
        decrypted in place.

    MessageLength - Supplies the length of the ciphertext message, in bytes.

    Aad - Supplies a pointer to the additional authentication data (AAD).

    AadLength - Supplies the length of the AAD, in bytes.

    Nonce - Supplies a pointer to the nonce value to use for CCM encryption.

    NonceLength - Supplies the length of the nonce value, in bytes.

    AuthenticationField - Supplies a pointer to the authentication bytes for
        the CCM decryption (i.e. the MIC).

    AuthenticationFieldSize - Supplies the length of the authentication field
        in bytes.

    LengthFieldSize - Supplies the size, in bytes, of the CCM length field.

Return Value:

    Status code.

--*/

{

    AES_CONTEXT AesContext;
    UCHAR AesInitializationVector[AES_INITIALIZATION_VECTOR_SIZE];
    ULONG BlockCount;
    UCHAR BlockIn[AES_BLOCK_SIZE];
    ULONG BytesRemaining;
    ULONG Index;
    UCHAR LocalAuthenticationField[NET80211_CCM_MAX_AUTHENTICATION_FIELD_SIZE];
    BOOL Match;

    ASSERT(NonceLength >= (AES_BLOCK_SIZE - 1 - LengthFieldSize));
    ASSERT(MessageLength < MAX_USHORT);

    //
    // Initialize the AES context in CTR mode.
    //

    AesInitializationVector[0] = LengthFieldSize - 1;
    RtlCopyMemory(AesInitializationVector + 1,
                  Nonce,
                  (AES_BLOCK_SIZE - 1 - LengthFieldSize));

    for (Index = AES_INITIALIZATION_VECTOR_SIZE - LengthFieldSize;
         Index < AES_INITIALIZATION_VECTOR_SIZE;
         Index += 1) {

        AesInitializationVector[Index] = 0;
    }

    CyAesInitialize(&AesContext,
                    AesModeCtr128,
                    Key->Value,
                    AesInitializationVector);

    //
    // The authentication value passes through the counter decryption first
    // to recompute the authentification field. The uninitialized portions of
    // the supplied block do not need to be zeroed.
    //

    RtlCopyMemory(BlockIn, AuthenticationField, AuthenticationFieldSize);
    CyAesCtrDecrypt(&AesContext, BlockIn, BlockIn, AES_BLOCK_SIZE);
    RtlCopyMemory(AuthenticationField, BlockIn, AuthenticationFieldSize);

    //
    // The message is now decrypted with the rest of the counter sequence.
    // Because of how the counter algorithm works, this can actually be done
    // in-place.
    //

    BlockCount = MessageLength / AES_BLOCK_SIZE;
    BytesRemaining = MessageLength % AES_BLOCK_SIZE;
    CyAesCtrDecrypt(&AesContext, Message, Message, BlockCount * AES_BLOCK_SIZE);

    //
    // If there are leftover bytes, then copy them to a local block, perform
    // the decryption and then copy back the ciphertext.
    //

    if (BytesRemaining != 0) {
        RtlCopyMemory(BlockIn,
                      Message + (BlockCount * AES_BLOCK_SIZE),
                      BytesRemaining);

        CyAesCtrDecrypt(&AesContext, BlockIn, BlockIn, AES_BLOCK_SIZE);
        RtlCopyMemory(Message + (BlockCount * AES_BLOCK_SIZE),
                      BlockIn,
                      BytesRemaining);
    }

    //
    // Compute the authentication field for the now decrypted message and
    // compare it to the recomputed authentication field.
    //

    Net80211pCcmComputeAuthenticationField(Key,
                                           Message,
                                           MessageLength,
                                           Aad,
                                           AadLength,
                                           Nonce,
                                           NonceLength,
                                           LocalAuthenticationField,
                                           AuthenticationFieldSize,
                                           LengthFieldSize);

    Match = RtlCompareMemory(AuthenticationField,
                             LocalAuthenticationField,
                             AuthenticationFieldSize);

    if (Match != FALSE) {
        return STATUS_SUCCESS;
    }

    RtlDebugPrint("802.11: CCM decryption found a bad authentication value!\n");
    return STATUS_UNSUCCESSFUL;
}

VOID
Net80211pCcmComputeAuthenticationField (
    PNET80211_KEY Key,
    PUCHAR Message,
    ULONG MessageLength,
    PUCHAR Aad,
    ULONG AadLength,
    PUCHAR Nonce,
    ULONG NonceLength,
    PUCHAR AuthenticationField,
    ULONG AuthenticationFieldSize,
    ULONG LengthFieldSize
    )

/*++

Routine Description:

    This routine computes the authentication field for the given plaintext
    message, additional information, and key. This is used for both encryption
    and decryption as a MIC at the end of the packet.

Arguments:

    Key - Supplies a pointer to the temporal key to use for CBC encryption.

    Message - Supplies a pointer to the plaintext message that is to be
        used as input to compute the authentication field.

    MessageLength - Supplies the length of the plaintext message, in bytes.

    Aad - Supplies a pointer to the additional authentication data (AAD).

    AadLength - Supplies the length of the AAD, in bytes.

    Nonce - Supplies a pointer to the nonce value to use.

    NonceLength - Supplies the length of the nonce value, in bytes.

    AuthenticationField - Supplies a pointer to a buffer that is to receive
        the authentication field for the CCM encryption (i.e. the MIC).

    AuthenticationFieldSize - Supplies the length of the authentication field
        in bytes.

    LengthFieldSize - Supplies the size, in bytes, of the CCM length field.

Return Value:

    None.

--*/

{

    ULONG AadIndex;
    AES_CONTEXT AesContext;
    UCHAR BlockIn[AES_BLOCK_SIZE];
    ULONG BlockIndex;
    UCHAR BlockOut[AES_BLOCK_SIZE];
    ULONG BytesRemaining;
    ULONG BytesThisRound;
    UCHAR Flags;
    ULONG Index;
    ULONG MessageIndex;

    ASSERT(AuthenticationFieldSize <=
           NET80211_CCM_MAX_AUTHENTICATION_FIELD_SIZE);

    ASSERT((LengthFieldSize >= NET80211_CCM_MIN_LENGTH_FIELD_SIZE) &&
           (LengthFieldSize <= NET80211_CCM_MAX_LENGTH_FIELD_SIZE));

    //
    // Initialize the AES context for CBC mode.
    //

    CyAesInitialize(&AesContext, AesModeCbc128, Key->Value, NULL);

    //
    // Initialize the first block based on the length field size,
    // authentication field size, nonce, and message length.
    //

    Flags = ((AuthenticationFieldSize - 2) / 2) <<
            NET80211_CCM_FLAG_AUTHENTICATION_FIELD_SHIFT;

    Flags |= (LengthFieldSize - 1) << NET80211_CCM_FLAG_LENGTH_SHIFT;
    if (AadLength != 0) {
        Flags |= NET80211_CCM_FLAG_AAD;
    }

    BlockIn[0] = Flags;
    RtlCopyMemory(BlockIn + 1, Nonce, (AES_BLOCK_SIZE - 1 - LengthFieldSize));
    for (Index = LengthFieldSize; Index > 0; Index -= 1) {
        BlockIn[AES_BLOCK_SIZE - Index] = ((PUCHAR)&MessageLength)[Index - 1];
    }

    //
    // Encrypt the first output block. Because this is a CBC algorithm that can
    // be called multiple times, the AES library internally remembers the last
    // out block and will XOR that with the next supplied in block before
    // encrypting.
    //

    CyAesCbcEncrypt(&AesContext, BlockIn, BlockOut, AES_BLOCK_SIZE);

    //
    // If an AAD was supplied, then that makes up the second block.
    //

    if (AadLength != 0) {
        if (AadLength <= NET80211_CCM_AAD_MAX_SHORT_LENGTH) {
            WRITE_UNALIGNED16(&(BlockIn[0]), CPU_TO_NETWORK16(AadLength));
            BlockIndex = 2;

        } else {
            WRITE_UNALIGNED16(&(BlockIn[0]), NET80211_CCM_AAD_LONG_ENCODING);
            WRITE_UNALIGNED32(&(BlockIn[2]), CPU_TO_NETWORK32(AadLength));
            BlockIndex = 6;
        }

        AadIndex = 0;
        BytesRemaining = AadLength;
        while (BytesRemaining != 0) {
            BytesThisRound = BytesRemaining;
            if (BytesThisRound > (AES_BLOCK_SIZE - BlockIndex)) {
                BytesThisRound = AES_BLOCK_SIZE - BlockIndex;
            }

            RtlCopyMemory(BlockIn + BlockIndex,
                          Aad + AadIndex,
                          BytesThisRound);

            BlockIndex += BytesThisRound;
            AadIndex += BytesThisRound;
            BytesRemaining -= BytesThisRound;

            //
            // Pad the block with zeros if necessary.
            //

            if (BlockIndex != AES_BLOCK_SIZE) {

                ASSERT(BytesRemaining == 0);

                RtlZeroMemory(BlockIn + BlockIndex,
                              AES_BLOCK_SIZE - BlockIndex);
            }

            //
            // Encrypt this block. It will get XOR'd with the previous block,
            // which is stored internally to the AES context.
            //

            CyAesCbcEncrypt(&AesContext, BlockIn, BlockOut, AES_BLOCK_SIZE);
            BlockIndex = 0;
        }
    }

    //
    // Fold the message into the computation. This must not modify the contents
    // of the message buffer. As a result, it is done block-by-block.
    //

    MessageIndex = 0;
    BytesRemaining = MessageLength;
    while (BytesRemaining != 0) {
        BytesThisRound = BytesRemaining;
        if (BytesThisRound > (AES_BLOCK_SIZE - BlockIndex)) {
            BytesThisRound = AES_BLOCK_SIZE - BlockIndex;
        }

        RtlCopyMemory(BlockIn, Message + MessageIndex, BytesThisRound);
        MessageIndex += BytesThisRound;
        BytesRemaining -= BytesThisRound;

        //
        // Pad the block with zeros if necessary.
        //

        if (BytesThisRound != AES_BLOCK_SIZE) {

            ASSERT(BytesRemaining == 0);

            RtlZeroMemory(BlockIn + BytesThisRound,
                          AES_BLOCK_SIZE - BytesThisRound);
        }

        //
        // Encrypt this block. It will get XOR'd with the previous block, which
        // is stored internally to the AES context.
        //

        CyAesCbcEncrypt(&AesContext, BlockIn, BlockOut, AES_BLOCK_SIZE);
    }

    //
    // The out block now stores the authentication field.
    //

    RtlCopyMemory(AuthenticationField, BlockOut, AuthenticationFieldSize);
    return;
}

VOID
Net80211pEapolCompletionRoutine (
    PVOID Context,
    KSTATUS Status
    )

/*++

Routine Description:

    This routine is called when an EAPOL exchange completes. It is supplied by
    the creator of the EAPOL instance.

Arguments:

    Context - Supplies a pointer to the context supplied by the creator of the
        EAPOL instance.

    Status - Supplies the completion status of the EAPOL exchange.

Return Value:

    None.

--*/

{

    PNET80211_LINK Link;
    NET80211_STATE State;

    Link = (PNET80211_LINK)Context;
    State = Net80211StateEncrypted;
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("802.11: EAPOL failed with status %d\n", Status);
        State = Net80211StateInitialized;
    }

    Net80211pSetState(Link, State);
    return;
}

