/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
#include <minoca/crypto.h>

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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

NET80211_API
KSTATUS
Net80211SetKey (
    PNET_LINK Link,
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
    PNET80211_KEY Key;
    PNET80211_LINK Net80211Link;
    PNET80211_KEY OldKey;
    KSTATUS Status;

    //
    // Make sure the key ID is valid and supported. The CCMP header only has
    // two bits for the key ID.
    //

    if (KeyId >= NET80211_MAX_KEY_COUNT) {
        return STATUS_INVALID_PARAMETER;
    }

    Net80211Link = Link->DataLinkContext;
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
    // Update the pointer in the array of keys.
    //

    KeAcquireQueuedLock(Net80211Link->Lock);
    OldKey = Net80211Link->Keys[KeyId];
    Net80211Link->Keys[KeyId] = Key;
    KeReleaseQueuedLock(Net80211Link->Lock);
    if (OldKey != NULL) {
        MmFreePagedPool(OldKey);
    }

    Status = STATUS_SUCCESS;

SetKeyEnd:
    if (!KSUCCESS(Status)) {
        if (Key != NULL) {
            MmFreePagedPool(Key);
        }
    }

    return Status;
}

KSTATUS
Net80211pEncryptPacket (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine encrypts the given network packet's plaintext data. The
    supplied packet buffer is modified directly and should already include the
    full MPDU (i.e. the 802.11 headers should be present).

Arguments:

    Link - Supplies a pointer to the 802.11 network link that owns the packet.

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
    // Use the default key.
    //

    KeyId = NET80211_DEFAULT_ENCRYPTION_KEY;
    Key = Link->Keys[KeyId];
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
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine decrypts the given network packet's ciphertext. The supplied
    packet buffer is modified directly and should contain the full encrypted
    MPDU, including the 802.11 headers.

Arguments:

    Link - Supplies a pointer to the 802.11 network link that owns the packet.

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

    Key = Link->Keys[KeyId];
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
            *((PUSHORT)&(BlockIn[0])) = CPU_TO_NETWORK16(AadLength);
            BlockIndex = 2;

        } else {
            *((PUSHORT)&(BlockIn[0])) = NET80211_CCM_AAD_LONG_ENCODING;
            *((PULONG)&(BlockIn[2])) = CPU_TO_NETWORK32(AadLength);
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

