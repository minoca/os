/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    net.h

Abstract:

    This header contains the device information structure format for
    networking devices.

Author:

    Evan Green 9-May-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define NETWORK_DEVICE_INFORMATION_UUID \
    {{0x0EF6E8C6, 0xAE4B4B90, 0xA2D2D0F7, 0x9BE9F31A}}

#define NETWORK_DEVICE_INFORMATION_VERSION 0x00010000

//
// Define network device information flags.
//

//
// This flag is set if the device is connected to some sort of network.
//

#define NETWORK_DEVICE_FLAG_MEDIA_CONNECTED 0x00000001

//
// This flag is set if the device has a network address entry and is configured.
//

#define NETWORK_DEVICE_FLAG_CONFIGURED 0x00000002

//
// Define the maximum number of DNS servers to remember.
//

#define NETWORK_DEVICE_MAX_DNS_SERVERS 4

//
// Define the UUID and version for the 802.11 networking device information.
//

#define NETWORK_80211_DEVICE_INFORMATION_UUID \
    {{0xc927b054, 0xead311e5, 0x8ea20401, 0x0fdd7401}}

#define NETWORK_80211_DEVICE_INFORMATION_VERSION 0x00010000

//
// Define the 802.11 network device information flags.
//

#define NETWORK_80211_DEVICE_FLAG_ASSOCIATED 0x00000001

//
// Define the maximum length of an SSID.
//

#define NETWORK_80211_MAX_SSID_LENGTH 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NETWORK_ADDRESS_CONFIGURATION_METHOD {
    NetworkAddressConfigurationInvalid,
    NetworkAddressConfigurationNone,
    NetworkAddressConfigurationStatic,
    NetworkAddressConfigurationDhcp,
    NetworkAddressConfigurationStateless,
} NETWORK_ADDRESS_CONFIGURATION_METHOD, *PNETWORK_ADDRESS_CONFIGURATION_METHOD;

/*++

Structure Description:

    This structure defines the information published by networking devices.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to NETWORK_DEVICE_INFORMATION_VERSION.

    Flags - Stores a bitfield of flags describing the network device. See
        NETWORK_DEVICE_FLAG_* definitions.

    Domain - Stores the socket network domain for which this information is
        valid. Network devices may be active on more than network domain
        simultaneously (IPv4 and IPv6 for example). The caller sets this to
        request information about a given network domain's configuration.

    ConfigurationMethod - Stores the method used to configure the address of
        this device.

    Address - Stores the network address of the link.

    Subnet - Stores the network subnet mask of the link.

    Gateway - Stores the default gateway network address for the link.

    DnsServer - Stores an array of network addresses of Domain Name Servers
        to try, in order.

    DnsServerCount - Stores the number of valid DNS servers in the array.

    PhysicalAddress - Stores the physical address of the link.

    LeaseServerAddress - Stores the network address of the server who provided
        the network address if it is a dynamic address.

    LeaseStartTime - Stores the time the lease on the network address began.
        This is only valid for dynamic address configuration methods.

    LeaseEndTime - Stores the time the lease on the network address ends. This
        is only valid for dynamic address configuration methods.

--*/

typedef struct _NETWORK_DEVICE_INFORMATION {
    ULONG Version;
    ULONG Flags;
    NET_DOMAIN_TYPE Domain;
    NETWORK_ADDRESS_CONFIGURATION_METHOD ConfigurationMethod;
    NETWORK_ADDRESS Address;
    NETWORK_ADDRESS Subnet;
    NETWORK_ADDRESS Gateway;
    NETWORK_ADDRESS DnsServers[NETWORK_DEVICE_MAX_DNS_SERVERS];
    ULONG DnsServerCount;
    NETWORK_ADDRESS PhysicalAddress;
    NETWORK_ADDRESS LeaseServerAddress;
    SYSTEM_TIME LeaseStartTime;
    SYSTEM_TIME LeaseEndTime;
} NETWORK_DEVICE_INFORMATION, *PNETWORK_DEVICE_INFORMATION;

typedef enum _NETWORK_ENCRYPTION_TYPE {
    NetworkEncryptionNone,
    NetworkEncryptionWep,
    NetworkEncryptionWpaPsk,
    NetworkEncryptionWpaEap,
    NetworkEncryptionWpa2Psk,
    NetworkEncryptionWpa2Eap,
    NetworkEncryptionInvalid
} NETWORK_ENCRYPTION_TYPE, *PNETWORK_ENCRYPTION_TYPE;

/*++

Structure Description:

    This structure defines the information published by 802.11 networking
    devices.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to NETWORK_80211_DEVICE_INFORMATION_VERSION.

    Flags - Stores a bitfield of flags describing the 802.11 network device.
        See NETWORK_80211_DEVICE_FLAG_* for definitions.

    PhysicalAddress - Stores the physical address of the link.

    Bssid - Stores the BSSID of access point to which the device is associated,
        if applicable.

    Ssid - Stores the null-terminated SSID of the associated network.

    Channel - Stores the channel on which the network operates.

    MaxRate - Stores the maximum rate supported by the wireless network, in
        megabits per second.

    Rssi - Stores the received signal strength indication value for the BSS.

    PairwiseEncryption - Stores the pairwise encryption method used for the
        network connection.

    GroupEncryption - Stores the group encryption method used for the network
        connection.

--*/

typedef struct _NETWORK_80211_DEVICE_INFORMATION {
    ULONG Version;
    ULONG Flags;
    NETWORK_ADDRESS PhysicalAddress;
    NETWORK_ADDRESS Bssid;
    UCHAR Ssid[NETWORK_80211_MAX_SSID_LENGTH + 1];
    ULONG Channel;
    ULONGLONG MaxRate;
    LONG Rssi;
    NETWORK_ENCRYPTION_TYPE PairwiseEncryption;
    NETWORK_ENCRYPTION_TYPE GroupEncryption;
} NETWORK_80211_DEVICE_INFORMATION, *PNETWORK_80211_DEVICE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

