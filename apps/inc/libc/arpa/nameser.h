/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    nameser.h

Abstract:

    This header contains Name Server definitions.

Author:

    Evan Green 14-Jan-2015

--*/

#ifndef _ARPA_NAMESER_H
#define _ARPA_NAMESER_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/param.h>
#include <sys/types.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read 16 and 32 bit values from a pointer value. The pointer is
// updated.
//

#define GETSHORT(_Short, _Pointer)                                          \
    u_char *_Pointer8 = (u_char *)(_Pointer);                               \
    (_Short) = ((uint16_t)_Pointer8[0] << 8) | ((uint16_t)_Pointer8[1]);    \
    (_Pointer) += INT16SZ;

#define GETLONG(_Long, _Pointer)                \
    u_char *_Pointer8 = (u_char *)(_Pointer);   \
    (_Long) = ((uint32_t)_Pointer8[0] << 24) |  \
              ((uint32_t)_Pointer8[1] << 16) |  \
              ((uint32_t)_Pointer8[2] << 8) |   \
              ((uint32_t)_Pointer8[3]);         \
    (_Pointer) += INT32SZ;

//
// These macros write 16 and 32 bit values to a pointer, updating the pointer.
//

#define PUTSHORT(_Short, _Pointer)              \
    uint16_t _ShortValue = (uint16_t)(_Short);  \
    u_char *_Pointer8 = (u_char *)(_Pointer);   \
    _Pointer8[0] = _ShortValue >> 8;            \
    _Pointer8[1] = _ShortValue;                 \
    _Pointer += INT16SZ;

#define PUTLONG(_Long, _Pointer)                \
    uint32_t _LongValue = (uint32_t)(_Long);    \
    u_char *_Pointer8 = (u_char *)(_Pointer);   \
    _Pointer8[0] = _LongValue >> 24;            \
    _Pointer8[1] = _LongValue >> 16;            \
    _Pointer8[2] = _LongValue >> 8;             \
    _Pointer8[3] = _LongValue;                  \
    (_Pointer) += INT32SZ;

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the revision information.
//

#define __BIND 19940417

//
// Define constants from RFC883.
//

//
// Maximum packet size
//

#define PACKETSZ 512

//
// Maximum domain name
//

#define MAXDNAME 256

//
// Maximum compressed comain name
//

#define MAXCDNAME 255

//
// Maximum length of a domain label
//

#define MAXLABEL 63

//
// Define the number of bytes of fixed data in the header.
//

#define HFIXEDSZ 12

//
// Define the maximum number of byte of fixed data in the query.
//

#define QFIXEDSZ 4

//
// Define the maximu number of bytes of fixed data in an R record.
//

#define RRFIXEDSZ 10

//
// Define the size of some basic types.
//

#define INT32SZ 4
#define INT16SZ 2
#define INADDRSZ 4

//
// Define the Internet Nameserver port number.
//

#define NAMESERVER_PORT 53

//
// Define opcodes.
//

//
// Standard query
//

#define QUERY 0

//
// Inverse query
//

#define IQUERY 1

//
// Nameserver status query
//

#define STATUS 2

//
// Notify secondary of SOA change
//

#define NS_NOTIFY_OP 4

//
// Define response codes.
//

//
// No error
//

#define NOERROR 0

//
// Format error
//

#define FORMERR 1

//
// Server failure
//

#define SERVFAIL 2

//
// Non-existant domain
//

#define NXDOMAIN 3

//
// Not implemented
//

#define NOTIMP 4

//
// Query refused
//

#define REFUSED 5

//
// Define type values for resources and queries
//

//
// Host address
//

#define T_A 1

//
// Authoritative server
//

#define T_NS 2

//
// Mail destination
//

#define T_MD 3

//
// Mail forwarder
//

#define T_MF 4

//
// Canonical name
//

#define T_CNAME 5

//
// Start of Authority name
//

#define T_SOA 6

//
// Mailbox domain name
//

#define T_MB 7

//
// Mail group member
//

#define T_MG 8

//
// Mail rename name
//

#define T_MR 9

//
// Null resource record
//

#define T_NULL 10

//
// Well known service
//

#define T_WKS 11

//
// Domain name pointer
//

#define T_PTR 12

//
// Host information
//

#define T_HINFO 13

//
// Mailbox information
//

#define T_MINFO 14

//
// Mail routing information
//

#define T_MX 15

//
// Text strings
//

#define T_TXT 16

//
// Responsible person
//

#define T_RP 17

//
// AFS cell database
//

#define T_AFSDB 18

//
// X_25 calling address
//

#define T_X25 19

//
// ISDN calling address
//

#define T_ISDN 20

//
// Router
//

#define T_RT 21

//
// NSAP address
//

#define T_NSAP 22

//
// Reverse NSAP lookup
//

#define T_NSAP_PTR 23

//
// Security signature
//

#define T_SIG 24

//
// Security key
//

#define T_KEY 25

//
// X.400 mail mapping
//

#define T_PX 26

//
// Geographical position (withdrawn)
//

#define T_GPOS 27

//
// IP6 Address
//

#define T_AAAA 28

//
// Location information
//

#define T_LOC 29

//
// User (finger) information
//

#define T_UINFO 100

//
// User ID
//

#define T_UID 101

//
// Group ID
//

#define T_GID 102

//
// Unspecified binary data
//

#define T_UNSPEC 103

//
// Transfer zone of authority
//

#define T_AXFR 252

//
// Transfer mailbox records
//

#define T_MAILB 253

//
// Transfer mail agent records
//

#define T_MAILA 254

//
// Wildcard match
//

#define T_ANY 255

//
// Define values for the class field.
//

//
// The ARPA Internet class
//

#define C_IN 1

//
// The chaos net (MIT)
//

#define C_CHAOS 3

//
// The Hesiod name server (MIT)
//

#define C_HS 4

//
// Wildcard match class
//

#define C_ANY 255

//
// Define status codes for T_UNSPEC conversion routines.
//

#define CONV_SUCCESS    0
#define CONV_OVERFLOW   (-1)
#define CONV_BADFMT     (-2)
#define CONV_BADCKSUM   (-3)
#define CONV_BADBUFLEN  (-4)

//
// Define the value used for handling compress domain names.
//

#define INDIR_MASK 0xC0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the nameserver query header format.

Members:

    id - Stores the query identification number.

    rd - Stores whether or not recursion is desired.

    tc - Stores whether or not this is a truncated message.

    aa - Stores whether or not this is an authoritative answer.

    opcode - Stores the opcode of the message.

    qr - Stores the response flag (1 for response, 0 for query).

    rcode - Stores the response code.

    unused - Stores a reserved field, which must be zero.

    pr - Stores the primary server request flag.

    ra - Stores whether or not recursion is available.

    qdount - Stores the number of question entries.

    ancount - Stores the number of answer entries.

    nscount - Stores the number of name server entries.

    arcount - Stores the number of resource entries.

--*/

typedef struct _HEADER {
    unsigned id:16;
    unsigned rd:1;
    unsigned tc:1;
    unsigned aa:1;
    unsigned opcode:4;
    unsigned qr:1;
    unsigned rcode:4;
    unsigned unused:2;
    unsigned pr:1;
    unsigned ra:1;
    unsigned qdcount:16;
    unsigned ancount:16;
    unsigned nscount:16;
    unsigned arcount:16;
} HEADER;

/*++

Structure Description:

    This structure defines a convenient structure for a nameserver resource
    record.

Members:

    r_zone - Stores the zone number.

    r_class - Stores the class number.

    r_type - Stores the type number.

    r_ttl - Stores the time to live.

    r_size - Stores the size of the data area.

    r_data - Stores a pointer to the data contents.

--*/

struct rrec {
    int16_t r_zone;
    int16_t r_class;
    int16_t r_type;
    uint32_t r_ttl;
    int r_size;
    char *r_data;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

