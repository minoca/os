/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    inet.h

Abstract:

    This header contains definitions for internet operations.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <inttypes.h>
#include <netinet/in.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
const char *
inet_ntop (
    int AddressFamily,
    const void *Source,
    char *Destination,
    socklen_t Size
    );

/*++

Routine Description:

    This routine converts a numeric address into a text string suitable for
    presentation. IPv4 addresses will be printed in standard dotted decimal
    form: ddd.ddd.ddd.ddd, where d is a one to three digit decimal number
    between 0 and 255. IPv6 addresses are represented in the form
    x:x:x:x:x:x:x:x, where x is the hexadecimal 16-bit piece of the address.
    Leading zeros may be omitted, but there shall be at least one numeral in
    each field. Alternatively, a string of contiguous zeros can be shown as
    "::". The "::" string can only appear once in an address. Unspecified
    addresses ("0:0:0:0:0:0:0:0") may be represented simply as "::".

Arguments:

    AddressFamily - Supplies the address family of the source argument. Valid
        values are AF_INET for IPv4 and AF_INET6 for IPv6.

    Source - Supplies a pointer to the address structure, whose type depends
        on the address family parameter. For IPv4 addresses this should be a
        pointer to a struct in_addr, and for IPv6 addresses this should be a
        pointer to a struct in6_addr (not a sockaddr* structure).

    Destination - Supplies a pointer to a buffer where the resulting string
        will be written on success. This must not be NULL.

    Size - Supplies the size in bytes of the supplied destination buffer.

Return Value:

    Returns the destination pointer on success.

    NULL on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
inet_pton (
    int AddressFamily,
    const char *Source,
    void *Destination
    );

/*++

Routine Description:

    This routine converts an address represented in text form into its
    corresponding binary address form. For IPv4 addresses, the text should be
    in the standard form ddd.ddd.ddd.ddd, where d is a one to three digit
    decimal number between 0 and 255. For IPv6 addresses, the text should be
    in the form "x:x:x:x:x:x:x:x", where x is a hexadecimal 16-bit piece
    of the address. Leading zeros may be omitted, but there shall be at least
    one numeral in each field. Alternatively, a string of contiguous zeros can
    be replaced with "::". The string "::" can appear only once in an address.
    The string "::" by itself translates to the address 0:0:0:0:0:0:0:0. As a
    third alternative, the address can be represented as "x:x:x:x:x:x:d.d.d.d",
    where x is the 16-bit hexadecimal portion of the address, and d is the
    decimal values of the four low-order 8-bit pieces of the address (standard
    IPv4 representation).

Arguments:

    AddressFamily - Supplies the address family of the source argument. Valid
        values are AF_INET for IPv4 and AF_INET6 for IPv6.

    Source - Supplies a pointer to the null-terminated string to convert into
        an address.

    Destination - Supplies a pointer to the address structure (whose type
        depends on the address family parameter) where the binary form of the
        address will be returned on success. This should be something like a
        pointer to an in_addr or in6_addr, not a sockaddr structure.

Return Value:

    1 if the conversion succeeds.

    0 if the conversion failed.

    -1 with errno set to EAFNOSUPPORT if the address family parameter is
    unrecognized.

--*/

LIBC_API
in_addr_t
inet_addr (
    const char *String
    );

/*++

Routine Description:

    This routine converts the given string to an integer value suitable for use
    as an integer address.

Arguments:

    String - Supplies a pointer to the string to convert.

Return Value:

    returns the IPv4 internet address associated with the string.

    (in_addr_t)(-1) on failure.

--*/

LIBC_API
char *
inet_ntoa (
    struct in_addr Address
    );

/*++

Routine Description:

    This routine converts the given IPv4 address into an internet standard
    dot notation string. This function is neither thread-safe nor reentrant.

Arguments:

    Address - Supplies the address to convert.

Return Value:

    Returns a pointer to the address string on success. This buffer will be
    overwritten on subsequent calls to this function.

--*/

LIBC_API
int
inet_aton (
    const char *String,
    struct in_addr *Address
    );

/*++

Routine Description:

    This routine converts the given string to an interger value suitable for
    use as an IPv4 address.

Arguments:

    String - Supplies a pointer to the string to convert.

    Address - Supplies a pointer that receives the converted IPv4 address.

Return Value:

    Returns non-zero if the address string is valid.

    0 if the address string is invalid.

--*/

#ifdef __cplusplus

}

#endif
#endif

