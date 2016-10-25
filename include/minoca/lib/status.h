/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    status.h

Abstract:

    This header contains definitions for kernel mode status codes.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define STATUS_SUCCESS                    (LONG)0
#define STATUS_UNSUCCESSFUL               (LONG)-1
#define STATUS_NOT_FOUND                  (LONG)-2
#define STATUS_NOT_CONFIGURED             (LONG)-3
#define STATUS_INTERRUPTED                (LONG)-4
#define STATUS_ACCESS_VIOLATION           (LONG)-5
#define STATUS_BROKEN_PIPE                (LONG)-6
#define STATUS_INSUFFICIENT_RESOURCES     (LONG)-7
#define STATUS_INVALID_HANDLE             (LONG)-8
#define STATUS_DATA_PAGED_OUT             (LONG)-9
#define STATUS_FIRMWARE_ERROR             (LONG)-10
#define STATUS_MEMORY_CONFLICT            (LONG)-11
#define STATUS_FILE_CORRUPT               (LONG)-12
#define STATUS_INVALID_PARAMETER          (LONG)-13
#define STATUS_NO_SUCH_DEVICE             (LONG)-14
#define STATUS_NO_SUCH_FILE               (LONG)-15
#define STATUS_INVALID_DIRECTORY          (LONG)-16
#define STATUS_END_OF_FILE                (LONG)-17
#define STATUS_RANGE_CONFLICT             (LONG)-18
#define STATUS_VERSION_MISMATCH           (LONG)-19
#define STATUS_NAME_TOO_LONG              (LONG)-20
#define STATUS_UNKNOWN_IMAGE_FORMAT       (LONG)-21
#define STATUS_NOT_SUPPORTED              (LONG)-22
#define STATUS_NO_MEMORY                  (LONG)-23
#define STATUS_DUPLICATE_ENTRY            (LONG)-24
#define STATUS_NO_ELIGIBLE_DEVICES        (LONG)-25
#define STATUS_UNKNOWN_DEVICE             (LONG)-26
#define STATUS_TOO_EARLY                  (LONG)-27
#define STATUS_TOO_LATE                   (LONG)-28
#define STATUS_DRIVER_FUNCTION_MISSING    (LONG)-29
#define STATUS_NO_DRIVERS                 (LONG)-30
#define STATUS_NOT_HANDLED                (LONG)-31
#define STATUS_NO_INTERFACE               (LONG)-32
#define STATUS_INCORRECT_BUFFER_SIZE      (LONG)-33
#define STATUS_ACCESS_DENIED              (LONG)-34
#define STATUS_BUFFER_TOO_SMALL           (LONG)-35
#define STATUS_DEVICE_IO_ERROR            (LONG)-36
#define STATUS_UNRECOGNIZED_FILE_SYSTEM   (LONG)-37
#define STATUS_FILE_IS_DIRECTORY          (LONG)-38
#define STATUS_NOT_A_DIRECTORY            (LONG)-39
#define STATUS_VOLUME_CORRUPT             (LONG)-40
#define STATUS_VOLUME_FULL                (LONG)-41
#define STATUS_FILE_EXISTS                (LONG)-42
#define STATUS_PATH_NOT_FOUND             (LONG)-43
#define STATUS_RESOURCE_IN_USE            (LONG)-44
#define STATUS_NOT_ALIGNED                (LONG)-45
#define STATUS_OUT_OF_BOUNDS              (LONG)-46
#define STATUS_DATA_LENGTH_MISMATCH       (LONG)-47
#define STATUS_INVALID_OPCODE             (LONG)-48
#define STATUS_MALFORMED_DATA_STREAM      (LONG)-49
#define STATUS_MORE_PROCESSING_REQUIRED   (LONG)-50
#define STATUS_ARGUMENT_EXPECTED          (LONG)-51
#define STATUS_CONVERSION_FAILED          (LONG)-52
#define STATUS_DIVIDE_BY_ZERO             (LONG)-53
#define STATUS_UNEXPECTED_TYPE            (LONG)-54
#define STATUS_TIMEOUT                    (LONG)-55
#define STATUS_PARITY_ERROR               (LONG)-56
#define STATUS_NOT_READY                  (LONG)-57
#define STATUS_BUFFER_OVERRUN             (LONG)-58
#define STATUS_INVALID_CONFIGURATION      (LONG)-59
#define STATUS_NOT_STARTED                (LONG)-60
#define STATUS_OPERATION_CANCELLED        (LONG)-61
#define STATUS_NO_DATA_AVAILABLE          (LONG)-62
#define STATUS_BUFFER_FULL                (LONG)-63
#define STATUS_NOT_IMPLEMENTED            (LONG)-64
#define STATUS_SERIAL_HARDWARE_ERROR      (LONG)-65
#define STATUS_NOT_INITIALIZED            (LONG)-66
#define STATUS_NO_SUCH_THREAD             (LONG)-67
#define STATUS_NO_SUCH_PROCESS            (LONG)-68
#define STATUS_INVALID_ADDRESS            (LONG)-69
#define STATUS_NO_NETWORK_CONNECTION      (LONG)-70
#define STATUS_DESTINATION_UNREACHABLE    (LONG)-71
#define STATUS_CONNECTION_RESET           (LONG)-72
#define STATUS_CONNECTION_EXISTS          (LONG)-73
#define STATUS_CONNECTION_CLOSED          (LONG)-74
#define STATUS_TOO_MANY_CONNECTIONS       (LONG)-75
#define STATUS_ADDRESS_IN_USE             (LONG)-76
#define STATUS_NOT_A_SOCKET               (LONG)-77
#define STATUS_OPERATION_WOULD_BLOCK      (LONG)-78
#define STATUS_TRY_AGAIN                  (LONG)-79
#define STATUS_INVALID_SEQUENCE           (LONG)-80
#define STATUS_INTEGER_OVERFLOW           (LONG)-81
#define STATUS_PARENT_AWAITING_REMOVAL    (LONG)-82
#define STATUS_DEVICE_QUEUE_CLOSING       (LONG)-83
#define STATUS_CHECKSUM_MISMATCH          (LONG)-84
#define STATUS_NOT_A_TERMINAL             (LONG)-85
#define STATUS_DEVICE_NOT_CONNECTED       (LONG)-86
#define STATUS_DIRECTORY_NOT_EMPTY        (LONG)-87
#define STATUS_CROSS_DEVICE               (LONG)-88
#define STATUS_NO_MATCH                   (LONG)-89
#define STATUS_NOT_A_MOUNT_POINT          (LONG)-90
#define STATUS_NOT_MOUNTABLE              (LONG)-91
#define STATUS_NO_ELIGIBLE_CHILDREN       (LONG)-92
#define STATUS_MISSING_IMPORT             (LONG)-93
#define STATUS_TOO_MANY_HANDLES           (LONG)-94
#define STATUS_NOT_BLOCK_DEVICE           (LONG)-95
#define STATUS_NO_MEDIA                   (LONG)-96
#define STATUS_ALREADY_INITIALIZED        (LONG)-97
#define STATUS_INVALID_ADDRESS_RANGE      (LONG)-98
#define STATUS_NOT_SUPPORTED_BY_PROTOCOL  (LONG)-99
#define STATUS_MESSAGE_TOO_LONG           (LONG)-100
#define STATUS_NOT_CONNECTED              (LONG)-101
#define STATUS_DESTINATION_REQUIRED       (LONG)-102
#define STATUS_PERMISSION_DENIED          (LONG)-103
#define STATUS_SYMBOLIC_LINK_LOOP         (LONG)-104
#define STATUS_BROKEN_PIPE_SILENT         (LONG)-105
#define STATUS_NO_SUCH_DEVICE_OR_ADDRESS  (LONG)-106
#define STATUS_DOMAIN_NOT_SUPPORTED       (LONG)-107
#define STATUS_PROTOCOL_NOT_SUPPORTED     (LONG)-108
#define STATUS_DOMAIN_ERROR               (LONG)-109
#define STATUS_MEDIA_CHANGED              (LONG)-110
#define STATUS_DEADLOCK                   (LONG)-111
#define STATUS_RESTART_AFTER_SIGNAL       (LONG)-112
#define STATUS_RESTART_NO_SIGNAL          (LONG)-113

//
// ------------------------------------------------------ Data Type Definitions
//

typedef LONG KSTATUS;

//
// --------------------------------------------------------------------- Macros
//

#define KSUCCESS(_Status) ((_Status) == STATUS_SUCCESS)
#define KSTATUS_CODE(_Status) (-(_Status))

//
// ----------------------------------------------- Internal Function Prototypes
//

