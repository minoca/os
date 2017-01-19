/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netaddr.c

Abstract:

    This module implements support for network address and name translation.

Author:

    Evan Green 5-Dec-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum lengths for a network address.
//

#define IP4_ADDRESS_STRING_SIZE sizeof("255.255.255.255")
#define IP6_ADDRESS_STRING_SIZE \
    sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")

//
// Define the number of 16-bit words in an IPv6 address.
//

#define IP6_WORD_COUNT 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

const char *
ClpConvertIp4AddressToString (
    const unsigned char *Source,
    char *Destination,
    socklen_t Size
    );

const char *
ClpConvertIp6AddressToString (
    const char *Source,
    char *Destination,
    socklen_t Size
    );

int
ClpConvertIp4AddressFromString (
    const char *Source,
    struct in_addr *Destination
    );

int
ClpConvertIp6AddressFromString (
    const char *Source,
    struct in6_addr *Destination
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an area for the static IP4 address string buffer.
//

CHAR ClIp4StringBuffer[IP4_ADDRESS_STRING_SIZE];

//
// Define the "any" address for IPv6.
//

LIBC_API const struct in6_addr in6addr_any = IN6_ANY_INIT;

//
// Define the IPv6 loopback address.
//

LIBC_API const struct in6_addr in6addr_loopback = IN6_LOOPBACK_INIT;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
const char *
inet_ntop (
    int AddressFamily,
    const void *Source,
    char *Destination,
    socklen_t Size
    )

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

{

    const char *Result;

    Result = NULL;
    switch (AddressFamily) {
    case AF_INET:
        Result = ClpConvertIp4AddressToString(Source, Destination, Size);
        break;

    case AF_INET6:
        Result = ClpConvertIp6AddressToString(Source, Destination, Size);
        break;

    default:
        errno = EAFNOSUPPORT;
        break;
    }

    return Result;
}

LIBC_API
int
inet_pton (
    int AddressFamily,
    const char *Source,
    void *Destination
    )

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

{

    int Result;

    Result = -1;
    switch (AddressFamily) {
    case AF_INET:
        Result = ClpConvertIp4AddressFromString(Source, Destination);
        break;

    case AF_INET6:
        Result = ClpConvertIp6AddressFromString(Source, Destination);
        break;

    default:
        errno = EAFNOSUPPORT;
        break;
    }

    return Result;
}

LIBC_API
in_addr_t
inet_addr (
    const char *String
    )

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

{

    struct in_addr IpAddress;
    int Result;

    Result = ClpConvertIp4AddressFromString(String, &IpAddress);

    //
    // Return -1 if the conversion failed.
    //

    if (Result == 0) {
        return (in_addr_t)-1;
    }

    return IpAddress.s_addr;
}

LIBC_API
char *
inet_ntoa (
    struct in_addr Address
    )

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

{

    const char *Result;

    Result = ClpConvertIp4AddressToString(
                                      (const unsigned char *)&(Address.s_addr),
                                      ClIp4StringBuffer,
                                      sizeof(ClIp4StringBuffer));

    return (char *)Result;
}

LIBC_API
int
inet_aton (
    const char *String,
    struct in_addr *Address
    )

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

{

    return ClpConvertIp4AddressFromString(String, Address);
}

//
// --------------------------------------------------------- Internal Functions
//

const char *
ClpConvertIp4AddressToString (
    const unsigned char *Source,
    char *Destination,
    socklen_t Size
    )

/*++

Routine Description:

    This routine converts an IPv4 address to a string, which will be printed in
    standard dotted decimal form: ddd.ddd.ddd.ddd, where d is a one to three
    digit decimal number between 0 and 255.

Arguments:

    Source - Supplies a pointer to the source IPv4 address.

    Destination - Supplies a pointer to a buffer where the resulting string
        will be written on success. This must not be NULL.

    Size - Supplies the size in bytes of the supplied destination buffer.

Return Value:

    Returns the destination pointer on success.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    size_t Length;
    CHAR WorkingBuffer[IP4_ADDRESS_STRING_SIZE];

    snprintf(WorkingBuffer,
             IP4_ADDRESS_STRING_SIZE,
             "%d.%d.%d.%d",
             Source[0],
             Source[1],
             Source[2],
             Source[3]);

    Length = strlen(WorkingBuffer);
    if (Length >= Size) {
        errno = ENOSPC;
        return NULL;
    }

    strcpy(Destination, WorkingBuffer);
    return Destination;
}

const char *
ClpConvertIp6AddressToString (
    const char *Source,
    char *Destination,
    socklen_t Size
    )

/*++

Routine Description:

    This routine converts a numeric IPv6 address into a text string suitable for
    for presentation. IPv6 addresses are represented in the form
    x:x:x:x:x:x:x:x, where x is the hexadecimal 16-bit piece of the address.
    Leading zeros may be omitted, but there shall be at least one numeral in
    each field. Alternatively, a string of contiguous zeros can be shown as
    "::". The "::" string can only appear once in an address. Unspecified
    addresses ("0:0:0:0:0:0:0:0") may be represented simply as "::".

Arguments:

    AddressFamily - Supplies the address family of the source argument. Valid
        values are AF_INET for IPv4 and AF_INET6 for IPv6.

    Source - Supplies a pointer to the IPv6 address.

    Destination - Supplies a pointer to a buffer where the resulting string
        will be written on success. This must not be NULL.

    Size - Supplies the size in bytes of the supplied destination buffer.

Return Value:

    Returns the destination pointer on success.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    LONG CurrentRun;
    LONG CurrentRunSize;
    PSTR String;
    UINTN StringSize;
    LONG WinnerRun;
    LONG WinnerRunSize;
    ULONG WordIndex;
    USHORT Words[IP6_WORD_COUNT];
    CHAR WorkingString[IP6_ADDRESS_STRING_SIZE];

    //
    // Copy the address into its word array.
    //

    for (WordIndex = 0; WordIndex < IP6_WORD_COUNT; WordIndex += 1) {
        Words[WordIndex] = (((UCHAR)Source[WordIndex * 2]) << 8) |
                           (UCHAR)Source[(WordIndex * 2) + 1];
    }

    //
    // Find the longest run of zeroes in the array. This makes for a nice
    // interview question.
    //

    WinnerRun = -1;
    WinnerRunSize = 0;
    CurrentRun = -1;
    CurrentRunSize = 0;
    for (WordIndex = 0; WordIndex < IP6_WORD_COUNT; WordIndex += 1) {

        //
        // If a zero is found, start or update the current run.
        //

        if (Words[WordIndex] == 0) {
            if (CurrentRun == -1) {
                CurrentRun = WordIndex;
                CurrentRunSize = 1;

            } else {
                CurrentRunSize += 1;
            }

            //
            // Keep the max up to date as well.
            //

            if (CurrentRunSize > WinnerRunSize) {
                WinnerRun = CurrentRun;
                WinnerRunSize = CurrentRunSize;
            }

        //
        // The run is broken.
        //

        } else {
            CurrentRun = -1;
            CurrentRunSize = 0;
        }
    }

    //
    // Print the formatted string.
    //

    String = WorkingString;
    for (WordIndex = 0; WordIndex < IP6_WORD_COUNT; WordIndex += 1) {

        //
        // Represent the run of zeros with a single extra colon (so it looks
        // like "::").
        //

        if ((WinnerRun != -1) && (WordIndex >= WinnerRun) &&
            (WordIndex < (WinnerRun + WinnerRunSize))) {

            if (WordIndex == WinnerRun) {
                *String = ':';
                String += 1;
            }

            continue;
        }

        //
        // Every number is preceded by a colon except the first.
        //

        if (WordIndex != 0) {
            *String = ':';
            String += 1;
        }

        //
        // Potentially print an encapsulated IPv4 address.
        //

        if ((WordIndex == 6) && (WinnerRun == 0) &&
            ((WinnerRunSize == 6) ||
             ((WinnerRunSize == 5) && (Words[5] == 0xFFFF)))) {

            ClpConvertIp4AddressToString((const unsigned char *)(Source + 12),
                                         String,
                                         IP4_ADDRESS_STRING_SIZE);

            String += strlen(String);
            break;
        }

        String += snprintf(String, 5, "%x", Words[WordIndex]);
    }

    //
    // If the winning run of zeros goes to the end, then a final extra colon
    // is needed since the lower half of the preceding loop never got a chance
    // to run.
    //

    if ((WinnerRun != -1) && ((WinnerRun + WinnerRunSize) == IP6_WORD_COUNT)) {
        *String = ':';
        String += 1;
    }

    //
    // Null terminate the string.
    //

    *String = '\0';
    String += 1;
    StringSize = (UINTN)String - (UINTN)WorkingString;

    assert(StringSize <= IP6_ADDRESS_STRING_SIZE);

    if (Size < StringSize) {
        errno = ENOSPC;
        return NULL;
    }

    strcpy(Destination, WorkingString);
    return Destination;
}

int
ClpConvertIp4AddressFromString (
    const char *Source,
    struct in_addr *Destination
    )

/*++

Routine Description:

    This routine converts an address represented in text form into its
    corresponding binary address form. For IPv4 addresses, the text should be
    in the standard form ddd.ddd.ddd.ddd, where d is a one to three digit
    decimal number between 0 and 255.

Arguments:

    Source - Supplies a pointer to the null-terminated string to convert into
        an address.

    Destination - Supplies a pointer to the address structure where the binary
        form of the address will be returned on success.

Return Value:

    1 if the conversion succeeds.

    0 if the conversion failed.

--*/

{

    PSTR AfterScan;
    PSTR CurrentString;
    ULONG Integer;
    ULONG IpAddress;
    ULONG Shift;

    CurrentString = (PSTR)Source;
    IpAddress = 0;
    Shift = 24;
    while (TRUE) {
        Integer = strtoul(CurrentString, &AfterScan, 0);
        if (AfterScan == CurrentString) {
            return 0;
        }

        CurrentString = AfterScan;
        if (*CurrentString == '\0') {
            IpAddress |= Integer;
            break;
        }

        if (*CurrentString == '.') {
            if (Integer > 255) {
                return 0;
            }

            IpAddress |= Integer << Shift;
            if (Shift >= BITS_PER_BYTE) {
                Shift -= BITS_PER_BYTE;
            }

            CurrentString += 1;

        //
        // Some funky character up in here.
        //

        } else {
            return 0;
        }
    }

    Destination->s_addr = htonl(IpAddress);
    return 1;
}

int
ClpConvertIp6AddressFromString (
    const char *Source,
    struct in6_addr *Destination
    )

/*++

Routine Description:

    This routine converts an address represented in text form into its
    corresponding binary address form. For IPv6 addresses, the text should be
    in the form "x:x:x:x:x:x:x:x:x", where x is a hexadecimal 16-bit piece
    of the address. Leading zeros may be omitted, but there shall be at least
    one numeral in each field. Alternatively, a string of contiguous zeros can
    be replaced with "::". The string "::" can appear only once in an address.
    The string "::" by itself translates to the address 0:0:0:0:0:0:0:0. As a
    third alternative, the address can be represented as "x:x:x:x:x:x:d.d.d.d",
    where x is the 16-bit hexadecimal portion of the address, and d is the
    decimal values of the four low-order 8-bit pieces of the address (standard
    IPv4 representation).

Arguments:

    Source - Supplies a pointer to the null-terminated string to convert into
        an address.

    Destination - Supplies a pointer to the address structure where the binary
        form of the address will be returned on success.

Return Value:

    1 if the conversion succeeds.

    0 if the conversion failed.

--*/

{

    PSTR AfterScan;
    ULONG DestinationIndex;
    ULONG Integer;
    struct in_addr *Ip4Address;
    ULONG PrefixLength;
    int Result;
    const char *String;
    CHAR Suffix[16];
    ULONG SuffixIndex;
    ULONG SuffixLength;

    PrefixLength = 0;
    String = Source;
    SuffixLength = 0;
    memset(Destination, 0, sizeof(struct in6_addr));
    memset(Suffix, 0, sizeof(Suffix));

    //
    // Scan things in directly as long as a double colon was not found.
    //

    while (TRUE) {
        if (*String == ':') {
            String += 1;

            //
            // Break out of this loop if a double colon was found.
            //

            if (*String == ':') {
                String += 1;
                break;
            }
        }

        Integer = strtoul(String, &AfterScan, 16);
        if (AfterScan == String) {
            return 0;
        }

        //
        // The last 4 bytes may be written as an IPv4 address.
        //

        if ((PrefixLength == 12) && (*AfterScan == '.')) {
            Ip4Address = (struct in_addr *)&(Destination->s6_addr[12]);
            Result = ClpConvertIp4AddressFromString(String, Ip4Address);
            String += strlen(String);
            goto ConvertIp6AddressFromStringEnd;
        }

        Destination->s6_addr[PrefixLength] = Integer >> BITS_PER_BYTE;
        PrefixLength += 1;
        Destination->s6_addr[PrefixLength] = Integer & 0xFF;
        PrefixLength += 1;
        String = AfterScan;
        if (PrefixLength == 16) {
            goto ConvertIp6AddressFromStringEnd;
        }
    }

    //
    // Scan in the remainder after a double colon.
    //

    while (TRUE) {
        if (*String == ':') {
            if (SuffixLength == 0) {
                break;
            }

            String += 1;

        } else if (SuffixLength != 0) {
            break;
        }

        Integer = strtoul(String, &AfterScan, 16);
        if (AfterScan == String) {
            break;
        }

        if (((SuffixLength + PrefixLength) <= 12) && (*AfterScan == '.')) {
            Ip4Address = (struct in_addr *)&(Suffix[SuffixLength]);
            Result = ClpConvertIp4AddressFromString(String, Ip4Address);
            if (Result == 0) {
                return 0;
            }

            SuffixLength += 4;
            String += strlen(String);
            break;
        }

        Suffix[SuffixLength] = Integer >> BITS_PER_BYTE;
        SuffixLength += 1;
        Suffix[SuffixLength] = Integer & 0xFF;
        SuffixLength += 1;
        String = AfterScan;
        if (PrefixLength + SuffixLength >= 16) {
            break;
        }
    }

    //
    // Now that the suffix length is known, copy it into the destination.
    //

    for (SuffixIndex = 0; SuffixIndex < SuffixLength; SuffixIndex += 1) {
        DestinationIndex = 16 - SuffixLength + SuffixIndex;
        Destination->s6_addr[DestinationIndex] = Suffix[SuffixIndex];
    }

ConvertIp6AddressFromStringEnd:

    //
    // Allow a % at the end, but otherwise it's an error for the string not to
    // be completely used up.
    //

    if (*String != '\0') {
        if (*String != '%') {
            return 0;
        }
    }

    return 1;
}

