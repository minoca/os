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
#include <arpa/nameser_compat.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the revision information.
//

#define __NAMESER 19991006

//
// Define constants from RFC883.
//

//
// Maximum packet size
//

#define NS_PACKETSZ 512

//
// Maximum domain name
//

#define NS_MAXDNAME 256

//
// Maximum compressed comain name
//

#define NS_MAXCDNAME 255

//
// Maximum length of a domain label
//

#define NS_MAXLABEL 63

//
// Define the number of bytes of fixed data in the header.
//

#define NS_HFIXEDSZ 12

//
// Define the maximum number of byte of fixed data in the query.
//

#define NS_QFIXEDSZ 4

//
// Define the maximu number of bytes of fixed data in an R record.
//

#define NS_RRFIXEDSZ 10

//
// Define the size of some basic types.
//

#define NS_INT32SZ 4
#define NS_INT16SZ 2
#define NS_INADDRSZ 4
#define NS_IN6ADDRSZ 16

//
// Define the value used for handling compress domain names.
//

#define NS_CMPRSFLGS 0xC0

//
// Define the Internet Nameserver port number.
//

#define NS_DEFAULTPORT 53

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the possible values for the class field.
//

typedef enum __ns_class {
    ns_c_invalid = 0,   // Invalid value
    ns_c_in = 1,        // Internet
    ns_c_2 = 2,         // Reserved
    ns_c_chaos = 3,     // CHAOS net (MIT)
    ns_c_hs = 4,        // Hesiod name server (MIT)
    ns_c_none = 254,    // Empty value
    ns_c_any = 255,     // Wildcard match class
    ns_c_max = 65536    // Max for enum sizing.
} ns_class;

//
// Define the possible types for queries and responses.
//

typedef enum __ns_type {
    ns_t_invalid = 0,   // Invalid value
    ns_t_a = 1,         // Host address
    ns_t_ns = 2,        // Name server
    ns_t_md = 3,        // Mail destination
    ns_t_mf = 4,        // Mail forwarder
    ns_t_cname = 5,     // Canonical name
    ns_t_soa = 6,       // Start of Authority zone
    ns_t_mb = 7,        // Mailbox domain name
    ns_t_mg = 8,        // Mail group member
    ns_t_mr = 9,        // Mail rename name
    ns_t_null = 10,     // Null record
    ns_t_wks = 11,      // Well Known Service
    ns_t_ptr = 12,      // Domain name pointer
    ns_t_hinfo = 13,    // Host information
    ns_t_minfo = 14,    // Mailbox information
    ns_t_mx = 15,       // Mail routing information
    ns_t_txt = 16,      // Generic text record
    ns_t_rp = 17,       // Responsible preson
    ns_t_afsdb = 18,    // AFS cell database
    ns_t_x25 = 19,      // X25 calling address
    ns_t_isdn = 20,     // ISDN calling address
    ns_t_rt = 21,       // Router
    ns_t_nsap = 22,     // NSAP address
    ns_t_nsap_ptr = 23, // NSAP pointer
    ns_t_sig = 24,      // Security signature
    ns_t_key = 25,      // Security key
    ns_t_px = 26,       // X.400 mail mapping
    ns_t_gpos = 27,     // Geographical position
    ns_t_aaaa = 28,     // IPv6 address
    ns_t_loc = 29,      // Location information
    ns_t_nxt = 30,      // Next domain
    ns_t_eid = 31,      // Endpoint identifier
    ns_t_nimloc = 32,   // Nimrod locator
    ns_t_srv = 33,      // Server selection
    ns_t_atma = 34,     // ATM address
    ns_t_naptr = 35,    // Naming authority pointer
    ns_t_kx = 36,       // Key exchange
    ns_t_cert = 37,     // Certificate record
    ns_t_a6 = 38,       // IPv6 address
    ns_t_dname = 39,    // Non-terminal DNAME
    ns_t_sink = 40,     // Kitchen sink
    ns_t_opt = 41,      // EDNS0 option
    ns_t_apl = 42,      // Address prefix list
    ns_t_ds = 43,       // Delegation signer
    ns_t_sshfp = 44,    // SSH key fingerprint
    ns_t_rrsig = 46,    // Resource record signature
    ns_t_nsec = 47,     // Next secure
    ns_t_dnskey = 48,   // DNS public key
    ns_t_uinfo = 100,   // User (finger) information
    nt_t_uid = 101,     // User record
    ns_t_gid = 102,     // Group record
    ns_t_unspec = 103,  // Unspecified record
    ns_t_tkey = 249,    // Transaction key
    ns_t_tsig = 250,    // Transaction signature
    ns_t_ixfr = 251,    // Incremental zone transfer
    ns_t_axfr = 252,    // Transfer zone of authority
    ns_t_mailb = 253,   // Transfer mailbox records
    ns_t_maila = 254,   // Transfer mail agent records
    ns_t_any = 255,     // Wildcard match
    ns_t_xzfr = 256,    // BIND-specific
    ns_t_max = 65536    // Max value for enum sizing
} ns_type;

//
// Define the name server opcode values.
//

typedef enum __ns_opcode {
    ns_o_query = 0,     // Standard query
    ns_o_iquery = 1,    // Inverse query
    ns_o_status = 2,    // Name server status query
    ns_o_notify = 4,    // Zone change notification
    ns_o_update = 5,    // Zone change message
    ns_o_max = 6        // Limit
} ns_opcode;

//
// Define the response code values.
//

typedef enum __ns_rcode {
    ns_r_noerror = 0,   // Successful response
    ns_r_formerr = 1,   // Format error
    ns_r_servfail = 2,  // Server failure
    ns_r_nxdomain = 3,  // Name error
    ns_r_notimpl = 4,   // Not implemented
    ns_r_refused = 5,   // Operation refused
    ns_r_yxdomain = 6,  // Name exists
    ns_r_yxrrset = 7,   // RRset exists
    ns_r_nxrrset = 8,   // RRset does nto exist
    ns_r_notauth = 9,   // Not authoritative for zone
    ns_r_notzone = 10,  // Zone of record differs from zone section
    ns_r_max = 11,      // Old max
    ns_r_badsig = 16,   // Invalid signature
    ns_r_badkey = 17,   // Invalid key
    ns_r_badtime = 18,  // Invalid timestamp
} ns_rcode;

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

