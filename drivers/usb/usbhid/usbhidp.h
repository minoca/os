/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhidp.h

Abstract:

    This header contains internal definitions for the USB HID parser.

Author:

    Evan Green 14-Mar-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#define USBHID_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhid.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of nested push operations supported.
//

#define USB_HID_STATE_STACK_SIZE 5

//
// Define the maximum number of different report IDs supported.
//

#define USB_HID_MAX_REPORT_IDS 20

//
// Define the maximum depth of the usage queue.
//

#define USB_HID_MAX_USAGE_QUEUE 32

//
// Define the maximum depth of the collection path stack.
//

#define USB_HID_MAX_COLLECTION_STACK 10

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
PVOID
(*PUSBHID_REALLOCATE) (
    PVOID Allocation,
    UINTN Size
    );

/*++

Routine Description:

    This routine is called to allocate, reallocate, or free memory.

Arguments:

    Context - Supplies a pointer to the context passed into the parser.

    Allocation - Supplies an optional pointer to an existing allocation to
        either reallocate or free. If NULL, then a new allocation is being
        requested.

    Size - Supplies the size of the allocation request, in bytes. If this is
        non-zero, then an allocation or reallocation is being requested. If
        this is is 0, then the given memory should be freed.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure or free.

--*/

/*++

Structure Description:

    This structure stores the bit sizes of each type of data (input, output,
    and feature) for each report.

Members:

    ReportId - Stores the report ID in question.

    Sizes - Stores the current size in bits for each data type.

--*/

typedef struct _USB_HID_REPORT_SIZES {
    UCHAR ReportId;
    USHORT Sizes[UsbhidDataTypeCount];
} USB_HID_REPORT_SIZES, *PUSB_HID_REPORT_SIZES;

/*++

Structure Description:

    This structure stores the current state of the parser.

Members:

    Properties - Stores the current set of properties being defined.

    ReportCount - Stores the number of reports in the descriptor.

    ReportId - Stores the current report ID being parsed.

--*/

typedef struct _USB_HID_STATE {
    USB_HID_ITEM_PROPERTIES Properties;
    UCHAR ReportCount;
    UCHAR ReportId;
} USB_HID_STATE, *PUSB_HID_STATE;

/*++

Structure Description:

    This structure stores the internal state of the USB HID parser.

Members:

    State - Stores the stack of current states.

    ReportSizes - Stores the report data sizes for all reports.

    UsageQueue - Stores the queue of local usages.

    CollectionPath - Stores the path of collection paths.

    StateCount - Stores the number of saved states.

    HasReportIds - Stores a boolean indicating whether or not report IDs have
        been used anywhere. If so, then every report is prefixed with a report
        ID byte.

    ReportCount - Stores the number of different report IDs.

    UsageCount - Stores the size of the usage stack.

    CollectionPathCount - Stores the number of collection paths in the stack.

    UsageLimits - Stores the usage limits.

    Items - Stores a pointer to the array of HID items.

    ItemCount - Stores the number of valid items in the array.

    ItemCapacity - Stores the maximum capacity of the items array before it
        will need to be reallocated.

--*/

struct _USB_HID_PARSER {
    USB_HID_STATE State[USB_HID_STATE_STACK_SIZE];
    USB_HID_REPORT_SIZES ReportSizes[USB_HID_MAX_REPORT_IDS];
    USHORT UsageQueue[USB_HID_MAX_USAGE_QUEUE];
    USB_HID_COLLECTION_PATH CollectionPath[USB_HID_MAX_COLLECTION_STACK];
    USHORT StateCount;
    USHORT ReportCount;
    BOOL HasReportIds;
    USHORT UsageCount;
    USHORT CollectionPathCount;
    USB_HID_LIMITS UsageLimits;
    PUSB_HID_ITEM Items;
    USHORT ItemCount;
    USHORT ItemCapacity;
};

//
// -------------------------------------------------------------------- Globals
//

//
// This must be set by whoever is using the library before any HID parser
// functions are called.
//

extern PUSBHID_REALLOCATE UsbhidReallocate;

//
// -------------------------------------------------------- Function Prototypes
//
