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
#include <netinet/in.h>
#include <ifaddrs.h>
#include <errno.h>
#include <stdlib.h>
#include "net.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CL_NETWORK_NAME_FORMAT_COUNT 2

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
    NET_DOMAIN_TYPE Domain,
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
    struct ifaddrs **Interface
    );

VOID
ClpDestroyNetworkInterfaceAddress (
    struct ifaddrs *Interface
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR ClNetworkNameFormats[CL_NETWORK_NAME_FORMAT_COUNT] = {
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
                                                  &Interface);

        if (!KSUCCESS(Status)) {
            break;
        }

        *PreviousNextPointer = Interface;
        PreviousNextPointer = &(Interface->ifa_next);
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
    KSTATUS Status;

    Status = ClpGetNetworkDeviceDomain(DeviceId, &Domain);
    if (!KSUCCESS(Status)) {
        goto GetNetworkDeviceNameEnd;
    }

    Status = ClpCreateNetworkDeviceName(DeviceId, Domain, Buffer, BufferLength);
    if (!KSUCCESS(Status)) {
        goto GetNetworkDeviceNameEnd;
    }

GetNetworkDeviceNameEnd:
    return Status;
}

KSTATUS
ClpCreateNetworkDeviceName (
    DEVICE_ID DeviceId,
    NET_DOMAIN_TYPE Domain,
    PSTR *Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine creates a network device name based on the given device ID and
    network domain. The caller is expected to release the name if a NULL buffer
    is supplied.

Arguments:

    DeviceId - Supplies the ID of a network device.

    Domain - Supplies the domain of the network device.

    Buffer - Supplies a pointer to a buffer that receives the device name
        string. If the buffer is NULL, then a buffer will be allocated for the
        caller to release.

    BufferLength - Supplies the length of the device name buffer.

Return Value:

    Status code.

--*/

{

    ULONG FormatIndex;
    ULONG NameLength;
    KSTATUS Status;

    ASSERT(Domain >= NET_DOMAIN_PHYSICAL_BASE);

    FormatIndex = Domain - NET_DOMAIN_PHYSICAL_BASE;
    NameLength = RtlPrintToString(NULL,
                                  0,
                                  CharacterEncodingDefault,
                                  ClNetworkNameFormats[FormatIndex],
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
                                  ClNetworkNameFormats[FormatIndex],
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
    struct ifaddrs **Interface
    )

/*++

Routine Description:

    This routine creates a network interface address structure for the network
    device indicated by the given device ID.

Arguments:

    DeviceId - Supplies the ID of the network device for which the network
        interface address structure is to be created.

    Interface - Supplies a pointer that receives a pointer to a newly allocated
        network interface address structure. The caller is responsible for
        releasing the allocation.

Return Value:

    Status code.

--*/

{

    ULONG Address;
    socklen_t AddressLength;
    struct sockaddr_in *Broadcast;
    NETWORK_DEVICE_INFORMATION Information;
    struct ifaddrs *NewInterface;
    KSTATUS Status;
    ULONG Subnet;

    NewInterface = NULL;

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
    Status = ClpCreateNetworkDeviceName(DeviceId,
                                        Information.PhysicalAddress.Domain,
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
        Address = *((PULONG)Information.Address.Address);
        Subnet = *((PULONG)Information.Subnet.Address);
        Address &= Subnet;
        Address |= ~Subnet;
        Broadcast->sin_addr.s_addr = Address;
        NewInterface->ifa_broadaddr = (struct sockaddr *)Broadcast;
        NewInterface->ifa_flags = IFF_BROADCAST;
    }

CreateNetworkInterfaceAddressEnd:
    if (!KSUCCESS(Status)) {
        if (NewInterface != NULL) {
            ClpDestroyNetworkInterfaceAddress(NewInterface);
            NewInterface = NULL;
        }
    }

    *Interface = NewInterface;
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

