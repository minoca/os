/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    types.h

Abstract:

    This header contains basic type definitions for UEFI.

Author:

    Evan Green 7-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#if defined(__i386)

#include <minoca/uefi/x86/procdef.h>

#elif defined(__arm__)

#include <minoca/uefi/arm/procdef.h>

#elif defined(__amd64)

#include <minoca/uefi/x64/procdef.h>

#else

#error No known architecture was defined.

#endif

//
// --------------------------------------------------------------------- Macros
//

//
// Define vararg macros.
//

#define VA_START(_Marker, _Parameter) __builtin_va_start(_Marker, _Parameter)
#define VA_ARG(_Marker, _Type) __builtin_va_arg(_Marker, _Type)
#define VA_END(_Marker) __builtin_va_end(_Marker)
#define VA_COPY(_Destination, _Source) __builtin_va_copy(_Destination, _Source)

//
// This macro returns the offset of the given field within the given structure.
//

#ifndef OFFSET_OF

#define OFFSET_OF(_Type, _Field) ((UINTN) &(((_Type *)0)->_Field))

#endif

//
// This macro rounds a value up to the next boundary using a given power of two
// alignment.
//

#define ALIGN_VALUE(_Value, _Alignment) \
    ((_Value) + (((_Alignment) - (_Value)) & ((_Alignment) - 1)))

//
// This macro aligns a pointer by adding the minimum offset required for it to
// be aligned on the given alignment boundary.
//

#define ALIGN_POINTER(_Pointer, _Alignment) \
    ((VOID *)(ALIGN_VALUE((UINTN)(_Pointer), (_Alignment))))

//
// This macro aligns the given variable up to the next natural boundary for the
// current CPU (4 bytes for 32-bit CPUs and 8 bytes for 64-bit CPUs).
//

#define ALIGN_VARIABLE(_Value) ALIGN_VALUE((_Value), sizeof(UINTN))

//
// This macro returns the absolute value of the given integer.
//

#define ABS(_Value) (((_Value) < 0) ? (-(_Value)) : (_Value))

//
// This macro returns the maximum of the two given integers.
//

#define MAX(_FirstValue, _SecondValue) \
    (((_FirstValue) > (_SecondValue)) ? (_FirstValue) : (_SecondValue))

//
// This macro returns the minimum of the two given integers.
//

#define MIN(_FirstValue, _SecondValue) \
    (((_FirstValue) < (_SecondValue)) ? (_FirstValue) : (_SecondValue))

//
// This macro creates an error value.
//

#define ENCODE_ERROR(_StatusCode) ((RETURN_STATUS)(MAX_BIT | (_StatusCode)))
#define EFIERR(_StatusCode) ENCODE_ERROR(_StatusCode)

//
// This macro creates a warning value.
//

#define ENCODE_WARNING(_StatusCode) ((RETURN_STATUS)(_StatusCode))

//
// This macro returns non-zero if the given status code has the high bit set.
//

#define RETURN_ERROR(_StatusCode) (((INTN)(RETURN_STATUS)(_StatusCode)) < 0)
#define EFI_ERROR(_StatusCode) RETURN_ERROR(_StatusCode)

//
// This macro converts a size in bytes into a number of EFI_PAGES.
//

#define EFI_SIZE_TO_PAGES(_Bytes) \
    (((_Bytes) >> EFI_PAGE_SHIFT) + (((_Bytes) & EFI_PAGE_MASK) ? 1 : 0))

//
// This macro converts a EFI_PAGE count into a number of bytes.
//

#define EFI_PAGES_TO_SIZE(_Pages) ((_Pages) << EFI_PAGE_SHIFT)

//
// This macro determines whether or not a given machine type matches supported
// machine types on this processor.
//

#if defined(EFI_X86)

#define EFI_IMAGE_MACHINE_TYPE_SUPPORTED(_Machine) \
    ((_Machine) == EFI_IMAGE_MACHINE_IA32)

#elif defined(EFI_ARM)

#define EFI_IMAGE_MACHINE_TYPE_SUPPORTED(_Machine) \
    ((_Machine) == EFI_IMAGE_MACHINE_ARMTHUMB_MIXED)

#elif defined(EFI_X64)

#define EFI_IMAGE_MACHINE_TYPE_SUPPORTED(_Machine) \
    ((_Machine) == EFI_IMAGE_MACHINE_X64)

#else

#error Unsupported Architecture

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some inherent constants.
//

#ifndef TRUE

#define TRUE 1

#endif

#ifndef FALSE

#define FALSE 0

#endif

#ifndef NULL

#define NULL ((VOID *)0)

#endif

#define CONST const
#define STATIC static

#ifndef VOID

#define VOID void

#endif

#define PACKED __attribute__((__packed__))
#define __USED __attribute__((used))

//
// Define standard type limits.
//

#define MAX_INT8    ((INT8)0x7F)
#define MAX_UINT8   ((UINT8)0xFF)
#define MAX_INT16   ((INT16)0x7FFF)
#define MAX_UINT16  ((UINT16)0xFFFF)
#define MAX_INT32   ((INT32)0x7FFFFFFF)
#define MAX_UINT32  ((UINT32)0xFFFFFFFF)
#define MAX_INT64   ((INT64)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINT64  ((UINT64)0xFFFFFFFFFFFFFFFFULL)

//
// Define the success status code.
//

#define RETURN_SUCCESS 0

//
// Define internal status codes.
//

//
// The image failed to load.
//

#define RETURN_LOAD_ERROR            ENCODE_ERROR(1)

//
// The parameter was incorrect.
//

#define RETURN_INVALID_PARAMETER     ENCODE_ERROR(2)

//
// The operation is not supported.
//

#define RETURN_UNSUPPORTED           ENCODE_ERROR(3)

//
// The buffer was not the proper size for the request.
//

#define RETURN_BAD_BUFFER_SIZE       ENCODE_ERROR(4)

//
// The buffer was not large enough to hold the requested data.
// The required buffer size is returned in the appropriate parameter when this
// error occurs.
//

#define RETURN_BUFFER_TOO_SMALL      ENCODE_ERROR(5)

//
// There is no data pending upon return.
//

#define RETURN_NOT_READY             ENCODE_ERROR(6)

//
// The physical device reported an error while attempting the operation.
//

#define RETURN_DEVICE_ERROR          ENCODE_ERROR(7)

//
// The device can not be written to.
//

#define RETURN_WRITE_PROTECTED       ENCODE_ERROR(8)

//
// The resource has run out.
//

#define RETURN_OUT_OF_RESOURCES      ENCODE_ERROR(9)

//
// An inconsistency was detected on the file system causing the operation to
// fail.
//

#define RETURN_VOLUME_CORRUPTED      ENCODE_ERROR(10)

//
// There is no more space on the file system.
//

#define RETURN_VOLUME_FULL           ENCODE_ERROR(11)

//
// The device does not contain any medium to perform the operation.
//

#define RETURN_NO_MEDIA              ENCODE_ERROR(12)

//
// The medium in the device has changed since the last access.
//

#define RETURN_MEDIA_CHANGED         ENCODE_ERROR(13)

//
// The item was not found.
//

#define RETURN_NOT_FOUND             ENCODE_ERROR(14)

//
// Access was denied.
//

#define RETURN_ACCESS_DENIED         ENCODE_ERROR(15)

//
// The server was not found or did not respond to the request.
//

#define RETURN_NO_RESPONSE           ENCODE_ERROR(16)

//
// A mapping to the device does not exist.
//

#define RETURN_NO_MAPPING            ENCODE_ERROR(17)

//
// A timeout time expired.
//

#define RETURN_TIMEOUT               ENCODE_ERROR(18)

//
// The protocol has not been started.
//

#define RETURN_NOT_STARTED           ENCODE_ERROR(19)

//
// The protocol has already been started.
//

#define RETURN_ALREADY_STARTED       ENCODE_ERROR(20)

//
// The operation was aborted.
//

#define RETURN_ABORTED               ENCODE_ERROR(21)

//
// An ICMP error occurred during the network operation.
//

#define RETURN_ICMP_ERROR            ENCODE_ERROR(22)

//
// A TFTP error occurred during the network operation.
//

#define RETURN_TFTP_ERROR            ENCODE_ERROR(23)

//
// A protocol error occurred during the network operation.
//

#define RETURN_PROTOCOL_ERROR        ENCODE_ERROR(24)

//
// A function encountered an internal version that was incompatible with a
// version requested by the caller.
//

#define RETURN_INCOMPATIBLE_VERSION  ENCODE_ERROR(25)

//
// The function was not performed due to a security violation.
//

#define RETURN_SECURITY_VIOLATION    ENCODE_ERROR(26)

//
// A CRC error was detected.
//

#define RETURN_CRC_ERROR             ENCODE_ERROR(27)

//
// The beginning or end of media was reached.
//

#define RETURN_END_OF_MEDIA          ENCODE_ERROR(28)

//
// The end of the file was reached.
//

#define RETURN_END_OF_FILE           ENCODE_ERROR(31)

//
// The language specified was invalid.
//

#define RETURN_INVALID_LANGUAGE      ENCODE_ERROR(32)

//
// The security status of the data is unknown or compromised and the data must
// be updated or replaced to restore a valid security status.
//

#define RETURN_COMPROMISED_DATA      ENCODE_ERROR(33)

//
// The string contained one or more characters that the device could not render
// and were skipped.
//

#define RETURN_WARN_UNKNOWN_GLYPH    ENCODE_WARNING(1)

//
// The handle was closed, but the file was not deleted.
//

#define RETURN_WARN_DELETE_FAILURE   ENCODE_WARNING(2)

//
// The handle was closed, but the data to the file was not flushed properly.
//

#define RETURN_WARN_WRITE_FAILURE    ENCODE_WARNING(3)

//
// The resulting buffer was too small, and the data was truncated to the buffer
// size.
//

#define RETURN_WARN_BUFFER_TOO_SMALL ENCODE_WARNING(4)

//
// The data has not been updated within the timeframe set by local policy for
// this type of data.
//

#define RETURN_WARN_STALE_DATA       ENCODE_WARNING(5)

//
// Define the status codes in the UEFI spec.
//

#define EFI_SUCCESS               RETURN_SUCCESS
#define EFI_LOAD_ERROR            RETURN_LOAD_ERROR
#define EFI_INVALID_PARAMETER     RETURN_INVALID_PARAMETER
#define EFI_UNSUPPORTED           RETURN_UNSUPPORTED
#define EFI_BAD_BUFFER_SIZE       RETURN_BAD_BUFFER_SIZE
#define EFI_BUFFER_TOO_SMALL      RETURN_BUFFER_TOO_SMALL
#define EFI_NOT_READY             RETURN_NOT_READY
#define EFI_DEVICE_ERROR          RETURN_DEVICE_ERROR
#define EFI_WRITE_PROTECTED       RETURN_WRITE_PROTECTED
#define EFI_OUT_OF_RESOURCES      RETURN_OUT_OF_RESOURCES
#define EFI_VOLUME_CORRUPTED      RETURN_VOLUME_CORRUPTED
#define EFI_VOLUME_FULL           RETURN_VOLUME_FULL
#define EFI_NO_MEDIA              RETURN_NO_MEDIA
#define EFI_MEDIA_CHANGED         RETURN_MEDIA_CHANGED
#define EFI_NOT_FOUND             RETURN_NOT_FOUND
#define EFI_ACCESS_DENIED         RETURN_ACCESS_DENIED
#define EFI_NO_RESPONSE           RETURN_NO_RESPONSE
#define EFI_NO_MAPPING            RETURN_NO_MAPPING
#define EFI_TIMEOUT               RETURN_TIMEOUT
#define EFI_NOT_STARTED           RETURN_NOT_STARTED
#define EFI_ALREADY_STARTED       RETURN_ALREADY_STARTED
#define EFI_ABORTED               RETURN_ABORTED
#define EFI_ICMP_ERROR            RETURN_ICMP_ERROR
#define EFI_TFTP_ERROR            RETURN_TFTP_ERROR
#define EFI_PROTOCOL_ERROR        RETURN_PROTOCOL_ERROR
#define EFI_INCOMPATIBLE_VERSION  RETURN_INCOMPATIBLE_VERSION
#define EFI_SECURITY_VIOLATION    RETURN_SECURITY_VIOLATION
#define EFI_CRC_ERROR             RETURN_CRC_ERROR
#define EFI_END_OF_MEDIA          RETURN_END_OF_MEDIA
#define EFI_END_OF_FILE           RETURN_END_OF_FILE
#define EFI_INVALID_LANGUAGE      RETURN_INVALID_LANGUAGE
#define EFI_COMPROMISED_DATA      RETURN_COMPROMISED_DATA
#define EFI_WARN_UNKNOWN_GLYPH    RETURN_WARN_UNKNOWN_GLYPH
#define EFI_WARN_DELETE_FAILURE   RETURN_WARN_DELETE_FAILURE
#define EFI_WARN_WRITE_FAILURE    RETURN_WARN_WRITE_FAILURE
#define EFI_WARN_BUFFER_TOO_SMALL RETURN_WARN_BUFFER_TOO_SMALL
#define EFI_WARN_STALE_DATA       RETURN_WARN_STALE_DATA

//
// Define additional ICMP error codes.
//

#define EFI_NETWORK_UNREACHABLE   EFIERR(100)
#define EFI_HOST_UNREACHABLE      EFIERR(101)
#define EFI_PROTOCOL_UNREACHABLE  EFIERR(102)
#define EFI_PORT_UNREACHABLE      EFIERR(103)

//
// Define additional TCP error codes.
//

#define EFI_CONNECTION_FIN        EFIERR(104)
#define EFI_CONNECTION_RESET      EFIERR(105)
#define EFI_CONNECTION_REFUSED    EFIERR(106)

//
// Define the page size of EFI pages, which are doled out by the EFI page
// allocator. This is not necessarily the same as the processor page size.
//

#define EFI_PAGE_SIZE             0x1000
#define EFI_PAGE_MASK             0xFFF
#define EFI_PAGE_SHIFT            12

//
// Define PE32+ Machine Types.
//

#define EFI_IMAGE_MACHINE_IA32            0x014C
#define EFI_IMAGE_MACHINE_IA64            0x0200
#define EFI_IMAGE_MACHINE_EBC             0x0EBC
#define EFI_IMAGE_MACHINE_X64             0x8664
#define EFI_IMAGE_MACHINE_ARMTHUMB_MIXED  0x01C2
#define EFI_IMAGE_MACHINE_AARCH64         0xAA64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the vararg list type.
//

typedef __builtin_va_list VA_LIST;

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} GUID;

//
// Define the type for a physical memory address.
//

#ifndef __MINOCA_TYPES_H

typedef UINT64 PHYSICAL_ADDRESS;

#endif

//
// Define the internal type for a standard return status.
//

typedef UINTN RETURN_STATUS;

//
// External types defined by the UEFI specification.
//

//
// 128-bit buffer containing a unique identifier value.
//

typedef GUID EFI_GUID;

//
// Function return status for EFI API.
//

typedef RETURN_STATUS EFI_STATUS;

//
// A collection of related interfaces.
//

typedef VOID *EFI_HANDLE;

//
// Handle to an event structure.
//

typedef VOID *EFI_EVENT;

//
// Task priority level.
//

typedef UINTN EFI_TPL;

//
// Logical block address.
//

typedef UINT64 EFI_LBA;

//
// 64-bit physical memory address.
//

typedef UINT64 EFI_PHYSICAL_ADDRESS;

//
// 64-bit virtual memory address.
//

typedef UINT64 EFI_VIRTUAL_ADDRESS;

/*++

Structure Description:

    This structure defines a point in calendar time.

Members:

    Year - Stores the year. Valid values are between 1900 and 9999, inclusive.

    Month - Stores the month. Valid values are between 1 and 12, inclusive.

    Day - Stores the day of the month. Valid values are between 1 and 31,
        inclusive (well, sometimes less depending on the month).

    Hour - Stores the hour of the day. Valid values are between 0 and 23,
        inclusive.

    Minute - Stores the minute of the hour. Valid values are between 0 and 59,
        inclusive.

    Second - Stores the second of the minute. Valid values are between 0 and
        59, inclusive. Leap seconds are not accounted for.

    Pad1 - Stores a reserved byte used to pad the structure.

    Nanosecond - Stores the nanosecond of the second. Valid values are between
        0 and 999999999, inclusive.

    TimeZone - Stores the offset from UTC this time is relative to. Valid
        values are between -1440 to 1440, inclusive, or 2047.

    Daylight - Stores daylight saving flags. See EFI_TIME_* definitions.

    Pad2 - Stores another reserved byte used to pad the structure.

--*/

typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

/*++

Structure Description:

    This structure defines an Internet Protocol v4 address.

Members:

    Addr - Stores the 4-byte address.

--*/

typedef struct {
    UINT8 Addr[4];
} EFI_IPv4_ADDRESS;

/*++

Structure Description:

    This structure defines an Internet Protocol v6 address.

Members:

    Addr - Stores the 16-byte address.

--*/

typedef struct {
    UINT8 Addr[16];
} EFI_IPv6_ADDRESS;

/*++

Structure Description:

    This structure defines an Media Access Control address.

Members:

    Addr - Stores the 32-byte address.

--*/

typedef struct {
    UINT8 Addr[32];
} EFI_MAC_ADDRESS;

/*++

Structure Description:

    This union defines a storage unit that any type of network address can fit
    into.

Members:

    Addr - Stores the data, used to create a minimum size and alignment.

    v4 - Stores the IPv4 address.

    v6 - Stores the IPv6 address.

--*/

typedef union {
    UINT32 Addr[4];
    EFI_IPv4_ADDRESS v4;
    EFI_IPv6_ADDRESS v6;
} EFI_IP_ADDRESS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
