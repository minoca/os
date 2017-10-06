/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    netdb.h

Abstract:

    This header contains definitions for network database operations.

Author:

    Evan Green 5-Dec-2013

--*/

#ifndef _NETDB_H
#define _NETDB_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define address information (addrinfo) flags.
//

//
// Set if the socket address is intended for a call to bind.
//

#define AI_PASSIVE 0x00000001

//
// Set to request a canonical name.
//

#define AI_CANONNAME 0x00000002

//
// Set to inhibit service name resolution.
//

#define AI_NUMERICHOST 0x00000004

//
// Set to inhibit service resolution.
//

#define AI_NUMERICSERV 0x00000008

//
// Set to query for IPv4 addresses and return IPv4-mapped IPv6 addresses if no
// IPv6 addresses are found.
//

#define AI_V4MAPPED 0x00000010

//
// Set to query for both IPv4 and IPv6 addresses.
//

#define AI_ALL 0x00000020

//
// Set to query for IPv4 address only when an IPv4 address is configured; query
// for IPv6 addresses only when an IPv6 address is configured.
//

#define AI_ADDRCONFIG 0x00000040

//
// Define constants passed to getnameinfo.
//

//
// Set if only the nodename portion of the FQDN is returned for local hosts.
//

#define NI_NOFQDN 0x00000001

//
// Set if the numeric form of the node's address is returned instead of its
// name.
//

#define NI_NUMERICHOST 0x00000002

//
// Set to return an error if the node's name cannot be located in the database.
//

#define NI_NAMEREQD 0x00000004

//
// Set to get the numeric form of the service address instead of its name.
//

#define NI_NUMERICSERV 0x00000008

//
// Set to return the numeric form of the scope identifier instead of its name
// for IPv6 addresses.
//

#define NI_NUMERICSCOPE 0x00000010

//
// Set to indicate the service is a datagram service (SOCK_DGRAM). If not
// specified, the service will be assumed to be a stream service (SOCK_STREAM).
//

#define NI_DGRAM 0x00000020

//
// Define the maximum length of a fully qualified domain name for getnameinfo.
//

#define NI_MAXHOST 1025

//
// Define the maximum length of a service name.
//

#define NI_MAXSERV 32

//
// Define errors returned by the address information functions.
//

//
// The address family for the hostname is not supported.
//

#define EAI_ADDRFAMILY 1

//
// The name could not be resolved at this time. Future attempts may succeed.
//

#define EAI_AGAIN 2

//
// The flags had an invalid value.
//

#define EAI_BADFLAGS 3

//
// A non-recoverable error occurred.
//

#define EAI_FAIL 4

//
// The address family was not recognized or the address length was invalid for
// the specified family.
//

#define EAI_FAMILY 5

//
// There was a memory allocation failure.
//

#define EAI_MEMORY 6

//
// No address is associated with the hostname.
//

#define EAI_NODATA 7

//
// The name does not resolve for the supplied parameters. NI_NAMEREQD is set
// and the host's name cannot be located, or both nodename and servname were
// null.
//

#define EAI_NONAME 8

//
// The service passed was not recognized for the specified socket type.
//

#define EAI_SERVICE 9

//
// The intended socket type was not recognized.
//

#define EAI_SOCKTYPE 10

//
// A system error occurred. The error code can be found in errno.
//

#define EAI_SYSTEM 11

//
// An argument buffer overflowed.
//

#define EAI_OVERFLOW 12

//
// Define errors returned by gethostbyaddr and gethostbyname.
//

//
// No such host is known.
//

#define HOST_NOT_FOUND 1

//
// The server recognized the request and name, but no address is available.
// Another type of request to the name server for the domain might return an
// answer.
//

#define NO_DATA 2

//
// An unexpected server failure occurred which cannot be recovered from.
//

#define NO_RECOVERY 3

//
// A temporary and possibly transient error occurred, such as a failure of a
// server to respond.
//

#define TRY_AGAIN 4

//
// Define a pretend member of the hostent structure which goes to the first
// address in the list.
//

#define h_addr h_addr_list[0]

//
// Define h_errno as a macro so that tests to see if it is defined succeed.
//

#define h_errno h_errno

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a network host entry.

Members:

    h_name - Stores a pointer to a string containing the official name of the
        host.

    h_aliases - Stores a pointer to an array of pointers to alternative host
        names, terminated by a null pointer.

    h_addrtype - Stores the address type of this entry.

    h_length - Stores the length in bytes of the address.

    h_addr_list - Stores a pointer to an array of pointers to network addresses
        (in network byte order) for the host, terminated by a null pointer.

--*/

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

/*++

Structure Description:

    This structure defines a network entry.

Members:

    n_name - Stores a pointer to the official, fully-qualified (including the
        domain) name of the host.

    n_aliases - Stores a pointer to an array of pointers to alternative network
        names, terminated by a null pointer.

    n_addrtype - Stores the address type of the network.

    n_net - Stores the network number, in host byte order.

--*/

struct netent {
    char *n_name;
    char **n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

/*++

Structure Description:

    This structure defines a protocol entry.

Members:

    p_name - Stores a pointer to the official name of the protocol.

    p_aliases - Stores a pointer to an array of pointers to alternative
        protocol names, terminated by a null pointer.

    p_proto - Stores the protocol number.

--*/

struct protoent {
    char *p_name;
    char **p_aliases;
    int p_proto;
};

/*++

Structure Description:

    This structure defines a service entry.

Members:

    s_name - Stores a pointer to the official name of the service.

    s_aliases - Stores a pointer to an array of pointers to alternative service
        names, terminated by a null pointer.

    s_port - Stores the port number of the service in network byte order.

    s_proto - Stores the name of the protocol to use when contacting the
        service.

--*/

struct servent {
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};

/*++

Structure Description:

    This structure defines the address information structure.

Members:

    ai_flags - Stores the input flags. See AI_* definitions.

    ai_family - Stores the address family of the socket.

    ai_socktype - Stores the socket type.

    ai_protocol - Stores the protocol of the socket.

    ai_addrlen - Stores the length of the buffer pointed to by ai_addr.

    ai_addr - Stores a pointer to the socket address of the socket.

    ai_canonname - Stores the canonical name of the service location.

    ai_next - Stores a pointer to the next address information structure in the
        list.

--*/

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Define the error variable set by the gethostbyaddr and gethostbyname
// functions.
//

LIBC_API extern __THREAD int h_errno;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
void
freeaddrinfo (
    struct addrinfo *AddressInformation
    );

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

LIBC_API
int
getaddrinfo (
    const char *NodeName,
    const char *ServiceName,
    const struct addrinfo *Hints,
    struct addrinfo **Result
    );

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
    );

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
        addresses. If the address is "::", the lookup is not performed and
        EAI_NONAME is returned.

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

LIBC_API
const char *
gai_strerror (
    int ErrorCode
    );

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

LIBC_API
struct hostent *
gethostbyaddr (
    const void *Address,
    socklen_t Length,
    int FamilyType
    );

/*++

Routine Description:

    This routine returns a host entry containing addresses of the given family
    type. This function is neither thread safe nor reentrant.

Arguments:

    Address - Supplies a pointer to the address (whose type depends on the
        family type parameter).

    Length - Supplies the length of the address buffer.

    FamilyType - Supplies the family type of the address to return.

Return Value:

    Returns a pointer to the host information. This buffer may be overwritten
    by subsequent calls to this routine.

    NULL on failure, and h_errno will be set to contain more information.
    Common values for h_errno are:

    HOST_NOT_FOUND if no such host is known.

    NO_DATA if the server recognized the request and the name, but no address
    is available.

    NO_RECOVERY if an unexpected server failure occurred.

    TRY_AGAIN if a temporary and possibly transient error occurred, such as a
    failure of the server to responsd.

--*/

LIBC_API
struct hostent *
gethostbyname (
    const char *Name
    );

/*++

Routine Description:

    This routine returns a host entry containing addresses of family AF_INET
    for the host with the given name. This function is neither thread safe nor
    reentrant.

Arguments:

    Name - Supplies a pointer to a null terminated string containing the name
        of the node.

Return Value:

    Returns a pointer to the host information. This buffer may be overwritten
    by subsequent calls to this routine.

    NULL on failure, and h_errno will be set to contain more information.
    Common values for h_errno are:

    HOST_NOT_FOUND if no such host is known.

    NO_DATA if the server recognized the request and the name, but no address
    is available.

    NO_RECOVERY if an unexpected server failure occurred.

    TRY_AGAIN if a temporary and possibly transient error occurred, such as a
    failure of the server to responsd.

--*/

LIBC_API
void
sethostent (
    int StayOpen
    );

/*++

Routine Description:

    This routine opens a connection to the host database and sets the next
    entry for retrieval to the first entry in the database. This routine is
    neither reentrant nor thread safe.

Arguments:

    StayOpen - Supplies a value that if non-zero indicates that the connection
        shall not be closed by a call to gethostent, gethostbyname or
        gethostbyaddr, and the implementation may maintain an open file
        descriptor.

Return Value:

    None.

--*/

LIBC_API
struct hostent *
gethostent (
    void
    );

/*++

Routine Description:

    This routine reads the next entry in the host database. This routine is
    neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the next entry in the host database. This buffer may
    be overwritten by subsequent calls to this routine, gethostbyname, or
    gethostbyaddr.

--*/

LIBC_API
void
endhostent (
    void
    );

/*++

Routine Description:

    This routine closes any open database connection established by the
    sethostent routine.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
setnetent (
    int StayOpen
    );

/*++

Routine Description:

    This routine opens a connection to the network database and sets the next
    entry for retrieval to the first entry in the database. This routine is
    neither reentrant nor thread safe.

Arguments:

    StayOpen - Supplies a value that if non-zero indicates that the
        implementation may maintain an open file descriptor to the network
        database.

Return Value:

    None.

--*/

LIBC_API
struct netent *
getnetent (
    void
    );

/*++

Routine Description:

    This routine reads the next entry in the network database. This routine is
    neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the next entry in the network database. This buffer
    may be overwritten by subsequent calls to this routine, getnetbyaddr, or
    getnetbyname.

--*/

LIBC_API
void
endnetent (
    void
    );

/*++

Routine Description:

    This routine closes any open database connection established by the
    setnetent routine.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
struct netent *
getnetbyaddr (
    uint32_t Network,
    int AddressFamily
    );

/*++

Routine Description:

    This routine searches the network database from the beginning and attempts
    to find the first entry matching the given address family (in
    netent.n_addrtype) and network number (in netent.n_net). This routine is
    neither thread safe nor reentrant.

Arguments:

    Network - Supplies the network to match against.

    AddressFamily - Supplies the address type to match against.

Return Value:

    Returns a pointer to a matching entry in the network database. This buffer
    may be overwritten by subsequent calls to this routine, getnetent, or
    getnetbyname.

    NULL on failure.

--*/

LIBC_API
struct netent *
getnetbyname (
    const char *Name
    );

/*++

Routine Description:

    This routine searches the network database from the beginning and attempts
    to find the first entry matching the given name (in netent.n_name). This
    routine is neither thread safe nor reentrant.

Arguments:

    Name - Supplies a pointer to a string containing the name of the network to
        search for.

Return Value:

    Returns a pointer to a matching entry in the network database. This buffer
    may be overwritten by subsequent calls to this routine, getnetent, or
    getnetbyaddr.

    NULL on failure.

--*/

LIBC_API
void
setprotoent (
    int StayOpen
    );

/*++

Routine Description:

    This routine opens a connection to the protocol database and sets the next
    entry for retrieval to the first entry in the database. This routine is
    neither reentrant nor thread safe.

Arguments:

    StayOpen - Supplies a value that if non-zero indicates that the
        implementation may maintain an open file descriptor to the protocol
        database.

Return Value:

    None.

--*/

LIBC_API
struct protoent *
getprotoent (
    void
    );

/*++

Routine Description:

    This routine reads the next entry in the protocol database. This routine is
    neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the next entry in the protocol database. This buffer
    may be overwritten by subsequent calls to this routine, getprotobyname, or
    getnetbynumber.

--*/

LIBC_API
void
endprotoent (
    void
    );

/*++

Routine Description:

    This routine closes any open database connection established by the
    setprotoent routine. This routine is neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
struct protoent *
getprotobynumber (
    int ProtocolNumber
    );

/*++

Routine Description:

    This routine searches the protocol database from the beginning and attempts
    to find the first entry matching the given protocol number. This routine is
    neither thread safe nor reentrant.

Arguments:

    ProtocolNumber - Supplies the number of the protocol to find.

Return Value:

    Returns a pointer to a matching entry in the protocol database. This buffer
    may be overwritten by subsequent calls to this routine, getprotoent, or
    getprotobyname.

    NULL on failure.

--*/

LIBC_API
struct protoent *
getprotobyname (
    const char *Name
    );

/*++

Routine Description:

    This routine searches the protocol database from the beginning and attempts
    to find the first entry matching the given name. This routine is neither
    thread safe nor reentrant.

Arguments:

    Name - Supplies a pointer to a string containing the name of the protocol
        to search for.

Return Value:

    Returns a pointer to a matching entry in the protocol database. This buffer
    may be overwritten by subsequent calls to this routine, getprotoent, or
    getprotobynumber.

    NULL on failure.

--*/

LIBC_API
void
setservent (
    int StayOpen
    );

/*++

Routine Description:

    This routine opens a connection to the network service database and sets
    the next entry for retrieval to the first entry in the database. This
    routine is neither reentrant nor thread safe.

Arguments:

    StayOpen - Supplies a value that if non-zero indicates that the
        implementation may maintain an open file descriptor to the protocol
        database.

Return Value:

    None.

--*/

LIBC_API
struct servent *
getservent (
    void
    );

/*++

Routine Description:

    This routine reads the next entry in the network service database. This
    routine is neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the next entry in the network service database. This
    buffer may be overwritten by subsequent calls to this routine,
    getservbyname, or getservbyport.

--*/

LIBC_API
void
endservent (
    void
    );

/*++

Routine Description:

    This routine closes any open database connection established by the
    setservent routine. This routine is neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
struct servent *
getservbyport (
    int Port,
    const char *Protocol
    );

/*++

Routine Description:

    This routine searches the protocol database from the beginning and attempts
    to find the first entry where the given port matches the s_port member and
    the protocol name matches the s_proto member of the servent structure.

Arguments:

    Port - Supplies the port number to match, in network byte order.

    Protocol - Supplies an optional pointer to a string containing the protocol
        to match. If this is null, any protocol will match.

Return Value:

    Returns a pointer to a matching entry in the network service database. This
    buffer may be overwritten by subsequent calls to this routine, getservent,
    or getprotobyname.

    NULL on failure.

--*/

LIBC_API
struct servent *
getservbyname (
    const char *Name,
    const char *Protocol
    );

/*++

Routine Description:

    This routine searches the network service database from the beginning and
    attempts to find the first entry where the given name matches the s_name
    member and the given protcol matches the s_proto member. This routine is
    neither thread safe nor reentrant.

Arguments:

    Name - Supplies a pointer to a string containing the name of the service
        to search for.

    Protocol - Supplies an optional pointer to the string containing the
        protocol to match. If this is null, any protocol will match.

Return Value:

    Returns a pointer to a matching entry in the network service database. This
    buffer may be overwritten by subsequent calls to this routine, getservent,
    or getservbyport.

    NULL on failure.

--*/

#ifdef __cplusplus

}

#endif
#endif

