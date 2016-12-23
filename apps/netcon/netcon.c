/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netcon.c

Abstract:

    This module implements the network configuration program.

Author:

    Chris Stevens 14-Mar-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/net80211.h>
#include <minoca/net/netlink.h>
#include <minoca/lib/mlibc.h>
#include <minoca/lib/netlink.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//
// ---------------------------------------------------------------- Definitions
//

#define NETCON_VERSION_MAJOR 1
#define NETCON_VERSION_MINOR 0

#define NETCON_USAGE                                                           \
    "usage: netcon [-d device] [-j ssid -p] [-l] [-s] [-v]\n\n"                \
    "The netcon utility configures network devices.\n\n"                       \
    "Options:\n"                                                               \
    "  -d --device=device -- Specifies the network device to configure.\n"     \
    "      This is optional for wireless commands if there is only 1\n"        \
    "      wireless device on the system.\n"                                   \
    "  -j --join=ssid -- Attempts to join the given wireless network.\n"       \
    "  -l --leave -- Attempts to leave the current wireless network.\n"        \
    "  -p --password -- Indicates that the user wants to be prompted for a\n"  \
    "      password during a join operation.\n"                                \
    "  -s --scan -- Displays the list of wireless networks available to\n"     \
    "      the network device specified by -d.\n"                              \
    "  -v --verbose -- Display more detailed information.\n"                   \
    "  --help -- Display this help text.\n"                                    \
    "  --version -- Display the application version and exit.\n\n"

#define NETCON_OPTIONS_STRING "d:j:lspvh"

//
// Define the set of network configuration flags.
//

#define NETCON_FLAG_DEVICE_ID 0x00000001
#define NETCON_FLAG_JOIN      0x00000002
#define NETCON_FLAG_LEAVE     0x00000004
#define NETCON_FLAG_PASSWORD  0x00000008
#define NETCON_FLAG_SCAN      0x00000010
#define NETCON_FLAG_VERBOSE   0x00000020

#define NETCON_FLAG_WIRELESS_MASK \
    (NETCON_FLAG_JOIN | NETCON_FLAG_LEAVE | NETCON_FLAG_SCAN)

//
// Define the set of network device description flags.
//

#define NETCON_DEVICE_FLAG_IP4   0x00000001
#define NETCON_DEVICE_FLAG_IP6   0x00000002
#define NETCON_DEVICE_FLAG_80211 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for network configuration requests.

Members:

    Flags - Stores a bitmask of network configuration flags. See NETCON_FLAG_*
        for definitions.

    Device - Stores the ID of the network device that is being configured.

    Ssid - Stores a pointer to the SSID of the network to join.

--*/

typedef struct _NETCON_CONTEXT {
    ULONG Flags;
    DEVICE_ID DeviceId;
    PSTR Ssid;
} NETCON_CONTEXT, *PNETCON_CONTEXT;

/*++

Structure Description:

    This structure defines a network device.

Members:

    Flags - Stores a bitmask of flags. See NETCON_DEVICE_FLAG_* for definitions.

    DeviceId - Stores the network device's ID.

    NetworkIp4 - Stores the IPv4 network information.

    NetworkIp6 - Stores the IPv6 network information.

    Net80211 - Stores the 802.11 device information.

--*/

typedef struct _NETCON_DEVICE_DESCRIPTION {
    ULONG Flags;
    DEVICE_ID DeviceId;
    NETWORK_DEVICE_INFORMATION NetworkIp4;
    NETWORK_DEVICE_INFORMATION NetworkIp6;
    NETWORK_80211_DEVICE_INFORMATION Net80211;
} NETCON_DEVICE_DESCRIPTION, *PNETCON_DEVICE_DESCRIPTION;

/*++

Structure Description:

    This structure defines a BSS scan result.

Members:

    Bssid - Stores the BSSID.

    SignalStrength - Stores the BSS's signal strength in MBM's.

    Status - Stores the BSS's status.

    Capability - Stores the BSS's capability flags.

    BeaconInterval - Stores the BSS's beacon interval.

    Elements - Stores a pointer to the BSS's information elements. These
        include the SSID, channel, and security information.

    ElementsSize - Stores the size of the information elements, in bytes.

--*/

typedef struct _NETCON_BSS {
    NETWORK_ADDRESS Bssid;
    LONG SignalStrength;
    ULONG Status;
    USHORT BeaconInterval;
    USHORT Capabilities;
    PVOID Elements;
    ULONG ElementsSize;
} NETCON_BSS, *PNETCON_BSS;

/*++

Structure Description:

    This structure defines a set of scan results.

Members:

    Valid - Stores a boolean indicating whether or not the results are valid.

    BssCount - Stores the number of BSS element pointers in the array.

    BssArray - Stores an array of pointers to the BSS elements.

--*/

typedef struct _NETCON_SCAN_RESULTS {
    BOOL Valid;
    ULONG BssCount;
    PNETCON_BSS *BssArray;
} NETCON_SCAN_RESULTS, *PNETCON_SCAN_RESULTS;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetconListDevices (
    VOID
    );

INT
NetconEnumerateDevices (
    PNETCON_DEVICE_DESCRIPTION *DeviceArray,
    PULONG DeviceCount
    );

INT
NetconGetDeviceInformation (
    DEVICE_ID DeviceId,
    PNETCON_DEVICE_DESCRIPTION Device
    );

VOID
NetconJoinNetwork (
    PNETCON_CONTEXT Context
    );

VOID
NetconLeaveNetwork (
    PNETCON_CONTEXT Context
    );

VOID
NetconScanForNetworks (
    PNETCON_CONTEXT Context
    );

VOID
NetconParseScanNotification (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    );

VOID
NetconParseScanResult (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    );

VOID
NetconPrintScanResults (
    PNETCON_CONTEXT Context,
    PNETCON_SCAN_RESULTS Results
    );

PVOID
NetconGet80211InformationElement (
    PVOID Elements,
    ULONG ElementsSize,
    UCHAR ElementId
    );

VOID
NetconPrintRsnInformation (
    PUCHAR Rsn
    );

VOID
NetconPrintDeviceInformation (
    PNETCON_DEVICE_DESCRIPTION Device
    );

VOID
NetconPrintAddress (
    PNETWORK_ADDRESS Address
    );

VOID
NetconPrintEncryption (
    NETWORK_ENCRYPTION_TYPE EncryptionType
    );

VOID
NetconPrintCipherSuite (
    ULONG Suite
    );

VOID
NetconPrintRssi (
    LONG Rssi
    );

VOID
NetconPrintRates (
    PVOID RatesElement
    );

INT
NetconGet80211DeviceId (
    PDEVICE_ID DeviceId
    );

//
// -------------------------------------------------------------------- Globals
//

struct option NetconLongOptions[] = {
    {"device", required_argument, 0, 'd'},
    {"join", required_argument, 0, 'j'},
    {"leave", no_argument, 0, 'l'},
    {"password", no_argument, 0, 'p'},
    {"scan", no_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

UUID NetconDeviceInformationUuid = NETWORK_DEVICE_INFORMATION_UUID;
UUID Netcon80211DeviceInformationUuid = NETWORK_80211_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the network configuration user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    NETCON_CONTEXT Context;
    NETCON_DEVICE_DESCRIPTION Device;
    INT Option;
    INT ReturnValue;

    memset(&Context, 0, sizeof(NETCON_CONTEXT));
    ReturnValue = 0;
    NlInitialize(NULL);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             NETCON_OPTIONS_STRING,
                             NetconLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'd':
            Context.DeviceId = strtoull(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid device id '%s'.\n", optarg);
                ReturnValue = EINVAL;
                goto mainEnd;
            }

            Context.Flags |= NETCON_FLAG_DEVICE_ID;
            break;

        case 'j':
            Context.Ssid = optarg;
            Context.Flags |= NETCON_FLAG_JOIN;
            break;

        case 'l':
            Context.Flags |= NETCON_FLAG_LEAVE;
            break;

        case 's':
            Context.Flags |= NETCON_FLAG_SCAN;
            break;

        case 'p':
            Context.Flags |= NETCON_FLAG_PASSWORD;
            break;

        case 'v':
            Context.Flags |= NETCON_FLAG_VERBOSE;
            break;

        case 'V':
            printf("netcon version %d.%02d\n",
                   NETCON_VERSION_MAJOR,
                   NETCON_VERSION_MINOR);

            ReturnValue = 1;
            goto mainEnd;

        case 'h':
            printf(NETCON_USAGE);
            return 1;

        default:

            assert(FALSE);

            ReturnValue = 1;
            goto mainEnd;
        }
    }

    //
    // Wireless commands require a device ID to operate. If no ID is specified,
    // then attempt to find one. If there is more than one device present on
    // the system, an error will be printed along with the available devices.
    //

    if (((Context.Flags & NETCON_FLAG_DEVICE_ID) == 0) &&
        ((Context.Flags & NETCON_FLAG_WIRELESS_MASK) != 0)) {

        ReturnValue = NetconGet80211DeviceId(&(Context.DeviceId));
        if (ReturnValue != 0) {
            goto mainEnd;
        }

        Context.Flags |= NETCON_FLAG_DEVICE_ID;
    }

    //
    // Process the command.
    //

    if ((Context.Flags & NETCON_FLAG_JOIN) != 0) {
        NetconJoinNetwork(&Context);

    } else if ((Context.Flags & NETCON_FLAG_LEAVE) != 0) {
        NetconLeaveNetwork(&Context);

    } else if ((Context.Flags & NETCON_FLAG_SCAN) != 0) {
        NetconScanForNetworks(&Context);

    } else if ((Context.Flags & NETCON_FLAG_DEVICE_ID) != 0) {
        ReturnValue = NetconGetDeviceInformation(Context.DeviceId, &Device);
        if (ReturnValue != 0) {
            goto mainEnd;
        }

        NetconPrintDeviceInformation(&Device);

    } else {
        NetconListDevices();
    }

mainEnd:
    if (ReturnValue == EINVAL) {
        printf(NETCON_USAGE);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetconListDevices (
    VOID
    )

/*++

Routine Description:

    This routine prints the list of available network devices.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PNETCON_DEVICE_DESCRIPTION DeviceArray;
    ULONG DeviceCount;
    ULONG Index;
    INT Result;

    Result = NetconEnumerateDevices(&DeviceArray, &DeviceCount);
    if (Result != 0) {
        return;
    }

    if (DeviceCount == 0) {
        printf("No network devices detected.\n");
        return;
    }

    printf("Minoca Network Configuration:\n\n");
    for (Index = 0; Index < DeviceCount; Index += 1) {
        NetconPrintDeviceInformation(&(DeviceArray[Index]));
        printf("\n");
    }

    free(DeviceArray);
    return;
}

INT
NetconEnumerateDevices (
    PNETCON_DEVICE_DESCRIPTION *DeviceArray,
    PULONG DeviceCount
    )

/*++

Routine Description:

    This routine enumerates all the network devices on the system.

Arguments:

    DeviceArray - Supplies a pointer where an array of network structures
        will be returned on success.

    DeviceCount - Supplies a pointer where the number of elements in the
        partition array will be returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    UINTN AllocationSize;
    UINTN ArrayIndex;
    PNETCON_DEVICE_DESCRIPTION Devices;
    INT Result;
    ULONG ResultCount;
    UINTN ResultIndex;
    PDEVICE_INFORMATION_RESULT Results;
    KSTATUS Status;

    ArrayIndex = 0;
    Devices = NULL;
    ResultCount = 0;
    Results = NULL;

    //
    // Enumerate all the devices that support getting network device
    // information.
    //

    Status = OsLocateDeviceInformation(&NetconDeviceInformationUuid,
                                       NULL,
                                       NULL,
                                       &ResultCount);

    if (Status != STATUS_BUFFER_TOO_SMALL) {
        goto EnumerateDevicesEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto EnumerateDevicesEnd;
    }

    AllocationSize = sizeof(DEVICE_INFORMATION_RESULT) * ResultCount;
    Results = malloc(AllocationSize);
    if (Results == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateDevicesEnd;
    }

    memset(Results, 0, AllocationSize);
    Status = OsLocateDeviceInformation(&NetconDeviceInformationUuid,
                                       NULL,
                                       Results,
                                       &ResultCount);

    if (!KSUCCESS(Status)) {
        goto EnumerateDevicesEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto EnumerateDevicesEnd;
    }

    //
    // Allocate the real array.
    //

    AllocationSize = sizeof(NETCON_DEVICE_DESCRIPTION) * ResultCount;
    Devices = malloc(AllocationSize);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateDevicesEnd;
    }

    memset(Devices, 0, AllocationSize);

    //
    // Loop through the results setting up the structure elements.
    //

    for (ResultIndex = 0; ResultIndex < ResultCount; ResultIndex += 1) {
        Result = NetconGetDeviceInformation(Results[ResultIndex].DeviceId,
                                            &(Devices[ArrayIndex]));

        if (Result != 0) {
            continue;
        }

        ArrayIndex += 1;
    }

    Status = STATUS_SUCCESS;

EnumerateDevicesEnd:
    if (Results != NULL) {
        free(Results);
    }

    if (!KSUCCESS(Status)) {
        if (Devices != NULL) {
            free(Devices);
            Devices = NULL;
        }

        return ClConvertKstatusToErrorNumber(Status);
    }

    *DeviceArray = Devices;
    *DeviceCount = ArrayIndex;
    return 0;
}

INT
NetconGetDeviceInformation (
    DEVICE_ID DeviceId,
    PNETCON_DEVICE_DESCRIPTION Device
    )

/*++

Routine Description:

    This routine retrieves the network information for the given network device.

Arguments:

    DeviceId - Supplies the device ID of the device whose information is to be
        retrieved.

    Device - Supplies a pointer to a network device descriptor that receives
        the retrieved device information.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    UINTN DataSize;
    PNETWORK_80211_DEVICE_INFORMATION Net80211;
    PNETWORK_DEVICE_INFORMATION Network;
    PNETWORK_ADDRESS PhysicalAddress;
    KSTATUS Status;

    Device->Flags = 0;
    Device->DeviceId = DeviceId;
    PhysicalAddress = NULL;

    //
    // Get the IPv4 network information.
    //

    DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
    Network = &(Device->NetworkIp4);
    Network->Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Network->Domain = NetDomainIp4;
    Status = OsGetSetDeviceInformation(Device->DeviceId,
                                       &NetconDeviceInformationUuid,
                                       Network,
                                       &DataSize,
                                       FALSE);

    if (KSUCCESS(Status)) {
        Device->Flags |= NETCON_DEVICE_FLAG_IP4;
        PhysicalAddress = &(Network->PhysicalAddress);
    }

    //
    // Get the IPv6 network information.
    //

    DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
    Network = &(Device->NetworkIp6);
    Network->Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Network->Domain = NetDomainIp6;
    Status = OsGetSetDeviceInformation(Device->DeviceId,
                                       &NetconDeviceInformationUuid,
                                       Network,
                                       &DataSize,
                                       FALSE);

    if (KSUCCESS(Status)) {
        Device->Flags |= NETCON_DEVICE_FLAG_IP6;
        PhysicalAddress = &(Network->PhysicalAddress);
    }

    //
    // If the physical address is an 802.11 address, then attempt to get the
    // 802.11 information.
    //

    if ((PhysicalAddress != NULL) &&
        (PhysicalAddress->Domain == NetDomain80211)) {

        DataSize = sizeof(NETWORK_80211_DEVICE_INFORMATION);
        Net80211 = &(Device->Net80211);
        Net80211->Version = NETWORK_80211_DEVICE_INFORMATION_VERSION;
        Status = OsGetSetDeviceInformation(Device->DeviceId,
                                           &Netcon80211DeviceInformationUuid,
                                           Net80211,
                                           &DataSize,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            goto GetDeviceInformationEnd;
        }

        Device->Flags |= NETCON_DEVICE_FLAG_80211;
    }

    Status = STATUS_SUCCESS;

GetDeviceInformationEnd:
    return ClConvertKstatusToErrorNumber(Status);
}

VOID
NetconJoinNetwork (
    PNETCON_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to join the network specified by the SSID in the
    given netcon context.

Arguments:

    Context - Supplies a pointer to the network connection context.

Return Value:

    None.

--*/

{

    USHORT FamilyId;
    PNL_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    NL_RECEIVE_PARAMETERS Parameters;
    PSTR Password;
    UINTN PasswordLength;
    ULONG PayloadLength;
    PNL_SOCKET Socket;
    UINTN SsidLength;
    INT Status;

    Password = NULL;
    Socket = NULL;

    //
    // Validate the SSID.
    //

    SsidLength = strlen(Context->Ssid) + 1;
    if (SsidLength > (NET80211_MAX_SSID_LENGTH + 1)) {
        printf("netcon: SSID \"%s\" is too long. Max SSID length is %d.\n",
               Context->Ssid,
               NET80211_MAX_SSID_LENGTH);

        errno = EINVAL;
        Status = -1;
        goto JoinNetworkEnd;
    }

    //
    // If a password is required, get it now.
    //

    if ((Context->Flags & NETCON_FLAG_PASSWORD) != 0) {
        Password = getpass("Password: ");
        if (Password == NULL) {
            Status = -1;
            goto JoinNetworkEnd;
        }

        PasswordLength = strlen(Password) + 1;
    }

    Status = NlCreateSocket(NETLINK_GENERIC, NL_ANY_PORT_ID, 0, &Socket);
    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    Status = NlGenericGetFamilyId(Socket,
                                  NETLINK_GENERIC_80211_NAME,
                                  &FamilyId);

    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    //
    // Build out a message to join a network. This requires specifying the
    // device ID of the wireless networking device that is to join the network,
    // the SSID of the network and an optional passphrase.
    //

    PayloadLength = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID)) +
                    NETLINK_ATTRIBUTE_SIZE(SsidLength);

    if (Password != NULL) {
        PayloadLength += NETLINK_ATTRIBUTE_SIZE(PasswordLength);
    }

    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    FamilyId,
                                    0,
                                    NETLINK_80211_COMMAND_JOIN,
                                    0);

    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    Status = NlAppendAttribute(Message,
                               NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                               &(Context->DeviceId),
                               sizeof(DEVICE_ID));

    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    Status = NlAppendAttribute(Message,
                               NETLINK_80211_ATTRIBUTE_SSID,
                               Context->Ssid,
                               SsidLength);

    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    if (Password != NULL) {
        Status = NlAppendAttribute(Message,
                                   NETLINK_80211_ATTRIBUTE_PASSPHRASE,
                                   Password,
                                   PasswordLength);

        if (Status != 0) {
            goto JoinNetworkEnd;
        }
    }

    //
    // Send off the request to join the given network.
    //

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto JoinNetworkEnd;
    }

    //
    // Wait for an acknowledgement message.
    //

    memset(&Parameters, 0, sizeof(NL_RECEIVE_PARAMETERS));
    Parameters.Flags |= NL_RECEIVE_FLAG_PORT_ID;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Status = NlReceiveMessage(Socket, &Parameters);
    if (Status != 0) {
        goto JoinNetworkEnd;
    }

JoinNetworkEnd:
    if (Password != NULL) {
        memset(Password, 0, strlen(Password));
    }

    if (Socket != NULL) {
        NlDestroySocket(Socket);
    }

    if (Status != 0) {
        perror("netcon: failed to join network");
    }

    return;
}

VOID
NetconLeaveNetwork (
    PNETCON_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to leave the network to which the specified device
    is connected. The device is stored in the context structure.

Arguments:

    Context - Supplies a pointer to the network connection context.

Return Value:

    None.

--*/

{

    USHORT FamilyId;
    PNL_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    NL_RECEIVE_PARAMETERS Parameters;
    ULONG PayloadLength;
    PNL_SOCKET Socket;
    INT Status;

    Status = NlCreateSocket(NETLINK_GENERIC, NL_ANY_PORT_ID, 0, &Socket);
    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    Status = NlGenericGetFamilyId(Socket,
                                  NETLINK_GENERIC_80211_NAME,
                                  &FamilyId);

    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    //
    // Build out a request to leave the current network on the given device.
    // This only requires supplying the device ID as an attribute.
    //

    PayloadLength = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    FamilyId,
                                    0,
                                    NETLINK_80211_COMMAND_LEAVE,
                                    0);

    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    Status = NlAppendAttribute(Message,
                               NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                               &(Context->DeviceId),
                               sizeof(DEVICE_ID));

    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    //
    // Send off the request to leave the current network.
    //

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

    //
    // Wait for an acknowledgement message.
    //

    memset(&Parameters, 0, sizeof(NL_RECEIVE_PARAMETERS));
    Parameters.Flags |= NL_RECEIVE_FLAG_PORT_ID;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Status = NlReceiveMessage(Socket, &Parameters);
    if (Status != 0) {
        goto LeaveNetworkEnd;
    }

LeaveNetworkEnd:
    if (Socket != NULL) {
        NlDestroySocket(Socket);
    }

    if (Status != 0) {
        perror("netcon: failed to leave network");
    }

    return;
}

VOID
NetconScanForNetworks (
    PNETCON_CONTEXT Context
    )

/*++

Routine Description:

    This routine scans for all the wireless networks that are within range of
    the device stored in the given context. It then prints them to standard out.

Arguments:

    Context - Supplies a pointer to the network connection context.

Return Value:

    None.

--*/

{

    BOOL AckReceived;
    USHORT FamilyId;
    ULONG Flags;
    PNL_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    NL_RECEIVE_PARAMETERS Parameters;
    ULONG PayloadLength;
    NETCON_SCAN_RESULTS ScanResults;
    BOOL ScanResultsReady;
    PNL_SOCKET Socket;
    INT Status;

    Message = NULL;
    Status = NlCreateSocket(NETLINK_GENERIC, NL_ANY_PORT_ID, 0, &Socket);
    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlGenericGetFamilyId(Socket,
                                  NETLINK_GENERIC_80211_NAME,
                                  &FamilyId);

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    //
    // Join the 802.11 scan multicast group in order to get progress updates
    // from the scan.
    //

    Status = NlGenericJoinMulticastGroup(Socket,
                                         FamilyId,
                                         NETLINK_80211_MULTICAST_SCAN_NAME);

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    //
    // Build and send a request to scan for all networks on the given device.
    // This only requires supplying the device ID as an attribute.
    //

    PayloadLength = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    FamilyId,
                                    0,
                                    NETLINK_80211_COMMAND_SCAN_START,
                                    0);

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlAppendAttribute(Message,
                               NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                               &(Context->DeviceId),
                               sizeof(DEVICE_ID));

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    NlFreeBuffer(Message);
    Message = NULL;

    //
    // Loop waiting for a few possible messages. The scan start request should
    // be acknowledged by the kernel. If the scan is acknowledged, then either
    // a scan aborted or scan results message should arrive. That is not to
    // say that the acknowledge will always come before a scan aborted or
    // results message.
    //

    memset(&Parameters, 0, sizeof(NL_RECEIVE_PARAMETERS));
    Parameters.ReceiveRoutine = NetconParseScanNotification;
    Parameters.ReceiveContext.Type = FamilyId;
    Parameters.ReceiveContext.PrivateContext = &ScanResultsReady;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Flags = NL_RECEIVE_FLAG_PORT_ID;
    ScanResultsReady = FALSE;
    AckReceived = FALSE;
    while ((AckReceived == FALSE) || (ScanResultsReady == FALSE)) {
        Parameters.Flags = Flags;
        Status = NlReceiveMessage(Socket, &Parameters);
        if (Status != 0) {
            goto ScanForNetworksEnd;
        }

        if (Parameters.ReceiveContext.Status != 0) {
            Status = Parameters.ReceiveContext.Status;
            goto ScanForNetworksEnd;
        }

        //
        // If an ACK was received, make sure to stop waiting for one.
        //

        if ((Parameters.Flags & NL_RECEIVE_FLAG_ACK_RECEIVED) != 0) {
            AckReceived = TRUE;
            Flags |= NL_RECEIVE_FLAG_NO_ACK_WAIT;
        }
    }

    //
    // The scan has completed. Get the scan results and print them out for
    // the user. Start by building a scan results get request and sending that
    // off to the kernel.
    //

    Message = NULL;
    PayloadLength = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    FamilyId,
                                    NETLINK_HEADER_FLAG_DUMP,
                                    NETLINK_80211_COMMAND_SCAN_GET_RESULTS,
                                    0);

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlAppendAttribute(Message,
                               NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                               &(Context->DeviceId),
                               sizeof(DEVICE_ID));

    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto ScanForNetworksEnd;
    }

    //
    // Now wait for the ACK and the results messages to come in.
    //

    memset(&Parameters, 0, sizeof(NL_RECEIVE_PARAMETERS));
    memset(&ScanResults, 0, sizeof(NETCON_SCAN_RESULTS));
    Parameters.ReceiveRoutine = NetconParseScanResult;
    Parameters.ReceiveContext.Type = FamilyId;
    Parameters.ReceiveContext.PrivateContext = &ScanResults;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Flags = NL_RECEIVE_FLAG_PORT_ID;
    ScanResultsReady = FALSE;
    AckReceived = FALSE;
    while ((AckReceived == FALSE) || (ScanResults.Valid == FALSE)) {
        Parameters.Flags = Flags;
        Status = NlReceiveMessage(Socket, &Parameters);
        if (Status != 0) {
            goto ScanForNetworksEnd;
        }

        if (Parameters.ReceiveContext.Status != 0) {
            Status = Parameters.ReceiveContext.Status;
            goto ScanForNetworksEnd;
        }

        //
        // If an ACK was received, make sure to stop waiting for one.
        //

        if ((Parameters.Flags & NL_RECEIVE_FLAG_ACK_RECEIVED) != 0) {
            AckReceived = TRUE;
            Flags |= NL_RECEIVE_FLAG_NO_ACK_WAIT;
        }
    }

    //
    // Print out those scan results!
    //

    NetconPrintScanResults(Context, &ScanResults);

ScanForNetworksEnd:
    if (Message != NULL) {
        NlFreeBuffer(Message);
    }

    if (Socket != NULL) {
        NlDestroySocket(Socket);
    }

    if (Status != 0) {
        perror("netcon: failed to scan for networks");
    }

    return;
}

VOID
NetconParseScanNotification (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    )

/*++

Routine Description:

    This routine parses a netlink message looking for a scan notification.

Arguments:

    Socket - Supplies a pointer to the netlink socket that received the message.

    Context - Supplies a pointer to the receive context given to the receive
        message handler.

    Message - Supplies a pointer to the beginning of the netlink message. The
        length of which can be obtained from the header; it was already
        validated.

Return Value:

    None.

--*/

{

    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    ULONG MessageLength;
    PBOOL ScanResultsReady;

    Header = (PNETLINK_HEADER)Message;

    //
    // Skip any messages that are not of the 802.11 family type.
    //

    if (Header->Type != Context->Type) {
        return;
    }

    //
    // Parse the generic header to determine the 802.11 command.
    //

    MessageLength = Header->Length;
    MessageLength -= NETLINK_HEADER_LENGTH;
    if (MessageLength < sizeof(NETLINK_GENERIC_HEADER)) {
        return;
    }

    //
    // Fail the scan request if the system aborted the scan.
    //

    GenericHeader = NETLINK_DATA(Header);
    if (GenericHeader->Command == NETLINK_80211_COMMAND_SCAN_ABORTED) {
        errno = ECANCELED;
        Context->Status = -1;

    //
    // If it is a scan result notification, mark that it has been seen.
    //

    } else if (GenericHeader->Command == NETLINK_80211_COMMAND_SCAN_RESULT) {
        ScanResultsReady = (PBOOL)Context->PrivateContext;
        *ScanResultsReady = TRUE;
        Context->Status = 0;
    }

    return;
}

VOID
NetconParseScanResult (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    )

/*++

Routine Description:

    This routine parses a netlink message looking for a scan result.

Arguments:

    Socket - Supplies a pointer to the netlink socket that received the message.

    Context - Supplies a pointer to the receive context given to the receive
        message handler.

    Message - Supplies a pointer to the beginning of the netlink message. The
        length of which can be obtained from the header; it was already
        validated.

Return Value:

    None.

--*/

{

    USHORT AttributeLength;
    PVOID Attributes;
    PUSHORT BeaconInterval;
    PNETCON_BSS Bss;
    PNETCON_BSS *BssArray;
    PVOID BssAttribute;
    USHORT BssAttributeLength;
    ULONG BssCount;
    PVOID Bssid;
    PULONG BssStatus;
    PUSHORT Capabilities;
    PVOID Elements;
    USHORT ElementsSize;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    ULONG MessageLength;
    PNETCON_SCAN_RESULTS ScanResults;
    PLONG SignalMbm;
    KSTATUS Status;

    Bss = NULL;
    Header = (PNETLINK_HEADER)Message;
    Status = 0;

    //
    // Skip any messages that are not of the 802.11 family type.
    //

    if (Header->Type != Context->Type) {
        goto ParseScanResultEnd;
    }

    //
    // Parse the generic header to determine the 802.11 command and toss
    // anything that isn't a scan result.
    //

    MessageLength = Header->Length;
    MessageLength -= NETLINK_HEADER_LENGTH;
    if (MessageLength < sizeof(NETLINK_GENERIC_HEADER)) {
        goto ParseScanResultEnd;
    }

    GenericHeader = NETLINK_DATA(Header);
    if (GenericHeader->Command != NETLINK_80211_COMMAND_SCAN_RESULT) {
        goto ParseScanResultEnd;
    }

    //
    // Get the BSS region of the attributes.
    //

    MessageLength -= NETLINK_GENERIC_HEADER_LENGTH;
    Attributes = NETLINK_GENERIC_DATA(GenericHeader);
    Status = NlGetAttribute(Attributes,
                            MessageLength,
                            NETLINK_80211_ATTRIBUTE_BSS,
                            &BssAttribute,
                            &BssAttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_BSSID,
                            &Bssid,
                            &AttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    if (AttributeLength != NET80211_ADDRESS_SIZE) {
        errno = ERANGE;
        Status = -1;
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_CAPABILITY,
                            (PVOID *)&Capabilities,
                            &AttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    if (AttributeLength != sizeof(USHORT)) {
        errno = ERANGE;
        Status = -1;
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_BEACON_INTERVAL,
                            (PVOID *)&BeaconInterval,
                            &AttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    if (AttributeLength != sizeof(USHORT)) {
        errno = ERANGE;
        Status = -1;
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_SIGNAL_MBM,
                            (PVOID *)&SignalMbm,
                            &AttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    if (AttributeLength != sizeof(LONG)) {
        errno = ERANGE;
        Status = -1;
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_SIGNAL_MBM,
                            (PVOID *)&BssStatus,
                            &AttributeLength);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    if (AttributeLength != sizeof(ULONG)) {
        errno = ERANGE;
        Status = -1;
        goto ParseScanResultEnd;
    }

    Status = NlGetAttribute(BssAttribute,
                            BssAttributeLength,
                            NETLINK_80211_BSS_ATTRIBUTE_INFORMATION_ELEMENTS,
                            &Elements,
                            &ElementsSize);

    if (Status != 0) {
        goto ParseScanResultEnd;
    }

    //
    // Allocate a new BSS element and fill it out with the information
    // collected above.
    //

    Bss = malloc(sizeof(NETCON_BSS) + ElementsSize);
    if (Bss == NULL) {
        goto ParseScanResultEnd;
    }

    memset(Bss, 0, sizeof(NETCON_BSS));
    Bss->Bssid.Domain = NetDomain80211;
    memcpy(Bss->Bssid.Address, Bssid, NET80211_ADDRESS_SIZE);
    Bss->SignalStrength = *SignalMbm;
    Bss->Status = *BssStatus;
    Bss->BeaconInterval = *BeaconInterval;
    Bss->Capabilities = *Capabilities;
    Bss->Elements = Bss + 1;
    Bss->ElementsSize = ElementsSize;
    memcpy(Bss->Elements, Elements, ElementsSize);

    //
    // Expand the scan results array to incorporate this BSS element.
    //

    ScanResults = (PNETCON_SCAN_RESULTS)Context->PrivateContext;
    BssCount = ScanResults->BssCount + 1;
    BssArray = realloc(ScanResults->BssArray,
                       BssCount * sizeof(PNETCON_BSS));

    if (BssArray == NULL) {
        ScanResults->BssCount = 0;
        Status = -1;
        goto ParseScanResultEnd;
    }

    ScanResults->BssArray = BssArray;
    ScanResults->BssCount = BssCount;
    ScanResults->BssArray[BssCount - 1] = Bss;
    ScanResults->Valid = TRUE;

ParseScanResultEnd:
    if (Status != 0) {
        if (Bss != NULL) {
            free(Bss);
        }
    }

    return;
}

VOID
NetconPrintScanResults (
    PNETCON_CONTEXT Context,
    PNETCON_SCAN_RESULTS Results
    )

/*++

Routine Description:

    This routine prints the given scan results list to standard out.

Arguments:

    Context - Supplies a pointer to the network connection context.

    Results - Supplies a pointer to the network scan results.

Return Value:

    None.

--*/

{

    PNETCON_BSS Bss;
    PVOID Channel;
    ULONG Index;
    PVOID RatesElement;
    PVOID Rsn;
    UCHAR Ssid[NET80211_MAX_SSID_LENGTH + 1];
    PVOID SsidElement;
    ULONG SsidLength;

    printf("Device 0x%I64x:\n", Context->DeviceId);
    printf("Networks Visisble: %d\n\n", Results->BssCount);
    for (Index = 0; Index < Results->BssCount; Index += 1) {
        Bss = Results->BssArray[Index];
        SsidElement = NetconGet80211InformationElement(Bss->Elements,
                                                       Bss->ElementsSize,
                                                       NET80211_ELEMENT_SSID);

        SsidLength = 0;
        if (SsidElement != NULL) {
            SsidLength = NET80211_GET_ELEMENT_LENGTH(SsidElement);
            memcpy(Ssid, NET80211_GET_ELEMENT_DATA(SsidElement), SsidLength);
        }

        Ssid[SsidLength] = STRING_TERMINATOR;
        printf("SSID %d: %s\n", Index, Ssid);
        if (Bss->Status == NETLINK_80211_BSS_STATUS_ASSOCIATED) {
            printf("\tStatus: Connected\n");
        }

        Rsn = NetconGet80211InformationElement(Bss->Elements,
                                               Bss->ElementsSize,
                                               NET80211_ELEMENT_RSN);

        if (Rsn != NULL) {
            NetconPrintRsnInformation(Rsn);

        } else {
            printf("\tAuthentication: Open\n");
            printf("\tEncryption: None\n");
        }

        if ((Context->Flags & NETCON_FLAG_VERBOSE) == 0) {
            printf("\n");
            continue;
        }

        printf("\tBSSID: ");
        NetconPrintAddress(&(Bss->Bssid));
        NetconPrintRssi(Bss->SignalStrength / 100);
        Channel = NetconGet80211InformationElement(Bss->Elements,
                                                   Bss->ElementsSize,
                                                   NET80211_ELEMENT_DSSS);

        if (Channel != NULL) {
            printf("\tChannel: %d\n", *NET80211_GET_ELEMENT_DATA(Channel));
        }

        printf("\tBeacon Interval: %d ms\n", Bss->BeaconInterval);
        printf("\tCapabilities: 0x%04x\n", Bss->Capabilities);
        RatesElement = NetconGet80211InformationElement(
                                             Bss->Elements,
                                             Bss->ElementsSize,
                                             NET80211_ELEMENT_SUPPORTED_RATES);

        if (RatesElement != NULL) {
            printf("\tSupported Rates (Mbps):");
            NetconPrintRates(RatesElement);
        }

        RatesElement = NetconGet80211InformationElement(
                                    Bss->Elements,
                                    Bss->ElementsSize,
                                    NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES);

        if (RatesElement != NULL) {
            printf("\tExtended Rates (Mbps):");
            NetconPrintRates(RatesElement);
        }

        printf("\n");
    }

    return;
}

PVOID
NetconGet80211InformationElement (
    PVOID Elements,
    ULONG ElementsSize,
    UCHAR ElementId
    )

/*++

Routine Description:

    This routine searches the given information element buffer for the element
    with the given ID, returning a pointer to the element's header.

Arguments:

    Elements - Supplies a pointer to the set of information elements to search.

    ElementsSize - Supplies the size of the informatin elements, in bytes.

    ElementId - Supplies the ID of the element to lookup.

Return Value:

    Returns a pointer to the information element on success or NULL on failure.

--*/

{

    PVOID Element;
    ULONG ElementLength;
    PVOID FoundElement;
    ULONG RemainingSize;

    FoundElement = NULL;

    //
    // A valid element must have at least two bytes - the ID and the length.
    //

    Element = Elements;
    RemainingSize = ElementsSize;
    while (RemainingSize >= NET80211_ELEMENT_HEADER_SIZE) {
        ElementLength = NET80211_GET_ELEMENT_LENGTH(Element) +
                        NET80211_ELEMENT_HEADER_SIZE;

        if (ElementLength > RemainingSize) {
            break;
        }

        if (NET80211_GET_ELEMENT_ID(Element) == ElementId) {
            FoundElement = Element;
            break;
        }

        RemainingSize -= ElementLength;
        Element += ElementLength;
    }

    return FoundElement;
}

VOID
NetconPrintRsnInformation (
    PUCHAR Rsn
    )

/*++

Routine Description:

    This routine prints the encryption information encapsulated by the RSN
    information element.

Arguments:

    Rsn - Supplies a pointer to the RSN element, the first byte of which is the
        element ID.

Return Value:

    None.

--*/

{

    ULONG AkmSuite;
    ULONG AkmSuiteCount;
    PULONG AkmSuites;
    NETWORK_ENCRYPTION_TYPE Authentication;
    ULONG GroupSuite;
    ULONG Index;
    ULONG Offset;
    ULONG PairwiseSuite;
    ULONG PairwiseSuiteCount;
    PULONG PairwiseSuites;
    ULONG RsnLength;
    INT Status;
    ULONG Suite;
    USHORT Version;

    ASSERT(NET80211_GET_ELEMENT_ID(Rsn) == NET80211_ELEMENT_RSN);

    Status = 0;
    Offset = NET80211_ELEMENT_HEADER_SIZE;
    RsnLength = NET80211_GET_ELEMENT_LENGTH(Rsn);
    AkmSuite = 0;
    GroupSuite = 0;
    PairwiseSuite = 0;
    PairwiseSuites = NULL;

    //
    // The version field is the only non-optional field.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        Status = -1;
        goto PrintRsnInformationEnd;
    }

    Version = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if (Version != NET80211_RSN_VERSION) {
        Status = -1;
        goto PrintRsnInformationEnd;
    }

    //
    // Save the group suite if it is present.
    //

    if ((Offset + sizeof(ULONG)) > RsnLength) {
        goto PrintRsnInformationEnd;
    }

    GroupSuite = NETWORK_TO_CPU32(*((PULONG)&(Rsn[Offset])));
    Offset += sizeof(ULONG);

    //
    // Save a pointer to the pairwise suites, if present.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto PrintRsnInformationEnd;
    }

    PairwiseSuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if ((Offset + (sizeof(ULONG) * PairwiseSuiteCount)) > RsnLength) {
        goto PrintRsnInformationEnd;
    }

    PairwiseSuites = (PULONG)&(Rsn[Offset]);
    Offset += sizeof(ULONG) * PairwiseSuiteCount;

    //
    // Run through the pairwise suites to determine the most secure option.
    //

    for (Index = 0; Index < PairwiseSuiteCount; Index += 1) {
        Suite = NETWORK_TO_CPU32(PairwiseSuites[Index]);

        //
        // CCMP is always the highest.
        //

        if (Suite == NET80211_CIPHER_SUITE_CCMP) {
            PairwiseSuite = Suite;
            break;
        }

        //
        // TKIP beats nothing and WEP.
        //

        if ((Suite == NET80211_CIPHER_SUITE_TKIP) &&
            ((PairwiseSuite == 0) ||
             (PairwiseSuite == NET80211_CIPHER_SUITE_WEP_40) ||
             (PairwiseSuite == NET80211_CIPHER_SUITE_WEP_104))) {

            PairwiseSuite = Suite;
        }

        //
        // WEP only beats nothing.
        //

        if ((PairwiseSuite == 0) &&
            ((Suite == NET80211_CIPHER_SUITE_WEP_40) ||
             (Suite == NET80211_CIPHER_SUITE_WEP_104))) {

            PairwiseSuite = Suite;
        }
    }

    //
    // The PSK authentication and key management (AKM) must be one of the
    // optional AKM suites.
    //

    if ((Offset + sizeof(USHORT)) > RsnLength) {
        goto PrintRsnInformationEnd;
    }

    AkmSuiteCount = *((PUSHORT)&(Rsn[Offset]));
    Offset += sizeof(USHORT);
    if ((Offset + (AkmSuiteCount * sizeof(ULONG))) > RsnLength) {
        goto PrintRsnInformationEnd;
    }

    AkmSuites = (PULONG)&(Rsn[Offset]);
    for (Index = 0; Index < AkmSuiteCount; Index += 1) {
        Suite = NETWORK_TO_CPU32(AkmSuites[Index]);
        if ((Suite == NET80211_AKM_SUITE_PSK) ||
            (Suite == NET80211_AKM_SUITE_PSK_SHA256)) {

            AkmSuite = Suite;
            break;
        }

        if ((Suite == NET80211_AKM_SUITE_8021X) ||
            (Suite == NET80211_AKM_SUITE_8021X_SHA256)) {

            AkmSuite = Suite;
            break;
        }
    }

    Offset += sizeof(ULONG) * AkmSuiteCount;

PrintRsnInformationEnd:
    if (Status != 0) {
        printf("\tAuthentication: unknown\n");

    } else {
        Authentication = NetworkEncryptionInvalid;
        switch (PairwiseSuite) {
        case NET80211_CIPHER_SUITE_WEP_40:
        case NET80211_CIPHER_SUITE_WEP_104:
            Authentication = NetworkEncryptionWep;
            break;

        case NET80211_CIPHER_SUITE_TKIP:
            switch (AkmSuite) {
            case NET80211_AKM_SUITE_PSK:
            case NET80211_AKM_SUITE_PSK_SHA256:
                Authentication = NetworkEncryptionWpaPsk;
                break;

            case NET80211_AKM_SUITE_8021X:
            case NET80211_AKM_SUITE_8021X_SHA256:
                Authentication = NetworkEncryptionWpaEap;
                break;

            default:
                break;
            }

            break;

        case NET80211_CIPHER_SUITE_CCMP:
            switch (AkmSuite) {
            case NET80211_AKM_SUITE_PSK:
            case NET80211_AKM_SUITE_PSK_SHA256:
                Authentication = NetworkEncryptionWpa2Psk;
                break;

            case NET80211_AKM_SUITE_8021X:
            case NET80211_AKM_SUITE_8021X_SHA256:
                Authentication = NetworkEncryptionWpa2Eap;
                break;

            default:
                break;
            }

            break;

        default:
            break;
        }

        printf("\tAuthentication: ");
        NetconPrintEncryption(Authentication);
        if (PairwiseSuites != NULL) {
            printf("\tPairwise Encryption:");
            for (Index = 0; Index < PairwiseSuiteCount; Index += 1) {
                Suite = NETWORK_TO_CPU32(PairwiseSuites[Index]);
                printf(" ");
                NetconPrintCipherSuite(Suite);
            }

            printf("\n");
        }

        if ((GroupSuite != 0) &&
            (GroupSuite != NET80211_CIPHER_SUITE_GROUP_NOT_ALLOWED)) {

            printf("\tGroup Encryption: ");
            NetconPrintCipherSuite(GroupSuite);
            printf("\n");
        }
    }

    return;
}

VOID
NetconPrintDeviceInformation (
    PNETCON_DEVICE_DESCRIPTION Device
    )

/*++

Routine Description:

    This routine prints out the network information for the given device.

Arguments:

    Device - Supplies a pointer to the network device whose information is to
        be printed.

Return Value:

    None.

--*/

{

    NETWORK_ADDRESS_CONFIGURATION_METHOD Configuration;
    PNETWORK_80211_DEVICE_INFORMATION Net80211;
    PNETWORK_DEVICE_INFORMATION Network;
    PNETWORK_ADDRESS PhysicalAddress;

    printf("Network Device 0x%I64x:\n", Device->DeviceId);
    PhysicalAddress = NULL;
    if ((Device->Flags & NETCON_DEVICE_FLAG_IP4) != 0) {
        PhysicalAddress = &(Device->NetworkIp4.PhysicalAddress);

    } else if ((Device->Flags & NETCON_DEVICE_FLAG_IP6) == 0) {
        PhysicalAddress = &(Device->NetworkIp6.PhysicalAddress);
    }

    if (PhysicalAddress == NULL) {
        return;
    }

    //
    // The physical address should always be present.
    //

    printf("\tPhysical Address: ");
    NetconPrintAddress(PhysicalAddress);

    //
    // Print the IPv4 address line to show that the device is IPv4 capable, but
    // only print the actual address if it is configured.
    //

    if ((Device->Flags & NETCON_DEVICE_FLAG_IP4) != 0) {
        printf("\tIPv4 Address: ");
        Network = &(Device->NetworkIp4);
        Configuration = Network->ConfigurationMethod;
        if ((Configuration != NetworkAddressConfigurationInvalid) &&
            (Configuration != NetworkAddressConfigurationNone)) {

            NetconPrintAddress(&(Network->Address));
            printf("\tSubnet Mask: ");
            NetconPrintAddress(&(Network->Subnet));
            printf("\tGateway: ");
            NetconPrintAddress(&(Network->Gateway));

        } else {
            printf("(not configured)\n");
        }
    }

    //
    // Print the IPv6 address line to show that the device is IPv4 capable, but
    // only print the actual address if it is configured.
    //

    if ((Device->Flags & NETCON_DEVICE_FLAG_IP6) != 0) {
        printf("\tIPv6 Address: ");
        Network = &(Device->NetworkIp6);
        Configuration = Network->ConfigurationMethod;
        if ((Configuration != NetworkAddressConfigurationInvalid) &&
            (Configuration != NetworkAddressConfigurationNone)) {

            NetconPrintAddress(&(Network->Address));
            printf("\tSubnet Mask: ");
            NetconPrintAddress(&(Network->Subnet));
            printf("\tGateway: ");
            NetconPrintAddress(&(Network->Gateway));

        } else {
            printf("(not configured)\n");
        }
    }

    //
    // If the device supports 802.11, at least print the SSID or
    // "Not Associated".
    //

    if ((Device->Flags & NETCON_DEVICE_FLAG_80211) != 0) {
        Net80211 = &(Device->Net80211);
        printf("\tSSID: ");
        if ((Net80211->Flags & NETWORK_80211_DEVICE_FLAG_ASSOCIATED) != 0) {
            printf("\"%s\"\n", Net80211->Ssid);
            printf("\tBSSID: ");
            NetconPrintAddress(&(Net80211->Bssid));
            printf("\tChannel: %d\n", Net80211->Channel);
            printf("\tMax Rate: %.1f Mbps\n",
                   (double)Net80211->MaxRate / 1000000ULL);

            NetconPrintRssi(Net80211->Rssi);
            printf("\tPairwise Encryption: ");
            NetconPrintEncryption(Net80211->PairwiseEncryption);
            printf("\tGroup Encryption: ");
            NetconPrintEncryption(Net80211->GroupEncryption);

        } else {
            printf("(not associated)\n");
        }
    }

    printf("\n");
    return;
}

VOID
NetconPrintAddress (
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine prints the given network address to standard out.

Arguments:

    Address - Supplies a pointer to the network address to print.

Return Value:

    None.

--*/

{

    socklen_t AddressLength;
    PUCHAR BytePointer;
    struct sockaddr_in Ip4Address;
    struct sockaddr_in6 Ip6Address;
    CHAR PrintBuffer[60];
    KSTATUS Status;

    switch (Address->Domain) {
    case NetDomainIp4:
        AddressLength = sizeof(struct sockaddr_in);
        Status = ClConvertFromNetworkAddress(Address,
                                             (struct sockaddr *)&Ip4Address,
                                             &AddressLength,
                                             NULL,
                                             0);

        if (!KSUCCESS(Status)) {
            break;
        }

        inet_ntop(Ip4Address.sin_family,
                 &(Ip4Address.sin_addr.s_addr),
                 PrintBuffer,
                 sizeof(PrintBuffer));

        printf("%s", PrintBuffer);
        break;

    case NetDomainIp6:
        AddressLength = sizeof(struct sockaddr_in6);
        Status = ClConvertFromNetworkAddress(Address,
                                             (struct sockaddr *)&Ip6Address,
                                             &AddressLength,
                                             NULL,
                                             0);

        if (!KSUCCESS(Status)) {
            break;
        }

        inet_ntop(Ip6Address.sin6_family,
                  &(Ip6Address.sin6_addr.s6_addr),
                  PrintBuffer,
                  sizeof(PrintBuffer));

        printf("%s", PrintBuffer);
        break;

    case NetDomain80211:
    case NetDomainEthernet:
        BytePointer = (PUCHAR)(Address->Address);
        printf("%02X:%02X:%02X:%02X:%02X:%02X",
               BytePointer[0],
               BytePointer[1],
               BytePointer[2],
               BytePointer[3],
               BytePointer[4],
               BytePointer[5]);

        break;

    default:
        break;
    }

    printf("\n");
    return;
}

VOID
NetconPrintEncryption (
    NETWORK_ENCRYPTION_TYPE EncryptionType
    )

/*++

Routine Description:

    This routine prints the name of the given encryption type.

Arguments:

    EncryptionType - Supplies the encryption type to print.

Return Value:

    None.

--*/

{

    switch (EncryptionType) {
    case NetworkEncryptionNone:
        printf("none");
        break;

    case NetworkEncryptionWep:
        printf("WEP");
        break;

    case NetworkEncryptionWpaPsk:
        printf("WPA-PSK");
        break;

    case NetworkEncryptionWpaEap:
        printf("WPA-EAP");
        break;

    case NetworkEncryptionWpa2Psk:
        printf("WPA2-PSK");
        break;

    case NetworkEncryptionWpa2Eap:
        printf("WPA2-EAP");
        break;

    default:
        printf("unknown");
        break;
    }

    printf("\n");
    return;
}

VOID
NetconPrintCipherSuite (
    ULONG Suite
    )

/*++

Routine Description:

    This routine prints the name of the given cipher suite.

Arguments:

    Suite - Supplies the cipher suite to print.

Return Value:

    None.

--*/

{

    switch (Suite) {
    case NET80211_CIPHER_SUITE_USE_GROUP_CIPHER:
        printf("Group Only");
        break;

    case NET80211_CIPHER_SUITE_WEP_40:
        printf("WEP-40");
        break;

    case NET80211_CIPHER_SUITE_TKIP:
        printf("TKIP");
        break;

    case NET80211_CIPHER_SUITE_CCMP:
        printf("CCMP");
        break;

    case NET80211_CIPHER_SUITE_WEP_104:
        printf("WEP-104");
        break;

    case NET80211_CIPHER_SUITE_BIP:
        printf("BIP");
        break;

    default:
        printf("unknown");
        break;
    }

    return;
}

VOID
NetconPrintRssi (
    LONG Rssi
    )

/*++

Routine Description:

    This routine prints the signal strength out to standard out.

Arguments:

    Rssi - Supplies the RSSI value for signal strength in dBm.

Return Value:

    None.

--*/

{

    ULONG Percentage;

    if (Rssi < -100) {
        Percentage = 0;

    } else if (Rssi > -50) {
        Percentage = 100;

    } else {
        Percentage = 2 * (Rssi + 100);
    }

    printf("\tSignal Strength: %d%% (%d dBm)\n", Percentage, Rssi);
    return;
}

VOID
NetconPrintRates (
    PVOID RatesElement
    )

/*++

Routine Description:

    This routine prints out each rate stored in the given rate information
    element. That values are printed out in Mbps.

Arguments:

    RatesElement - Supplies a pointer to the rates element to print.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Rate;
    PUCHAR Rates;

    Rates = NET80211_GET_ELEMENT_DATA(RatesElement);
    for (Index = 0;
         Index < NET80211_GET_ELEMENT_LENGTH(RatesElement);
         Index += 1) {

        Rate = Rates[Index] & NET80211_RATE_VALUE_MASK;
        Rate *= NET80211_RATE_UNIT;
        if ((Rate % 1000000) == 0) {
            printf(" %d", Rate / 1000000);

        } else {
            printf(" %.1f", (double)Rate / 1000000);
        }
    }

    printf("\n");
    return;
}

INT
NetconGet80211DeviceId (
    PDEVICE_ID DeviceId
    )

/*++

Routine Description:

    This routine gets the device ID of the system's 802.11 device. If there
    are multiple 802.11 devices then it prints an error and the available
    wireless devices.

Arguments:

    DeviceId - Supplies a pointer that receives the 802.11 device's ID.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PNETCON_DEVICE_DESCRIPTION DeviceArray;
    ULONG DeviceCount;
    ULONG Index;
    ULONG Net80211DeviceCount;
    ULONG Net80211DeviceIndex;
    INT Result;

    Result = NetconEnumerateDevices(&DeviceArray, &DeviceCount);
    if (Result != 0) {
        return Result;
    }

    Net80211DeviceIndex = 0;
    Net80211DeviceCount = 0;
    for (Index = 0; Index < DeviceCount; Index += 1) {
        if ((DeviceArray[Index].Flags & NETCON_DEVICE_FLAG_80211) != 0) {
            Net80211DeviceCount += 1;
            Net80211DeviceIndex = Index;
        }
    }

    if (Net80211DeviceCount == 0) {
        printf("netcon: failed to find a wireless device.\n");
        Result = ENODEV;

    } else if (Net80211DeviceCount == 1) {
        *DeviceId = DeviceArray[Net80211DeviceIndex].DeviceId;

    } else {
        printf("There are %d wireless devices available. Please specify "
               "a device ID with the -d parameter.\n",
               Net80211DeviceCount);

        printf("Wireless Devices:\n\n");
        for (Index = 0; Index < DeviceCount; Index += 1) {
            if ((DeviceArray[Index].Flags & NETCON_DEVICE_FLAG_80211) != 0) {
                NetconPrintDeviceInformation(&(DeviceArray[Index]));
                printf("\n");
            }
        }

        Result = ENODEV;
    }

    free(DeviceArray);
    return Result;
}

