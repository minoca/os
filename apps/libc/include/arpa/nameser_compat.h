/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    nameser_compat.h

Abstract:

    This header contains (older) Name Server definitions.

Author:

    Evan Green 14-Jan-2015

--*/

#ifndef _ARPA_NAMESER_COMPAT_H
#define _ARPA_NAMESER_COMPAT_H

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

#define GETSHORT(_Short, _Pointer) NS_GET16((_Short), (_Pointer))
#define GETLONG(_Long, _Pointer) NS_GET32((_Long), (_Pointer))

//
// These macros write 16 and 32 bit values to a pointer, updating the pointer.
//

#define PUTSHORT(_Short, _Pointer) NS_PUT16((_Short), (_Pointer))
#define PUTLONG(_Long, _Pointer) NS_PUT32((_Long), (_Pointer))

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

#define PACKETSZ NS_PACKETSZ

//
// Maximum domain name
//

#define MAXDNAME NS_MAXDNAME

//
// Maximum compressed comain name
//

#define MAXCDNAME NS_MAXCDNAME

//
// Maximum length of a domain label
//

#define MAXLABEL NS_MAXLABEL

//
// Define the number of bytes of fixed data in the header.
//

#define HFIXEDSZ NS_HFIXEDSZ

//
// Define the maximum number of byte of fixed data in the query.
//

#define QFIXEDSZ NS_QFIXEDSZ

//
// Define the maximu number of bytes of fixed data in an R record.
//

#define RRFIXEDSZ NS_RRFIXEDSZ

//
// Define the size of some basic types.
//

#define INT32SZ NS_INT32SZ
#define INT16SZ NS_INT16SZ
#define INADDRSZ NS_INADDRSZ

//
// Define the value used for handling compress domain names.
//

#define INDIR_MASK NS_CMPRSFLGS

//
// Define the Internet Nameserver port number.
//

#define NAMESERVER_PORT NS_DEFAULTPORT

//
// Define request opcodes. See the enum comments for descriptions.
//

#define QUERY ns_o_query
#define IQUERY ns_o_iquery
#define STATUS ns_o_status
#define NS_NOTIFY_OP ns_o_notify

//
// Define response codes. See the enum comments for descriptions.
//

#define NOERROR ns_r_noerror
#define FORMERR ns_r_formerr
#define SERVFAIL ns_r_servfail
#define NXDOMAIN ns_r_nxdomain
#define NOTIMP ns_r_notimpl
#define REFUSED ns_r_refused

//
// Define type values for resources and queries. See the enum comments for
// proper descriptions.
//

#define T_A ns_t_a
#define T_NS ns_t_ns
#define T_MD ns_t_md
#define T_MF ns_t_mf
#define T_CNAME ns_t_cname
#define T_SOA ns_t_soa
#define T_MB ns_t_mb
#define T_MG ns_t_mg
#define T_MR ns_t_mr
#define T_NULL ns_t_null
#define T_WKS ns_t_wks
#define T_PTR ns_t_ptr
#define T_HINFO ns_t_hinfo
#define T_MINFO ns_t_minfo
#define T_MX ns_t_mx
#define T_TXT ns_t_txt
#define T_RP ns_t_rp
#define T_AFSDB ns_t_afsdb
#define T_X25 ns_t_x25
#define T_ISDN ns_t_isdn
#define T_RT ns_t_rt
#define T_NSAP ns_t_nsap
#define T_NSAP_PTR ns_t_nsap_ptr
#define T_SIG ns_t_sig
#define T_KEY ns_t_key
#define T_PX ns_t_px
#define T_GPOS ns_t_gpos
#define T_AAAA ns_t_aaaa
#define T_LOC ns_t_loc
#define T_UINFO ns_t_uinfo
#define T_UID ns_t_uid
#define T_GID ns_t_gid
#define T_UNSPEC ns_t_unspec
#define T_AXFR ns_t_axfr
#define T_MAILB ns_t_mailb
#define T_MAILA ns_t_maila
#define T_ANY ns_t_any

//
// Define values for the class field.
//

//
// The ARPA Internet class
//

#define C_IN ns_c_in

//
// The chaos net (MIT)
//

#define C_CHAOS ns_c_chaos

//
// The Hesiod name server (MIT)
//

#define C_HS ns_c_hs

//
// Nothing
//

#define C_NONE ns_c_none

//
// Wildcard match class
//

#define C_ANY ns_c_any

//
// Define status codes for T_UNSPEC conversion routines.
//

#define CONV_SUCCESS    0
#define CONV_OVERFLOW   (-1)
#define CONV_BADFMT     (-2)
#define CONV_BADCKSUM   (-3)
#define CONV_BADBUFLEN  (-4)

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

