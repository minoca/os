/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    eapol.h

Abstract:

    This header contains definitions for the Extensible Authentication Protocol
    over LAN, which facilitates authenticating nodes over a secured network.

Author:

    Chris Stevens 4-Nov-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum EAPOL_MODE {
    EapolModeInvalid,
    EapolModeSupplicant,
    EapolModeAuthenticator
} EAPOL_MODE, *PEAPOL_MODE;

typedef
VOID
(*PEAPOL_COMPLETION_ROUTINE) (
    PVOID Context,
    KSTATUS Status
    );

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

/*++

Structure Description:

    This structure defines the parameters required to create an EAPOL instance.

Members:

    Mode - Store the mode in which this EAPOL instance should act.

    NetworkLink - Stores a pointer to the network link over which this EAPOL
        instance will send and receive data.

    Net80211Link - Stores a pointer to the 802.11 link over which this EAPOL
        instance will send and receive data.

    SupplicantAddress - Stores the physical address of the EAPOL supplicant.

    AuthenticatorAddress - Stores the physical address of the EAPOL
        authenticator.

    Ssid - Stores a pointer to the SSID of the BSS for which the authentication
        is taking place.

    SsidLength - Stores the length of the SSID.

    Passphrase - Stores a pointer to the passphrase for the BSS.

    PassphraseLength - Stores the length of the passphrase.

    SupplicantRsn - Stores the RSN information from the supplicant's IEEE
        802.11 association request packet.

    SupplicantRsnSize - Stores the size of the supplicant's RSN information.

    AuthenticatorRsn - Stores the RSN information from the authenticator's
        IEEE 802.11 beacon packet or probe response packet.

    AuthenticatorRsnSize - Stores the size of the authenticator's RSN
        information.

    CompletionRoutine - Stores a pointer to the completion routine.

    CompletionContext - Stores a pointer to the completion context.

--*/

typedef struct _EAPOL_CREATION_PARAMETERS {
    EAPOL_MODE Mode;
    PNET_LINK NetworkLink;
    PNET80211_LINK Net80211Link;
    PNETWORK_ADDRESS SupplicantAddress;
    PNETWORK_ADDRESS AuthenticatorAddress;
    PUCHAR Ssid;
    ULONG SsidLength;
    PUCHAR Passphrase;
    ULONG PassphraseLength;
    PUCHAR SupplicantRsn;
    ULONG SupplicantRsnSize;
    PUCHAR AuthenticatorRsn;
    ULONG AuthenticatorRsnSize;
    PEAPOL_COMPLETION_ROUTINE CompletionRoutine;
    PVOID CompletionContext;
} EAPOL_CREATION_PARAMETERS, *PEAPOL_CREATION_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Net80211pEapolCreateInstance (
    PEAPOL_CREATION_PARAMETERS Parameters,
    PHANDLE EapolHandle
    );

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

VOID
Net80211pEapolDestroyInstance (
    HANDLE EapolHandle
    );

/*++

Routine Description:

    This routine destroys the given EAPOL instance.

Arguments:

    EapolHandle - Supplies the handle to the EAPOL instance to destroy.

Return Value:

    None.

--*/

