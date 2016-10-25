/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netent.c

Abstract:

    This module implements support for network, protocol, and service name
    resolution.

Author:

    Evan Green 8-Jan-2014

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <netdb.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the error variable set by the gethostbyaddr and gethostbyname
// functions.
//

LIBC_API __THREAD int h_errno;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
struct hostent *
gethostbyaddr (
    const void *Address,
    socklen_t Length,
    int FamilyType
    )

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

{

    //
    // TODO: Implement gethostbyaddr.
    //

    h_errno = NO_RECOVERY;
    return NULL;
}

LIBC_API
struct hostent *
gethostbyname (
    const char *Name
    )

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

{

    //
    // TODO: Implement gethostbyname.
    //

    h_errno = NO_RECOVERY;
    return NULL;
}

LIBC_API
void
sethostent (
    int StayOpen
    )

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

{

    //
    // TODO: Implement sethostent.
    //

    return;
}

LIBC_API
struct hostent *
gethostent (
    void
    )

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

{

    //
    // TODO: Implement gethostent.
    //

    return NULL;
}

LIBC_API
void
endhostent (
    void
    )

/*++

Routine Description:

    This routine closes any open database connection established by the
    sethostent routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // TODO: Implement endhostent.
    //

    return;
}

LIBC_API
void
setnetent (
    int StayOpen
    )

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

{

    //
    // TODO: Implement setnetent.
    //

    return;
}

LIBC_API
struct netent *
getnetent (
    void
    )

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

{

    //
    // TODO: Implement getnetent.
    //

    return NULL;
}

LIBC_API
void
endnetent (
    void
    )

/*++

Routine Description:

    This routine closes any open database connection established by the
    setnetent routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // TODO: Implement endnetent.
    //

    return;
}

LIBC_API
struct netent *
getnetbyaddr (
    uint32_t Network,
    int AddressFamily
    )

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

{

    //
    // TODO: Implement getnetbyaddr.
    //

    return NULL;
}

LIBC_API
struct netent *
getnetbyname (
    const char *Name
    )

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

{

    //
    // TODO: Implement getnetbyname.
    //

    return NULL;
}

LIBC_API
void
setprotoent (
    int StayOpen
    )

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

{

    //
    // TODO: Implement setprotoent.
    //

    return;
}

LIBC_API
struct protoent *
getprotoent (
    void
    )

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

{

    //
    // TODO: Implement getprotoent.
    //

    return NULL;
}

LIBC_API
void
endprotoent (
    void
    )

/*++

Routine Description:

    This routine closes any open database connection established by the
    setprotoent routine. This routine is neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // TODO: Implement endprotoent.
    //

    return;
}

LIBC_API
struct protoent *
getprotobynumber (
    int ProtocolNumber
    )

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

{

    //
    // TODO: Implement getprotobynumber.
    //

    return NULL;
}

LIBC_API
struct protoent *
getprotobyname (
    const char *Name
    )

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

{

    //
    // TODO: Implement getprotobyname.
    //

    return NULL;
}

LIBC_API
void
setservent (
    int StayOpen
    )

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

{

    //
    // TODO: Implement setservent.
    //

    return;
}

LIBC_API
struct servent *
getservent (
    void
    )

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

{

    //
    // TODO: Implement getservent.
    //

    return NULL;
}

LIBC_API
void
endservent (
    void
    )

/*++

Routine Description:

    This routine closes any open database connection established by the
    setservent routine. This routine is neither thread safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // TODO: Implement endservent.
    //

    return;
}

LIBC_API
struct servent *
getservbyport (
    int Port,
    const char *Protocol
    )

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

{

    //
    // TODO: Implement getservbyport.
    //

    return NULL;
}

LIBC_API
struct servent *
getservbyname (
    const char *Name,
    const char *Protocol
    )

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

{

    //
    // TODO: Implement getservbyname.
    //

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

