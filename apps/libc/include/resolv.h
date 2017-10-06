/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    resolv.h

Abstract:

    This header contains definitions for the name resolver.

Author:

    Evan Green 14-Jan-2015

--*/

#ifndef _RESOLV_H
#define _RESOLV_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/nameser.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the resolver revision number.
//

#define __RES 19991006

//
// Define the maximum number of name servers to track.
//

#define MAXNS 3

//
// Define the number of default domain levels to try.
//

#define MAXDFLSRCH 3

//
// Define the maximum number of domains in the search path.
//

#define MAXDNSRCH 6

//
// Define the minimum number of levels in a name that is local.
//

#define LOCALDOMAINPARTS 2

//
// Define the minimum number of seconds between retries.
//

#define RES_TIMEOUT 5

//
// Define the number of net to sort on.
//

#define MAXRESOLVSORT 10

#define RES_MAXNDOTS 15
#define RES_MAXRETRY 5
#define RES_DFLRETRY 2
#define RES_MAXTIME 65535

//
// Define the nsaddr member of __res_state, for backwards compatibility.
//

#define nsaddr nsaddr_list[0]

//
// Define the path to the resolver configuration file.
//

#define _PATH_RESCONF "/etc/resolv.conf"

//
// Define resolver flags.
//

//
// This flag is set if the socket is TCP.
//

#define RES_F_VC 0x00000001

//
// This flag is set if the socket is connected.
//

#define RES_F_CONN 0x00000002

//
// This flag is set if EDNS0 caused errors.
//

#define RES_F_EDNS0ERR 0x00000004

//
// Define res_findzonecut options.
//

//
// Set this flag to always do all queries.
//

#define RES_EXHAUSTIVE 0x00000001

//
// Define resolver options.
//

//
// This flag is set if the resolver is initialized.
//

#define RES_INIT 0x00000001

//
// This flag is set if the resolver should print debug messages.
//

#define RES_DEBUG 0x00000002

//
// This flag is set if the resolver should return only authoritative answers.
//

#define RES_AAONLY 0x00000004

//
// This flag is set if the resolver should use a virtual circuit.
//

#define RES_USEVC 0x00000008

//
// This flag is set if the resolver should query the primary server only.
//

#define RES_PRIMARY 0x00000010

//
// This flag is set if the resolver should ignore truncation errors.
//

#define RES_IGNTC 0x00000020

//
// This flag is set if recursion is desired.
//

#define RES_RECURSE 0x00000040

//
// This flag is set if the resolver should use the default domain name.
//

#define RES_DEFNAMES 0x00000080

//
// This flag is set if the resolver should keep the TCP socket open.
//

#define RES_STAYOPEN 0x00000100

//
// This flag is set if the resolver should search up the local domain tree.
//

#define RES_DNSRCH 0x00000200

//
// This flag is set if type 1 security is disabled.
//

#define RES_INSECURE1 0x00000400

//
// This flag is set if type 2 security is disabled.
//

#define RES_INSECURE2 0x00000800

//
// This flag is set to disable the HOSTALIASES feature.
//

#define RES_NOALIASES 0x00001000

//
// This flag is set to use or map IPv6 addresses in gethostbyname.
//

#define RES_USE_INET6 0x00002000

//
// This flag is set if the name server list should be rotated after each query.
//

#define RES_ROTATE 0x00004000

//
// This flag is set if the resolver should not check names for sanity.
//

#define RES_NOCHECKNAME 0x00008000

//
// This flag is set if the resolver should not strip TSIG records.
//

#define RES_KEEPTSIG 0x00010000

//
// This flag is set if the resolver should blast all recursive servers.
//

#define RES_BLAST 0x00020000

//
// This flag is set if the resolver should do IPv6 reverse lookup with byte
// strings.
//

#define RES_USEBSTRING 0x00040000

//
// This flag is set if the resolver should not use .ip6.int in IPv6 reverse
// lookup.
//

#define RES_NOIP6DOTINT 0x00080000

//
// This flag is set if the resolver should use EDNS0.
//

#define RES_USE_EDNS0 0x00100000

//
// This flag is set if the resolver should respond to only one outstanding
// request at a time.
//

#define RES_SNGLKUP 0x00200000

//
// This flag is the same as the single lookup flag, but a new socket is opened
// up for each request.
//

#define RES_SNGLKUPREOP 0x00400000

//
// This flag is set if the resolver should use DNSSEC in OPT.
//

#define RES_USE_DNSSEC 0x00800000

//
// This flag is set if the resolver should not look up an unqualified name
// as a TLD.
//

#define RES_NOTLDQUERY 0x01000000

//
// Define the default flags.
//

#define RES_DEFAULT (RES_RECURSE | RES_DEFNAMES | RES_DNSRCH | RES_NOIP6DOTINT)

//
// Define values for the pfcode member of the resolver state.
//

#define RES_PRF_STATS   0x00000001
#define RES_PRF_UPDATE  0x00000002
#define RES_PRF_CLASS   0x00000004
#define RES_PRF_CMD     0x00000008
#define RES_PRF_QUES    0x00000010
#define RES_PRF_ANS     0x00000020
#define RES_PRF_AUTH    0x00000040
#define RES_PRF_ADD     0x00000080
#define RES_PRF_HEAD1   0x00000100
#define RES_PRF_HEAD2   0x00000200
#define RES_PRF_TTLID   0x00000400
#define RES_PRF_HEADX   0x00000800
#define RES_PRF_QUERY   0x00001000
#define RES_PRF_REPLY   0x00002000
#define RES_PRF_INIT    0x00004000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _res_sendhookact {
    res_goahead,
    res_nextns,
    res_modified,
    res_done,
    res_error
} res_sendhookact;

typedef struct __res_state *res_state;

typedef
res_sendhookact
(*res_send_qhook) (
    struct sockaddr_in *const *NameServer,
    const u_char **Query,
    int *QueryLength,
    u_char *Answer,
    int AnswerSize,
    int *ResponseLength
    );

/*++

Routine Description:

    This routine implements an optional hook function called when a query is
    sent.

Arguments:

    NameServer - Supplies a pointer to a pointer to the name server being
        queried.

    Query - Supplies a pointer that on input points to the query being sent.

    QueryLength - Supplies a pointer that contains the query length.

    Answer - Supplies a pointer to the answer.

    AnswerSize - Supplies the size of the answer.

    ResponseLength - Supplies a pointer that contains the response length.

Return Value:

    Returns a hook action.

--*/

typedef
res_sendhookact
(*res_send_rhook) (
    struct sockaddr_in *NameServer,
    const u_char *Query,
    int QueryLength,
    u_char *Answer,
    int AnswerSize,
    int *ResponseLength
    );

/*++

Routine Description:

    This routine implements an optional hook function called when a query is
    sent.

Arguments:

    NameServer - Supplies a pointer to the name server being queried.

    Query - Supplies a pointer to the query being sent.

    QueryLength - Supplies the query length.

    Answer - Supplies a pointer to the answer.

    AnswerSize - Supplies the size of the answer.

    ResponseLength - Supplies a pointer that contains the response length.

Return Value:

    Returns a hook action.

--*/

/*++

Structure Description:

    This structure defines the resolver state.

Members:

    retrans - Stores the retransmission time interval.

    retry - Stores the number of times to retransmit.

    options - Stores the option flags.

    nscount - Stores the number of name servers.

    nsaddr_list - Stores an array of the name server addresses.

    id - Stores the current message identifier.

    dnsrch - Stores the components of the domain to search.

    defdname - Stores the default domain, deprecated.

    pfcode - Stores a bitfield of protocol flags. See RES_PF_* definitions.

    ndots - Stores the threshold for the initial absolute query.

    nsort - Storse the number of elements in the sort list.

    ipv6_unavail - Stores a boolean indicating that connecting on IPv6 failed.

    unused - Stores unused/reserved bits.

    sort_list - Stores the array of addresses to sort.

    qhook - Stores an optional pointer to a query hook function.

    rhook - Stores an optional pointer to a response hook function.

    res_h_errno - Stores the error number.

    _sock - Stores the private socket.

    _flags - Stores the private flags.

    _u - Stores a private state area, some of which is defined.

--*/

struct __res_state {
    int retrans;
    int retry;
    u_long options;
    int nscount;
    struct sockaddr_in nsaddr_list[MAXNS];
    u_short id;
    char *dnsrch[MAXDNSRCH + 1];
    char defdname[MAXCDNAME + 1];
    u_long pfcode;
    unsigned ndots:4;
    unsigned nsort:4;
    unsigned ipv6_unavail:1;
    unsigned unused:23;
    struct {
        struct in_addr addr;
        u_int32_t mask;
    } sort_list[MAXRESOLVSORT];

    res_send_qhook qhook;
    res_send_rhook rhook;
    int res_h_errno;
    int _sock;
    u_int _flags;
    union {
        char pad[52];
        struct {
            u_int16_t nscount;
            u_int16_t nsmap[MAXNS];
            int nssocks[MAXNS];
            u_int16_t nscount6;
            u_int16_t nsinit;
            struct sockaddr_in6 *nsaddrs[MAXNS];
            unsigned long long initstamp;
        } _ext;
    } _u;
};

/*++

Structure Description:

    This structure defines the resolver symbol.

Members:

    number - Stores an identifying number (like T_MX).

    name - Stores the name of the resource, like "MX".

    humanname - Stores a descriptive name, like "Mail Exchanger".

--*/

struct res_sym {
    int number;
    char *name;
    char *humanname;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Store the resolver state, somewhat accessible by applications.
//

extern LIBC_API struct __res_state _res;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
res_init (
    void
    );

/*++

Routine Description:

    This routine initializes the global resolver state.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information.

--*/

LIBC_API
int
res_search (
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine constructs a query, sends it to the DNS server, awaits a
    response, and performs preliminary checks on the reply. It is the same as
    res_nquery, except that it also implements the default and search rules
    controlled by the RES_DEFNAMES and RES_DNSRCH options. It returns the first
    successful reply.

Arguments:

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer.

Return Value:

    Returns the size of the response on success.

    -1 on failure.

--*/

LIBC_API
int
res_query (
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine constructs a query, sends it to the DNS server, awaits a
    response, and performs preliminary checks on the reply.

Arguments:

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer.

Return Value:

    Returns the size of the response on success.

    -1 on failure.

--*/

LIBC_API
int
res_mkquery (
    int Op,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Data,
    int DataLength,
    struct rrec *NewRecord,
    u_char *Buffer,
    int BufferLength
    );

/*++

Routine Description:

    This routine constructs a DNS query from the given parameters.

Arguments:

    Op - Supplies the operation to perform. This is usually QUERY but can be
        any op from nameser.h.

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Data - Supplies an unused data pointer.

    DataLength - Supplies the length of the data.

    NewRecord - Supplies a new record pointer, currently unused.

    Buffer - Supplies a pointer where the DNS query will be returned.

    BufferLength - Supplies the length of the return buffer in bytes.

Return Value:

    Returns the size of the query created, or -1 on failure.

--*/

LIBC_API
int
res_send (
    const u_char *Message,
    int MessageLength,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine sends a message to the currently configured DNS server and
    returns the reply.

Arguments:

    Message - Supplies a pointer to the message to send.

    MessageLength - Supplies the length of the message in bytes.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer in bytes.

Return Value:

    Returns the length of the reply message on success.

    -1 on failure.

--*/

LIBC_API
void
res_close (
    void
    );

/*++

Routine Description:

    This routine closes the socket for the global resolver state.

Arguments:

    None.

Return Value:

    None.

--*/

//
// These resolver interface functions operate on a state pointer passed in,
// rather than a global object.
//

LIBC_API
int
res_ninit (
    res_state State
    );

/*++

Routine Description:

    This routine initializes the resolver state.

Arguments:

    State - Supplies the state to initialize, a pointer type.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information.

--*/

LIBC_API
int
res_nsearch (
    res_state State,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine constructs a query, sends it to the DNS server, awaits a
    response, and performs preliminary checks on the reply. It is the same as
    res_nquery, except that it also implements the default and search rules
    controlled by the RES_DEFNAMES and RES_DNSRCH options. It returns the first
    successful reply.

Arguments:

    State - Supplies the state, a pointer type.

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer.

Return Value:

    Returns the size of the response on success.

    -1 on failure.

--*/

LIBC_API
int
res_nquery (
    res_state State,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine constructs a query, sends it to the DNS server, awaits a
    response, and performs preliminary checks on the reply.

Arguments:

    State - Supplies the state, a pointer type.

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer.

Return Value:

    Returns the size of the response on success.

    -1 on failure.

--*/

LIBC_API
int
res_nmkquery (
    res_state State,
    int Op,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Data,
    int DataLength,
    struct rrec *NewRecord,
    u_char *Buffer,
    int BufferLength
    );

/*++

Routine Description:

    This routine constructs a DNS query from the given parameters.

Arguments:

    State - Supplies the state, a pointer type.

    Op - Supplies the operation to perform. This is usually QUERY but can be
        any op from nameser.h.

    DomainName - Supplies the domain name to query for.

    Class - Supplies the class to put in the query.

    Type - Supplies the type to put in the query.

    Data - Supplies an unused data pointer.

    DataLength - Supplies the length of the data.

    NewRecord - Supplies a new record pointer, currently unused.

    Buffer - Supplies a pointer where the DNS query will be returned.

    BufferLength - Supplies the length of the return buffer in bytes.

Return Value:

    Returns the size of the query created, or -1 on failure.

--*/

LIBC_API
int
res_nsend (
    res_state State,
    const u_char *Message,
    int MessageLength,
    u_char *Answer,
    int AnswerLength
    );

/*++

Routine Description:

    This routine sends a message to the currently configured DNS server and
    returns the reply.

Arguments:

    State - Supplies the resolver state, a pointer type.

    Message - Supplies a pointer to the message to send.

    MessageLength - Supplies the length of the message in bytes.

    Answer - Supplies a pointer where the answer will be returned.

    AnswerLength - Supplies the length of the answer buffer in bytes.

Return Value:

    Returns the length of the reply message on success.

    -1 on failure.

--*/

LIBC_API
void
res_nclose (
    res_state State
    );

/*++

Routine Description:

    This routine closes the socket for the given resolver state.

Arguments:

    State - Supplies a pointer to the state to close.

Return Value:

    None.

--*/

LIBC_API
int
dn_expand (
    const u_char *Message,
    const u_char *MessageEnd,
    const u_char *Source,
    u_char *Destination,
    unsigned int DestinationSize
    );

/*++

Routine Description:

    This routine expands a DNS name in compressed format.

Arguments:

    Message - Supplies a pointer to the DNS query or result.

    MessageEnd - Supplies one beyond the last valid byte in the message.

    Source - Supplies a pointer to the compressed name to decompress.

    Destination - Supplies a pointer where the decompressed name will be
        returned on success.

    DestinationSize - Supplies the size of the decompressed name buffer in
        bytes.

Return Value:

    Returns the size of the compressed name.

    -1 on error.

--*/

LIBC_API
int
dn_comp (
    const char *Source,
    u_char *Destination,
    unsigned int DestinationSize,
    u_char **DomainNames,
    u_char **LastDomainName
    );

/*++

Routine Description:

    This routine compresses a name for a format suitable for DNS queries and
    responses.

Arguments:

    Source - Supplies the source name to compress.

    Destination - Supplies a pointer where the compressed name will be returned
        on success.

    DestinationSize - Supplies the size of the destination buffer on success.

    DomainNames - Supplies an array of previously compressed names in the
        message. The first pointer must point to the beginning of the message.
        The list ends with NULL.

    LastDomainName - Supplies one beyond the end of the array of domain name
        pointers.

Return Value:

    Returns the size of the compressed name.

    -1 on error.

--*/

LIBC_API
int
dn_skipname (
    const u_char *Name,
    const u_char *MessageEnd
    );

/*++

Routine Description:

    This routine skips over a compressed DNS name.

Arguments:

    Name - Supplies a pointer to the compressed name.

    MessageEnd - Supplies a pointer one byte beyond the last valid byte in the
        query or response.

Return Value:

    Returns the size of the compressed name.

    -1 on error.

--*/

#ifdef __cplusplus

}

#endif
#endif

