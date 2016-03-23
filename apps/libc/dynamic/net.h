/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    net.h

Abstract:

    This header contains internal definitions for networking support in the
    C library.

Author:

    Evan Green 23-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the standard reserved port number for DNS requests.
//

#define DNS_PORT_NUMBER 53

//
// Define DNS request/response flags. These flags code a 16-bit field assuming
// a little endian machine.
//

#define DNS_HEADER_FLAG_RESPONSE 0x0080
#define DNS_HEADER_OPCODE_SHIFT 3
#define DNS_HEADER_OPCODE_QUERY 0x0
#define DNS_HEADER_OPCODE_INVERSE_QUERY 0x1
#define DNS_HEADER_OPCODE_STATUS 0x2
#define DNS_HEADER_OPCODE_UPDATE 0x5
#define DNS_HEADER_OPCODE_MASK 0xF
#define DNS_HEADER_FLAG_AUTHORITATIVE_ANSWER 0x0004
#define DNS_HEADER_FLAG_TRUNCATION 0x0002
#define DNS_HEADER_FLAG_RECURSION_DESIRED 0x0001
#define DNS_EHADER_FLAG_RECURSION_AVAILABLE 0x8000
#define DNS_HEADER_RESPONSE_SHIFT 8
#define DNS_HEADER_RESPONSE_SUCCESS 0x0
#define DNS_HEADER_RESPONSE_FORMAT_ERROR 0x1
#define DNS_HEADER_RESPONSE_SERVER_FAILURE 0x2
#define DNS_HEADER_RESPONSE_NAME_ERROR 0x3
#define DNS_HEADER_RESPONSE_NOT_IMPLEMENTED 0x4
#define DNS_HEADER_RESPONSE_REFUSED 0x5
#define DNS_HEADER_RESPONSE_MASK 0xF

#define DNS_COMPRESSION_MASK 0xC0
#define DNS_COMPRESSION_VALUE 0xC0

#define DNS_MAX_NAME 255

#define NETWORK_DEVICE_COUNT_ESTIMATE 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a DNS request and response header.

Members:

    Identifier - Stores a 16-bit identifier used to uniquely identify the
        request.

    Flags - Stores a set of flags relating to the request or response.

    QuestionCount - Stores the number of questions in the remainder of the
        packet.

    AnswerCount - Stores the numer of answers in the remainder of the packet.

    NameServerCount - Stores the number of name server responses in the packet.

    AdditionalResourceCount - Stores the number of additional resources in the
        packet.

--*/

typedef struct _DNS_HEADER {
    USHORT Identifier;
    USHORT Flags;
    USHORT QuestionCount;
    USHORT AnswerCount;
    USHORT NameServerCount;
    USHORT AdditionalResourceCount;
} PACKED DNS_HEADER, *PDNS_HEADER;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the network device information UUID.
//

extern UUID ClNetworkDeviceInformationUuid;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
ClpConvertToNetworkAddress (
    const struct sockaddr *Address,
    UINTN AddressLength,
    PNETWORK_ADDRESS NetworkAddress,
    PSTR *Path,
    PUINTN PathSize
    );

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

    Path - Supplies an optional pointer where a pointer to the path will be
        returned if this is a Unix address.

    PathSize - Supplies an optional pointer where the path size will be
        returned if this is a Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

KSTATUS
ClpConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    PSTR Path,
    UINTN PathSize
    );

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

    Path - Supplies the path, if this is a local Unix address.

    PathSize - Supplies the size of the path, if this is a local Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

