/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    resolv.c

Abstract:

    This module implements the standard DNS resolver functions.

Author:

    Evan Green 23-Jan-2015

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
#include <resolv.h>
#include <stdlib.h>
#include "net.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro evaluates to non-zero if the given character is a special
// character (per the DNS spec).
//

#define DNS_SPECIAL_CHARACTER(_Character) \
    (((_Character) == '"') || ((_Character) == '.') || \
     ((_Character) == ';') || ((_Character) == '\\') || \
     ((_Character) == '@') || ((_Character) == '$'))

//
// This macro evaluates to non-zero if the given character is printable
// according to the DNS spec.
//

#define DNS_PRINTABLE_CHARACTER(_Character) \
    (((_Character) > ' ') && ((_Character) < 0x7F))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the name of an environment variable to use as a DNS server address,
// overriding the configuration.
//

#define DNS_DNSCACHEIP_VARIABLE "DNSCACHEIP"

//
// Define the maximum size of the resolver configuration file.
//

#define DNS_RESOLVER_CONFIGURATION_MAX 4096

//
// Define the maximum number of supported local domains.
//

#define DNS_DOMAIN_COUNT 8

//
// Define the maximum size of a DNS query.
//

#define DNS_QUERY_MAX 512

//
// Define the maximum size of a DNS name component.
//

#define DNS_COMPONENT_MAX 63

//
// Define the maximum number of times in a row to call a hook.
//

#define DNS_MAX_HOOK_CALLS 50

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpDnsReadStartFiles (
    res_state State
    );

int
ClpDnsParseSocketAddress (
    PSTR Address,
    PVOID SocketAddress
    );

INT
ClpDnsAddConfiguredServers (
    res_state State,
    NET_DOMAIN_TYPE Domain
    );

INT
ClpDnsMatchQueries (
    PUCHAR Buffer1,
    PUCHAR Buffer1End,
    PUCHAR Buffer2,
    PUCHAR Buffer2End
    );

INT
ClpDnsIsNameInQuery (
    PUCHAR Name,
    INT Type,
    INT Class,
    PUCHAR Buffer,
    PUCHAR BufferEnd
    );

INT
ClpDnsIsSameName (
    PCHAR Name1,
    PCHAR Name2
    );

INT
ClpDnsMakeNameCanonical (
    PCHAR Source,
    PCHAR Destination,
    UINTN DestinationSize
    );

INT
ClpDnsIsNameServer (
    res_state State,
    struct sockaddr_in6 *Address
    );

INT
ClpDnsCompressName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    );

INT
ClpDnsDecompressName (
    PUCHAR Message,
    PUCHAR MessageEnd,
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    );

INT
ClpDnsPackName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    );

INT
ClpDnsUnpackName (
    PUCHAR Message,
    PUCHAR MessageEnd,
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    );

INT
ClpDnsEncodeName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    );

INT
ClpDnsDecodeName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    );

INT
ClpDnsFindName (
    PUCHAR Domain,
    PUCHAR Message,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    );

INT
ClpDnsSkipName (
    PUCHAR *Name,
    PUCHAR MessageEnd
    );

INT
ClpCompareIp4Addresses (
    struct sockaddr_in *Address1,
    struct sockaddr_in *Address2
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the resolver state, somewhat accessible by applications.
//

LIBC_API struct __res_state _res;

//
// Store the local domains.
//

int ClDnsSearch;
PSTR ClDnsDomains[DNS_DOMAIN_COUNT];

//
// ------------------------------------------------------------------ Functions
//

//
// TODO: Handle locking on resolver functions when threading is implemented.
//

LIBC_API
int
res_init (
    void
    )

/*++

Routine Description:

    This routine initializes the global resolver state.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information.

--*/

{

    return res_ninit(&_res);
}

LIBC_API
int
res_search (
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    )

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

{

    return res_nsearch(&_res, DomainName, Class, Type, Answer, AnswerLength);
}

LIBC_API
int
res_query (
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    )

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

{

    return res_nquery(&_res, DomainName, Class, Type, Answer, AnswerLength);
}

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
    )

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

{

    int Result;

    Result = res_nmkquery(&_res,
                          Op,
                          DomainName,
                          Class,
                          Type,
                          Data,
                          DataLength,
                          NewRecord,
                          Buffer,
                          BufferLength);

    return Result;
}

LIBC_API
int
res_send (
    const u_char *Message,
    int MessageLength,
    u_char *Answer,
    int AnswerLength
    )

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

{

    return res_nsend(&_res, Message, MessageLength, Answer, AnswerLength);
}

LIBC_API
void
res_close (
    void
    )

/*++

Routine Description:

    This routine closes the socket for the global resolver state.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return res_nclose(&_res);
}

//
// These resolver interface functions operate on a state pointer passed in,
// rather than a global object.
//

LIBC_API
int
res_ninit (
    res_state State
    )

/*++

Routine Description:

    This routine initializes the resolver state.

Arguments:

    State - Supplies the state to initialize, a pointer type.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information.

--*/

{

    int Result;

    State->nscount = 0;
    Result = ClpDnsReadStartFiles(State);
    if (Result < 0) {
        return -1;
    }

    if ((State->options & RES_INIT) == 0) {
        State->retry = 1;
        State->retrans = RES_TIMEOUT;
        State->options |= RES_INIT;
        State->_sock = -1;
    }

    return Result;
}

LIBC_API
int
res_nsearch (
    res_state State,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    )

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

{

    //
    // For now, this is the same as res_nquery.
    //

    return res_nquery(State, DomainName, Class, Type, Answer, AnswerLength);
}

LIBC_API
int
res_nquery (
    res_state State,
    const char *DomainName,
    int Class,
    int Type,
    u_char *Answer,
    int AnswerLength
    )

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

{

    UCHAR Buffer[DNS_QUERY_MAX];
    PDNS_HEADER Header;
    UINT ResponseCode;
    int Result;
    UINT Size;

    Header = (PDNS_HEADER)Answer;
    Header->Flags = 0;
    Result = res_nmkquery(State,
                          QUERY,
                          DomainName,
                          Class,
                          Type,
                          NULL,
                          0,
                          NULL,
                          Buffer,
                          sizeof(Buffer));

    if (Result < 0) {
        return Result;
    }

    Size = Result;
    Result = res_nsend(State, Buffer, Size, Answer, AnswerLength);
    if (Result < 0) {
        return Result;
    }

    ResponseCode = (Header->Flags >> DNS_HEADER_RESPONSE_SHIFT) &
                   DNS_HEADER_RESPONSE_MASK;

    if ((ResponseCode != DNS_HEADER_RESPONSE_SUCCESS) ||
        (ntohs(Header->AnswerCount) == 0)) {

        if (ResponseCode == DNS_HEADER_RESPONSE_NAME_ERROR) {
            errno = ENOENT;
            return -1;

        } else if (ResponseCode == DNS_HEADER_RESPONSE_SERVER_FAILURE) {
            errno = EAGAIN;
            return -1;

        } else if (ResponseCode == DNS_HEADER_RESPONSE_SUCCESS) {
            errno = ENODATA;
            return -1;

        } else {
            errno = ENOTRECOVERABLE;
            return -1;
        }
    }

    Size = Result;
    return Size;
}

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
    )

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

{

    PUCHAR CurrentByte;
    DNS_HEADER DnsHeader;
    UINTN NameLength;
    UCHAR Packet[DNS_QUERY_MAX];
    PSTR Search;
    UINTN Size;

    if ((State->options & RES_INIT) == 0) {
        if (res_ninit(State) < 0) {
            return -1;
        }
    }

    memset(&DnsHeader, 0, sizeof(DNS_HEADER));
    DnsHeader.Identifier = time(NULL) ^ rand() ^ getpid();
    DnsHeader.Flags = (Op << DNS_HEADER_OPCODE_SHIFT);
    if ((State->options & RES_RECURSE) != 0) {
        DnsHeader.Flags |= DNS_HEADER_FLAG_RECURSION_DESIRED;
    }

    DnsHeader.QuestionCount = htons(1);
    memcpy(Packet, &DnsHeader, sizeof(DNS_HEADER));
    CurrentByte = Packet + sizeof(DNS_HEADER);
    NameLength = 0;
    while (*DomainName != '\0') {

        //
        // Skip dots.
        //

        while (*DomainName == '.') {
            DomainName += 1;
        }

        //
        // Find the next dot.
        //

        Search = (PSTR)DomainName;
        while ((*Search != '\0') && (*Search != '.')) {
            Search += 1;
        }

        if (Search - DomainName > DNS_COMPONENT_MAX) {
            return -1;
        }

        Size = (UINTN)(Search - DomainName);
        *CurrentByte = Size;
        if (Size == 0) {
            break;
        }

        NameLength += *CurrentByte + 1;
        if (NameLength >= DNS_MAX_NAME) {
            return -1;
        }

        CurrentByte += 1;
        memcpy(CurrentByte, DomainName, Size);
        CurrentByte += Size;
        if (*Search == '\0') {
            *CurrentByte = '\0';
            break;
        }

        DomainName = Search;
    }

    //
    // Terminate the name.
    //

    CurrentByte += 1;
    *CurrentByte = '\0';

    //
    // Add the type and class.
    //

    CurrentByte += 1;
    *CurrentByte = Type;
    CurrentByte += 1;
    *CurrentByte = 0;
    CurrentByte += 1;
    *CurrentByte = Class;
    CurrentByte += 1;

    //
    // If the generated packet is too big, fail.
    //

    Size = (UINTN)(CurrentByte - Packet);
    if (Size > BufferLength) {
        return -1;
    }

    //
    // Copy the packet over and return.
    //

    memcpy(Buffer, Packet, Size);
    return Size;
}

LIBC_API
int
res_nsend (
    res_state State,
    const u_char *Message,
    int MessageLength,
    u_char *Answer,
    int AnswerLength
    )

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

{

    res_sendhookact Action;
    PDNS_HEADER AnswerHeader;
    UINTN BadNameServer;
    ssize_t BytesSent;
    BOOL ConnectionReset;
    PVOID CurrentBuffer;
    time_t CurrentTime;
    fd_set DescriptorMask;
    BOOL Done;
    INT Error;
    time_t Finish;
    struct sockaddr_in From;
    socklen_t FromLength;
    BOOL GotSomewhere;
    int HighestDescriptor;
    struct iovec IoVector[2];
    struct sockaddr_in Ip4Address;
    CHAR Junk[DNS_QUERY_MAX];
    ssize_t Length;
    INT Loops;
    int NewSocket;
    PDNS_HEADER QueryHeader;
    UCHAR ResponseCode;
    int ResponseLength;
    int Result;
    INT Seconds;
    struct sockaddr_in *ServerAddress;
    INTN ServerIndex;
    socklen_t SocketSize;
    time_t Start;
    UINTN Timeout;
    struct timeval TimeValue;
    BOOL Truncated;
    UINTN Try;
    BOOL VirtualCircuit;

    if ((State->options & RES_INIT) == 0) {
        if (res_ninit(State) < 0) {
            return -1;
        }
    }

    if (AnswerLength < sizeof(DNS_HEADER)) {
        errno = EINVAL;
        return -1;
    }

    VirtualCircuit = FALSE;
    if (((State->options & RES_USEVC) != 0) ||
        (MessageLength > DNS_QUERY_MAX)) {

        VirtualCircuit = TRUE;
    }

    AnswerHeader = (PDNS_HEADER)Answer;
    BadNameServer = 0;
    ConnectionReset = FALSE;
    Error = ETIMEDOUT;
    GotSomewhere = FALSE;
    HighestDescriptor = FD_SETSIZE - 1;
    QueryHeader = (PDNS_HEADER)Message;
    ResponseLength = 0;

    //
    // Rotate through name servers if desired.
    //

    if ((State->nscount > 0) && ((State->options & RES_ROTATE) != 0)) {
        Ip4Address = State->nsaddr_list[0];
        for (ServerIndex = 0;
             ServerIndex < State->nscount - 1;
             ServerIndex += 1) {

            State->nsaddr_list[ServerIndex] =
                                           State->nsaddr_list[ServerIndex + 1];
        }

        State->nsaddr_list[ServerIndex] = Ip4Address;
    }

    //
    // Loop trying to send a request and get a response.
    //

    for (Try = 0; Try < State->retry; Try += 1) {

        //
        // Loop over each name server in the list.
        //

        for (ServerIndex = 0; ServerIndex < State->nscount; ServerIndex += 1) {
            ServerAddress = &(State->nsaddr_list[ServerIndex]);
            if ((BadNameServer & (1 << ServerIndex)) != 0) {
                res_nclose(State);
                continue;
            }

            //
            // Call the query hook if it's set.
            //

            if (State->qhook != NULL) {
                Done = FALSE;
                Loops = 0;
                do {
                    Action = State->qhook(&ServerAddress,
                                          &Message,
                                          &MessageLength,
                                          Answer,
                                          AnswerLength,
                                          &ResponseLength);

                    switch (Action) {
                    case res_goahead:
                        Done = TRUE;
                        break;

                    case res_nextns:
                        res_nclose(State);
                        Done = TRUE;
                        break;

                    case res_done:
                        return ResponseLength;

                    case res_modified:
                        if (Loops < DNS_MAX_HOOK_CALLS) {
                            break;
                        }

                        //
                        // Fall through.
                        //

                    case res_error:
                    default:
                        return -EINVAL;
                    }

                } while (Done == FALSE);

                if (Action == res_nextns) {
                    continue;
                }
            }

            if ((State->options & RES_DEBUG) != 0) {
                fprintf(stderr,
                        "res_send: Querying server %d, try %d.\n",
                        ServerIndex,
                        Try);
            }

            if (VirtualCircuit != FALSE) {

                //
                // Only try once on a virtual circuit.
                //

                Try = State->retry;
                Truncated = FALSE;

                //
                // Ensure this is still the expected connection.
                //

                if ((State->_sock >= 0) &&
                    ((State->_flags & RES_F_VC) != 0)) {

                    SocketSize = sizeof(Ip4Address);
                    Result = getpeername(State->_sock,
                                         (struct sockaddr *)&Ip4Address,
                                         &SocketSize);

                    if ((Result != 0) ||
                        (ClpCompareIp4Addresses(&Ip4Address,
                                                ServerAddress) == 0)) {

                        res_nclose(State);
                        State->_flags &= ~RES_F_VC;
                    }
                }

                //
                // Fire up a connection.
                //

                if ((State->_sock < 0) ||
                    ((State->_flags & RES_F_VC) == 0)) {

                    if (State->_sock >= 0) {
                        res_nclose(State);
                    }

                    State->_sock = socket(PF_INET, SOCK_STREAM, 0);
                    if ((State->_sock < 0) ||
                        (State->_sock > HighestDescriptor)) {

                        Error = errno;
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: Failed to open socket");
                        }

                        return -Error;
                    }

                    errno = 0;
                    Result = connect(State->_sock,
                                     (struct sockaddr *)ServerAddress,
                                     sizeof(*ServerAddress));

                    if (Result < 0) {
                        Error = errno;
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: Failed to connect");
                        }

                        BadNameServer |= (1 << ServerIndex);
                        res_nclose(State);
                        continue;
                    }

                    State->_flags |= RES_F_VC;
                }

                //
                // Write out the request.
                //

                IoVector[0].iov_base = &MessageLength;
                IoVector[0].iov_len = INT16SZ;
                IoVector[1].iov_base = (void *)Message;
                IoVector[1].iov_len = MessageLength;
                if (writev(State->_sock, IoVector, 2) !=
                    (MessageLength + INT16SZ)) {

                    Error = errno;
                    if ((State->options & RES_DEBUG) != 0) {
                        perror("res_send: Failed to writev");
                    }

                    BadNameServer |= 1 << ServerIndex;
                    res_nclose(State);
                    continue;
                }

                //
                // Read the length and response.
                //

                Length = INT16SZ;
                ResponseLength = 0;
                CurrentBuffer = &ResponseLength;
                BytesSent = 0;
                while (Length != 0) {
                    do {
                        BytesSent = read(State->_sock, CurrentBuffer, Length);

                    } while ((BytesSent < 0) && (errno == EINTR));

                    if (BytesSent <= 0) {
                        break;
                    }

                    CurrentBuffer += BytesSent;
                    Length -= BytesSent;
                }

                if (BytesSent <= 0) {
                    Error = errno;
                    if ((State->options & RES_DEBUG) != 0) {
                        perror("res_send: Failed to read");
                    }

                    res_nclose(State);

                    //
                    // Give one retry a shot.
                    //

                    if ((errno == ECONNREFUSED) && (ConnectionReset == FALSE)) {
                        ConnectionReset = TRUE;
                        res_nclose(State);
                        ServerIndex -= 1;
                        continue;
                    }
                }

                Length = ResponseLength;
                if (ResponseLength > AnswerLength) {
                    if ((State->options & RES_DEBUG) != 0) {
                        fprintf(stderr, "res_send: Response truncated.\n");
                    }

                    Truncated = TRUE;
                    Length = AnswerLength;
                }

                //
                // Handle an undersized message.
                //

                if (Length < sizeof(DNS_HEADER)) {
                    if ((State->options & RES_DEBUG) != 0) {
                        fprintf(stderr, "res_send: Undersized response.\n");
                    }

                    Error = ENOSPC;
                    BadNameServer |= (1 << ServerIndex);
                    res_nclose(State);
                    continue;
                }

                CurrentBuffer = Answer;
                while (Length != 0) {
                    do {
                        BytesSent = read(State->_sock, CurrentBuffer, Length);

                    } while ((BytesSent < 0) && (errno == EINTR));

                    if (BytesSent <= 0) {
                        break;
                    }

                    CurrentBuffer += BytesSent;
                    Length -= BytesSent;
                }

                if (BytesSent <= 0) {
                    Error = errno;
                    if ((State->options & RES_DEBUG) != 0) {
                        perror("res_send: Failed to read");
                    }

                    res_nclose(State);
                    continue;
                }

                //
                // Flush out the rest of the answer if the response was
                // truncated so things don't get out of sync.
                //

                if (Truncated != FALSE) {
                    AnswerHeader->Flags |= DNS_HEADER_FLAG_TRUNCATION;
                    Length = ResponseLength - AnswerLength;
                    while (Length != 0) {
                        BytesSent = Length;
                        if (BytesSent > sizeof(Junk)) {
                            BytesSent = sizeof(Junk);
                        }

                        do {
                            BytesSent = read(State->_sock, Junk, BytesSent);

                        } while ((BytesSent < 0) && (errno == EINTR));

                        if (BytesSent > 0) {
                            Length -= BytesSent;

                        } else {
                            break;
                        }
                    }
                }

                //
                // Validate the response ID.
                //

                if (AnswerHeader->Identifier != QueryHeader->Identifier) {
                    if ((State->options & RES_DEBUG) != 0) {
                        fprintf(stderr, "res_send: Unexpected response.\n");
                    }

                    continue;
                }

            //
            // This is not a virtual circuit, use datagrams.
            //

            } else {

                //
                // Create a socket if there is none.
                //

                if ((State->_sock < 0) ||
                    ((State->_flags & RES_F_VC) != 0)) {

                    if ((State->_flags & RES_F_VC) != 0) {
                        res_nclose(State);
                    }

                    State->_sock = socket(PF_INET, SOCK_DGRAM, 0);
                    if ((State->_sock < 0) ||
                        (State->_sock > HighestDescriptor)) {

                        Error = errno;
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: Failed to create socket");
                        }

                        return -Error;
                    }

                    State->_flags &= ~RES_F_CONN;
                }

                if ((State->nscount == 1) ||
                    ((Try == 0) && (ServerIndex == 0))) {

                    //
                    // Only connect if there's no possibility of receiving a
                    // response from another server.
                    //

                    if ((State->_flags & RES_F_CONN) == 0) {
                        Result = connect(State->_sock,
                                         (struct sockaddr *)ServerAddress,
                                         sizeof(*ServerAddress));

                        if (Result != 0) {
                            if ((State->options & RES_DEBUG) != 0) {
                                perror("res_send: Failed to connect");
                            }

                            BadNameServer |= 1 << ServerIndex;
                            res_nclose(State);
                            continue;
                        }

                        State->_flags |= RES_F_CONN;
                    }

                    //
                    // Fire off the request.
                    //

                    BytesSent = send(State->_sock,
                                     (const char *)Message,
                                     MessageLength,
                                     0);

                    if (BytesSent != MessageLength) {
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: Failed to send");
                        }

                        BadNameServer |= 1 << ServerIndex;
                        res_nclose(State);
                        continue;
                    }

                } else {

                    //
                    // Disconnect if trying to receive to responses from
                    // multiple servers.
                    //

                    if ((State->_flags & RES_F_CONN) != 0) {
                        SocketSize = sizeof(Ip4Address);
                        NewSocket = socket(PF_INET, SOCK_DGRAM, 0);
                        Result = getsockname(State->_sock,
                                             (struct sockaddr *)&Ip4Address,
                                             &SocketSize);

                        if (NewSocket < 0) {
                            Error = errno;
                            if ((State->options & RES_DEBUG) != 0) {
                                perror("res_send: Failed to create socket");
                            }

                            return -Error;
                        }

                        dup2(NewSocket, State->_sock);
                        close(NewSocket);
                        if (Result == 0) {

                            //
                            // Re-bind to the original port.
                            //

                            Ip4Address.sin_addr.s_addr = htonl(0);
                            bind(State->_sock,
                                 (struct sockaddr *)&Ip4Address,
                                 SocketSize);
                        }

                        State->_flags &= ~RES_F_CONN;
                        errno = 0;
                    }

                    //
                    // Fire off the request.
                    //

                    Result = sendto(State->_sock,
                                    Message,
                                    MessageLength,
                                    0,
                                    (struct sockaddr *)ServerAddress,
                                    sizeof(*ServerAddress));

                    if (Result != MessageLength) {
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: Failed to send");
                        }

                        BadNameServer |= 1 << ServerIndex;
                        res_nclose(State);
                        continue;
                    }
                }

                if ((State->_sock < 0) || (State->_sock > HighestDescriptor)) {
                    if ((State->options & RES_DEBUG) != 0) {
                        perror("res_send: Invalid socket");
                    }

                    res_nclose(State);
                    continue;
                }

                //
                // Wait for a reply.
                //

                Seconds = State->retry << Try;
                if (Try > 0) {
                    Seconds /= State->nscount;
                }

                if (Seconds == 0) {
                    Seconds = 1;
                }

                time(&CurrentTime);
                Start = CurrentTime;
                Timeout = Seconds;
                Finish = Start + Timeout;
                while (TRUE) {
                    FD_ZERO(&DescriptorMask);
                    FD_SET(State->_sock, &DescriptorMask);
                    TimeValue.tv_sec = Timeout;
                    TimeValue.tv_usec = 0;
                    Result = select(State->_sock + 1,
                                    &DescriptorMask,
                                    NULL,
                                    NULL,
                                    &TimeValue);

                    if (Result == 0) {
                        if ((State->options & RES_DEBUG) != 0) {
                            fprintf(stderr, "res_send: DNS Server Timeout\n");
                        }

                        GotSomewhere = TRUE;
                        break;
                    }

                    if (Result < 0) {
                        if (errno == EINTR) {
                            time(&CurrentTime);
                            if (Finish >= CurrentTime) {
                                Timeout = Finish - CurrentTime;
                                continue;
                            }
                        }

                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: select failed");
                        }

                        res_nclose(State);
                        break;
                    }

                    errno = 0;

                    //
                    // Grab that response.
                    //

                    FromLength = sizeof(struct sockaddr_in);
                    ResponseLength = recvfrom(State->_sock,
                                              Answer,
                                              AnswerLength,
                                              0,
                                              (struct sockaddr *)&From,
                                              &FromLength);

                    if (ResponseLength <= 0) {
                        if ((State->options & RES_DEBUG) != 0) {
                            perror("res_send: select failed");
                        }

                        res_nclose(State);
                        Result = -1;
                        break;
                    }

                    GotSomewhere = TRUE;
                    if (ResponseLength < sizeof(DNS_HEADER)) {
                        if ((State->options & RES_DEBUG) != 0) {
                            fprintf(stderr, "res_send: Undersized packet.\n");
                        }

                        Error = ENOSPC;
                        BadNameServer |= 1 << ServerIndex;
                        res_nclose(State);
                        Result = -1;
                        break;
                    }

                    //
                    // Ignore any answers whose IDs don't match.
                    //

                    if (QueryHeader->Identifier != AnswerHeader->Identifier) {
                        if ((State->options & RES_DEBUG) != 0) {
                            fprintf(stderr, "res_send: Ignoring packet.\n");
                        }

                        continue;
                    }

                    //
                    // Ensure the response came from the server.
                    //

                    if ((State->options & RES_INSECURE1) == 0) {
                        Result = ClpDnsIsNameServer(
                                                 State,
                                                 (struct sockaddr_in6 *)&From);

                        if (Result == 0) {
                            if ((State->options & RES_DEBUG) != 0) {
                                fprintf(stderr,
                                        "res_send: Ignoring packet from "
                                        "unknown server.\n");
                            }

                            continue;
                        }
                    }

                    //
                    // Ensure the response matches the query.
                    //

                    if ((State->options & RES_INSECURE2) == 0) {
                        Result = ClpDnsMatchQueries(
                                               (PUCHAR)Message,
                                               (PUCHAR)Message + MessageLength,
                                               Answer,
                                               Answer + AnswerLength);

                        if (Result == 0) {
                            if ((State->options & RES_DEBUG) != 0) {
                                fprintf(stderr,
                                        "res_send: Ignoring packet from "
                                        "mismatched query.\n");
                            }

                            continue;
                        }
                    }

                    break;
                }

                //
                // If a response failed to come in, go to the next name server.
                //

                if (Result <= 0) {
                    continue;
                }

                //
                // See if the server rejected the query.
                //

                ResponseCode = (AnswerHeader->Flags >>
                                DNS_HEADER_RESPONSE_SHIFT) &
                               DNS_HEADER_RESPONSE_MASK;

                if ((ResponseCode == DNS_HEADER_RESPONSE_SERVER_FAILURE) ||
                    (ResponseCode ==
                     DNS_HEADER_RESPONSE_NOT_IMPLEMENTED) ||
                    (ResponseCode == DNS_HEADER_RESPONSE_REFUSED)) {

                    if ((State->options & RES_DEBUG) != 0) {
                        fprintf(stderr,
                                "res_send: Server rejected query: %d.\n",
                                ResponseCode);
                    }

                    BadNameServer |= 1 << ServerIndex;
                    res_nclose(State);
                    if (State->pfcode == 0) {
                        continue;
                    }
                }

                //
                // Handle truncation.
                //

                if (((State->options & RES_IGNTC) == 0) &&
                    ((AnswerHeader->Flags & DNS_HEADER_FLAG_TRUNCATION) != 0)) {

                    if ((State->options & RES_DEBUG) != 0) {
                        fprintf(stderr,
                                "res_send: Response truncated.\n");
                    }

                    //
                    // Get the rest of the answer using TCP on the same server.
                    //

                    VirtualCircuit = TRUE;
                    res_nclose(State);
                    ServerIndex -= 1;
                    continue;
                }
            }

            if ((State->options & RES_DEBUG) != 0) {
                fprintf(stderr, "res_send: Got answer.\n");
            }

            //
            // Potentially close the socket.
            //

            if (((VirtualCircuit != FALSE) &&
                 (((State->options & RES_USEVC) == 0) || (ServerIndex != 0))) ||
                ((State->options & RES_STAYOPEN) == 0)) {

                res_nclose(State);
            }

            //
            // Call the response hook.
            //

            if (State->rhook != NULL) {
                Done = FALSE;
                Loops = 0;
                do {
                    Action = State->rhook(ServerAddress,
                                          Message,
                                          MessageLength,
                                          Answer,
                                          AnswerLength,
                                          &ResponseLength);

                    switch (Action) {
                    case res_goahead:
                    case res_done:
                        Done = TRUE;
                        break;

                    case res_nextns:
                        res_nclose(State);
                        break;

                    case res_modified:
                        if (Loops < DNS_MAX_HOOK_CALLS) {
                            break;
                        }

                        //
                        // Fall through.
                        //

                    case res_error:
                    default:
                        return -EINVAL;
                    }

                } while (Done == FALSE);

                if (Action == res_nextns) {
                    continue;
                }
            }

            return AnswerLength;
        }
    }

    res_nclose(State);
    if (VirtualCircuit == FALSE) {
        if (GotSomewhere == FALSE) {
            Error = ECONNREFUSED;

        } else {
            errno = ETIMEDOUT;
        }
    }

    assert(Error > 0);

    return -Error;
}

LIBC_API
void
res_nclose (
    res_state State
    )

/*++

Routine Description:

    This routine closes the socket for the given resolver state.

Arguments:

    State - Supplies a pointer to the state to close.

Return Value:

    None.

--*/

{

    if (State->_sock >= 0) {
        close(State->_sock);
        State->_sock = -1;
        State->_flags &= ~(RES_F_VC | RES_F_CONN);
    }

    return;
}

LIBC_API
int
dn_expand (
    const u_char *Message,
    const u_char *MessageEnd,
    const u_char *Source,
    u_char *Destination,
    unsigned int DestinationSize
    )

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

{

    int NameSize;

    NameSize = ClpDnsDecompressName((PUCHAR)Message,
                                    (PUCHAR)MessageEnd,
                                    (PUCHAR)Source,
                                    Destination,
                                    DestinationSize);

    if ((NameSize > 0) && (Destination[0] == '.')) {
        Destination[0] = '\0';
    }

    return NameSize;
}

LIBC_API
int
dn_comp (
    const char *Source,
    u_char *Destination,
    unsigned int DestinationSize,
    u_char **DomainNames,
    u_char **LastDomainName
    )

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

{

    int NameSize;

    NameSize = ClpDnsCompressName((PUCHAR)Source,
                                  Destination,
                                  DestinationSize,
                                  DomainNames,
                                  LastDomainName);

    return NameSize;
}

LIBC_API
int
dn_skipname (
    const u_char *Name,
    const u_char *MessageEnd
    )

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

{

    PUCHAR NameEnd;

    NameEnd = (PUCHAR)Name;
    if (ClpDnsSkipName(&NameEnd, (PUCHAR)MessageEnd) < 0) {
        return -1;
    }

    return NameEnd - Name;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpDnsReadStartFiles (
    res_state State
    )

/*++

Routine Description:

    This routine reads the resolver configuration file and sets up the global
    resolver state.

Arguments:

    State - Supplies a pointer to the state to initialize.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    CHAR Buffer[DNS_RESOLVER_CONFIGURATION_MAX];
    PSTR CacheIpAddress;
    PSTR CurrentBuffer;
    int DnsDomainCount;
    PSTR End;
    int File;
    ssize_t Length;
    char OriginalCharacter;
    int Result;
    PSTR Search;

    //
    // If there are already name servers set up, then don't do anything.
    //

    if (State->nscount > 0) {
        return 0;
    }

    CacheIpAddress = getenv(DNS_DNSCACHEIP_VARIABLE);
    ClDnsSearch = 0;
    if (CacheIpAddress != NULL) {
        if (ClpDnsParseSocketAddress(CacheIpAddress, State->nsaddr_list) != 0) {
            State->nscount += 1;
        }
    }

    State->options = RES_RECURSE;
    File = open(_PATH_RESCONF, O_RDONLY);
    if (File < 0) {
        goto DnsReadStartFilesEnd;
    }

    do {
        Length = read(File, Buffer, DNS_RESOLVER_CONFIGURATION_MAX);

    } while ((Length < 0) && (errno == EINTR));

    close(File);
    DnsDomainCount = sizeof(ClDnsDomains) / sizeof(ClDnsDomains[0]);
    CurrentBuffer = Buffer;
    End = Buffer + Length;
    while (CurrentBuffer < End) {
        if (strncmp(CurrentBuffer, "nameserver", 10) == 0) {
            CurrentBuffer += 10;

            //
            // Loop through every name server listed on the line.
            //

            while ((CurrentBuffer < End) && (*CurrentBuffer != '\n')) {

                //
                // Get past blank space.
                //

                while ((CurrentBuffer < End) &&
                       (isblank(*CurrentBuffer) != 0)) {

                    CurrentBuffer += 1;
                }

                Search = CurrentBuffer;
                while ((Search < End) && (isspace(*Search) == 0)) {
                    Search += 1;
                }

                if (Search >= End) {
                    break;
                }

                OriginalCharacter = *Search;
                *Search = '\0';
                Result = ClpDnsParseSocketAddress(
                                        CurrentBuffer,
                                        &(State->nsaddr_list[State->nscount]));

                if (Result != 0) {
                    if (State->nscount < MAXNS) {
                        State->nscount += 1;
                    }
                }

                *Search = OriginalCharacter;
                CurrentBuffer = Search;
            }

        } else if (((strncmp(CurrentBuffer, "search", 6) == 0) ||
                    (strncmp(CurrentBuffer, "domain", 6) == 0)) &&
                   (ClDnsSearch < DnsDomainCount)) {

            CurrentBuffer += 6;

            //
            // Loop through all search or domain entries on this line.
            //

            while ((CurrentBuffer < End) && (*CurrentBuffer != '\n')) {

                //
                // Get past blank space or commas.
                //

                while ((CurrentBuffer < End) &&
                       ((*CurrentBuffer == ',') ||
                        (isblank(*CurrentBuffer) != 0))) {

                    CurrentBuffer += 1;
                }

                ClDnsDomains[ClDnsSearch] = CurrentBuffer;
                while ((CurrentBuffer < End) &&
                       ((*CurrentBuffer == '.') ||
                        (*CurrentBuffer == '-') ||
                        (isalnum(*CurrentBuffer) != 0))) {

                    CurrentBuffer += 1;
                }

                OriginalCharacter = *CurrentBuffer;
                if (CurrentBuffer < End) {
                    *CurrentBuffer = '\0';
                }

                if (ClDnsDomains[ClDnsSearch] < CurrentBuffer) {
                    ClDnsDomains[ClDnsSearch] =
                                             strdup(ClDnsDomains[ClDnsSearch]);

                    if (ClDnsDomains[ClDnsSearch] != NULL) {
                        ClDnsSearch += 1;
                    }
                }

                if (CurrentBuffer < End) {
                    *CurrentBuffer = OriginalCharacter;
                }
            }

            continue;
        }

        //
        // Scan past the rest of the line, and any newlines.
        //

        while ((CurrentBuffer < End) && (*CurrentBuffer != '\n')) {
            CurrentBuffer += 1;
        }

        while ((CurrentBuffer < End) && (*CurrentBuffer == '\n')) {
            CurrentBuffer += 1;
        }
    }

DnsReadStartFilesEnd:

    //
    // Add DNS servers from the network link configuration itself.
    //

    Result = ClpDnsAddConfiguredServers(State, NetDomainIp4);
    if (Result != 0) {
        errno = Result;
        return -1;
    }

    return 0;
}

int
ClpDnsParseSocketAddress (
    PSTR Address,
    PVOID SocketAddress
    )

/*++

Routine Description:

    This routine attempts to convert an address string into a socket address.

Arguments:

    Address - Supplies the address string to convert.

    SocketAddress - Supplies a pointer where the socket address will be
        returned on success (either sockaddr_in or sockaddr_in6).

Return Value:

    Non-zero on success.

    Zero on failure.

--*/

{

    struct sockaddr_in Ip4Address;
    struct sockaddr_in6 Ip6Address;
    int Result;

    Result = 0;
    memset(&Ip4Address, 0, sizeof(Ip4Address));
    if (inet_pton(AF_INET, Address, &(Ip4Address.sin_addr)) != 0) {
        Ip4Address.sin_port = htons(DNS_PORT_NUMBER);
        Ip4Address.sin_family = AF_INET;
        memcpy(SocketAddress, &Ip4Address, sizeof(struct sockaddr_in));
        Result = 1;

    } else {
        memset(&Ip6Address, 0, sizeof(Ip6Address));
        Result = inet_pton(AF_INET6, Address, &Ip6Address);
        if (Result != 0) {
            Ip6Address.sin6_port = htons(DNS_PORT_NUMBER);
            Ip6Address.sin6_family = AF_INET6;
            memcpy(SocketAddress, &Ip6Address, sizeof(struct sockaddr_in6));
        }
    }

    return Result;
}

INT
ClpDnsAddConfiguredServers (
    res_state State,
    NET_DOMAIN_TYPE Domain
    )

/*++

Routine Description:

    This routine gets the known DNS server addresses from the system.

Arguments:

    State - Supplies the state pointer to add the servers to.

    Domain - Supplies the network domain to get DNS servers for.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL AddedOne;
    socklen_t AddressLength;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    DEVICE_INFORMATION_RESULT *Devices;
    NETWORK_DEVICE_INFORMATION Information;
    PVOID NewBuffer;
    INT Result;
    NET_DOMAIN_TYPE ServerDomain;
    ULONG ServerIndex;
    UINTN Size;
    KSTATUS Status;

    Devices = NULL;
    if (State->nscount == MAXNS) {
        Status = STATUS_SUCCESS;
        goto DnsAddConfiguredServersEnd;
    }

    //
    // Get the array of devices that return network device information.
    //

    DeviceCount = NETWORK_DEVICE_COUNT_ESTIMATE;
    Devices = malloc(sizeof(DEVICE_INFORMATION_RESULT) * DeviceCount);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DnsAddConfiguredServersEnd;
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
                goto DnsAddConfiguredServersEnd;
            }

            NewBuffer = Devices;
            Status = OsLocateDeviceInformation(&ClNetworkDeviceInformationUuid,
                                               NULL,
                                               Devices,
                                               &DeviceCount);

            if (!KSUCCESS(Status)) {
                goto DnsAddConfiguredServersEnd;
            }

        } else {
            goto DnsAddConfiguredServersEnd;
        }
    }

    if (DeviceCount == 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto DnsAddConfiguredServersEnd;
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
            ServerDomain = Information.DnsServers[ServerIndex].Domain;

            //
            // TODO: Support IPv6 address in __res_state._u._ext.nsaddrs.
            //

            if ((State->nscount < MAXNS) &&
                (ServerDomain == NetDomainIp4)) {

                AddressLength = sizeof(struct sockaddr_in);
                Status = ClConvertFromNetworkAddress(
                      &(Information.DnsServers[ServerIndex]),
                      (struct sockaddr *)&(State->nsaddr_list[State->nscount]),
                      &AddressLength,
                      NULL,
                      0);

                if (KSUCCESS(Status)) {
                    AddedOne = TRUE;
                    State->nscount += 1;
                }

                continue;
            }
        }
    }

    if (AddedOne == FALSE) {
        Status = STATUS_NOT_FOUND;
        goto DnsAddConfiguredServersEnd;
    }

    Status = STATUS_SUCCESS;

DnsAddConfiguredServersEnd:
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
ClpDnsMatchQueries (
    PUCHAR Buffer1,
    PUCHAR Buffer1End,
    PUCHAR Buffer2,
    PUCHAR Buffer2End
    )

/*++

Routine Description:

    This routine determines if two DNS queries match each other.

Arguments:

    Buffer1 - Supplies a pointer to the first DNS query.

    Buffer1End - Supplies one beyond the last valid byte of the first DNS query.

    Buffer2 - Supplies a pointer to the second DNS query.

    Buffer2End - Supplies one beyond the last valid byte of the second DNS
        query.

Return Value:

    -1 on error.

    0 if the queries do not match.

    1 if the queries match.

--*/

{

    INT Class;
    PUCHAR CurrentPointer;
    PDNS_HEADER Header1;
    PDNS_HEADER Header2;
    UCHAR Name[DNS_MAX_NAME];
    int NameSize;
    UINT Op1;
    UINT Op2;
    INT QuestionCount;
    INT Type;

    Header1 = (PDNS_HEADER)Buffer1;
    Header2 = (PDNS_HEADER)Buffer2;
    CurrentPointer = Buffer1 + sizeof(DNS_HEADER);
    if ((Buffer1 + sizeof(DNS_HEADER) > Buffer1End) ||
        (Buffer2 + sizeof(DNS_HEADER) > Buffer2End)) {

        return -1;
    }

    QuestionCount = ntohs(Header1->QuestionCount);
    Op1 = (Header1->Flags >> DNS_HEADER_OPCODE_SHIFT) & DNS_HEADER_OPCODE_MASK;
    Op2 = (Header2->Flags >> DNS_HEADER_OPCODE_SHIFT) & DNS_HEADER_OPCODE_MASK;
    if ((Op1 == DNS_HEADER_OPCODE_UPDATE) &&
        (Op2 == DNS_HEADER_OPCODE_UPDATE)) {

        return 1;
    }

    if (ntohs(Header2->QuestionCount) != QuestionCount) {
        return 0;
    }

    while (QuestionCount > 0) {
        QuestionCount -= 1;
        NameSize = dn_expand(Buffer1,
                             Buffer1End,
                             CurrentPointer,
                             Name,
                             sizeof(Name));

        if (NameSize < 0) {
            return -1;
        }

        CurrentPointer += NameSize;
        if (CurrentPointer + (2 * INT16SZ) > Buffer1End) {
            return -1;
        }

        Type = READ_UNALIGNED16(CurrentPointer);
        CurrentPointer += INT16SZ;
        Class = READ_UNALIGNED16(CurrentPointer);
        CurrentPointer += INT16SZ;
        if (ClpDnsIsNameInQuery(Name, Type, Class, Buffer2, Buffer2End) == 0) {
            return 0;
        }
    }

    return 1;
}

INT
ClpDnsIsNameInQuery (
    PUCHAR Name,
    INT Type,
    INT Class,
    PUCHAR Buffer,
    PUCHAR BufferEnd
    )

/*++

Routine Description:

    This routine determines if the given name, type, and class are located in
    the query section of the given packet.

Arguments:

    Name - Supplies a pointer to the name to look for.

    Type - Supplies the type to look for.

    Class - Supplies the class to look for.

    Buffer - Supplies a pointer to the DNS query.

    BufferEnd - Supplies one beyond the last valid byte of the DNS query.

Return Value:

    -1 on error.

    0 if the name was not found.

    1 if the name was found.

--*/

{

    PUCHAR CurrentPointer;
    PDNS_HEADER Header;
    INT PacketClass;
    UCHAR PacketName[DNS_MAX_NAME];
    INT PacketNameSize;
    INT PacketType;
    INT QuestionCount;

    Header = (PDNS_HEADER)Buffer;
    CurrentPointer = Buffer + sizeof(DNS_HEADER);
    QuestionCount = ntohs(Header->QuestionCount);
    while (QuestionCount > 0) {
        QuestionCount -= 1;
        PacketNameSize = dn_expand(Buffer,
                                   BufferEnd,
                                   CurrentPointer,
                                   PacketName,
                                   sizeof(PacketName));

        if (PacketNameSize < 0) {
            return -1;
        }

        CurrentPointer += PacketNameSize;
        if (CurrentPointer + (2 * INT16SZ) > BufferEnd) {
            return -1;
        }

        PacketType = READ_UNALIGNED16(CurrentPointer);
        CurrentPointer += INT16SZ;
        PacketClass = READ_UNALIGNED16(CurrentPointer);
        CurrentPointer += INT16SZ;
        if ((PacketType == Type) && (PacketClass == Class) &&
            (ClpDnsIsSameName((PSTR)PacketName, (PSTR)Name) == 1)) {

            return 1;
        }
    }

    return 0;
}

INT
ClpDnsIsSameName (
    PCHAR Name1,
    PCHAR Name2
    )

/*++

Routine Description:

    This routine determines if the two domain names are the same.

Arguments:

    Name1 - Supplies a pointer to the first name.

    Name2 - Supplies a pointer to the second name.

Return Value:

    -1 on error.

    0 if the names are not the same.

    1 if the names are the same.

--*/

{

    CHAR CanonicalName1[DNS_MAX_NAME];
    CHAR CanonicalName2[DNS_MAX_NAME];
    INT Result;

    Result = ClpDnsMakeNameCanonical(Name1,
                                     CanonicalName1,
                                     sizeof(CanonicalName1));

    if (Result < 0) {
        return Result;
    }

    Result = ClpDnsMakeNameCanonical(Name2,
                                     CanonicalName2,
                                     sizeof(CanonicalName2));

    if (Result < 0) {
        return Result;
    }

    if (strcasecmp(CanonicalName1, CanonicalName2) == 0) {
        return 1;
    }

    return 0;
}

INT
ClpDnsMakeNameCanonical (
    PCHAR Source,
    PCHAR Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine makes a canonical copy of the given domain name, removing
    extra dots but making sure a dot is at the end.

Arguments:

    Source - Supplies a pointer to the name to canonicalize.

    Destination - Supplies a pointer where the canonicalized name will be
        returned.

    DestinationSize - Supplies the number of bytes in the destination buffer.

Return Value:

    -1 on error.

    0 on success.

--*/

{

    size_t Length;

    Length = strlen(Source);
    if (Length + sizeof(".") > DestinationSize) {
        return -1;
    }

    strcpy(Destination, Source);
    while ((Length > 0) && (Destination[Length - 1] == '.')) {
        if ((Length > 1) && (Destination[Length - 2] == '\\') &&
            ((Length < 2) || (Destination[Length - 3] != '\\'))) {

            break;
        }

        Length -= 1;
        Destination[Length] = '\0';
    }

    Destination[Length] = '.';
    Length += 1;
    Destination[Length] = '\0';
    return 0;
}

INT
ClpDnsIsNameServer (
    res_state State,
    struct sockaddr_in6 *Address
    )

/*++

Routine Description:

    This routine determines if the given address is in the list of name servers.

Arguments:

    State - Supplies the state (pointer) containing the acceptable name
        servers.

    Address - Supplies a pointer to the address to check.

Return Value:

    -1 on error.

    0 on success.

--*/

{

    struct sockaddr_in *Ip4Address;
    struct sockaddr_in *Ip4ServerAddress;
    struct sockaddr_in6 *Ip6ServerAddress;
    int Result;
    UINTN ServerIndex;

    if (Address->sin6_family == AF_INET) {
        Ip4Address = (struct sockaddr_in *)Address;
        for (ServerIndex = 0; ServerIndex < State->nscount; ServerIndex += 1) {
            Ip4ServerAddress = &(State->nsaddr_list[ServerIndex]);
            if ((Ip4ServerAddress->sin_family == Ip4Address->sin_family) &&
                (Ip4ServerAddress->sin_port = Ip4Address->sin_port) &&
                (Ip4ServerAddress->sin_addr.s_addr ==
                 Ip4Address->sin_addr.s_addr)) {

                return 1;
            }
        }

    } else if (Address->sin6_family == AF_INET6) {
        for (ServerIndex = 0; ServerIndex < MAXNS; ServerIndex += 1) {
            Ip6ServerAddress = State->_u._ext.nsaddrs[ServerIndex];
            if ((Ip6ServerAddress != NULL) &&
                (Ip6ServerAddress->sin6_family == AF_INET6) &&
                (Ip6ServerAddress->sin6_port == Address->sin6_port)) {

                //
                // It matches if it's not the ANY address and it matches the
                // server.
                //

                Result = memcmp(&(Ip6ServerAddress->sin6_addr),
                                &in6addr_any,
                                sizeof(struct in6_addr));

                if (Result == 0) {
                    Result = memcmp(&(Ip6ServerAddress->sin6_addr),
                                    &(Address->sin6_addr),
                                    sizeof(struct in6_addr));

                    if (Result != 0) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

INT
ClpDnsCompressName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    )

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

{

    UCHAR Name[DNS_MAX_NAME];
    INT Result;

    if (ClpDnsEncodeName(Source, Name, DNS_MAX_NAME) == -1) {
        return -1;
    }

    Result = ClpDnsPackName(Name,
                            Destination,
                            DestinationSize,
                            DomainNames,
                            LastDomainName);

    return Result;
}

INT
ClpDnsDecompressName (
    PUCHAR Message,
    PUCHAR MessageEnd,
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine expands a compressed name to presentation format.

Arguments:

    Message - Supplies a pointer to the beginning of the DNS query or response.

    MessageEnd - Supplies a pointer one beyond the last valid byte in the DNS
        query or response.

    Source - Supplies a pointer within the message to the name to expand.

    Destination - Supplies a pointer where the decompressed name will be
        returned on success.

    DestinationSize - Supplies the size of the destination buffer in bytes.

Return Value:

    Returns the number of bytes read out of the source buffer, or -1 on error.

--*/

{

    UCHAR Name[DNS_MAX_NAME];
    INT NameSize;
    INT Result;

    NameSize = ClpDnsUnpackName(Message,
                                MessageEnd,
                                Source,
                                Name,
                                sizeof(Name));

    if (NameSize < 0) {
        return -1;
    }

    Result = ClpDnsDecodeName(Name, Destination, DestinationSize);
    if (Result < 0) {
        return -1;
    }

    return NameSize;
}

INT
ClpDnsPackName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    )

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

{

    PUCHAR *CurrentDomain;
    PUCHAR DestinationPointer;
    PUCHAR End;
    PUCHAR *LastDomain;
    INT Length;
    PUCHAR Message;
    INT Result;
    UINT Size;
    PUCHAR SourcePointer;

    CurrentDomain = NULL;
    DestinationPointer = Destination;
    End = Destination + DestinationSize;
    LastDomain = NULL;
    Message = NULL;
    Result = -1;
    SourcePointer = Source;
    if (DomainNames != NULL) {
        Message = DomainNames[0];
        DomainNames += 1;
        if (Message != NULL) {
            CurrentDomain = DomainNames;
            while (*CurrentDomain != NULL) {
                CurrentDomain += 1;
            }

            LastDomain = CurrentDomain;
        }
    }

    //
    // Make sure the domain looks good.
    //

    Length = 0;
    do {
        Size = *SourcePointer;
        if ((Size & DNS_COMPRESSION_MASK) != 0) {
            goto DnsPackNameEnd;
        }

        Length += Size + 1;
        if (Length > MAXCDNAME) {
            goto DnsPackNameEnd;
        }

        SourcePointer += Size + 1;

    } while (Size != 0);

    SourcePointer = Source;
    do {
        Size = *SourcePointer;
        if ((Size != 0) && (Message != NULL)) {
            Length = ClpDnsFindName(SourcePointer,
                                    Message,
                                    DomainNames,
                                    LastDomain);

            if (Length >= 0) {
                if (DestinationPointer + 1 >= End) {
                    goto DnsPackNameEnd;
                }

                *DestinationPointer = (Length >> 8) | DNS_COMPRESSION_VALUE;
                DestinationPointer += 1;
                *DestinationPointer = (Length & 0xFF);
                return DestinationPointer - Destination;
            }

            if ((LastDomainName != NULL) &&
                (CurrentDomain < (LastDomainName - 1))) {

                *CurrentDomain = DestinationPointer;
                CurrentDomain += 1;
                *CurrentDomain = NULL;
            }
        }

        //
        // Copy the label.
        //

        if ((Size & DNS_COMPRESSION_MASK) != 0) {
            goto DnsPackNameEnd;
        }

        if (DestinationPointer + 1 + Size >= End) {
            goto DnsPackNameEnd;
        }

        memcpy(DestinationPointer, SourcePointer, Size + 1);
        SourcePointer += Size + 1;
        DestinationPointer += Size + 1;

    } while (Size != 0);

    if (DestinationPointer > End) {
        if (Message != NULL) {
            *LastDomain = NULL;
            goto DnsPackNameEnd;
        }
    }

    Result = 0;

DnsPackNameEnd:
    if (Result != 0) {
        errno = EMSGSIZE;
    }

    return Result;
}

INT
ClpDnsUnpackName (
    PUCHAR Message,
    PUCHAR MessageEnd,
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine unpacks a name from a source that might be compressed.

Arguments:

    Message - Supplies a pointer to the beginning of the DNS query or response.

    MessageEnd - Supplies a pointer one beyond the last valid byte in the DNS
        query or response.

    Source - Supplies a pointer within the message to the name to expand.

    Destination - Supplies a pointer where the decompressed name will be
        returned on success.

    DestinationSize - Supplies the size of the destination buffer in bytes.

Return Value:

    Returns the number of bytes read out of the source buffer, or -1 on error.

--*/

{

    UINT Byte;
    INT Checked;
    PUCHAR DestinationLimit;
    PUCHAR DestinationPointer;
    INT Length;
    PUCHAR SourcePointer;

    Checked = 0;
    DestinationLimit = Destination + DestinationSize;
    DestinationPointer = Destination;
    Length = -1;
    SourcePointer = Source;
    if ((SourcePointer < Message) || (SourcePointer >= MessageEnd)) {
        errno = EMSGSIZE;
        return -1;
    }

    //
    // Loop getting labels in the domain name.
    //

    while (TRUE) {
        Byte = *SourcePointer;
        SourcePointer += 1;
        if (Byte == '\0') {
            break;
        }

        switch (Byte & DNS_COMPRESSION_MASK) {
        case 0:
            if (((DestinationPointer + Byte + 1) >= DestinationLimit) ||
                (SourcePointer + Byte >= MessageEnd)) {

                errno = EMSGSIZE;
                return -1;
            }

            Checked += Byte + 1;
            *DestinationPointer = Byte;
            DestinationPointer += 1;
            memcpy(DestinationPointer, SourcePointer, Byte);
            DestinationPointer += Byte;
            SourcePointer += Byte;
            break;

        case DNS_COMPRESSION_VALUE:
            if (SourcePointer >= MessageEnd) {
                errno = EMSGSIZE;
                return -1;
            }

            if (Length < 0) {
                Length = SourcePointer - Source + 1;
            }

            SourcePointer = Message +
                            (((Byte & (~DNS_COMPRESSION_MASK)) <<
                              BITS_PER_BYTE) |
                             (*SourcePointer & 0xFF));

            if ((SourcePointer < Message) || (SourcePointer >= MessageEnd)) {
                errno = EMSGSIZE;
                return -1;
            }

            Checked += 2;

            //
            // Check for loops in the compressed name.
            //

            if (Checked >= MessageEnd - Message) {
                errno = EMSGSIZE;
                return -1;
            }

            break;

        default:
            errno = EMSGSIZE;
            return -1;
        }
    }

    *DestinationPointer = '\0';
    if (Length < 0) {
        Length = SourcePointer - Source;
    }

    return Length;
}

INT
ClpDnsEncodeName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine converts an ASCII string into an encoded name.

Arguments:

    Source - Supplies a pointer to the name to encode.

    Destination - Supplies a pointer where the encoded name will be returned on
        success.

    DestinationSize - Supplies the size of the destination buffer in bytes.

Return Value:

    0 if the string was not fully qualified.

    1 if the string was fully qualified.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    INT Character;
    PUCHAR Current;
    PUCHAR End;
    BOOL Escaped;
    PUCHAR Label;
    INT Value;

    Current = Destination;
    Escaped = FALSE;
    End = Destination + DestinationSize;
    Label = Destination + 1;
    while (TRUE) {
        Character = *Source;
        Source += 1;
        if (Character == '\0') {
            break;
        }

        if (Escaped != FALSE) {
            if (isdigit(Character)) {
                Value = (Character - '0') * 100;
                Character = *Source;
                Source += 1;
                if ((Character == '\0') || (!isdigit(Character))) {
                    errno = EMSGSIZE;
                    return -1;
                }

                Value += (Character - '0') * 10;
                Character = *Source;
                Source += 1;
                if ((Character == '\0') || (!isdigit(Character))) {
                    errno = EMSGSIZE;
                    return -1;
                }

                Value += Character - '0';
                if (Value > 0xFF) {
                    errno = EMSGSIZE;
                    return -1;
                }

                Character = Value;
            }

            Escaped = FALSE;

        } else if (Character == '\\') {
            Escaped = TRUE;
            continue;

        } else if (Character == '.') {
            Character = Current - Label - 1;

            //
            // Watch out for the leg being too big (or off the end).
            //

            if (((Character & DNS_COMPRESSION_MASK) != 0) ||
                (Label >= End)) {

                errno = EMSGSIZE;
                return -1;
            }

            *Label = Character;

            //
            // Handle a fully qualified name.
            //

            if (*Source == '\0') {
                if (Character != 0) {
                    if (Current >= End) {
                        errno = EMSGSIZE;
                        return -1;
                    }

                    *Current = '\0';
                    Current += 1;
                }

                if ((Current - Destination) > MAXCDNAME) {
                    errno = EMSGSIZE;
                    return -1;
                }

                return 1;
            }

            if ((Character == 0) || (*Source == '.')) {
                errno = EMSGSIZE;
                return -1;
            }

            Label = Current;
            Current += 1;
            continue;
        }

        if (Current >= End) {
            errno = EMSGSIZE;
            return -1;
        }

        *Current = Character;
        Current += 1;
    }

    Character = Current - Label - 1;
    if (((Character & DNS_COMPRESSION_MASK) != 0) ||
        (Label >= End)) {

        errno = EMSGSIZE;
        return -1;
    }

    if (Label >= End) {
        errno = EMSGSIZE;
        return -1;
    }

    *Label = Character;
    if (Character != 0) {
        if (Current >= End) {
            errno = EMSGSIZE;
            return -1;
        }

        *Current = '\0';
        Current += 1;
    }

    if ((Current - Destination) > MAXCDNAME) {
        errno = EMSGSIZE;
        return -1;
    }

    return 0;
}

INT
ClpDnsDecodeName (
    PUCHAR Source,
    PUCHAR Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine converts an encoded name to a printable ASCII name.

Arguments:

    Source - Supplies a pointer to the name to decode.

    Destination - Supplies a pointer where the decoded name will be returned on
        success.

    DestinationSize - Supplies the size of the destination buffer in bytes.

Return Value:

    Returns the number of bytes written to the buffer, or -1 on error.

--*/

{

    UCHAR Character;
    PUCHAR CurrentPointer;
    PUCHAR DestinationPointer;
    PUCHAR End;
    UINT Size;

    CurrentPointer = Source;
    DestinationPointer = Destination;
    End = Destination + DestinationSize;
    while (TRUE) {
        Size = *CurrentPointer;
        CurrentPointer += 1;
        if (Size == 0) {
            break;
        }

        //
        // The name is supposed to already be decompressed.
        //

        if ((Size & DNS_COMPRESSION_MASK) != 0) {
            errno = EMSGSIZE;
            return -1;
        }

        if (DestinationPointer != Destination) {
            if (DestinationPointer >= End) {
                errno = EMSGSIZE;
                return -1;
            }

            *DestinationPointer = '.';
            DestinationPointer += 1;
        }

        if (DestinationPointer + Size >= End) {
            errno = EMSGSIZE;
            return -1;
        }

        while (Size > 0) {
            Character = *CurrentPointer;
            CurrentPointer += 1;
            if (DNS_SPECIAL_CHARACTER(Character)) {
                if (DestinationPointer + 1 >= End) {
                    errno = EMSGSIZE;
                    return -1;
                }

                *DestinationPointer = '\\';
                DestinationPointer += 1;
                *DestinationPointer = Character;
                DestinationPointer += 1;

            } else if (!DNS_PRINTABLE_CHARACTER(Character)) {
                if (DestinationPointer + 3 >= End) {
                    errno = EMSGSIZE;
                    return -1;
                }

                *DestinationPointer = '\\';
                DestinationPointer += 1;
                *DestinationPointer = '0' + (Character / 100);
                DestinationPointer += 1;
                *DestinationPointer = '0' + ((Character % 100) / 10);
                DestinationPointer += 1;
                *DestinationPointer = '0' + (Character % 10);
                DestinationPointer += 1;

            } else {
                if (DestinationPointer >= End) {
                    errno = EMSGSIZE;
                    return -1;
                }

                *DestinationPointer = Character;
                DestinationPointer += 1;
            }

            Size -= 1;
        }
    }

    if (DestinationPointer == Destination) {
        if (DestinationPointer >= End) {
            errno = EMSGSIZE;
            return -1;
        }

        *DestinationPointer = '.';
        DestinationPointer += 1;
    }

    if (DestinationPointer >= End) {
        errno = EMSGSIZE;
        return -1;
    }

    *DestinationPointer = '\0';
    DestinationPointer += 1;
    return (DestinationPointer - Destination);
}

INT
ClpDnsFindName (
    PUCHAR Domain,
    PUCHAR Message,
    PUCHAR *DomainNames,
    PUCHAR *LastDomainName
    )

/*++

Routine Description:

    This routine attempts to find the counted label name in an array of
    compressed names.

Arguments:

    Domain - Supplies the domain to search for.

    Message - Supplies a pointer to the start of the query or response.

    DomainNames - Supplies an array of compressed names to search.

    LastDomainName - Supplies one beyond the last element in the domain names
        array.

Return Value:

    Returns the offset from the start of the message if found.

    -1 if not found.

--*/

{

    PUCHAR *CurrentDomain;
    PUCHAR CurrentPointer;
    PUCHAR DomainName;
    UINT Size;
    PUCHAR Start;

    CurrentDomain = DomainNames;
    while (CurrentDomain < LastDomainName) {
        DomainName = Domain;
        CurrentPointer = *CurrentDomain;
        Start = *CurrentDomain;
        Size = *CurrentPointer;
        CurrentPointer += 1;
        while (Size != 0) {
            if ((Size & DNS_COMPRESSION_MASK) == 0) {
                if (*DomainName != Size) {
                    break;
                }

                DomainName += 1;
                while (Size > 0) {
                    if (tolower(*DomainName) != tolower(*CurrentPointer)) {
                        break;
                    }

                    DomainName += 1;
                    CurrentPointer += 1;
                    Size -= 1;
                }

                if (Size != 0) {
                    break;
                }

                if ((*DomainName == '\0') && (*CurrentPointer == '\0')) {
                    return Start - Message;
                }

                if (*DomainName == '\0') {
                    break;
                }

            } else if ((Size & DNS_COMPRESSION_MASK) == DNS_COMPRESSION_VALUE) {
                CurrentPointer = Message +
                                 (((Size & (~DNS_COMPRESSION_MASK)) <<
                                   BITS_PER_BYTE) |
                                  *CurrentPointer);

            } else {
                errno = EMSGSIZE;
                return -1;
            }
        }

        CurrentDomain += 1;
    }

    errno = ENOENT;
    return -1;
}

INT
ClpDnsSkipName (
    PUCHAR *Name,
    PUCHAR MessageEnd
    )

/*++

Routine Description:

    This routine skips a compressed DNS name.

Arguments:

    Name - Supplies a pointer that on input points to the name to skip. On
        successful output, this will point after the compressed name.

    MessageEnd - Supplies a pointer to the end of the DNS query or response,
        to avoid buffer overruns.

Return Value:

    0 on success.

    -1 on failure, an errno will be set to contain more information.

--*/

{

    PUCHAR Current;
    UINT Size;

    Current = *Name;
    while (Current <= MessageEnd) {
        Size = *Current;
        Current += 1;
        if (Size == 0) {
            break;
        }

        switch (Size & DNS_COMPRESSION_MASK) {
        case 0:
            Current += Size;
            continue;

        case DNS_COMPRESSION_VALUE:
            Current += 1;
            break;

        default:
            errno = EMSGSIZE;
            return -1;
        }

        break;
    }

    if (Current > MessageEnd) {
        errno = EMSGSIZE;
        return -1;
    }

    *Name = Current;
    return 0;
}

INT
ClpCompareIp4Addresses (
    struct sockaddr_in *Address1,
    struct sockaddr_in *Address2
    )

/*++

Routine Description:

    This routine compares two IPv4 addresses.

Arguments:

    Address1 - Supplies a pointer to the first address.

    Address2 - Supplies a pointer to the second address.

Return Value:

    TRUE if the addresses are the same.

    FALSE if the addresses are different.

--*/

{

    if ((Address1->sin_family == Address2->sin_family) &&
        (Address1->sin_port == Address2->sin_port) &&
        (Address1->sin_addr.s_addr == Address2->sin_addr.s_addr)) {

        return TRUE;
    }

    return FALSE;
}

