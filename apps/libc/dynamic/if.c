/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    if.c

Abstract:

    This module implements support for network interface enumeration.

Author:

    Chris Stevens 21-Jul-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <minoca/devinfo/net.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <errno.h>
#include <stdlib.h>
#include "net.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This routine returns the network name index for a given network domain.
//

#define CL_NETWORK_NAME_INDEX_FROM_DOMAIN(_Domain) \
    (((_Domain) - NET_DOMAIN_PHYSICAL_BASE) + CL_NETWORK_NAME_DOMAIN_OFFSET)

//
// ---------------------------------------------------------------- Definitions
//

#define CL_NETWORK_NAME_FORMAT_COUNT 3
#define CL_NETWORK_NAME_LINK_LAYER_INDEX 0
#define CL_NETWORK_NAME_DOMAIN_OFFSET 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ClpGetNetworkDevices (
    PDEVICE_INFORMATION_RESULT *Devices,
    PULONG DeviceCount
    );

KSTATUS
ClpGetNetworkDeviceName (
    DEVICE_ID DeviceId,
    PSTR *Buffer,
    ULONG BufferLength
    );

KSTATUS
ClpCreateNetworkDeviceName (
    DEVICE_ID DeviceId,
    PCSTR FormatString,
    PSTR *Buffer,
    ULONG BufferLength
    );

KSTATUS
ClpGetNetworkDeviceDomain (
    DEVICE_ID DeviceId,
    PNET_DOMAIN_TYPE Domain
    );

KSTATUS
ClpGetNetworkDeviceInformation (
    DEVICE_ID DeviceId,
    PNETWORK_DEVICE_INFORMATION Information
    );

KSTATUS
ClpCreateNetworkInterfaceAddress (
    DEVICE_ID DeviceId,
    struct ifaddrs **Interface,
    struct ifaddrs **LinkInterface
    );

VOID
ClpDestroyNetworkInterfaceAddress (
    struct ifaddrs *Interface
    );

//
// -------------------------------------------------------------------- Globals
//

PCSTR ClNetworkNameFormats[CL_NETWORK_NAME_FORMAT_COUNT] = {
    "il%d",
    "eth%d",
    "wlan%d"
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
struct if_nameindex *
if_nameindex (
    void
    )

/*++

Routine Description:

    This routine returns an array of all the available network interfaces on
    the system.

Arguments:

    None.

Return Value:

    An array of interface structures on success. The end of the array is
    indicated by a structure with a 0 index and NULL name.

    NULL on error, and errno will be set to contain more information.

--*/

{

    UINTN AllocationSize;
    ULONG DeviceCount;
    PDEVICE_INFORMATION_RESULT Devices;
    ULONG Index;
    struct if_nameindex *Interfaces;
    KSTATUS Status;

    Devices = NULL;
    DeviceCount = 0;
    Devices = NULL;
    Interfaces = NULL;

    //
    // Enumerate all the devices that support getting network device
    // information.
    //

    Status = ClpGetNetworkDevices(&Devices, &DeviceCount);
    if (!KSUCCESS(Status)) {
        goto EnumerateDevicesEnd;
    }

    //
    // Allocate enough name-index structures, including a empty one for the end
    // of the array.
    //

    AllocationSize = sizeof(struct if_nameindex) * (DeviceCount + 1);
    Interfaces = malloc(AllocationSize);
    if (Interfaces == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateDevicesEnd;
    }

    memset(Interfaces, 0, AllocationSize);
    for (Index = 0; Index < DeviceCount; Index += 1) {
        Status = ClpGetNetworkDeviceName(Devices[Index].DeviceId,
                                         &(Interfaces[Index].if_name),
                                         0);

        if (!KSUCCESS(Status)) {
            break;
        }

        Interfaces[Index].if_index = (unsigned)Devices[Index].DeviceId;
    }

    Status = STATUS_SUCCESS;

EnumerateDevicesEnd:
    if (Devices != NULL) {
        free(Devices);
    }

    if (!KSUCCESS(Status)) {
        if (Interfaces != NULL) {
            if_freenameindex(Interfaces);
            Interfaces = NULL;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return Interfaces;
}

LIBC_API
void
if_freenameindex (
    struct if_nameindex *Interfaces
    )

/*++

Routine Description:

    This routine releases an array of network interfaces.

Arguments:

    Interfaces - Supplies a pointer to the array of network interfaces to
        release.

Return Value:

    None.

--*/

{

    struct if_nameindex *Interface;

    Interface = Interfaces;
    while (Interface->if_name != NULL) {
        free(Interface->if_name);
        Interface += 1;
    }

    free(Interfaces);
    return;
}

LIBC_API
char *
if_indextoname (
    unsigned Index,
    char *Name
    )

/*++

Routine Description:

    This routine returns the name of the network interface with the given index.

Arguments:

    Index - Supplies the index of a network interface.

    Name - Supplies a pointer to a buffer where the interface name will be
        stored. The buffer must be at least IF_NAMESIZE.

Return Value:

    A pointer to the supplied name buffer on success.

    NULL on error, and errno will be set to contain more information.

--*/

{

    KSTATUS Status;

    Status = ClpGetNetworkDeviceName(Index, &Name, IF_NAMESIZE);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    return Name;
}

LIBC_API
unsigned
if_nametoindex (
    const char *Name
    )

/*++

Routine Description:

    This routine returns the index of the network interface with the given name.

Arguments:

    Name - Supplies the name of a network interface.

Return Value:

    The index of the network interface on success.

    0 otherwise.

--*/

{

    ULONG DeviceId;
    PSTR DeviceName;
    CHAR DeviceNameBuffer[IF_NAMESIZE];
    ULONG FoundDeviceId;
    ULONG Index;
    ULONG ItemsScanned;
    BOOL Match;
    KSTATUS Status;

    FoundDeviceId = 0;
    for (Index = 0; Index < CL_NETWORK_NAME_FORMAT_COUNT; Index += 1) {
        Status = RtlStringScan((PSTR)Name,
                               IF_NAMESIZE,
                               ClNetworkNameFormats[Index],
                               strlen(ClNetworkNameFormats[Index]) + 1,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &DeviceId);

        //
        // On success, make sure the device ID corresponds to an actual device
        // by matching the name.
        //

        if (KSUCCESS(Status)) {
            DeviceName = &(DeviceNameBuffer[0]);
            Status = ClpGetNetworkDeviceName(DeviceId,
                                             &DeviceName,
                                             IF_NAMESIZE);

            if (KSUCCESS(Status)) {
                Match = RtlAreStringsEqual((PSTR)Name, DeviceName, IF_NAMESIZE);
                if (Match != FALSE) {
                    FoundDeviceId = DeviceId;
                }
            }

            break;
        }
    }

    return FoundDeviceId;
}

LIBC_API
int
getifaddrs (
    struct ifaddrs **Interfaces
    )

/*++

Routine Description:

    This routine creates a linked list of network interfaces structures
    describing all of the network interfaces on the local system.

Arguments:

    Interfaces - Supplies a pointer that receives a pointer to the linked list
        of network interfaces.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    ULONG DeviceCount;
    PDEVICE_INFORMATION_RESULT Devices;
    ULONG Index;
    struct ifaddrs *Interface;
    struct ifaddrs *LinkInterface;
    struct ifaddrs **PreviousNextPointer;
    KSTATUS Status;

    Devices = NULL;
    DeviceCount = 0;
    Devices = NULL;
    Interface = NULL;
    *Interfaces = NULL;

    //
    // Enumerate all the devices that support getting network device
    // information.
    //

    Status = ClpGetNetworkDevices(&Devices, &DeviceCount);
    if (!KSUCCESS(Status)) {
        goto EnumerateInterfacesEnd;
    }

    //
    // Get the network information for each device and convert it to a network
    // interface address structure.
    //

    PreviousNextPointer = Interfaces;
    for (Index = 0; Index < DeviceCount; Index += 1) {
        Status = ClpCreateNetworkInterfaceAddress(Devices[Index].DeviceId,
                                                  &Interface,
                                                  &LinkInterface);

        if (!KSUCCESS(Status)) {
            break;
        }

        *PreviousNextPointer = Interface;
        Interface->ifa_next = LinkInterface;
        PreviousNextPointer = &(LinkInterface->ifa_next);
    }

    Status = STATUS_SUCCESS;

EnumerateInterfacesEnd:
    if (Devices != NULL) {
        free(Devices);
    }

    if (!KSUCCESS(Status)) {
        if (*Interfaces != NULL) {
            freeifaddrs(*Interfaces);
            *Interfaces = NULL;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
void
freeifaddrs (
    struct ifaddrs *Interfaces
    )

/*++

Routine Description:

    This routine releases a list of network interfaces.

Arguments:

    Interfaces - Supplies a pointer to the list of network interfaces to
        release.

Return Value:

    None.

--*/

{

    struct ifaddrs *Interface;
    struct ifaddrs *NextInterface;

    Interface = Interfaces;
    while (Interface != NULL) {
        NextInterface = Interface->ifa_next;
        ClpDestroyNetworkInterfaceAddress(Interface);
        Interface = NextInterface;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ClpGetNetworkDevices (
    PDEVICE_INFORMATION_RESULT *Devices,
    PULONG DeviceCount
    )

/*++

Routine Description:

    This routine gets the list of network devices present on the system.

Arguments:

    Devices - Supplies a pointer that receives a pointer to an array of network
        device information results. The caller is responsible for releasing the
        resources.

    DeviceCount - Supplies a pointer that receives the number of network device
        information results returned in the array.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    ULONG ResultCount;
    PDEVICE_INFORMATION_RESULT Results;
    KSTATUS Status;

    ResultCount = 0;
    Results = NULL;

    //
    // Enumerate all the devices that support getting network device
    // information.
    //

    Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                       NULL,
                                       NULL,
                                       &ResultCount);

    if (Status != STATUS_BUFFER_TOO_SMALL) {
        goto GetNetworkDevicesEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto GetNetworkDevicesEnd;
    }

    AllocationSize = sizeof(DEVICE_INFORMATION_RESULT) * ResultCount;
    Results = malloc(AllocationSize);
    if (Results == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetNetworkDevicesEnd;
    }

    memset(Results, 0, AllocationSize);
    Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                       NULL,
                                       Results,
                                       &ResultCount);

    if (!KSUCCESS(Status)) {
        goto GetNetworkDevicesEnd;
    }

    if (ResultCount == 0) {
        free(Results);
        Results = NULL;
        Status = STATUS_SUCCESS;
        goto GetNetworkDevicesEnd;
    }

GetNetworkDevicesEnd:
    if (!KSUCCESS(Status)) {
        if (Results != NULL) {
            free(Results);
            Results = NULL;
        }

        ResultCount = 0;
    }

    *Devices = Results;
    *DeviceCount = ResultCount;
    return Status;
}

KSTATUS
ClpGetNetworkDeviceName (
    DEVICE_ID DeviceId,
    PSTR *Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine gets the network device name for the given device ID.

Arguments:

    DeviceId - Supplies the ID of a network device.

    Buffer - Supplies a pointer to a buffer that receives the device name
        string. If the buffer is NULL, then a buffer will be allocated for the
        caller to release.

    BufferLength - Supplies the length of the device name buffer.

Return Value:

    Status code.

--*/

{

    NET_DOMAIN_TYPE Domain;
    PCSTR FormatString;
    ULONG Index;
    KSTATUS Status;

    Status = ClpGetNetworkDeviceDomain(DeviceId, &Domain);
    if (!KSUCCESS(Status)) {
        goto GetNetworkDeviceNameEnd;
    }

    ASSERT(Domain >= NET_DOMAIN_PHYSICAL_BASE);

    Index = CL_NETWORK_NAME_INDEX_FROM_DOMAIN(Domain);
    FormatString = ClNetworkNameFormats[Index];
    Status = ClpCreateNetworkDeviceName(DeviceId,
                                        FormatString,
                                        Buffer,
                                        BufferLength);

    if (!KSUCCESS(Status)) {
        goto GetNetworkDeviceNameEnd;
    }

GetNetworkDeviceNameEnd:
    return Status;
}

KSTATUS
ClpCreateNetworkDeviceName (
    DEVICE_ID DeviceId,
    PCSTR FormatString,
    PSTR *Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine creates a network device name based on the given device ID and
    the format string. The caller is expected to release the name if a NULL
    buffer is supplied.

Arguments:

    DeviceId - Supplies the ID of a network device.

    FormatString - Supplies a pointer to the format string to use to create the
        device name.

    Buffer - Supplies a pointer to a buffer that receives the device name
        string. If the buffer is NULL, then a buffer will be allocated for the
        caller to release.

    BufferLength - Supplies the length of the device name buffer.

Return Value:

    Status code.

--*/

{

    ULONG NameLength;
    KSTATUS Status;

    NameLength = RtlPrintToString(NULL,
                                  0,
                                  CharacterEncodingDefault,
                                  FormatString,
                                  DeviceId);

    if (*Buffer != NULL) {
        if (BufferLength < NameLength) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto CreateNetworkDeviceNameEnd;
        }

    } else {
        *Buffer = malloc(NameLength);
        if (*Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNetworkDeviceNameEnd;
        }
    }

    NameLength = RtlPrintToString(*Buffer,
                                  NameLength,
                                  CharacterEncodingDefault,
                                  FormatString,
                                  DeviceId);

    Status = STATUS_SUCCESS;

CreateNetworkDeviceNameEnd:
    return Status;
}

KSTATUS
ClpGetNetworkDeviceDomain (
    DEVICE_ID DeviceId,
    PNET_DOMAIN_TYPE Domain
    )

/*++

Routine Description:

    This routine looks up the given device's network domain type.

Arguments:

    DeviceId - Supplies the ID of a network device.

    Domain - Supplies a pointer that receives the network domain for the given
        network device ID.

Return Value:

    Status code.

--*/

{

    NETWORK_DEVICE_INFORMATION Information;
    KSTATUS Status;

    Status = ClpGetNetworkDeviceInformation(DeviceId, &Information);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    *Domain = Information.PhysicalAddress.Domain;
    return STATUS_SUCCESS;
}

KSTATUS
ClpGetNetworkDeviceInformation (
    DEVICE_ID DeviceId,
    PNETWORK_DEVICE_INFORMATION Information
    )

/*++

Routine Description:

    This routine gets the network device information for the given device.

Arguments:

    DeviceId - Supplies the ID of a network device.

    Information - Supplies a pointer to a structure that will receive the
        network device information on success.

Return Value:

    Status code.

--*/

{

    UINTN DataSize;
    KSTATUS Status;

    //
    // Try the IPv4 network information.
    //

    DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
    Information->Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information->Domain = NetDomainIp4;
    Status = OsGetSetDeviceInformation(DeviceId,
                                       &ClNetworkDeviceInformationUuid,
                                       Information,
                                       &DataSize,
                                       FALSE);

    if (KSUCCESS(Status)) {
        return Status;
    }

    //
    // If that was unsuccessful, try IPv6.
    //

    DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
    Information->Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information->Domain = NetDomainIp6;
    Status = OsGetSetDeviceInformation(DeviceId,
                                       &ClNetworkDeviceInformationUuid,
                                       Information,
                                       &DataSize,
                                       FALSE);

    if (KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
ClpCreateNetworkInterfaceAddress (
    DEVICE_ID DeviceId,
    struct ifaddrs **Interface,
    struct ifaddrs **LinkInterface
    )

/*++

Routine Description:

    This routine creates network interface address structures for the network
    device indicated by the given device ID and for its associated link-layer
    address.

Arguments:

    DeviceId - Supplies the ID of the network device for which the network
        interface address structure is to be created.

    Interface - Supplies a pointer that receives a pointer to a newly allocated
        network interface address structure. The caller is responsible for
        releasing the allocation.

    LinkInterface - Supplies a pointer that receives a pointer to a newly
        allocated network interface address structure for the link-layer
        associated with this network device. The caller is responsible for
        releasing the allocation.

Return Value:

    Status code.

--*/

{

    ULONG Address;
    socklen_t AddressLength;
    size_t AllocationSize;
    struct sockaddr_in *Broadcast;
    size_t DataLength;
    NET_DOMAIN_TYPE Domain;
    PCSTR FormatString;
    ULONG Index;
    NETWORK_DEVICE_INFORMATION Information;
    struct sockaddr_dl *LinkAddress;
    size_t MaxDataLength;
    size_t NameLength;
    struct ifaddrs *NewInterface;
    struct ifaddrs *NewLinkInterface;
    KSTATUS Status;
    ULONG Subnet;

    NewInterface = NULL;
    NewLinkInterface = NULL;

    //
    // Query the system for the network information associated with this
    // device ID.
    //

    Status = ClpGetNetworkDeviceInformation(DeviceId, &Information);
    if (!KSUCCESS(Status)) {
        goto CreateNetworkInterfaceAddressEnd;
    }

    //
    // Create a C library network interface address structure based on the
    // network device information.
    //

    NewInterface = malloc(sizeof(struct ifaddrs));
    if (NewInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateNetworkInterfaceAddressEnd;
    }

    memset(NewInterface, 0, sizeof(struct ifaddrs));
    Domain = Information.PhysicalAddress.Domain;

    ASSERT(Domain >= NET_DOMAIN_PHYSICAL_BASE);

    Index = CL_NETWORK_NAME_INDEX_FROM_DOMAIN(Domain);
    FormatString = ClNetworkNameFormats[Index];
    Status = ClpCreateNetworkDeviceName(DeviceId,
                                        FormatString,
                                        &(NewInterface->ifa_name),
                                        0);

    if (!KSUCCESS(Status)) {
        goto CreateNetworkInterfaceAddressEnd;
    }

    //
    // If the network device is present in the query, then consider it "up". It
    // is only "running" if it fully configured and ready to receive traffic.
    //

    NewInterface->ifa_flags = IFF_UP;
    if ((Information.Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0) {
        NewInterface->ifa_flags |= IFF_RUNNING;
    }

    if (Information.Address.Domain != NetDomainInvalid) {
        NewInterface->ifa_addr = malloc(sizeof(struct sockaddr));
        if (NewInterface->ifa_addr == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNetworkInterfaceAddressEnd;
        }

        AddressLength = sizeof(struct sockaddr);
        Status = ClConvertFromNetworkAddress(&(Information.Address),
                                             NewInterface->ifa_addr,
                                             &AddressLength,
                                             NULL,
                                             0);

        if (!KSUCCESS(Status)) {
            goto CreateNetworkInterfaceAddressEnd;
        }
    }

    if (Information.Subnet.Domain != NetDomainInvalid) {
        NewInterface->ifa_netmask = malloc(sizeof(struct sockaddr));
        if (NewInterface->ifa_netmask == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNetworkInterfaceAddressEnd;
        }

        AddressLength = sizeof(struct sockaddr);
        Status = ClConvertFromNetworkAddress(&(Information.Subnet),
                                             NewInterface->ifa_netmask,
                                             &AddressLength,
                                             NULL,
                                             0);

        if (!KSUCCESS(Status)) {
            goto CreateNetworkInterfaceAddressEnd;
        }
    }

    //
    // Create a broadcast address if this is IPv4.
    //

    if ((Information.Address.Domain == NetDomainIp4) &&
        (Information.Subnet.Domain == NetDomainIp4)) {

        Broadcast = malloc(sizeof(struct sockaddr_in));
        if (Broadcast == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNetworkInterfaceAddressEnd;
        }

        memset(Broadcast, 0, sizeof(struct sockaddr_in));
        Broadcast->sin_family = AF_INET;
        Address = (ULONG)(Information.Address.Address[0]);
        Subnet = (ULONG)(Information.Subnet.Address[0]);
        Address &= Subnet;
        Address |= ~Subnet;
        Broadcast->sin_addr.s_addr = Address;
        NewInterface->ifa_broadaddr = (struct sockaddr *)Broadcast;
        NewInterface->ifa_flags |= IFF_BROADCAST;
    }

    //
    // Create a C library network interface structure for the link-layer
    // address. The native Minoca system returns both the link and socket layer
    // addresses together. The C library needs them separate.
    //

    NewLinkInterface = malloc(sizeof(struct ifaddrs));
    if (NewLinkInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateNetworkInterfaceAddressEnd;
    }

    memset(NewLinkInterface, 0, sizeof(struct ifaddrs));
    FormatString = ClNetworkNameFormats[CL_NETWORK_NAME_LINK_LAYER_INDEX];
    Status = ClpCreateNetworkDeviceName(DeviceId,
                                        FormatString,
                                        &(NewLinkInterface->ifa_name),
                                        0);

    if (!KSUCCESS(Status)) {
        goto CreateNetworkInterfaceAddressEnd;
    }

    //
    // If the network device is present in the query, then consider it "up". It
    // is only "running" if it fully configured and ready to receive traffic.
    //

    NewLinkInterface->ifa_flags = IFF_UP;
    if ((Information.Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0) {
        NewLinkInterface->ifa_flags |= IFF_RUNNING;
    }

    if (Information.PhysicalAddress.Domain != NetDomainInvalid) {
        AllocationSize = sizeof(struct sockaddr_dl);
        MaxDataLength = AllocationSize -
                        FIELD_OFFSET(struct sockaddr_dl, sdl_data);

        NameLength = strlen(NewLinkInterface->ifa_name);
        DataLength = NameLength + ETHERNET_ADDRESS_SIZE;
        if (DataLength > MaxDataLength) {
            AllocationSize += DataLength - MaxDataLength;
        }

        //
        // The name length better not be too long for the socket address
        // structure.
        //

        if (NameLength > MAX_UCHAR) {
            Status = STATUS_NAME_TOO_LONG;
            goto CreateNetworkInterfaceAddressEnd;
        }

        LinkAddress = malloc(AllocationSize);
        if (LinkAddress == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNetworkInterfaceAddressEnd;
        }

        RtlZeroMemory(LinkAddress, AllocationSize);
        LinkAddress->sdl_len = AllocationSize;
        LinkAddress->sdl_family = AF_LINK;
        LinkAddress->sdl_type = IFT_ETHER;
        if (Information.PhysicalAddress.Domain == NetDomain80211) {
            LinkAddress->sdl_type = IFT_IEEE80211;
        }

        LinkAddress->sdl_nlen = NameLength;
        LinkAddress->sdl_alen = ETHERNET_ADDRESS_SIZE;
        RtlCopyMemory(LinkAddress->sdl_data,
                      NewLinkInterface->ifa_name,
                      NameLength);

        RtlCopyMemory(LLADDR(LinkAddress),
                      Information.PhysicalAddress.Address,
                      ETHERNET_ADDRESS_SIZE);

        NewLinkInterface->ifa_addr = (struct sockaddr *)LinkAddress;
    }

CreateNetworkInterfaceAddressEnd:
    if (!KSUCCESS(Status)) {
        if (NewInterface != NULL) {
            ClpDestroyNetworkInterfaceAddress(NewInterface);
            NewInterface = NULL;
        }

        if (NewLinkInterface != NULL) {
            ClpDestroyNetworkInterfaceAddress(NewLinkInterface);
            NewLinkInterface = NULL;
        }
    }

    *Interface = NewInterface;
    *LinkInterface = NewLinkInterface;
    return Status;
}

VOID
ClpDestroyNetworkInterfaceAddress (
    struct ifaddrs *Interface
    )

/*++

Routine Description:

    This routine destroys the given network interface address structure and
    all its resources.

Arguments:

    Interface - Supplies a pointer to the interface to destroy.

Return Value:

    None.

--*/

{

    if (Interface->ifa_name != NULL) {
        free(Interface->ifa_name);
    }

    if (Interface->ifa_addr != NULL) {
        free(Interface->ifa_addr);
    }

    if (Interface->ifa_netmask != NULL) {
        free(Interface->ifa_netmask);
    }

    if (Interface->ifa_broadaddr != NULL) {
        free(Interface->ifa_broadaddr);
    }

    if (Interface->ifa_dstaddr != NULL) {
        free(Interface->ifa_dstaddr);
    }

    if (Interface->ifa_data != NULL) {
        free(Interface->ifa_data);
    }

    free(Interface);
    return;
}

