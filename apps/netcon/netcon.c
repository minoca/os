/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include <osbase.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/net80211.h>
#include <minoca/net/netlink.h>
#include <mlibc.h>
#include <netlink.h>

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
    "usage: netcon [-d device [-j ssid -p] [-l] [-n]]\n\n"                     \
    "The netcon utility configures network devices.\n\n"                       \
    "Options:\n"                                                               \
    "  -d --device=device -- Specifies the network device to configure.\n"     \
    "  -j --join=ssid -- Attempts to join the given wireless network.\n"       \
    "  -l --leave -- Attempts to leave the current wireless network.\n"        \
    "  -p --password -- Indicates that the user wants to be prompted for a\n"  \
    "      password during a join operation.\n"                                \
    "  -s --scan -- Displays the list of wireless networks available to\n"     \
    "      the network device specified by -d.\n"                              \
    "  --help -- Display this help text.\n"                                    \
    "  --version -- Display the application version and exit.\n\n"

#define NETCON_OPTIONS_STRING "d:j:lsph"

//
// Define the set of network configuration flags.
//

#define NETCON_FLAG_DEVICE_ID 0x00000001
#define NETCON_FLAG_JOIN      0x00000002
#define NETCON_FLAG_LEAVE     0x00000004
#define NETCON_FLAG_PASSWORD  0x00000008
#define NETCON_FLAG_SCAN      0x00000010

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

--*/

typedef struct _NETCON_BSS {
    NETWORK_ADDRESS Bssid;
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

//
// -------------------------------------------------------------------- Globals
//

struct option NetconLongOptions[] = {
    {"device", required_argument, 0, 'd'},
    {"join", required_argument, 0, 'j'},
    {"leave", no_argument, 0, 'l'},
    {"password", no_argument, 0, 'p'},
    {"scan", no_argument, 0, 's'},
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

        case 'V':
            printf("netcon version %d.%02d.%d\n",
                   NETCON_VERSION_MAJOR,
                   NETCON_VERSION_MINOR,
                   REVISION);

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
    // If no device ID was specified, then list all of the local network
    // interfaces and their statuses.
    //

    if ((Context.Flags & NETCON_FLAG_DEVICE_ID) == 0) {
        if (ArgumentCount != 1) {
            ReturnValue = EINVAL;
            goto mainEnd;
        }

        NetconListDevices();

    } else if ((Context.Flags & NETCON_FLAG_JOIN) != 0) {
        NetconJoinNetwork(&Context);

    } else if ((Context.Flags & NETCON_FLAG_LEAVE) != 0) {
        NetconLeaveNetwork(&Context);

    } else if ((Context.Flags & NETCON_FLAG_SCAN) != 0) {
        NetconScanForNetworks(&Context);

    } else {
        ReturnValue = NetconGetDeviceInformation(Context.DeviceId, &Device);
        if (ReturnValue != 0) {
            goto mainEnd;
        }

        NetconPrintDeviceInformation(&Device);
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

    PVOID Attributes;
    PNETCON_BSS Bss;
    PVOID BssAttribute;
    USHORT BssAttributeLength;
    ULONG BssCount;
    PVOID Bssid;
    USHORT BssidLength;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    ULONG MessageLength;
    PNETCON_SCAN_RESULTS ScanResults;
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
                            &BssidLength);

    if ((Status != 0) || (BssidLength != NET80211_ADDRESS_SIZE)) {
        goto ParseScanResultEnd;
    }

    //
    // Allocate a new BSS element and fill it out with the information
    // collected above.
    //

    Bss = malloc(sizeof(NETCON_BSS));
    if (Bss == NULL) {
        goto ParseScanResultEnd;
    }

    memset(Bss, 0, sizeof(NETCON_BSS));
    Bss->Bssid.Domain = NetDomain80211;
    memcpy(Bss->Bssid.Address, Bssid, NET80211_ADDRESS_SIZE);

    //
    // Expand the scan results array to incorporate this BSS element.
    //

    ScanResults = (PNETCON_SCAN_RESULTS)Context->PrivateContext;
    BssCount = ScanResults->BssCount + 1;
    ScanResults->BssArray = realloc(ScanResults->BssArray,
                                    BssCount * sizeof(PNETCON_BSS));

    if (ScanResults->BssArray == NULL) {
        ScanResults->BssCount = 0;
        Status = -1;
        goto ParseScanResultEnd;
    }

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
    ULONG Index;

    printf("Network Scan for Device 0x%I64x:\n", Context->DeviceId);
    if (Results->BssCount == 0) {
        printf("No networks found.\n");
        return;
    }

    for (Index = 0; Index < Results->BssCount; Index += 1) {
        Bss = Results->BssArray[Index];
        printf("\tBSSID: ");
        NetconPrintAddress(&(Bss->Bssid));
        printf("\n");
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
        printf("\n\tIPv4 Address: ");
        Network = &(Device->NetworkIp4);
        Configuration = Network->ConfigurationMethod;
        if ((Configuration != NetworkAddressConfigurationInvalid) &&
            (Configuration != NetworkAddressConfigurationNone)) {

            NetconPrintAddress(&(Network->Address));
            printf("\n\tSubnet Mask: ");
            NetconPrintAddress(&(Network->Subnet));
            printf("\n\tGateway: ");
            NetconPrintAddress(&(Network->Gateway));

        } else {
            printf("(not configured)");
        }
    }

    //
    // Print the IPv6 address line to show that the device is IPv4 capable, but
    // only print the actual address if it is configured.
    //

    if ((Device->Flags & NETCON_DEVICE_FLAG_IP6) != 0) {
        printf("\n\tIPv6 Address: ");
        Network = &(Device->NetworkIp6);
        Configuration = Network->ConfigurationMethod;
        if ((Configuration != NetworkAddressConfigurationInvalid) &&
            (Configuration != NetworkAddressConfigurationNone)) {

            NetconPrintAddress(&(Network->Address));
            printf("\n\tSubnet Mask: ");
            NetconPrintAddress(&(Network->Subnet));
            printf("\n\tGateway: ");
            NetconPrintAddress(&(Network->Gateway));

        } else {
            printf("(not configured)");
        }
    }

    //
    // If the device supports 802.11, at least print the SSID or
    // "Not Associated".
    //

    if ((Device->Flags & NETCON_DEVICE_FLAG_80211) != 0) {
        Net80211 = &(Device->Net80211);
        printf("\n\tSSID: ");
        if ((Net80211->Flags & NETWORK_80211_DEVICE_FLAG_ASSOCIATED) != 0) {
            printf("\"%s\"", Net80211->Ssid);
            printf("\n\tBSSID: ");
            NetconPrintAddress(&(Net80211->Bssid));
            printf("\n\tChannel: %d", Net80211->Channel);
            printf("\n\tMax Rate: %I64d mbps", Net80211->MaxRate / 1000000ULL);
            printf("\n\tRSSI: %d dBm", Net80211->Rssi);
            printf("\n\tPairwise Encryption: ");
            NetconPrintEncryption(Net80211->PairwiseEncryption);
            printf("\n\tGroup Encryption: ");
            NetconPrintEncryption(Net80211->GroupEncryption);

        } else {
            printf("(not associated)");
        }
    }

    printf("\n");
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

    case NetworkEncryptionWpa2Psk:
        printf("WPA2-PSK");
        break;

    default:
        printf("unknown");
        break;
    }

    return;
}

