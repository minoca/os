/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gaddrinf.c

Abstract:

    This module implements support for the getaddrinfo function.

Author:

    Evan Green 5-Dec-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <minoca/devinfo/net.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include "net.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DNS_CLASS_INTERNET 1

#define DNS_RECORD_TYPE_A 1
#define DNS_RECORD_TYPE_NS 2
#define DNS_RECORD_TYPE_CNAME 5
#define DNS_RECORD_TYPE_SOA 6
#define DNS_RECORD_TYPE_PTR 12
#define DNS_RECORD_TYPE_MX 15
#define DNS_RECORD_TYPE_TXT 16
#define DNS_RECORD_TYPE_AAAA 28

#define DNS_MAX_QUERY_COUNT 50

//
// Define the safe guess size of a DNS response.
//

#define DNS_RESPONSE_ALLOCATION_SIZE 4096

//
// Define the amount of time to wait for a response before giving up, in
// milliseconds.
//

#define DNS_RESPONSE_TIMEOUT 30000

//
// Define the maximum size of the reverse DNS string.
//

#define DNS_IP4_REVERSE_TRANSLATION_NAME_SIZE \
    sizeof("255.255.255.255.in-addr.arpa")

#define DNS_IP6_REVERSE_TRANSLATION_NAME_SIZE          \
    sizeof("F.F.F.F.F.F.F.F.F.F.F.F.F.F.F.F."          \
           "F.F.F.F.F.F.F.F.F.F.F.F.F.F.F.F.ip6.arpa")

//
// Define the IPv4 and IPv6 reverse DNS lookup formats.
//

#define DNS_IP4_REVERSE_TRANSLATION_FORMAT "%d.%d.%d.%d.in-addr.arpa"
#define DNS_IP6_REVERSE_TRANSLATION_FORMAT                     \
    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."         \
    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa"

//
// Define the suffix for the IPv6 reverse DNS lookup string.
//

#define DNS_IP6_REVERSE_TRANSLATION_SUFFIX "ip6.arpa"
#define DNS_IP6_REVERSE_TRANSLATION_SUFFIX_SIZE \
    sizeof(DNS_IP6_REVERSE_TRANSLATION_SUFFIX)

//
// Define the strings for the default and DGRAM protocols used by getnameinfo.
//

#define NAME_INFORMATION_DEFAULT_PROTOCOL_NAME "TCP"
#define NAME_INFORMATION_DGRAM_PROTOCOL_NAME "UDP"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the internal format (just for software, not anything
    out on the network) of a DNS response.

Members:

    ListEntry - Stores pointers to the next and previous results.

    Name - Stores a pointer to the heap allocated name of the resource.

    Type - Stores the type of the resource. See DNS_RECORD_TYPE_* definitions.

    Class - Stores the class of the resource. See DNS_CLASS_* definitions.

    ExpirationTime - Stores the time at which this record expires.

    Value - Stores a pointer to the heap allocated value string for record
        types whose values are also names.

    Address - Stores the network address value for record types whose values
        are addresses.

--*/

typedef struct _DNS_RESULT {
    LIST_ENTRY ListEntry;
    PSTR Name;
    USHORT Type;
    USHORT Class;
    time_t ExpirationTime;
    PSTR Value;
    struct sockaddr Address;
} DNS_RESULT, *PDNS_RESULT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpPerformDnsTranslation (
    PSTR Name,
    UCHAR RecordType,
    NET_DOMAIN_TYPE Domain,
    PLIST_ENTRY ListHead,
    ULONG RecursionDepth
    );

INT
ClpPerformDnsReverseTranslation (
    const struct sockaddr *SocketAddress,
    socklen_t SocketAddressLength,
    PLIST_ENTRY ListHead
    );

INT
ClpPerformDnsQuery (
    PSTR Name,
    CHAR RecordType,
    struct sockaddr *NameServer,
    socklen_t NameServerSize,
    PLIST_ENTRY ListHead
    );

INT
ClpCreateDnsQuery (
    PSTR Name,
    UCHAR ResponseType,
    PDNS_HEADER *NewRequest,
    PULONG RequestSize
    );

INT
ClpExecuteDnsQuery (
    struct sockaddr *NameServer,
    socklen_t NameServerSize,
    PDNS_HEADER Request,
    ULONG RequestSize,
    PDNS_HEADER *Response,
    PULONG ResponseSize
    );

INT
ClpParseDnsResponse (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PLIST_ENTRY ListHead
    );

INT
ClpParseDnsResponseElement (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PUCHAR *ResponseEntry,
    PLIST_ENTRY ListHead
    );

INT
ClpDecompressDnsName (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PUCHAR *DnsName,
    PSTR *OutputName
    );

INT
ClpScanDnsName (
    PUCHAR Response,
    ULONG ResponseSize,
    PUCHAR Name,
    PSTR OutputName,
    PULONG OutputNameSize
    );

VOID
ClpDestroyDnsResultList (
    PLIST_ENTRY ListHead
    );

VOID
ClpDestroyDnsResult (
    PDNS_RESULT Result
    );

VOID
ClpDebugPrintDnsResult (
    PDNS_RESULT Result
    );

INT
ClpGetNameServerAddress (
    PDNS_RESULT NameServer,
    UCHAR RecordType,
    PLIST_ENTRY TranslationList,
    struct sockaddr *NameServerAddress,
    ULONG RecursionDepth
    );

INT
ClpFindNameServerAddress (
    PDNS_RESULT NameServer,
    UCHAR RecordType,
    PLIST_ENTRY TranslationList,
    struct sockaddr *NameServerAddress
    );

INT
ClpSearchDnsResultList (
    PSTR Name,
    UCHAR RecordType,
    time_t CurrentTime,
    PLIST_ENTRY ListHead,
    PLIST_ENTRY DestinationListHead
    );

INT
ClpConvertDnsResultListToAddressInformation (
    PLIST_ENTRY ListHead,
    const struct addrinfo *Hints,
    ULONG Port,
    BOOL MapV4Addresses,
    struct addrinfo **AddressInformation
    );

INT
ClpConvertDnsResultToAddressInformation (
    PDNS_RESULT Result,
    BOOL CopyCanonicalName,
    ULONG Port,
    struct addrinfo **AddressInformation
    );

INT
ClpGetAddressInformationPort (
    PSTR ServiceName,
    const struct addrinfo *Hints,
    PULONG Port
    );

VOID
ClpDebugPrintAddressInformation (
    struct addrinfo *AddressInformation
    );

VOID
ClpFillInLoopbackAddress (
    int AddressFamily,
    struct sockaddr *Address
    );

INT
ClpGetDnsServers (
    NET_DOMAIN_TYPE Domain,
    struct sockaddr *PrimaryServer,
    PLIST_ENTRY AlternateList
    );

INT
ClpGetNetworkStatus (
    PBOOL Ip4Configured,
    PBOOL Ip6Configured
    );

INT
ClpGetLocalAddressInformation (
    int AddressFamily,
    PLIST_ENTRY ListHead
    );

INT
ClpIsLocalAddress (
    const struct sockaddr *SocketAddress,
    socklen_t SocketAddressLength,
    PBOOL LocalAddress,
    PBOOL UnspecifiedAddress
    );

BOOL
ClpIsNameSubdomain (
    PCSTR Query,
    PCSTR Domain
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to debug DNS queries.
//

BOOL ClDebugDns = FALSE;

//
// Define the address information error strings.
//

PSTR ClGetAddressInformationErrorStrings[] = {
    "No error",
    "Address family not supported for hostname",
    "Try again",
    "Invalid flags",
    "Failed",
    "Invalid address family",
    "Out of memory",
    "No address associated with hostname",
    "Name not found",
    "Service not supported",
    "Invalid socket type",
    "System error",
    "Buffer overflow"
};

//
// Define the network device information UUID.
//

UUID ClNetworkDeviceInformationUuid = NETWORK_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
freeaddrinfo (
    struct addrinfo *AddressInformation
    )

/*++

Routine Description:

    This routine frees the address structure returned by the getaddrinfo, along
    with any additional storage associated with those structures. If the
    ai_next field of the structure is not null, the entire list of structures
    shall be freed. This routine supports the freeing of arbitrary sublists of
    an addrinfo structure originally returned by getaddrinfo. This routine is
    thread safe.

Arguments:

    AddressInformation - Supplies a pointer to the address information to free.

Return Value:

    None.

--*/

{

    struct addrinfo *Next;

    while (AddressInformation != NULL) {
        Next = AddressInformation->ai_next;
        if (AddressInformation->ai_canonname != NULL) {
            free(AddressInformation->ai_canonname);
        }

        AddressInformation->ai_next = NULL;
        free(AddressInformation);
        AddressInformation = Next;
    }

    return;
}

LIBC_API
int
getaddrinfo (
    const char *NodeName,
    const char *ServiceName,
    const struct addrinfo *Hints,
    struct addrinfo **Result
    )

/*++

Routine Description:

    This routine trnalsates the name of a service location (a host name for
    example) and/or a service name and returns a set of socket addresses and
    associated information to be used in creating a socket with which to
    address the given service. In many cases this is backed by DNS. This
    routine is thread safe.

Arguments:

    NodeName - Supplies an optional pointer to a null-terminated string
        containing the node (host) name to get address information for. If no
        service name is supplied, this routine returns the network-level
        addresses for the specified node name. At least one (or both) of the
        node name and service name parameter must be non-null (they cannot both
        be null).

    ServiceName - Supplies an optional pointer to a null-terminated string
        containing the service name to get address information for. This can
        be a descriptive name string or a decimal port number string.

    Hints - Supplies an optional pointer to an address structure that may
        limit the returned information to a specific socket type, address
        family, and/or protocol. In this hints structure every member other
        than ai_flags, ai_faimly, ai_socktype, and ai_protocol shall be set to
        zero or NULL. A value of AF_UNSPEC for ai_family means that the caller
        accepts any address family. A value of zero for ai_socktype means that
        the caller accepts any socket type. A value of zero for ai_protocol
        means that the caller accepts any protocol. If this parameter is not
        supplied, it's the same as if a parameter were passed with zero for all
        these values. The ai_flags member can be set to any combination of the
        following flags:

        AI_PASSIVE - Specifies that the returned address information shall be
        suitable for use in binding a socket for incoming connections. If the
        node name argument is NULL, then the IP address portion of the returned
        structure will be INADDR_ANY or IN6ADDR_ANY_INIT. Otherwise, the
        returned information shall be suitable for a call to connect. If the
        node name parameter is NULL in this case, then the address returned
        shall be set to the loopback address. This flag is ignored if the node
        name is not NULL.

        AI_CANONNAME - Specifies that this routine should try to determine the
        canonical name of the given node name (for example, if the given node
        name is an alias or shorthand notation for a complete name).

        AI_NUMERICHOST - Specifies that a non-null node name parameter is a
        numeric host address string. Otherwise, an EAI_NONAME error shall be
        returned. This prevents any sort of name resolution service (like DNS)
        from being invoked.

        AI_NUMERICSERV - Specifies that a non-null service name supplied is a
        numeric port string. Otherwise, an EAI_NONAME error shall be returned.
        This flag prevents any sort of name resolution service (like NIS+) from
        being invoked.

        AI_V4MAPPED - Specifies that this routine should returne IPv4-mapped
        IPv6 address on finding no matching IPv6 addresses. This is ignored
        unless the ai_family parameter is set to AF_INET6. If the AI_ALL flag
        is used as well, then this routine returns all IPv6 and IPv4 addresses.
        The AI_ALL flag without the AI_V4MAPPED flag is ignored.

        AI_ADDRCONFIG - Specifies that IPv4 addresses should be returned only
        if an IPv4 address is configured on the local system, and IPv6
        addresses shall be returned only if an IPv6 address is configured on
        the local system.

    Result - Supplies a pointer where a linked list of address results will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure, and errno will contain more information. Common values
    of errno include:

    EAI_AGAIN if the name could not be resolved at this time. Future attempts
    may succeed.

    EAI_BADFLAGS if the flags parameter had an invalid value.

    EAI_FAIL if a non-recoverable error occurred when attempting to resolve the
    name.

    EAI_FAMILY if the address family was not recognized.

    EAI_MEMORY if there was a memory allocation failure.

    EAI_NONAME if the name does not resolve for the supplied parameters, or
    neither the node name or service name were supplied.

    EAI_SERVICE if the service passed was not recognized for the given socket
    type.

    EAI_SOCKTYPE if the given socket type was not recognized.

    EAI_SYSTEM if a system error occurred.

    EAI_OVERFLOW if an argument buffer overflowed.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDNS_RESULT DnsResult;
    INT Family;
    char HostName[HOST_NAME_MAX];
    struct addrinfo *Information;
    struct sockaddr_in *Ip4Address;
    BOOL Ip4Configured;
    struct sockaddr_in6 *Ip6Address;
    BOOL Ip6Configured;
    DNS_RESULT LocalResult[2];
    BOOL MapV4Addresses;
    ULONG Port;
    LIST_ENTRY ResultList;
    int Status;

    Family = 0;
    memset(&LocalResult, 0, sizeof(DNS_RESULT) * 2);
    Information = NULL;
    LocalResult[0].Class = DNS_CLASS_INTERNET;
    LocalResult[1].Class = DNS_CLASS_INTERNET;
    MapV4Addresses = FALSE;
    Port = 0;
    INITIALIZE_LIST_HEAD(&ResultList);

    //
    // One or both of the node name and service name must be supplied.
    //

    if ((NodeName == NULL) && (ServiceName == NULL)) {
        return EAI_NONAME;
    }

    //
    // Validate hints.
    //

    if (Hints != NULL) {
        if ((Hints->ai_addrlen != 0) ||
            (Hints->ai_canonname != NULL) ||
            (Hints->ai_addr != NULL) ||
            (Hints->ai_next != NULL)) {

            Status = EAI_BADFLAGS;
            goto getaddrinfoEnd;
        }

        if ((Hints->ai_family != AF_UNSPEC) &&
            (Hints->ai_family != AF_INET) &&
            (Hints->ai_family != AF_INET6)) {

            Status = EAI_FAMILY;
            goto getaddrinfoEnd;
        }

        if ((Hints->ai_socktype != 0) &&
            (Hints->ai_socktype != SOCK_STREAM) &&
            (Hints->ai_socktype != SOCK_DGRAM) &&
            (Hints->ai_socktype != SOCK_RAW)) {

            Status = EAI_SOCKTYPE;
            goto getaddrinfoEnd;
        }

        Family = Hints->ai_family;

        //
        // If the address configuration flag is supplied, limit the family to
        // what is configured on the local system.
        //

        if ((Hints->ai_flags & AI_ADDRCONFIG) != 0) {
            Status = ClpGetNetworkStatus(&Ip4Configured, &Ip6Configured);
            if (Status != 0) {
                goto getaddrinfoEnd;
            }

            if (Family == AF_UNSPEC) {
                if ((Ip4Configured == FALSE) && (Ip6Configured == FALSE)) {
                    Status = EAI_AGAIN;
                    goto getaddrinfoEnd;

                } else if (Ip4Configured == FALSE) {
                    Family = AF_INET6;

                } else if (Ip6Configured == FALSE) {
                    Family = AF_INET;
                }

            } else if (((Family == AF_INET) && (Ip4Configured == FALSE)) ||
                       ((Family == AF_INET6) && (Ip6Configured == FALSE))) {

                Status = EAI_NONAME;
                goto getaddrinfoEnd;
            }
        }
    }

    //
    // Convert the service name into a port number.
    //

    Status = ClpGetAddressInformationPort((PSTR)ServiceName, Hints, &Port);
    if (Status != 0) {
        goto getaddrinfoEnd;
    }

    //
    // If there is no node name, then honor the passive flag.
    //

    if (NodeName == NULL) {

        //
        // If passive is selected, stick the "any address" into the address
        // (which is just the zeroed address it already is).
        //

        if ((Hints != NULL) && ((Hints->ai_flags & AI_PASSIVE) != 0)) {
            if (Family != 0) {
                LocalResult[0].Address.sa_family = Family;
                INSERT_BEFORE(&(LocalResult[0].ListEntry), &ResultList);

            } else {
                LocalResult[0].Address.sa_family = AF_INET;
                INSERT_BEFORE(&(LocalResult[0].ListEntry), &ResultList);
                LocalResult[1].Address.sa_family = AF_INET6;
                INSERT_BEFORE(&(LocalResult[1].ListEntry), &ResultList);
            }

        //
        // Otherwise use the local loopback address.
        //

        } else {
            if (Family != 0) {
                ClpFillInLoopbackAddress(Family, &(LocalResult[0].Address));
                INSERT_BEFORE(&(LocalResult[0].ListEntry), &ResultList);

            } else {
                ClpFillInLoopbackAddress(AF_INET, &(LocalResult[0].Address));
                INSERT_BEFORE(&(LocalResult[0].ListEntry), &ResultList);
                ClpFillInLoopbackAddress(AF_INET6, &(LocalResult[1].Address));
                INSERT_BEFORE(&(LocalResult[1].ListEntry), &ResultList);
            }
        }

    //
    // Go do the hard work and translate the host name.
    //

    } else {

        //
        // First try to translate the host name to a number.
        //

        Status = 0;
        LocalResult[0].Name = (PSTR)NodeName;
        if ((Family == AF_UNSPEC) || (Family == AF_INET6)) {
            LocalResult[0].Type = DNS_RECORD_TYPE_AAAA;
            LocalResult[0].Address.sa_family = AF_INET6;
            Ip6Address = (struct sockaddr_in6 *)&(LocalResult[0].Address);
            Status = inet_pton(AF_INET6, NodeName, &(Ip6Address->sin6_addr));
        }

        if ((Status == 0) && ((Family == AF_UNSPEC) || (Family == AF_INET))) {
            LocalResult[0].Type = DNS_RECORD_TYPE_A;
            LocalResult[0].Address.sa_family = AF_INET;
            Ip4Address = (struct sockaddr_in *)&(LocalResult[0].Address);
            Status = inet_pton(AF_INET, NodeName, &(Ip4Address->sin_addr));
        }

        //
        // If the numeric translation worked, convert and finish.
        //

        if (Status == 1) {
            INSERT_BEFORE(&(LocalResult[0].ListEntry), &ResultList);
            Status = ClpConvertDnsResultListToAddressInformation(&ResultList,
                                                                 Hints,
                                                                 Port,
                                                                 MapV4Addresses,
                                                                 &Information);

            goto getaddrinfoEnd;
        }

        //
        // If the caller doesn't want name resolution, then this is where they
        // get off the bus.
        //

        if ((Hints != NULL) && ((Hints->ai_flags & AI_NUMERICHOST) != 0)) {
            Status = EAI_FAIL;
            goto getaddrinfoEnd;
        }

        //
        // Before reaching out with a DNS query, take a look at the local host
        // name.
        //

        Status = gethostname(HostName, sizeof(HostName));
        if (Status == 0) {
            if (strcmp(NodeName, HostName) == 0) {

                //
                // Get the local IP addresses based on the family type.
                //

                Status = ClpGetLocalAddressInformation(Family, &ResultList);
                if (Status != 0) {
                    goto getaddrinfoEnd;
                }

                Status = ClpConvertDnsResultListToAddressInformation(
                                                                &ResultList,
                                                                Hints,
                                                                Port,
                                                                MapV4Addresses,
                                                                &Information);

                goto getaddrinfoEnd;
            }
        }

        //
        // TODO: Remove this when IPv6 is supported. Allowing the IPv6
        // translation is problematic because the name servers returned may
        // also be IPv6 addresses, causing the translation to fail rather than
        // fall back to IPv4.
        //

        if (Family == AF_UNSPEC) {
            Family = AF_INET;
        }

        //
        // This is going to take the big leagues, translating a real address.
        // If IPv6 or any family is requested, get IPv6 translations.
        //

        if ((Family == AF_UNSPEC) || (Family == AF_INET6)) {
            Status = ClpPerformDnsTranslation((char *)NodeName,
                                              DNS_RECORD_TYPE_AAAA,
                                              NetDomainIp6,
                                              &ResultList,
                                              0);

            if (Status != 0) {
                goto getaddrinfoEnd;
            }
        }

        //
        // If IPv4 or any family is requested, get IPv4 translations.
        // Additionally, if the family is IPv6, the v4-mapped flag is set, and
        // there were no IPv6 translations (or the 'all' flag is set), also
        // get IPv4 translations.
        //

        if ((Family == AF_UNSPEC) || (Family == AF_INET) ||
            ((Family == AF_INET6) && (Hints != NULL) &&
             ((Hints->ai_flags & AI_V4MAPPED) != 0) &&
             (((Hints->ai_flags & AI_ALL) != 0) ||
              (LIST_EMPTY(&ResultList) != 0)))) {

            if (Family == AF_INET6) {
                MapV4Addresses = TRUE;
            }

            Status = ClpPerformDnsTranslation((char *)NodeName,
                                              DNS_RECORD_TYPE_A,
                                              NetDomainIp4,
                                              &ResultList,
                                              0);

            if (Status != 0) {
                goto getaddrinfoEnd;
            }
        }

        //
        // For a good time, print the DNS result list.
        //

        if (ClDebugDns != FALSE) {
            if (Status != 0) {
                fprintf(stderr,
                        "DNS: Translation failed: (%d) %s.\n",
                        Status,
                        gai_strerror(Status));

            } else if (LIST_EMPTY(&ResultList) != FALSE) {
                fprintf(stderr, "DNS: Found no translation.\n");

            } else {
                fprintf(stderr, "DNS: Final Results:\n");
                CurrentEntry = ResultList.Next;
                while (CurrentEntry != &ResultList) {
                    DnsResult = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
                    ClpDebugPrintDnsResult(DnsResult);
                    CurrentEntry = CurrentEntry->Next;
                }
            }
        }
    }

    Status = ClpConvertDnsResultListToAddressInformation(&ResultList,
                                                         Hints,
                                                         Port,
                                                         MapV4Addresses,
                                                         &Information);

    if (Status != 0) {
        goto getaddrinfoEnd;
    }

    Status = 0;

getaddrinfoEnd:
    if (ResultList.Next != &(LocalResult[0].ListEntry)) {

        assert(ResultList.Next != &(LocalResult[1].ListEntry));

        ClpDestroyDnsResultList(&ResultList);
    }

    if (Status != 0) {
        if (Information != NULL) {
            freeaddrinfo(Information);
            Information = NULL;
        }
    }

    if (ClDebugDns != FALSE) {
        fprintf(stderr,
                "getaddrinfo: Name %s Service %s: %s.\n",
                NodeName,
                ServiceName,
                gai_strerror(Status));

        ClpDebugPrintAddressInformation(Information);
    }

    //
    // If a success status is being returned, the result better be non-null.
    //

    assert((Status != 0) || (Information != NULL));

    *Result = Information;
    return Status;
}

LIBC_API
int
getnameinfo (
    const struct sockaddr *SocketAddress,
    socklen_t SocketAddressLength,
    char *Node,
    socklen_t NodeLength,
    char *Service,
    socklen_t ServiceLength,
    int Flags
    )

/*++

Routine Description:

    This routine translates the given socket address to a node name and service
    location, defined as in getaddrinfo.

Arguments:

    SocketAddress - Supplies a pointer to the socket address to be translated.
        If this is an IPv4-mapped IPv6 address or an IPv4-compatible IPv6
        address then the implementation shall extract the IPv4 address and look
        up the node name for that IPv4 address. The IPv6 unspecified address
        ("::") and the IPv6 loopback address ("::1") are not IPv4-compatible
        addresses.

    SocketAddressLength - Supplies the size of the socket address data.

    Node - Supplies an optional pointer to a buffer where the node name string
        will be returned on success. If the node name cannot be determined, the
        numeric form of the address will be returned.

    NodeLength - Supplies the size of the node buffer in bytes.

    Service - Supplies an optional pointer to a buffer where the service name
        string will be returned. If the service name cannot be returned, the
        numeric form of the service address (port number) shall be returned
        instead of its name.

    ServiceLength - Supplies the length of the service buffer in bytes.

    Flags - Supplies a bitfield of flags that governs the behavior of the
        function. See NI_* definitions.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    void *Address;
    char AddressString[INET6_ADDRSTRLEN];
    PLIST_ENTRY CurrentEntry;
    PDNS_RESULT DnsResult;
    char *DomainName;
    struct sockaddr_in *Ip4Address;
    struct sockaddr_in Ip4MappedAddress;
    struct sockaddr_in6 *Ip6Address;
    BOOL LocalAddress;
    const struct sockaddr *LookupAddress;
    socklen_t LookupAddressLength;
    in_port_t Port;
    const char *Protocol;
    LIST_ENTRY ResultList;
    struct servent *ServiceEntry;
    int Status;
    BOOL UnspecifiedAddress;

    if (SocketAddressLength < sizeof(struct sockaddr_in)) {
        Status = EAI_FAIL;
        goto getnameinfoEnd;
    }

    LookupAddress = SocketAddress;
    LookupAddressLength = SocketAddressLength;

    //
    // Collect the port information and determine whether or not the address
    // is one of the loopback address or unspecified address.
    //

    switch (SocketAddress->sa_family) {
    case AF_INET:
        Ip4Address = (struct sockaddr_in *)SocketAddress;
        Port = Ip4Address->sin_port;
        Address = &(Ip4Address->sin_addr.s_addr);
        break;

    case AF_INET6:
        Ip6Address = (struct sockaddr_in6 *)SocketAddress;
        Port = Ip6Address->sin6_port;
        Address = &(Ip6Address->sin6_addr.s6_addr);

        //
        // If this is an IPv4-mapped or IPv4-compatible address, then build out
        // an IPv4 structure for DNS lookup.
        //

        if ((IN6_IS_ADDR_V4MAPPED(&(Ip6Address->sin6_addr)) != FALSE) ||
            (IN6_IS_ADDR_V4COMPAT(&(Ip6Address->sin6_addr)) != FALSE)) {

            Ip4MappedAddress.sin_family = AF_INET;
            Ip4MappedAddress.sin_port = Port;
            memcpy(&(Ip4MappedAddress.sin_addr.s_addr),
                   &(Ip6Address->sin6_addr.s6_addr[12]),
                   sizeof(in_addr_t));

            LookupAddress = (struct sockaddr *)&Ip4MappedAddress;
            LookupAddressLength = sizeof(struct sockaddr_in);
        }

        break;

    default:
        Status = EAI_ADDRFAMILY;
        goto getnameinfoEnd;
    }

    //
    // Get the node name if requested.
    //

    if ((Node != NULL) && (NodeLength != 0)) {
        Status = EAI_NONAME;
        if ((Flags & NI_NUMERICHOST) == 0) {

            //
            // Test to see if the lookup address is a local address.
            //

            Status = ClpIsLocalAddress(LookupAddress,
                                       LookupAddressLength,
                                       &LocalAddress,
                                       &UnspecifiedAddress);

            if (Status == 0) {

                //
                // If the address cannot be resolved locally, setup up and
                // perform a reverse DNS lookup. If this succeeds then return
                // the information for the first PTR entry in the list of
                // results.
                //

                if (LocalAddress == FALSE) {
                    INITIALIZE_LIST_HEAD(&ResultList);
                    Status = ClpPerformDnsReverseTranslation(
                                                           LookupAddress,
                                                           LookupAddressLength,
                                                           &ResultList);

                    if (Status == 0) {
                        DnsResult = NULL;
                        CurrentEntry = ResultList.Next;
                        while (CurrentEntry != &(ResultList)) {
                            DnsResult = LIST_VALUE(ResultList.Next,
                                                   DNS_RESULT,
                                                   ListEntry);

                            if (DnsResult->Type == DNS_RECORD_TYPE_PTR) {
                                break;
                            }

                            DnsResult = NULL;
                        }

                        if (DnsResult != NULL) {
                            strncpy(Node, DnsResult->Value, NodeLength);

                        } else {
                            Status = EAI_NONAME;
                        }

                        ClpDestroyDnsResultList(&ResultList);
                    }

                //
                // Otherwise if it is a local address that is not the any
                // address, then return the host name.
                //

                } else if (UnspecifiedAddress == FALSE) {
                    Status = gethostname(Node, NodeLength);
                    if (Status != 0) {
                        Status = EAI_NONAME;

                    //
                    // On success, strip the domain if only the node name was
                    // requested.
                    //

                    } else if ((Flags & NI_NOFQDN) != 0) {
                        DomainName = strchr(Node, '.');
                        if (DomainName != NULL) {
                            *DomainName = '\0';
                        }
                    }

                //
                // Lastly, the unspecified address was supplied. There is no
                // name for that, unfortunately.
                //

                } else {
                    Status = EAI_NONAME;
                }
            }

            if ((Status != 0) && ((Flags & NI_NAMEREQD) != 0)) {
                goto getnameinfoEnd;
            }
        }

        //
        // If the lookup failed, or was never attempted, get the numeric string
        // and copy it into the node string.
        //

        if (Status != 0) {
            inet_ntop(SocketAddress->sa_family,
                      Address,
                      AddressString,
                      sizeof(AddressString));

            strncpy(Node, AddressString, NodeLength);
        }
    }

    //
    // Convert the service into a string.
    //

    if ((Service != NULL) && (ServiceLength != 0)) {
        ServiceEntry = NULL;
        if ((Flags & NI_NUMERICSERV) == 0) {
            if ((Flags & NI_DGRAM) != 0) {
                Protocol = NAME_INFORMATION_DGRAM_PROTOCOL_NAME;

            } else {
                Protocol = NAME_INFORMATION_DEFAULT_PROTOCOL_NAME;
            }

            ServiceEntry = getservbyport(Port, Protocol);
        }

        if (ServiceEntry != NULL) {
            strncpy(Service, ServiceEntry->s_name, ServiceLength);

        } else {
            snprintf(Service, ServiceLength, "%d", ntohs(Port));
        }
    }

    Status = 0;

getnameinfoEnd:
    return Status;
}

LIBC_API
const char *
gai_strerror (
    int ErrorCode
    )

/*++

Routine Description:

    This routine returns a string describing the given error value set by
    getaddrinfo or getnameinfo.

Arguments:

    ErrorCode - Supplies the value of errno set by the function. This routine
        returns strings for at least the following values: EAI_AGAIN,
        EAI_BADFLAGS, EAI_FAIL, EAI_FAMILY, EAI_MEMORY, EAI_NONAME,
        EAI_OVERFLOW, EAI_SERVICE, EAI_SOCKTYPE, and EAI_SYSTEM.

Return Value:

    Returns a pointer to a string describing the error.

--*/

{

    if ((ErrorCode < 0) || (ErrorCode > EAI_OVERFLOW)) {
        return "Unknown error";
    }

    return ClGetAddressInformationErrorStrings[ErrorCode];
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpPerformDnsTranslation (
    PSTR Name,
    UCHAR RecordType,
    NET_DOMAIN_TYPE Domain,
    PLIST_ENTRY ListHead,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine performs a DNS query on the given name.

Arguments:

    Name - Supplies the name to execute the DNS request for.

    RecordType - Supplies the type of record to query for. See
        DNS_RECORD_TYPE_* definitions.

    Domain - Supplies the default network domain to use for DNS queries.

    ListHead - Supplies a pointer to the initialized list head where the
        desired DNS results will be returned.

    RecursionDepth - Supplies the recursion depth of this function, used to
        avoid getting in a loop of calling nameservers to translate nameservers.
        Supply 0 initially.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    PDNS_RESULT Alias;
    BOOL FollowedAlias;
    INT MatchCount;
    PDNS_RESULT NameServer;
    struct sockaddr NameServerAddress;
    LIST_ENTRY NameServerList;
    INT QueryCount;
    LIST_ENTRY ResultList;
    time_t StartTime;
    INT Status;
    LIST_ENTRY TranslationList;

    assert((RecordType == DNS_RECORD_TYPE_A) ||
           (RecordType == DNS_RECORD_TYPE_AAAA) ||
           (RecordType == DNS_RECORD_TYPE_PTR));

    assert((Domain == NetDomainIp4) ||
           (Domain == NetDomainIp6));

    assert((RecordType != DNS_RECORD_TYPE_A) ||
           (Domain == NetDomainIp4));

    assert((RecordType != DNS_RECORD_TYPE_AAAA) ||
           (Domain == NetDomainIp6));

    QueryCount = 0;
    INITIALIZE_LIST_HEAD(&NameServerList);
    INITIALIZE_LIST_HEAD(&ResultList);
    INITIALIZE_LIST_HEAD(&TranslationList);
    StartTime = time(NULL);
    if (RecursionDepth > 10) {
        fprintf(stderr, "Error: DNS recursion loop.\n");
        return EAI_AGAIN;
    }

    //
    // Attempt to get the DNS servers. If the default does not exist, then try
    // to perform the lookup on another network.
    //

    Status = ClpGetDnsServers(Domain, &NameServerAddress, &NameServerList);
    if (Status != 0) {
        if (Status == ENOENT) {
            if (Domain == NetDomainIp4) {
                Domain = NetDomainIp6;

            } else {
                Domain = NetDomainIp4;
            }

            Status = ClpGetDnsServers(Domain,
                                      &NameServerAddress,
                                      &NameServerList);
        }

        if (Status != 0) {
            if (Status == ENOENT) {
                Status = EAI_AGAIN;

            } else {
                errno = Status;
                Status = EAI_SYSTEM;
            }

            goto PerformDnsTranslationEnd;
        }
    }

    //
    // Loop querying name servers for results.
    //

    while (TRUE) {
        Status = ClpPerformDnsQuery(Name,
                                    RecordType,
                                    &NameServerAddress,
                                    sizeof(struct sockaddr),
                                    &ResultList);

        if (Status != 0) {
            goto PerformDnsTranslationEnd;
        }

        QueryCount += 1;
        if (QueryCount >= DNS_MAX_QUERY_COUNT) {
            Status = EAI_FAIL;
            goto PerformDnsTranslationEnd;
        }

        //
        // Loop following CNAME entries.
        //

        FollowedAlias = FALSE;
        while (TRUE) {

            //
            // Look directly for a result that matches the record and name.
            //

            MatchCount = ClpSearchDnsResultList(Name,
                                                RecordType,
                                                StartTime,
                                                &ResultList,
                                                ListHead);

            if (MatchCount != 0) {
                if (ClDebugDns != FALSE) {
                    fprintf(stderr, "DNS: Found %d results.\n", MatchCount);
                }

                Status = 0;
                goto PerformDnsTranslationEnd;
            }

            //
            // Look for a CNAME entry.
            //

            MatchCount = ClpSearchDnsResultList(Name,
                                                DNS_RECORD_TYPE_CNAME,
                                                StartTime,
                                                &ResultList,
                                                ListHead);

            if (MatchCount == 0) {
                break;
            }

            //
            // There shouldn't be multiple aliases for the same name.
            //

            if (MatchCount > 1) {
                Status = EAI_FAIL;
                goto PerformDnsTranslationEnd;
            }

            //
            // Set the name to the alias name, and try again.
            //

            FollowedAlias = TRUE;
            Alias = LIST_VALUE(ListHead->Previous, DNS_RESULT, ListEntry);
            Name = Alias->Value;

            assert(Name != NULL);

            if (ClDebugDns != FALSE) {
                fprintf(stderr, "DNS: Following alias to %s.\n", Name);
            }

            //
            // Following an alias counts as a query to detect alias loops.
            //

            QueryCount += 1;
            if (QueryCount >= DNS_MAX_QUERY_COUNT) {
                Status = EAI_FAIL;
                goto PerformDnsTranslationEnd;
            }
        }

        //
        // If an alias was followed, keep the current name server.
        //

        if (FollowedAlias != FALSE) {
            continue;
        }

        //
        // Clear out any old translation results.
        //

        ClpDestroyDnsResultList(&TranslationList);

        //
        // Move all translations onto the translation list, and all name
        // servers onto the name server list.
        //

        ClpSearchDnsResultList(NULL,
                               DNS_RECORD_TYPE_A,
                               StartTime,
                               &ResultList,
                               &TranslationList);

        ClpSearchDnsResultList(NULL,
                               DNS_RECORD_TYPE_AAAA,
                               StartTime,
                               &ResultList,
                               &TranslationList);

        ClpSearchDnsResultList(NULL,
                               DNS_RECORD_TYPE_NS,
                               StartTime,
                               &ResultList,
                               &NameServerList);

        ClpSearchDnsResultList(NULL,
                               DNS_RECORD_TYPE_SOA,
                               StartTime,
                               &ResultList,
                               &NameServerList);

        ClpDestroyDnsResultList(&ResultList);

        //
        // Loop trying to find the next name server to query.
        //

        while (TRUE) {

            //
            // Get the next name server in the list. Translate the name server
            // to the the address of the name server.
            //

            if (LIST_EMPTY(&NameServerList) != FALSE) {
                if (ClDebugDns != FALSE) {
                    fprintf(stderr, "Out of DNS servers to try.\n");
                }

                Status = 0;
                goto PerformDnsTranslationEnd;
            }

            NameServer = LIST_VALUE(NameServerList.Next, DNS_RESULT, ListEntry);

            //
            // If the name server returned is a subdomain of the name being
            // translated, then skip it unless the answer's already there.
            //

            if ((NameServer->Value != NULL) &&
                (ClpIsNameSubdomain(NameServer->Value, Name) != FALSE)) {

                Status = ClpFindNameServerAddress(NameServer,
                                                  RecordType,
                                                  &TranslationList,
                                                  &NameServerAddress);

                if (Status != 0) {
                    if (ClDebugDns != FALSE) {
                        fprintf(stderr,
                                "Skipping name server %s, subdomain of %s\n",
                                NameServer->Value,
                                Name);
                    }
                }

            } else {
                if (ClDebugDns != FALSE) {
                    fprintf(stderr,
                            "Trying name server '%s'.\n",
                            NameServer->Value);
                }

                Status = ClpGetNameServerAddress(NameServer,
                                                 RecordType,
                                                 &TranslationList,
                                                 &NameServerAddress,
                                                 RecursionDepth);
            }

            //
            // Remove this name server from the list of name servers to try.
            //

            LIST_REMOVE(&(NameServer->ListEntry));
            ClpDestroyDnsResult(NameServer);
            if (Status == 0) {
                break;
            }
        }
    }

PerformDnsTranslationEnd:
    ClpDestroyDnsResultList(&ResultList);
    ClpDestroyDnsResultList(&TranslationList);
    ClpDestroyDnsResultList(&NameServerList);
    return Status;
}

INT
ClpPerformDnsReverseTranslation (
    const struct sockaddr *SocketAddress,
    socklen_t SocketAddressLength,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine performs a reverse DNS query on the given IP address.

Arguments:

    SocketAddress - Supplies a pointer to the socket address to be translated.
        This routine does not handle IPv4-mapped IPv6 addresses.

    SocketAddressLength - Supplies the size of the socket address data.

    ListHead - Supplies a pointer to the initialized list head where the
        desired reverse DNS results will be returned.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    unsigned char *Address;
    NET_DOMAIN_TYPE Domain;
    ULONG Index;
    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    uint8_t *Ip6AddressArray;
    char Name[DNS_IP6_REVERSE_TRANSLATION_NAME_SIZE];
    INT Status;
    char *String;

    ASSERT(DNS_IP6_REVERSE_TRANSLATION_NAME_SIZE >=
           DNS_IP4_REVERSE_TRANSLATION_NAME_SIZE);

    if (SocketAddressLength < sizeof(struct sockaddr_in)) {
        Status = EAI_FAIL;
        goto PerformDnsReverseTranslationEnd;
    }

    switch (SocketAddress->sa_family) {
    case AF_INET6:
        Ip6Address = (struct sockaddr_in6 *)SocketAddress;
        Ip6AddressArray = Ip6Address->sin6_addr.s6_addr;
        Domain = NetDomainIp6;
        String = Name;
        for (Index = 16; Index > 0; Index -= 1) {
            String += snprintf(String,
                               5,
                               "%x.%x.",
                               Ip6AddressArray[Index - 1] & 0xF,
                               (Ip6AddressArray[Index - 1] >> 4) & 0xF);
        }

        memcpy(String,
               DNS_IP6_REVERSE_TRANSLATION_SUFFIX,
               DNS_IP6_REVERSE_TRANSLATION_SUFFIX_SIZE);

        break;

    case AF_INET:
        Domain = NetDomainIp4;
        Ip4Address = (struct sockaddr_in *)SocketAddress;
        Address = (unsigned char *)&(Ip4Address->sin_addr.s_addr);
        snprintf(Name,
                 DNS_IP4_REVERSE_TRANSLATION_NAME_SIZE,
                 DNS_IP4_REVERSE_TRANSLATION_FORMAT,
                 Address[3],
                 Address[2],
                 Address[1],
                 Address[0]);

        break;

    default:
        Status = EAI_ADDRFAMILY;
        goto PerformDnsReverseTranslationEnd;
    }

    Status = ClpPerformDnsTranslation(Name,
                                      DNS_RECORD_TYPE_PTR,
                                      Domain,
                                      ListHead,
                                      0);

    if (Status != 0) {
        goto PerformDnsReverseTranslationEnd;
    }

PerformDnsReverseTranslationEnd:
    return Status;
}

INT
ClpPerformDnsQuery (
    PSTR Name,
    CHAR RecordType,
    struct sockaddr *NameServer,
    socklen_t NameServerSize,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine performs a DNS query on the given name.

Arguments:

    Name - Supplies the name to execute the DNS request for.

    RecordType - Supplies the type of record to query for. See
        DNS_RECORD_TYPE_* definitions.

    NameServer - Supplies a pointer to the address to connect to for DNS name
        resolutions.

    NameServerSize - Supplies the size of the name server address in bytes.

    ListHead - Supplies a pointer to the initialized list head where DNS
        results will be returned.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDNS_RESULT DnsResult;
    const char *ErrorString;
    PDNS_HEADER Request;
    ULONG RequestSize;
    PDNS_HEADER Response;
    ULONG ResponseSize;
    INT Status;

    Request = NULL;
    Response = NULL;
    if (ClDebugDns != FALSE) {
        fprintf(stderr, "DNS: Lookup '%s'.\n", Name);
    }

    Status = ClpCreateDnsQuery(Name, RecordType, &Request, &RequestSize);
    if (Status != 0) {
        goto PerformDnsQueryEnd;
    }

    Status = ClpExecuteDnsQuery(NameServer,
                                NameServerSize,
                                Request,
                                RequestSize,
                                &Response,
                                &ResponseSize);

    if (Status != 0) {
        goto PerformDnsQueryEnd;
    }

    if (Response->Identifier != Request->Identifier) {
        if (ClDebugDns != FALSE) {
            fprintf(stderr,
                    "DNS: Error: Identifier mismatch %x, %x.\n",
                    Request->Identifier,
                    Response->Identifier);

            Status = EAI_FAIL;
            goto PerformDnsQueryEnd;
        }
    }

    //
    // Save the current end of the list, and parse the response packet into
    // more entries that get stuck on the end of the list.
    //

    CurrentEntry = ListHead->Previous;
    Status = ClpParseDnsResponse(Response, ResponseSize, ListHead);
    if (Status != 0) {
        goto PerformDnsQueryEnd;
    }

    //
    // For a good time, print the DNS result list.
    //

    if (ClDebugDns != FALSE) {

        //
        // If the previous entry is still the end of the list, nothing was
        // added.
        //

        if (ListHead->Previous == CurrentEntry) {
            fprintf(stderr, "No responses\n");

        } else {

            //
            // If there was no previous entry, start at the first entry.
            //

            if (CurrentEntry == ListHead) {
                CurrentEntry = CurrentEntry->Next;
            }

            while (CurrentEntry != ListHead) {
                DnsResult = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
                ClpDebugPrintDnsResult(DnsResult);
                CurrentEntry = CurrentEntry->Next;
            }
        }
    }

PerformDnsQueryEnd:
    if (Status != 0) {
        if (ClDebugDns != FALSE) {
            if (Status == EAI_SYSTEM) {
                ErrorString = strerror(errno);

            } else {
                ErrorString = gai_strerror(Status);
            }

            fprintf(stderr, "DNS: Failed to execute query: %s.\n", ErrorString);
        }
    }

    if (Request != NULL) {
        free(Request);
    }

    if (Response != NULL) {
        free(Response);
    }

    return Status;
}

INT
ClpCreateDnsQuery (
    PSTR Name,
    UCHAR ResponseType,
    PDNS_HEADER *NewRequest,
    PULONG RequestSize
    )

/*++

Routine Description:

    This routine creates and initializes a DNS query packet.

Arguments:

    Name - Supplies the name of the host to query.

    ResponseType - Supplies the type of response requested.

    NewRequest - Supplies a pointer where a pointer to the new request will be
        returned. The caller is responsible for freeing this memory.

    RequestSize - Supplies a pointer where the size of the request in bytes
        will be returned.

Return Value:

    0 on success.

    Returns an EAI_ error code on failure.

--*/

{

    ULONG AllocationSize;
    PSTR CurrentString;
    INT Error;
    ULONG FieldLength;
    PUCHAR FieldLengthPointer;
    PUCHAR NameBuffer;
    PUSHORT Pointer16;
    PDNS_HEADER Request;
    size_t StringLength;

    Error = 0;
    StringLength = strlen(Name);

    //
    // The allocation size is the header, plus the name length (where dots get
    // converted into length bytes), plus extras for the initial field length
    // and terminator, plus a type and class field.
    //

    AllocationSize = sizeof(DNS_HEADER) + (2 * sizeof(USHORT)) + StringLength +
                     2;

    Request = malloc(AllocationSize);
    if (Request == NULL) {
        return EAI_MEMORY;
    }

    memset(Request, 0, AllocationSize);
    Request->Identifier = time(NULL) ^ rand();
    Request->Flags = (DNS_HEADER_OPCODE_QUERY << DNS_HEADER_OPCODE_SHIFT) |
                     DNS_HEADER_FLAG_RECURSION_DESIRED;

    Request->QuestionCount = htons(1);
    NameBuffer = (PUCHAR)(Request + 1);
    CurrentString = Name;

    //
    // Convert the name request into a DNS formatted name. DNS names are
    // broken into fields by the '.' character, and each field is preceded by
    // a length. The name is finally terminated by a zero-length field.
    //

    FieldLengthPointer = NameBuffer;
    NameBuffer += 1;
    FieldLength = 0;
    while (TRUE) {
        if ((*CurrentString == '.') || (*CurrentString == '\0')) {
            if ((FieldLength == 0) || (FieldLength > MAX_UCHAR)) {
                Error = EAI_FAIL;
                goto CreateDnsQueryEnd;
            }

            *FieldLengthPointer = FieldLength;
            FieldLengthPointer = NameBuffer;
            FieldLength = 0;
            if (*CurrentString == '\0') {
                break;
            }

        } else {
            *NameBuffer = *CurrentString;
            FieldLength += 1;
        }

        CurrentString += 1;
        NameBuffer += 1;
    }

    //
    // Terminate the name.
    //

    *NameBuffer = 0;
    NameBuffer += 1;

    //
    // Now add the type and class.
    //

    Pointer16 = (PUSHORT)NameBuffer;
    *Pointer16 = htons(ResponseType);
    Pointer16 += 1;
    *Pointer16 = htons(DNS_CLASS_INTERNET);
    Pointer16 += 1;

    assert((UINTN)Pointer16 - (UINTN)Request == AllocationSize);

CreateDnsQueryEnd:
    if (Error != 0) {
        if (Request != NULL) {
            free(Request);
            Request = NULL;
        }

        AllocationSize = 0;
    }

    *NewRequest = Request;
    *RequestSize = AllocationSize;
    return Error;
}

INT
ClpExecuteDnsQuery (
    struct sockaddr *NameServer,
    socklen_t NameServerSize,
    PDNS_HEADER Request,
    ULONG RequestSize,
    PDNS_HEADER *Response,
    PULONG ResponseSize
    )

/*++

Routine Description:

    This routine sends a DNS query and returns the response.

Arguments:

    NameServer - Supplies a pointer to the address to connect to for DNS name
        resolutions.

    NameServerSize - Supplies the size of the name server parameter.

    Request - Supplies a pointer to the DNS request.

    RequestSize - Supplies the size of the query in bytes.

    Response - Supplies a pointer where a pointer to the response will be
        returned on success. It is the caller's responsibility to free this
        memory when finished.

    ResponseSize - Supplies a pointer where the response size in bytes will be
        returned on success.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    ssize_t ByteCount;
    PDNS_HEADER DnsResponse;
    INT Error;
    struct sockaddr_in Ip4Address;
    struct sockaddr_in6 Ip6Address;
    struct pollfd Poll;
    INT Result;
    INT Socket;

    Socket = -1;
    DnsResponse = malloc(DNS_RESPONSE_ALLOCATION_SIZE);
    if (DnsResponse == NULL) {
        Error = EAI_MEMORY;
        goto ExecuteDnsQueryEnd;
    }

    Socket = socket(NameServer->sa_family, SOCK_DGRAM, IPPROTO_UDP);
    if (Socket == -1) {
        Error = EAI_SYSTEM;
        goto ExecuteDnsQueryEnd;
    }

    //
    // Create a local address with the same family as the name server
    // destination and bind to it.
    //

    switch (NameServer->sa_family) {
    case AF_INET:
        memset(&Ip4Address, 0, sizeof(struct sockaddr_in));
        Ip4Address.sin_family = AF_INET;
        Result = bind(Socket,
                      (struct sockaddr *)&Ip4Address,
                      sizeof(Ip4Address));

        if (Result != 0) {
            Error = EAI_SYSTEM;
            goto ExecuteDnsQueryEnd;
        }

        break;

    case AF_INET6:
        memset(&Ip6Address, 0, sizeof(struct sockaddr_in6));
        Ip6Address.sin6_family = AF_INET6;
        Result = bind(Socket,
                      (struct sockaddr *)&Ip6Address,
                      sizeof(Ip6Address));

        if (Result != 0) {
            Error = EAI_SYSTEM;
            goto ExecuteDnsQueryEnd;
        }

        break;

    default:

        assert(FALSE);

        Error = EAI_FAMILY;
        goto ExecuteDnsQueryEnd;
    }

    ByteCount = sendto(Socket,
                       Request,
                       RequestSize,
                       0,
                       NameServer,
                       NameServerSize);

    if (ByteCount != RequestSize) {
        Error = EAI_SYSTEM;
        goto ExecuteDnsQueryEnd;
    }

    //
    // Wait for a response.
    //

    Poll.fd = Socket;
    Poll.events = POLLIN;
    Poll.revents = 0;
    do {
        Result = poll(&Poll, 1, DNS_RESPONSE_TIMEOUT);

    } while ((Result < 0) && (errno == EINTR));

    if (Result <= 0) {
        Error = EAI_AGAIN;
        goto ExecuteDnsQueryEnd;
    }

    do {
        ByteCount = recv(Socket, DnsResponse, DNS_RESPONSE_ALLOCATION_SIZE, 0);

    } while ((ByteCount == 0) && (errno == EINTR));

    if (ByteCount == 0) {
        Error = EAI_SYSTEM;
        goto ExecuteDnsQueryEnd;
    }

    *ResponseSize = ByteCount;
    Error = 0;

ExecuteDnsQueryEnd:
    if (Error != 0) {
        if (DnsResponse != NULL) {
            free(DnsResponse);
            DnsResponse = NULL;
        }
    }

    if (Socket != -1) {
        close(Socket);
    }

    *Response = DnsResponse;
    return Error;
}

INT
ClpParseDnsResponse (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine parses a DNS response to a list of dns result structures,
    decompressing names along the way.

Arguments:

    Response - Supplies a pointer to the DNS response packet.

    ResponseSize - Supplies the size of the DNS response packet in bytes.

    ListHead - Supplies a pointer to an initialized list head where result
        structures will be returned. This list may have items on it even if
        the return code is failing. It is the caller's responsibility to free
        these items.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    ULONG AdditionalResourceCount;
    ULONG AdditionalResourceIndex;
    ULONG AnswerCount;
    ULONG AnswerIndex;
    PUCHAR Buffer;
    ULONG NameServerCount;
    ULONG NameServerIndex;
    ULONG QuestionCount;
    ULONG QuestionIndex;
    UCHAR ResponseCode;
    INT Status;

    //
    // Validate that the flags field came back okay.
    //

    if ((Response->Flags & DNS_HEADER_FLAG_RESPONSE) == 0) {
        Status = EAI_BADFLAGS;
        goto ParseDnsResponseEnd;
    }

    ResponseCode = (Response->Flags >> DNS_HEADER_RESPONSE_SHIFT) &
                   DNS_HEADER_RESPONSE_MASK;

    if (ResponseCode != DNS_HEADER_RESPONSE_SUCCESS) {
        if (ResponseCode == DNS_HEADER_RESPONSE_NAME_ERROR) {
            Status = EAI_NONAME;

        } else if (ResponseCode == DNS_HEADER_RESPONSE_REFUSED) {
            Status = EAI_AGAIN;

        } else {
            Status = EAI_FAIL;
        }

        goto ParseDnsResponseEnd;
    }

    QuestionCount = ntohs(Response->QuestionCount);
    AnswerCount = ntohs(Response->AnswerCount);
    NameServerCount = ntohs(Response->NameServerCount);
    AdditionalResourceCount = ntohs(Response->AdditionalResourceCount);
    if ((AnswerCount == 0) &&
        (NameServerCount == 0) &&
        (AdditionalResourceCount == 0)) {

        Status = 0;
        goto ParseDnsResponseEnd;
    }

    //
    // Zoom through the questions.
    //

    Buffer = (PUCHAR)(Response + 1);
    for (QuestionIndex = 0; QuestionIndex < QuestionCount; QuestionIndex += 1) {
        Status = ClpDecompressDnsName(Response, ResponseSize, &Buffer, NULL);
        if (Status != 0) {
            goto ParseDnsResponseEnd;
        }

        //
        // Also scan past the rest of the structure, which contains the type
        // and class, both 16-bits.
        //

        Buffer += 4;
        if ((UINTN)Buffer - (UINTN)Response > ResponseSize) {
            Status = EAI_OVERFLOW;
            goto ParseDnsResponseEnd;
        }
    }

    //
    // Parse the answers.
    //

    for (AnswerIndex = 0; AnswerIndex < AnswerCount; AnswerIndex += 1) {
        Status = ClpParseDnsResponseElement(Response,
                                            ResponseSize,
                                            &Buffer,
                                            ListHead);

        if (Status != 0) {
            goto ParseDnsResponseEnd;
        }
    }

    //
    // Parse the name servers.
    //

    for (NameServerIndex = 0;
         NameServerIndex < NameServerCount;
         NameServerIndex += 1) {

        Status = ClpParseDnsResponseElement(Response,
                                            ResponseSize,
                                            &Buffer,
                                            ListHead);

        if (Status != 0) {
            goto ParseDnsResponseEnd;
        }
    }

    //
    // Parse the additional data.
    //

    for (AdditionalResourceIndex = 0;
         AdditionalResourceIndex < AdditionalResourceCount;
         AdditionalResourceIndex += 1) {

        Status = ClpParseDnsResponseElement(Response,
                                            ResponseSize,
                                            &Buffer,
                                            ListHead);

        if (Status != 0) {
            goto ParseDnsResponseEnd;
        }
    }

    Status = 0;

ParseDnsResponseEnd:
    return Status;
}

INT
ClpParseDnsResponseElement (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PUCHAR *ResponseEntry,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine parses a single DNS response entry.

Arguments:

    Response - Supplies a pointer to the DNS response packet.

    ResponseSize - Supplies the size of the DNS response packet in bytes.

    ResponseEntry - Supplies a pointer that on input points to the response
        entry. On output, this buffer will be advanced past the response entry.

    ListHead - Supplies a pointer to an initialized list head where result
        structures will be returned. This list may have items on it even if
        the return code is failing. It is the caller's responsibility to free
        these items.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    PUCHAR Buffer;
    USHORT DataLength;
    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    PDNS_RESULT Result;
    INT Status;
    ULONG TimeToLive;

    Result = malloc(sizeof(DNS_RESULT));
    if (Result == NULL) {
        Status = EAI_MEMORY;
        goto ParseDnsResponseElementEnd;
    }

    memset(Result, 0, sizeof(DNS_RESULT));
    Buffer = *ResponseEntry;
    if ((UINTN)Buffer - (UINTN)Response >= ResponseSize) {
        Status = EAI_OVERFLOW;
        goto ParseDnsResponseElementEnd;
    }

    //
    // Responses start with a name.
    //

    Status = ClpDecompressDnsName(Response,
                                  ResponseSize,
                                  &Buffer,
                                  &(Result->Name));

    if (Status != 0) {
        goto ParseDnsResponseElementEnd;
    }

    //
    // Then comes the type (16 bits), class (16-bits), time-to-live (32-bits),
    // and data length (16 bits).
    //

    if ((UINTN)Buffer + 10 - (UINTN)Response >= ResponseSize) {
        Status = EAI_OVERFLOW;
        goto ParseDnsResponseElementEnd;
    }

    Result->Type = ((USHORT)*Buffer << BITS_PER_BYTE) | *(Buffer + 1);
    Buffer += 2;
    Result->Class = ((USHORT)*Buffer << BITS_PER_BYTE) | *(Buffer + 1);
    Buffer += 2;
    TimeToLive = ((ULONG)*Buffer << (3 * BITS_PER_BYTE)) |
                 ((ULONG)*(Buffer + 1) << (2 * BITS_PER_BYTE)) |
                 ((ULONG)*(Buffer + 2) << BITS_PER_BYTE) |
                 (ULONG)*(Buffer + 3);

    Result->ExpirationTime = time(NULL) + TimeToLive;
    Buffer += 4;
    DataLength = ((USHORT)*Buffer << BITS_PER_BYTE) | *(Buffer + 1);
    Buffer += 2;
    if ((UINTN)Buffer + DataLength <= ResponseSize) {
        Status = EAI_OVERFLOW;
        goto ParseDnsResponseElementEnd;
    }

    //
    // Parse the data into the appropriate value.
    //

    if (Result->Class != DNS_CLASS_INTERNET) {
        Buffer += DataLength;
        Status = EAI_FAIL;
        goto ParseDnsResponseElementEnd;
    }

    switch (Result->Type) {
    case DNS_RECORD_TYPE_A:
        if (DataLength != 4) {
            Status = EAI_FAIL;
            goto ParseDnsResponseElementEnd;
        }

        Ip4Address = (struct sockaddr_in *)&(Result->Address);
        Ip4Address->sin_family = AF_INET;

        assert(sizeof(in_addr_t) == 4);

        memcpy(&(Ip4Address->sin_addr.s_addr), Buffer, 4);
        Buffer += DataLength;
        break;

    case DNS_RECORD_TYPE_AAAA:
        if (DataLength != 16) {
            Status = EAI_FAIL;
            goto ParseDnsResponseElementEnd;
        }

        Ip6Address = (struct sockaddr_in6 *)&(Result->Address);
        Ip6Address->sin6_family = AF_INET6;
        memcpy(&(Ip6Address->sin6_addr.s6_addr), Buffer, 16);
        Buffer += DataLength;
        break;

    case DNS_RECORD_TYPE_NS:
    case DNS_RECORD_TYPE_CNAME:
    case DNS_RECORD_TYPE_SOA:
    case DNS_RECORD_TYPE_PTR:
        Status = ClpDecompressDnsName(Response,
                                      ResponseSize,
                                      &Buffer,
                                      &(Result->Value));

        if (Status != 0) {
            goto ParseDnsResponseElementEnd;
        }

        break;

    default:
        Buffer += DataLength;
        break;
    }

    INSERT_BEFORE(&(Result->ListEntry), ListHead);
    Status = 0;

ParseDnsResponseElementEnd:
    if (Status != 0) {
        if (Result != NULL) {
            ClpDestroyDnsResult(Result);
        }
    }

    *ResponseEntry = Buffer;
    return Status;
}

INT
ClpDecompressDnsName (
    PDNS_HEADER Response,
    ULONG ResponseSize,
    PUCHAR *DnsName,
    PSTR *OutputName
    )

/*++

Routine Description:

    This routine parses a DNS compressed name back into a regular string.

Arguments:

    Response - Supplies a pointer to the DNS response packet.

    ResponseSize - Supplies the size of the DNS response packet in bytes.

    DnsName - Supplies a pointer that upon input contains a pointer within the
        response where the name begins. On output, this value will be advanced
        just beyond the name.

    OutputName - Supplies an optional pointer where a pointer to the newly
        allocated decompressed name will be returned. It is the caller's
        responsibility to free this buffer. If this is not supplied, then the
        name will be advanced past, but not parsed out into a new buffer.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    PUCHAR BufferEnd;
    PSTR OutputNameBuffer;
    ULONG OutputNameSize;
    INT Status;

    OutputNameBuffer = NULL;

    //
    // Scan to figure out where the name ends.
    //

    BufferEnd = *DnsName;
    while (TRUE) {
        if ((UINTN)BufferEnd - (UINTN)Response >= ResponseSize) {
            Status = EAI_OVERFLOW;
            goto DecompressDnsNameEnd;
        }

        if (*BufferEnd == 0) {
            BufferEnd += 1;
            break;
        }

        //
        // If a link is found, then this is probably the end of the string.
        // Links are not allowed to go forward.
        //

        if ((*BufferEnd & DNS_COMPRESSION_MASK) == DNS_COMPRESSION_VALUE) {
            BufferEnd += 2;
            break;
        }

        BufferEnd += *BufferEnd + 1;
    }

    //
    // If no output is desired, then all the work is done.
    //

    if (OutputName == NULL) {
        Status = 0;
        goto DecompressDnsNameEnd;
    }

    //
    // Scan through the string once to figure out how big it is.
    //

    OutputNameSize = 0;
    Status = ClpScanDnsName((PUCHAR)Response,
                            ResponseSize,
                            *DnsName,
                            NULL,
                            &OutputNameSize);

    if (Status != 0) {
        goto DecompressDnsNameEnd;
    }

    assert(OutputNameSize != 0);

    //
    // Now allocate the string buffer and scan again to create the string.
    //

    OutputNameBuffer = malloc(OutputNameSize);
    if (OutputNameBuffer == NULL) {
        Status = EAI_MEMORY;
        goto DecompressDnsNameEnd;
    }

    Status = ClpScanDnsName((PUCHAR)Response,
                            ResponseSize,
                            *DnsName,
                            OutputNameBuffer,
                            &OutputNameSize);

    if (Status != 0) {
        goto DecompressDnsNameEnd;
    }

DecompressDnsNameEnd:
    if (Status != 0) {
        if (OutputNameBuffer != NULL) {
            free(OutputNameBuffer);
        }
    }

    *DnsName = BufferEnd;
    if (OutputName != NULL) {
        *OutputName = OutputNameBuffer;
    }

    return Status;
}

INT
ClpScanDnsName (
    PUCHAR Response,
    ULONG ResponseSize,
    PUCHAR Name,
    PSTR OutputName,
    PULONG OutputNameSize
    )

/*++

Routine Description:

    This routine scans through a DNS name.

Arguments:

    Response - Supplies a pointer to the DNS response packet.

    ResponseSize - Supplies the size of the DNS response packet in bytes.

    Name - Supplies a pointer to the compressed DNS name.

    OutputName - Supplies an optional pointer where the decompressed name will
        be returned.

    OutputNameSize - Supplies a pointer that on input supplies the size of
        the output name buffer. On output, contains the required size of the
        output name buffer.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    UCHAR LegCount;
    ULONG NeededSize;
    USHORT Offset;
    ULONG OutputSize;
    INT Status;

    NeededSize = 0;
    OutputSize = *OutputNameSize;

    assert((UINTN)Name - (UINTN)Response < ResponseSize);

    LegCount = 0;

    //
    // Loop scanning legs (that's an unofficial term).
    //

    while (TRUE) {

        //
        // Detect infinite loops.
        //

        if (NeededSize > DNS_MAX_NAME) {
            Status = EAI_OVERFLOW;
            goto ScanDnsNameEnd;
        }

        while (LegCount != 0) {
            if ((UINTN)Name - (UINTN)Response >= ResponseSize) {
                Status = EAI_OVERFLOW;
                goto ScanDnsNameEnd;
            }

            if (OutputName != NULL) {

                assert(OutputSize != 0);

                *OutputName = *Name;
                OutputName += 1;
                OutputSize -= 1;
            }

            NeededSize += 1;
            Name += 1;
            LegCount -= 1;
        }

        if ((UINTN)Name - (UINTN)Response >= ResponseSize) {
            Status = EAI_OVERFLOW;
            goto ScanDnsNameEnd;
        }

        //
        // A zero-length leg signifies the end.
        //

        if (*Name == 0) {
            break;
        }

        //
        // If the top two bits are not set, this is just another regular leg.
        //

        if ((*Name & DNS_COMPRESSION_MASK) != DNS_COMPRESSION_VALUE) {

            //
            // Add a dot to the output string (except for the very first time).
            //

            if (NeededSize != 0) {
                if (OutputName != NULL) {

                    assert(OutputSize != 0);

                    *OutputName = '.';
                    OutputName += 1;
                    OutputSize -= 1;
                }

                NeededSize += 1;
            }

            LegCount = *Name;
            Name += 1;
            continue;
        }

        //
        // The top two bits are set, so this is a jump elsewhere in the packet.
        //

        if ((UINTN)Name + 1 - (UINTN)Response >= ResponseSize) {
            Status = EAI_OVERFLOW;
            goto ScanDnsNameEnd;
        }

        Offset = ((USHORT)(*Name & (~DNS_COMPRESSION_MASK)) << BITS_PER_BYTE) |
                 *(Name + 1);

        //
        // Watch out for infinite loops (a link to this link).
        //

        if (Offset == (UINTN)Name - (UINTN)Response) {
            Status = EAI_OVERFLOW;
            goto ScanDnsNameEnd;
        }

        Name = Response + Offset;
        if ((UINTN)Name - (UINTN)Response >= ResponseSize) {
            Status = EAI_OVERFLOW;
            goto ScanDnsNameEnd;
        }

        //
        // Loop. Notice that the leg count is still zero, meaning another
        // iteration is required to get the leg count (or immediately follow
        // another link, etc).
        //

    }

    //
    // Null terminate the string.
    //

    if (OutputName != NULL) {

        assert(OutputSize != 0);

        *OutputName = '\0';
        OutputName += 1;
        OutputSize -= 1;
    }

    NeededSize += 1;
    Status = 0;

ScanDnsNameEnd:
    *OutputNameSize = NeededSize;
    return Status;
}

VOID
ClpDestroyDnsResultList (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine destroys a list of DNS result structures.

Arguments:

    ListHead - Supplies a pointer to the head of the list.

Return Value:

    None.

--*/

{

    PDNS_RESULT Result;

    while (LIST_EMPTY(ListHead) == FALSE) {
        Result = LIST_VALUE(ListHead->Next, DNS_RESULT, ListEntry);
        LIST_REMOVE(&(Result->ListEntry));
        ClpDestroyDnsResult(Result);
    }

    return;
}

VOID
ClpDestroyDnsResult (
    PDNS_RESULT Result
    )

/*++

Routine Description:

    This routine destroys a DNS result structure. It is assumed that this
    has already been pulled off of any list it may have been on.

Arguments:

    Result - Supplies a pointer to the result to destroy.

Return Value:

    None.

--*/

{

    if (Result == NULL) {
        return;
    }

    if (Result->Name != NULL) {
        free(Result->Name);
    }

    if (Result->Value != NULL) {
        free(Result->Value);
    }

    free(Result);
    return;
}

VOID
ClpDebugPrintDnsResult (
    PDNS_RESULT Result
    )

/*++

Routine Description:

    This routine prints a DNS result structure to standard error.

Arguments:

    Result - Supplies a pointer to the result to print.

Return Value:

    None.

--*/

{

    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    CHAR PrintBuffer[60];
    struct tm TimeStructure;
    PSTR TypeString;

    if (Result->Class != DNS_CLASS_INTERNET) {
        fprintf(stderr, "Class %x ", Result->Class);
    }

    switch (Result->Type) {
    case DNS_RECORD_TYPE_A:
        TypeString = "A";
        break;

    case DNS_RECORD_TYPE_AAAA:
        TypeString = "AAAA";
        break;

    case DNS_RECORD_TYPE_NS:
        TypeString = "NS";
        break;

    case DNS_RECORD_TYPE_CNAME:
        TypeString = "CNAME";
        break;

    case DNS_RECORD_TYPE_SOA:
        TypeString = "SOA";
        break;

    case DNS_RECORD_TYPE_MX:
        TypeString = "MX";
        break;

    case DNS_RECORD_TYPE_TXT:
        TypeString = "TXT";
        break;

    default:
        TypeString = NULL;
        break;
    }

    if (TypeString != NULL) {
        fprintf(stderr, "%s %s ", TypeString, Result->Name);

    } else {
        fprintf(stderr, "Unknown (%u) %s ", Result->Type, Result->Name);
    }

    switch (Result->Type) {
    case DNS_RECORD_TYPE_A:
        Ip4Address = (struct sockaddr_in *)&(Result->Address);

        assert(Ip4Address->sin_family == AF_INET);

        inet_ntop(Ip4Address->sin_family,
                  &(Ip4Address->sin_addr.s_addr),
                  PrintBuffer,
                  sizeof(PrintBuffer));

        fprintf(stderr, "%s", PrintBuffer);
        break;

    case DNS_RECORD_TYPE_AAAA:
        Ip6Address = (struct sockaddr_in6 *)&(Result->Address);

        assert(Ip6Address->sin6_family == AF_INET6);

        inet_ntop(Ip6Address->sin6_family,
                  &(Ip6Address->sin6_addr.s6_addr),
                  PrintBuffer,
                  sizeof(PrintBuffer));

        fprintf(stderr, "%s", PrintBuffer);
        break;

    case DNS_RECORD_TYPE_NS:
    case DNS_RECORD_TYPE_CNAME:
    case DNS_RECORD_TYPE_SOA:
        fprintf(stderr, "%s", Result->Value);
        break;

    default:
        break;
    }

    localtime_r(&(Result->ExpirationTime), &TimeStructure);
    asctime_r(&TimeStructure, PrintBuffer);
    fprintf(stderr, " Expires %s", PrintBuffer);
    return;
}

INT
ClpGetNameServerAddress (
    PDNS_RESULT NameServer,
    UCHAR RecordType,
    PLIST_ENTRY TranslationList,
    struct sockaddr *NameServerAddress,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine attempts to find the address of a name server.

Arguments:

    NameServer - Supplies the DNS name server or start of authority result.

    RecordType - Supplies the type of record to search for.

    TranslationList - Supplies the list of possible translations to the name
        server address. More results may be added to this list.

    NameServerAddress - Supplies a pointer where the address of the name server
        will be returned, including having its port set to the DNS port number.

    RecursionDepth - Supplies the current recursion depth.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    NET_DOMAIN_TYPE Domain;
    INT Status;

    //
    // If the record type is not nameserver, then assume the address
    // is there directly. It's probably an original name server address
    // from the top of the function.
    //

    if ((NameServer->Type != DNS_RECORD_TYPE_NS) &&
        (NameServer->Type != DNS_RECORD_TYPE_SOA)) {

        assert((NameServer->Type == DNS_RECORD_TYPE_A) ||
               (NameServer->Type == DNS_RECORD_TYPE_AAAA));

        memcpy(&NameServerAddress,
               &(NameServer->Address),
               sizeof(struct sockaddr));

        return 0;
    }

    //
    // Maybe the name server address was already returned in the list of
    // translations.
    //

    Status = ClpFindNameServerAddress(NameServer,
                                      RecordType,
                                      TranslationList,
                                      NameServerAddress);

    if (Status == 0) {
        return Status;
    }

    if (RecordType == DNS_RECORD_TYPE_AAAA) {
        Domain = NetDomainIp6;

    } else {
        Domain = NetDomainIp4;
    }

    //
    // Go start a whole new query to figure out the name server address.
    //

    Status = ClpPerformDnsTranslation(NameServer->Value,
                                      RecordType,
                                      Domain,
                                      TranslationList,
                                      RecursionDepth + 1);

    if (Status != 0) {
        if (ClDebugDns != FALSE) {
            fprintf(stderr,
                    "Error: Failed to get address of DNS server %s\n",
                    NameServer->Value);
        }

        return Status;
    }

    Status = ClpFindNameServerAddress(NameServer,
                                      RecordType,
                                      TranslationList,
                                      NameServerAddress);

    if (Status == 0) {
        return Status;
    }

    if (ClDebugDns != FALSE) {
        fprintf(stderr,
                "Error: Failed to get address of DNS server %s\n",
                NameServer->Value);
    }

    return Status;
}

INT
ClpFindNameServerAddress (
    PDNS_RESULT NameServer,
    UCHAR RecordType,
    PLIST_ENTRY TranslationList,
    struct sockaddr *NameServerAddress
    )

/*++

Routine Description:

    This routine attempts to find the address of a name server in the list of
    translations.

Arguments:

    NameServer - Supplies the DNS name server or start of authority result.

    RecordType - Supplies the type of record to search for.

    TranslationList - Supplies the list of possible translations to the name
        server address. More results may be added to this list.

    NameServerAddress - Supplies a pointer where the address of the name server
        will be returned, including having its port set to the DNS port number.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG MatchCount;
    struct sockaddr_in *NameServerAddressIp4;
    struct sockaddr_in6 *NameServerAddressIp6;
    LIST_ENTRY NameServerAddressList;
    INT Status;
    time_t Time;
    PDNS_RESULT Translation;

    MatchCount = 0;
    INITIALIZE_LIST_HEAD(&NameServerAddressList);
    Status = 0;
    Time = time(NULL);

    //
    // Look for IPv6 translations if the caller wants IPv6 translations.
    //

    if (RecordType == DNS_RECORD_TYPE_AAAA) {
        MatchCount = ClpSearchDnsResultList(NameServer->Value,
                                            DNS_RECORD_TYPE_AAAA,
                                            Time,
                                            TranslationList,
                                            &NameServerAddressList);

        if (MatchCount != 0) {
            Translation = LIST_VALUE(NameServerAddressList.Previous,
                                     DNS_RESULT,
                                     ListEntry);

            memcpy(NameServerAddress,
                   &(Translation->Address),
                   sizeof(struct sockaddr));

            NameServerAddressIp6 = (struct sockaddr_in6 *)NameServerAddress;
            NameServerAddressIp6->sin6_port = htons(DNS_PORT_NUMBER);
            goto FindNameServerAddressEnd;
        }
    }

    //
    // If no matches were found or weren't tried, try for IPv4
    // translations.
    //

    MatchCount = ClpSearchDnsResultList(NameServer->Value,
                                        DNS_RECORD_TYPE_A,
                                        Time,
                                        TranslationList,
                                        &NameServerAddressList);

    if (MatchCount != 0) {
        Translation = LIST_VALUE(NameServerAddressList.Previous,
                                 DNS_RESULT,
                                 ListEntry);

        memcpy(NameServerAddress,
               &(Translation->Address),
               sizeof(struct sockaddr));

        NameServerAddressIp4 = (struct sockaddr_in *)NameServerAddress;
        NameServerAddressIp4->sin_port = htons(DNS_PORT_NUMBER);
        goto FindNameServerAddressEnd;
    }

    Status = EAI_AGAIN;

FindNameServerAddressEnd:
    ClpDestroyDnsResultList(&NameServerAddressList);
    return Status;
}

INT
ClpSearchDnsResultList (
    PSTR Name,
    UCHAR RecordType,
    time_t CurrentTime,
    PLIST_ENTRY ListHead,
    PLIST_ENTRY DestinationListHead
    )

/*++

Routine Description:

    This routine searches through a DNS result list for a record with the
    given name and record type. Results that qualify will be moved onto the
    destination list.

Arguments:

    Name - Supplies the name of the record to match against. This is optional.

    RecordType - Supplies the type of record to search for.

    CurrentTime - Supplies the start time of the query. Expiration times
        greater than this will not qualify and be removed and destroyed.

    ListHead - Supplies a pointer to the head of the list to search through.

    DestinationListHead - Supplies a pointer to the list to move qualifying
        results over to.

Return Value:

    Returns the number of matching results.

--*/

{

    PLIST_ENTRY CurrentEntry;
    INT Matches;
    PDNS_RESULT Result;

    Matches = 0;
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Result = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Result->Class != DNS_CLASS_INTERNET) {
            continue;
        }

        //
        // Kill expired results.
        //

        if ((CurrentTime > Result->ExpirationTime) &&
            (Result->ExpirationTime != 0)) {

            LIST_REMOVE(&(Result->ListEntry));
            ClpDestroyDnsResult(Result);
            continue;
        }

        if ((Result->Type == RecordType) &&
            ((Name == NULL) || (strcmp(Name, Result->Name) == 0))) {

            Matches += 1;
            LIST_REMOVE(&(Result->ListEntry));

            //
            // Name servers go at the front of the list.
            //

            if ((Result->Type == DNS_RECORD_TYPE_NS) ||
                (Result->Type == DNS_RECORD_TYPE_SOA)) {

                INSERT_AFTER(&(Result->ListEntry), DestinationListHead);

            } else {
                INSERT_BEFORE(&(Result->ListEntry), DestinationListHead);
            }
        }
    }

    return Matches;
}

INT
ClpConvertDnsResultListToAddressInformation (
    PLIST_ENTRY ListHead,
    const struct addrinfo *Hints,
    ULONG Port,
    BOOL MapV4Addresses,
    struct addrinfo **AddressInformation
    )

/*++

Routine Description:

    This routine converts a DNS result list into an address information list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of DNS_RESULT
        structures.

    Hints - Supplies an optional pointer to the hints structure, which governs
        the behavior of the conversion.

    Port - Supplies the service number to fill in.

    MapV4Addresses - Supplies a boolean indicating if IPv4 addresses should be
        mapped to IPv6.

    AddressInformation - Supplies a pointer where a pointer to the address
        information list may be returned. If returned, the caller is
        responsible for freeing this list using the freeaddrinfo function. If
        the hints structure disqualifies this result, then NULL may be returned
        on output as this result is not a candidate for conversion.

Return Value:

    0 on success, which may or may not result in a valid address information
    list being returned.

    Returns an EAI_* error code on failure.

--*/

{

    struct addrinfo *Base;
    PLIST_ENTRY CurrentEntry;
    struct addrinfo *End;
    BOOL Ip4Ok;
    BOOL Ip6Ok;
    struct addrinfo *NewInformation;
    INT Protocol;
    PDNS_RESULT Result;
    INT SocketType;
    INT Status;
    BOOL WantCanonicalName;

    Base = NULL;
    End = NULL;

    //
    // Convert the optional hints into parameters.
    //

    WantCanonicalName = FALSE;
    if ((Hints != NULL) && ((Hints->ai_flags & AI_CANONNAME) != 0)) {
        WantCanonicalName = TRUE;
    }

    Ip4Ok = TRUE;
    Ip6Ok = TRUE;
    if ((Hints != NULL) && (Hints->ai_family != AF_UNSPEC)) {
        if (Hints->ai_family == AF_INET) {
            Ip6Ok = FALSE;

        } else if (Hints->ai_family == AF_INET6) {
            Ip4Ok = FALSE;

        } else {
            Status = EAI_FAMILY;
            goto ConvertDnsResultListToAddressInformationEnd;
        }
    }

    Protocol = 0;
    if (Hints != NULL) {
        Protocol = Hints->ai_protocol;
    }

    SocketType = 0;
    if (Hints != NULL) {
        SocketType = Hints->ai_socktype;
    }

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Result = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip the entry if it is not an A or AAAA record.
        //

        if ((Result->Class != DNS_CLASS_INTERNET) ||
            ((Result->Type != 0) && (Result->Type != DNS_RECORD_TYPE_A) &&
             (Result->Type != DNS_RECORD_TYPE_AAAA))) {

            continue;
        }

        //
        // Skip the entry if a family was provided.
        //

        if ((Result->Type == DNS_RECORD_TYPE_A) && (Ip4Ok == FALSE)) {
            continue;
        }

        if ((Result->Type == DNS_RECORD_TYPE_AAAA) && (Ip6Ok == FALSE)) {
            continue;
        }

        //
        // Do a translation for the stream socket type.
        //

        if (((SocketType == 0) || (SocketType == SOCK_STREAM)) &&
            ((Protocol == 0) || (Protocol == IPPROTO_TCP))) {

            Status = ClpConvertDnsResultToAddressInformation(Result,
                                                             WantCanonicalName,
                                                             Port,
                                                             &NewInformation);

            if (Status != 0) {
                goto ConvertDnsResultListToAddressInformationEnd;
            }

            NewInformation->ai_socktype = SOCK_STREAM;
            NewInformation->ai_protocol = IPPROTO_TCP;
            if (Base == NULL) {
                Base = NewInformation;
                End = Base;

            } else {
                End->ai_next = NewInformation;
                End = NewInformation;
            }
        }

        //
        // Do another translation for the datagram socket type.
        //

        if (((SocketType == 0) || (SocketType == SOCK_DGRAM)) &&
            ((Protocol == 0) || (Protocol == IPPROTO_UDP))) {

            Status = ClpConvertDnsResultToAddressInformation(Result,
                                                             WantCanonicalName,
                                                             Port,
                                                             &NewInformation);

            if (Status != 0) {
                goto ConvertDnsResultListToAddressInformationEnd;
            }

            NewInformation->ai_socktype = SOCK_DGRAM;
            NewInformation->ai_protocol = IPPROTO_UDP;
            if (Base == NULL) {
                Base = NewInformation;
                End = Base;

            } else {
                End->ai_next = NewInformation;
                End = NewInformation;
            }
        }
    }

    Status = 0;
    if (Base == NULL) {
        Status = EAI_SERVICE;
    }

ConvertDnsResultListToAddressInformationEnd:
    if (Status != 0) {
        if (Base != NULL) {
            freeaddrinfo(Base);
        }
    }

    *AddressInformation = Base;
    return Status;
}

INT
ClpConvertDnsResultToAddressInformation (
    PDNS_RESULT Result,
    BOOL CopyCanonicalName,
    ULONG Port,
    struct addrinfo **AddressInformation
    )

/*++

Routine Description:

    This routine converts a DNS result into an address information structure.

Arguments:

    Result - Supplies a pointer to the DNS result structure.

    CopyCanonicalName - Supplies a boolean indicating if the caller wants the
        canonical name.

    Port - Supplies the port number to use.

    AddressInformation - Supplies a pointer where a pointer to the address
        information may be returned. If returned, the caller is responsible for
        freeing this memory using the freeaddrinfo function.

Return Value:

    0 on success, which may or may not result in a valid address information
    structure being returned.

    Returns an EAI_* error code on failure.

--*/

{

    ULONG AllocationSize;
    struct addrinfo *Information;
    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    INT Status;

    AllocationSize = sizeof(struct addrinfo) + sizeof(struct sockaddr);
    Information = malloc(AllocationSize);
    if (Information == NULL) {
        Status = EAI_MEMORY;
        goto ConvertDnsResultToAddressInformationEnd;
    }

    memset(Information, 0, AllocationSize);
    Information->ai_family = Result->Address.sa_family;
    if (Information->ai_family == AF_INET) {
        Information->ai_addrlen = sizeof(struct sockaddr_in);

    } else if (Information->ai_family == AF_INET6) {
        Information->ai_addrlen = sizeof(struct sockaddr_in6);

    } else {

        assert(FALSE);

        Status = EAI_FAMILY;
        goto ConvertDnsResultToAddressInformationEnd;
    }

    Information->ai_addr = (struct sockaddr *)(Information + 1);
    memcpy(Information->ai_addr, &(Result->Address), Information->ai_addrlen);
    if (Information->ai_family == AF_INET) {
        Ip4Address = (struct sockaddr_in *)(Information->ai_addr);
        Ip4Address->sin_port = htons(Port);

    } else if (Information->ai_family == AF_INET6) {
        Ip6Address = (struct sockaddr_in6 *)(Information->ai_addr);
        Ip6Address->sin6_port = htons(Port);

    } else {

        assert(FALSE);

        Status = EAI_FAMILY;
        goto ConvertDnsResultToAddressInformationEnd;
    }

    if (CopyCanonicalName != FALSE) {

        assert(Result->Name != NULL);

        Information->ai_canonname = strdup(Result->Name);
        if (Information->ai_canonname == NULL) {
            Status = EAI_MEMORY;
            goto ConvertDnsResultToAddressInformationEnd;
        }
    }

    Status = 0;

ConvertDnsResultToAddressInformationEnd:
    if (Status != 0) {
        if (Information != NULL) {
            free(Information);
            Information = NULL;
        }
    }

    *AddressInformation = Information;
    return Status;
}

INT
ClpGetAddressInformationPort (
    PSTR ServiceName,
    const struct addrinfo *Hints,
    PULONG Port
    )

/*++

Routine Description:

    This routine converts the optional service string into a port number.

Arguments:

    ServiceName - Supplies the string version of the service. This may be a
        number or a name.

    Hints - Supplies an optional pointer to the hints structure passed into the
        address information request.

    Port - Supplies a pointer where the port will be returned (in host order).

Return Value:

    0 on success, which may or may not result in a valid address information
    structure being returned.

    Returns an EAI_* error code on failure.

--*/

{

    PSTR AfterScan;
    struct servent *ServiceEntry;
    ULONG Value;

    *Port = 0;
    if (ServiceName == NULL) {
        return 0;
    }

    Value = strtoul(ServiceName, &AfterScan, 0);
    if ((AfterScan != ServiceName) && (Value <= 0xFFFF)) {
        *Port = Value;
        return 0;
    }

    if ((Hints != NULL) && ((Hints->ai_flags & AI_NUMERICSERV) != 0)) {
        return EAI_NONAME;
    }

    ServiceEntry = getservbyname(ServiceName, NULL);
    if (ServiceEntry == NULL) {
        return EAI_SERVICE;
    }

    *Port = ntohs(ServiceEntry->s_port);
    return 0;
}

VOID
ClpDebugPrintAddressInformation (
    struct addrinfo *AddressInformation
    )

/*++

Routine Description:

    This routine prints an address information list.

Arguments:

    AddressInformation - Supplies the address information to print. The next
        pointer will also be followed and printed iteratively.

Return Value:

    None.

--*/

{

    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    CHAR PrintBuffer[60];

        while (AddressInformation != NULL) {
        switch (AddressInformation->ai_family) {
        case AF_INET:
            Ip4Address = (struct sockaddr_in *)(AddressInformation->ai_addr);

            assert(Ip4Address->sin_family == AF_INET);

            inet_ntop(Ip4Address->sin_family,
                      &(Ip4Address->sin_addr.s_addr),
                      PrintBuffer,
                      sizeof(PrintBuffer));

            fprintf(stderr, "%s", PrintBuffer);
            break;

        case AF_INET6:
            Ip6Address = (struct sockaddr_in6 *)(AddressInformation->ai_addr);

            assert(Ip6Address->sin6_family == AF_INET6);

            inet_ntop(Ip6Address->sin6_family,
                      &(Ip6Address->sin6_addr.s6_addr),
                      PrintBuffer,
                      sizeof(PrintBuffer));

            fprintf(stderr, "%s", PrintBuffer);
            break;

        default:
            fprintf(stderr,
                    "Unknown family %d.\n",
                    AddressInformation->ai_family);

            break;
        }

        if (AddressInformation->ai_canonname != NULL) {
            fprintf(stderr, " %s", AddressInformation->ai_canonname);
        }

        fprintf(stderr,
                " Flags %x SockType %d Protocol %d Addrlen %d.\n",
                AddressInformation->ai_flags,
                AddressInformation->ai_socktype,
                AddressInformation->ai_protocol,
                AddressInformation->ai_addrlen);

        AddressInformation = AddressInformation->ai_next;
    }

    return;
}

VOID
ClpFillInLoopbackAddress (
    int AddressFamily,
    struct sockaddr *Address
    )

/*++

Routine Description:

    This routine fills in the loopback address.

Arguments:

    AddressFamily - Supplies the address family.

    Address - Supplies a pointer where the loopback address will be returned.

Return Value:

    None.

--*/

{

    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;

    switch (AddressFamily) {
    case AF_INET:
        Ip4Address = (struct sockaddr_in *)Address;
        Ip4Address->sin_family = AF_INET;
        Ip4Address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        break;

    case AF_INET6:
        Ip6Address = (struct sockaddr_in6 *)Address;
        Ip6Address->sin6_family = AF_INET6;
        memcpy(&(Ip6Address->sin6_addr),
               &in6addr_loopback,
               sizeof(struct in6_addr));

        break;

    default:

        assert(FALSE);

        break;
    }

    return;
}

INT
ClpGetDnsServers (
    NET_DOMAIN_TYPE Domain,
    struct sockaddr *PrimaryServer,
    PLIST_ENTRY AlternateList
    )

/*++

Routine Description:

    This routine gets the known DNS server addresses from the system.

Arguments:

    Domain - Supplies the network domain to get DNS servers for.

    PrimaryServer - Supplies a pointer where the primary DNS server address
        will be returned on success.

    AlternateList - Supplies a pointer to the head of the list where additional
        name servers will be returned in the form of DNS_RESULT structures
        set to the A or AAAA type.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL AddedOne;
    socklen_t AddressLength;
    PDNS_RESULT Alternate;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    DEVICE_INFORMATION_RESULT *Devices;
    NETWORK_DEVICE_INFORMATION Information;
    PVOID NewBuffer;
    INT Result;
    ULONG ServerIndex;
    UINTN Size;
    KSTATUS Status;

    //
    // Get the array of devices that return network device information.
    //

    DeviceCount = NETWORK_DEVICE_COUNT_ESTIMATE;
    Devices = malloc(sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetDnsServersEnd;
    }

    Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                       NULL,
                                       Devices,
                                       &DeviceCount);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            DeviceCount += NETWORK_DEVICE_COUNT_ESTIMATE;
            NewBuffer = realloc(
                              Devices,
                              sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);

            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto GetDnsServersEnd;
            }

            NewBuffer = Devices;
            Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                               NULL,
                                               Devices,
                                               &DeviceCount);

            if (!KSUCCESS(Status)) {
                goto GetDnsServersEnd;
            }

        } else {
            goto GetDnsServersEnd;
        }
    }

    if (DeviceCount == 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto GetDnsServersEnd;
    }

    //
    // Loop through all the network devices.
    //

    AddedOne = FALSE;
    memset(&Information, 0, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information.Domain = Domain;
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        Size = sizeof(NETWORK_DEVICE_INFORMATION);
        Status = OsGetSetDeviceInformation(Devices[DeviceIndex].DeviceId,
                                           &ClNetworkDeviceInformationUuid,
                                           &Information,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            continue;
        }

        if (((Information.Flags & NETWORK_DEVICE_FLAG_MEDIA_CONNECTED) == 0) ||
            ((Information.Flags & NETWORK_DEVICE_FLAG_CONFIGURED) == 0)) {

            continue;
        }

        //
        // Loop through every listed server.
        //

        for (ServerIndex = 0;
             ServerIndex < Information.DnsServerCount;
             ServerIndex += 1) {

            Information.DnsServers[ServerIndex].Port = DNS_PORT_NUMBER;
            if (PrimaryServer != NULL) {
                AddressLength = sizeof(struct sockaddr);
                Status = ClConvertFromNetworkAddress(
                                        &(Information.DnsServers[ServerIndex]),
                                        PrimaryServer,
                                        &AddressLength,
                                        NULL,
                                        0);

                if (KSUCCESS(Status)) {
                    AddedOne = TRUE;
                    PrimaryServer = NULL;
                }

                continue;
            }

            //
            // Add this as an alternate entry.
            //

            Alternate = malloc(sizeof(DNS_RESULT));
            if (Alternate == NULL) {
                continue;
            }

            memset(Alternate, 0, sizeof(DNS_RESULT));
            AddressLength = sizeof(Alternate->Address);
            Status = ClConvertFromNetworkAddress(
                                        &(Information.DnsServers[ServerIndex]),
                                        &(Alternate->Address),
                                        &AddressLength,
                                        NULL,
                                        0);

            if (!KSUCCESS(Status)) {
                free(Alternate);
                continue;
            }

            if (Alternate->Address.sa_family == AF_INET) {
                Alternate->Type = DNS_RECORD_TYPE_A;

            } else if (Alternate->Address.sa_family == AF_INET6) {
                Alternate->Type = DNS_RECORD_TYPE_AAAA;

            } else {
                free(Alternate);
                continue;
            }

            INSERT_BEFORE(&(Alternate->ListEntry), AlternateList);
            AddedOne = TRUE;
        }
    }

    if (AddedOne == FALSE) {
        Status = STATUS_NOT_FOUND;
        goto GetDnsServersEnd;
    }

    Status = STATUS_SUCCESS;

GetDnsServersEnd:
    if (Devices != NULL) {
        free(Devices);
    }

    Result = 0;
    if (!KSUCCESS(Status)) {
        Result = ClConvertKstatusToErrorNumber(Status);
    }

    return Result;
}

INT
ClpGetNetworkStatus (
    PBOOL Ip4Configured,
    PBOOL Ip6Configured
    )

/*++

Routine Description:

    This routine determines whether or not IPv4 and/or IPv6 networks are
    currently configured on the system.

Arguments:

    Ip4Configured - Supplies a pointer that receives a boolean indicating
        whether or not an IPv4 network is configured on the system.

    Ip6Configured - Supplies a pointer that receives a boolean indicating
        whether or not an IPv6 network is configured on the system.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    ULONG DeviceCount;
    ULONG DeviceIndex;
    DEVICE_INFORMATION_RESULT *Devices;
    NETWORK_DEVICE_INFORMATION Information;
    PVOID NewBuffer;
    INT Result;
    UINTN Size;
    KSTATUS Status;

    *Ip4Configured = FALSE;
    *Ip6Configured = FALSE;

    //
    // Get the array of devices that return network device information.
    //

    DeviceCount = NETWORK_DEVICE_COUNT_ESTIMATE;
    Devices = malloc(sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetNetworkStatusEnd;
    }

    Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                       NULL,
                                       Devices,
                                       &DeviceCount);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            DeviceCount += NETWORK_DEVICE_COUNT_ESTIMATE;
            NewBuffer = realloc(
                              Devices,
                              sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);

            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto GetNetworkStatusEnd;
            }

            NewBuffer = Devices;
            Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                               NULL,
                                               Devices,
                                               &DeviceCount);

            if (!KSUCCESS(Status)) {
                goto GetNetworkStatusEnd;
            }

        } else {
            goto GetNetworkStatusEnd;
        }
    }

    if (DeviceCount == 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto GetNetworkStatusEnd;
    }

    //
    // Loop through all the network devices and determine if they have IPv4
    // and/or IPv6 networks configured.
    //

    memset(&Information, 0, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Size = sizeof(NETWORK_DEVICE_INFORMATION);
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        Information.Domain = NetDomainIp4;
        Status = OsGetSetDeviceInformation(Devices[DeviceIndex].DeviceId,
                                           &ClNetworkDeviceInformationUuid,
                                           &Information,
                                           &Size,
                                           FALSE);

        if (KSUCCESS(Status) &&
            ((Information.Flags & NETWORK_DEVICE_FLAG_MEDIA_CONNECTED) != 0) &&
            ((Information.Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0)) {

            *Ip4Configured = TRUE;
        }

        Information.Domain = NetDomainIp6;
        Status = OsGetSetDeviceInformation(Devices[DeviceIndex].DeviceId,
                                           &ClNetworkDeviceInformationUuid,
                                           &Information,
                                           &Size,
                                           FALSE);

        if (KSUCCESS(Status) &&
            ((Information.Flags & NETWORK_DEVICE_FLAG_MEDIA_CONNECTED) != 0) &&
            ((Information.Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0)) {

            *Ip6Configured = TRUE;
        }

        if ((*Ip4Configured != FALSE) && (*Ip6Configured != FALSE)) {
            break;
        }
    }

    Status = STATUS_SUCCESS;

GetNetworkStatusEnd:
    if (Devices != NULL) {
        free(Devices);
    }

    Result = 0;
    if (!KSUCCESS(Status)) {
        Result = ClConvertKstatusToErrorNumber(Status);
        if ((Result == ENOENT) || (Result == ENETDOWN)) {
            Result = EAI_AGAIN;

        } else {
            errno = Result;
            Result = EAI_SYSTEM;
        }
    }

    return Result;
}

INT
ClpGetLocalAddressInformation (
    int AddressFamily,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine queries the local host for its address information returning
    them as a set of DNS results.

Arguments:

    AddressFamily - Supplies the family of addresses to return.

    ListHead - Supplies a pointer to the initialized list head where the local
        address entries will be returned in the form of DNS_RESULT structures
        set to the A or AAAA type.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    socklen_t AddressLength;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    DEVICE_INFORMATION_RESULT *Devices;
    PDNS_RESULT DnsResult;
    ULONG Flags;
    PSTR FullName;
    NETWORK_DEVICE_INFORMATION Information;
    PVOID NewBuffer;
    INT Result;
    UINTN Size;
    KSTATUS Status;

    DnsResult = NULL;
    FullName = NULL;

    //
    // Get the array of devices that return network device information.
    //

    DeviceCount = NETWORK_DEVICE_COUNT_ESTIMATE;
    Devices = malloc(sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetLocalAddressInformationEnd;
    }

    Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                       NULL,
                                       Devices,
                                       &DeviceCount);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            DeviceCount += NETWORK_DEVICE_COUNT_ESTIMATE;
            NewBuffer = realloc(
                              Devices,
                              sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);

            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto GetLocalAddressInformationEnd;
            }

            NewBuffer = Devices;
            Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                               NULL,
                                               Devices,
                                               &DeviceCount);

            if (!KSUCCESS(Status)) {
                goto GetLocalAddressInformationEnd;
            }

        } else {
            goto GetLocalAddressInformationEnd;
        }
    }

    if (DeviceCount == 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto GetLocalAddressInformationEnd;
    }

    FullName = ClpGetFqdn();
    if (FullName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetLocalAddressInformationEnd;
    }

    //
    // Loop through all the network devices and get the address information for
    // the request address families.
    //

    memset(&Information, 0, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Size = sizeof(NETWORK_DEVICE_INFORMATION);
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        if ((AddressFamily == AF_UNSPEC) || (AddressFamily == AF_INET)) {
            Information.Domain = NetDomainIp4;
            Status = OsGetSetDeviceInformation(Devices[DeviceIndex].DeviceId,
                                               &ClNetworkDeviceInformationUuid,
                                               &Information,
                                               &Size,
                                               FALSE);

            Flags = Information.Flags;
            if (KSUCCESS(Status) &&
                ((Flags & NETWORK_DEVICE_FLAG_MEDIA_CONNECTED) != 0) &&
                ((Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0)) {

                DnsResult = malloc(sizeof(DNS_RESULT));
                if (DnsResult == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto GetLocalAddressInformationEnd;
                }

                memset(DnsResult, 0, sizeof(DNS_RESULT));
                AddressLength = sizeof(DnsResult->Address);
                Status = ClConvertFromNetworkAddress(&(Information.Address),
                                                     &(DnsResult->Address),
                                                     &AddressLength,
                                                     NULL,
                                                     0);

                if (!KSUCCESS(Status)) {
                    free(DnsResult);

                } else {
                    DnsResult->Type = DNS_RECORD_TYPE_A;
                    DnsResult->Class = DNS_CLASS_INTERNET;
                    DnsResult->Name = strdup(FullName);
                    if (DnsResult->Name == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto GetLocalAddressInformationEnd;
                    }

                    INSERT_BEFORE(&(DnsResult->ListEntry), ListHead);
                }

                DnsResult = NULL;
            }
        }

        if ((AddressFamily == AF_UNSPEC) || (AddressFamily == AF_INET6)) {
            Information.Domain = NetDomainIp6;
            Status = OsGetSetDeviceInformation(Devices[DeviceIndex].DeviceId,
                                               &ClNetworkDeviceInformationUuid,
                                               &Information,
                                               &Size,
                                               FALSE);

            Flags = Information.Flags;
            if (KSUCCESS(Status) &&
                ((Flags & NETWORK_DEVICE_FLAG_MEDIA_CONNECTED) != 0) &&
                ((Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0)) {

                DnsResult = malloc(sizeof(DNS_RESULT));
                if (DnsResult == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto GetLocalAddressInformationEnd;
                }

                memset(DnsResult, 0, sizeof(DNS_RESULT));
                AddressLength = sizeof(DnsResult->Address);
                Status = ClConvertFromNetworkAddress(&(Information.Address),
                                                     &(DnsResult->Address),
                                                     &AddressLength,
                                                     NULL,
                                                     0);

                if (!KSUCCESS(Status)) {
                    free(DnsResult);

                } else {
                    DnsResult->Type = DNS_RECORD_TYPE_AAAA;
                    DnsResult->Class = DNS_CLASS_INTERNET;
                    DnsResult->Name = strdup(FullName);
                    if (DnsResult->Name == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto GetLocalAddressInformationEnd;
                    }

                    INSERT_BEFORE(&(DnsResult->ListEntry), ListHead);
                }

                DnsResult = NULL;
            }
        }
    }

    Status = STATUS_SUCCESS;

GetLocalAddressInformationEnd:
    if (Devices != NULL) {
        free(Devices);
    }

    if (DnsResult != NULL) {
        free(DnsResult);
    }

    if (FullName != NULL) {
        free(FullName);
    }

    Result = 0;
    if (!KSUCCESS(Status)) {
        ClpDestroyDnsResultList(ListHead);
        Result = ClConvertKstatusToErrorNumber(Status);
        if ((Result == ENOENT) || (Result == ENETDOWN)) {
            Result = EAI_AGAIN;

        } else {
            errno = Result;
            Result = EAI_SYSTEM;
        }
    }

    return Result;
}

INT
ClpIsLocalAddress (
    const struct sockaddr *SocketAddress,
    socklen_t SocketAddressLength,
    PBOOL LocalAddress,
    PBOOL UnspecifiedAddress
    )

/*++

Routine Description:

    This routine determines whether or not the given socket address is a local
    address. The unspecified "any address" is included as a local address.

Arguments:

    SocketAddress - Supplies a pointer to the socket address to be translated.
        This routine does not handle IPv4-mapped IPv6 addresses.

    SocketAddressLength - Supplies the size of the socket address data.

    LocalAddress - Supplies a pointer that receives a boolean indicating
        whether or not the given address is a local address.

    UnspecifiedAddress - Supplies a pointer that receives a boolean indicating
        whether or not the given address is the "local" unspecified address.

Return Value:

    0 on success.

    Returns an EAI_* error code on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDNS_RESULT DnsResult;
    struct sockaddr_in *Ip4Address;
    struct sockaddr_in6 *Ip6Address;
    LIST_ENTRY ListHead;
    struct sockaddr_in *ResultIp4Address;
    struct sockaddr_in6 *ResultIp6Address;
    INT Status;

    INITIALIZE_LIST_HEAD(&ListHead);
    *LocalAddress = FALSE;
    *UnspecifiedAddress = FALSE;
    if (SocketAddressLength < sizeof(struct sockaddr_in)) {
        Status = EAI_FAIL;
        goto IsLocalAddressEnd;
    }

    //
    // Check for the unspecified address, loopback address, and node local
    // addresses based on the supplied family.
    //

    switch (SocketAddress->sa_family) {
    case AF_INET:
        Ip4Address = (struct sockaddr_in *)SocketAddress;
        if (Ip4Address->sin_addr.s_addr == htonl(INADDR_ANY)) {
            *UnspecifiedAddress = TRUE;
            *LocalAddress = TRUE;

        } else if (Ip4Address->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
            *LocalAddress = TRUE;

        } else {
            INITIALIZE_LIST_HEAD(&ListHead);
            Status = ClpGetLocalAddressInformation(AF_INET, &ListHead);
            if (Status != 0) {
                goto IsLocalAddressEnd;
            }

            CurrentEntry = ListHead.Next;
            while (CurrentEntry != &ListHead) {
                DnsResult = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
                CurrentEntry = CurrentEntry->Next;
                ResultIp4Address = (struct sockaddr_in *)&(DnsResult->Address);
                if (ResultIp4Address->sin_addr.s_addr ==
                    Ip4Address->sin_addr.s_addr) {

                    *LocalAddress = TRUE;
                    break;
                }
            }
        }

        break;

    case AF_INET6:
        Ip6Address = (struct sockaddr_in6 *)SocketAddress;
        if (IN6_IS_ADDR_UNSPECIFIED(&(Ip6Address->sin6_addr)) != FALSE) {
            *UnspecifiedAddress = TRUE;
            *LocalAddress = TRUE;

        } else if (IN6_IS_ADDR_LOOPBACK(&(Ip6Address->sin6_addr)) != FALSE) {
            *LocalAddress = TRUE;

        } else {
            Status = ClpGetLocalAddressInformation(AF_INET6, &ListHead);
            if (Status != 0) {
                goto IsLocalAddressEnd;
            }

            CurrentEntry = ListHead.Next;
            while (CurrentEntry != &ListHead) {
                DnsResult = LIST_VALUE(CurrentEntry, DNS_RESULT, ListEntry);
                CurrentEntry = CurrentEntry->Next;
                ResultIp6Address = (struct sockaddr_in6 *)&(DnsResult->Address);
                if (memcmp(&(ResultIp6Address->sin6_addr.s6_addr),
                           &(Ip6Address->sin6_addr.s6_addr),
                           sizeof(struct in6_addr)) == 0) {

                    *LocalAddress = TRUE;
                    break;
                }
            }
        }

        break;

    default:
        Status = EAI_ADDRFAMILY;
        goto IsLocalAddressEnd;
    }

    Status = 0;

IsLocalAddressEnd:
    ClpDestroyDnsResultList(&ListHead);
    return Status;
}

BOOL
ClpIsNameSubdomain (
    PCSTR Query,
    PCSTR Domain
    )

/*++

Routine Description:

    This routine determines if the given name is a subdomain of the given
    domain.

Arguments:

    Query - Supplies the name in question.

    Domain - Supplies the domain to match against.

Return Value:

    TRUE if the query is a subdomain of the given domain.

    FALSE if it is not.

--*/

{

    size_t DomainLength;
    size_t QueryLength;

    QueryLength = strlen(Query);
    DomainLength = strlen(Domain);
    if (QueryLength < DomainLength) {
        return FALSE;
    }

    if (strcmp(Query + QueryLength - DomainLength, Domain) == 0) {
        return TRUE;
    }

    return FALSE;
}

