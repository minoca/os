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
#include <errno.h>
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
ClpGetNetworkDeviceDomain (
    DEVICE_ID DeviceId,
    PNET_DOMAIN_TYPE Domain
    );

KSTATUS
ClpGetNetworkDeviceInformation (
    DEVICE_ID DeviceId,
    PNETWORK_DEVICE_INFORMATION Information
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
    ULONG FormatIndex;
    ULONG NameLength;
    KSTATUS Status;

    Status = ClpGetNetworkDeviceDomain(DeviceId, &Domain);
    if (!KSUCCESS(Status)) {
        goto GetNetworkDeviceNameEnd;
    }

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
            goto GetNetworkDeviceNameEnd;
        }

    } else {
        *Buffer= malloc(NameLength);
        if (*Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetNetworkDeviceNameEnd;
        }
    }

    NameLength = RtlPrintToString(*Buffer,
                                  NameLength,
                                  CharacterEncodingDefault,
                                  ClNetworkNameFormats[FormatIndex],
                                  DeviceId);

GetNetworkDeviceNameEnd:
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

