/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhid.c

Abstract:

    This module implements support for parsing HID descriptors.

Author:

    Evan Green 14-Mar-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "usbhidp.h"

//
// --------------------------------------------------------------------- Macros
//

#define UsbhidAllocate(_Size) UsbhidReallocate(NULL, _Size)
#define UsbhidFree(_Allocation) UsbhidReallocate(_Allocation, 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
UsbhidpResetParser (
    PUSB_HID_PARSER Parser
    );

KSTATUS
UsbhidpParse (
    PUSB_HID_PARSER Parser,
    PCUCHAR Data,
    UINTN Length
    );

PUSB_HID_ITEM
UsbhidpAllocateItem (
    PUSB_HID_PARSER Parser
    );

LONG
UsbhidpSignExtendItem (
    UCHAR Item,
    ULONG Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// This must be set by whoever is using the library before any HID parser
// functions are called.
//

PUSBHID_REALLOCATE UsbhidReallocate = NULL;

const UCHAR UsbhidItemSizes[4] = {0, 1, 2, 4};

//
// ------------------------------------------------------------------ Functions
//

USBHID_API
PUSB_HID_PARSER
UsbhidCreateParser (
    VOID
    )

/*++

Routine Description:

    This routine creates a new USB HID parser.

Arguments:

    None.

Return Value:

    Returns a pointer to the new parser on success.

    NULL on allocation failure.

--*/

{

    PUSB_HID_PARSER Parser;

    Parser = UsbhidAllocate(sizeof(USB_HID_PARSER));
    if (Parser == NULL) {
        return NULL;
    }

    RtlZeroMemory(Parser, sizeof(USB_HID_PARSER));
    return Parser;
}

USBHID_API
VOID
UsbhidDestroyParser (
    PUSB_HID_PARSER Parser
    )

/*++

Routine Description:

    This routine destroys a HID parser.

Arguments:

    Parser - Supplies a pointer to the HID parser to destroy.

Return Value:

    None.

--*/

{

    if (Parser == NULL) {
        return;
    }

    if (Parser->Items != NULL) {
        UsbhidFree(Parser->Items);
    }

    UsbhidFree(Parser);
    return;
}

USBHID_API
KSTATUS
UsbhidParseReportDescriptor (
    PUSB_HID_PARSER Parser,
    PCUCHAR Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine parses a HID report descriptor.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Data - Supplies a pointer to the report descriptor.

    Length - Supplies the size of the descriptor array in bytes.

Return Value:

    Status code.

--*/

{

    UsbhidpResetParser(Parser);
    return UsbhidpParse(Parser, Data, Length);
}

USBHID_API
VOID
UsbhidReadReport (
    PUSB_HID_PARSER Parser,
    PCUCHAR Report,
    UINTN Length
    )

/*++

Routine Description:

    This routine reads all fields of a report into their respective items.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Report - Supplies a pointer to the raw report.

    Length - Supplies the size of the report in bytes.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < Parser->ItemCount; Index += 1) {
        UsbhidReadItemData(Report, Length, &(Parser->Items[Index]));
    }

    return;
}

USBHID_API
VOID
UsbhidWriteReport (
    PUSB_HID_PARSER Parser,
    PUCHAR Report,
    UINTN Length
    )

/*++

Routine Description:

    This routine writes all fields of a report into a raw buffer.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Report - Supplies a pointer to the raw report.

    Length - Supplies the size of the report in bytes.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < Parser->ItemCount; Index += 1) {
        UsbhidWriteItemData(&(Parser->Items[Index]), Report, Length);
    }

    return;
}

USBHID_API
KSTATUS
UsbhidReadItemData (
    PCUCHAR Report,
    ULONG ReportSize,
    PUSB_HID_ITEM Item
    )

/*++

Routine Description:

    This routine reads the contents from a report into the item value.

Arguments:

    Report - Supplies a pointer to the raw report.

    ReportSize - Supplies the size of the report in bytes.

    Item - Supplies a pointer to the item.

Return Value:

    STATUS_SUCCESS if the data was read.

    STATUS_DATA_LENGTH_MISMATCH if the report is not large enough for the item.

    STATUS_NO_MATCH if this report's ID does not correspond to this item.

--*/

{

    ULONG BitIndex;
    ULONG BitMask;
    PCUCHAR Bytes;
    ULONG CurrentBit;
    ULONG FieldEnd;
    KSTATUS Status;

    if (ReportSize == 0) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (Item->ReportId != 0) {
        if (Item->ReportId != Report[0]) {
            return STATUS_NO_MATCH;
        }

        Report += 1;
        ReportSize -= 1;
    }

    Item->PreviousValue = Item->Value;
    Item->Value = 0;
    FieldEnd = ALIGN_RANGE_UP(Item->BitOffset + Item->Properties.BitSize,
                              BITS_PER_BYTE) / BITS_PER_BYTE;

    if (FieldEnd > ReportSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    //
    // Try to do a reasonable read.
    //

    if ((Item->BitOffset % BITS_PER_BYTE) == 0) {
        Bytes = Report + Item->BitOffset / BITS_PER_BYTE;
        switch (Item->Properties.BitSize) {
        case BITS_PER_BYTE:
            Item->Value = *Bytes;
            Status = STATUS_SUCCESS;
            goto ReadItemDataEnd;

        case BITS_PER_BYTE * 2:
            Item->Value = READ_UNALIGNED16(Bytes);
            Status = STATUS_SUCCESS;
            goto ReadItemDataEnd;

        case BITS_PER_BYTE * 4:
            Item->Value = READ_UNALIGNED32(Bytes);
            Status = STATUS_SUCCESS;
            goto ReadItemDataEnd;

        default:
            break;
        }
    }

    //
    // Read the field out a bit at a time.
    //

    CurrentBit = Item->BitOffset;
    BitMask = 0x1;
    for (BitIndex = 0; BitIndex < Item->Properties.BitSize; BitIndex += 1) {
        if ((Report[CurrentBit / BITS_PER_BYTE] &
             (1 << (CurrentBit % BITS_PER_BYTE))) != 0) {

            Item->Value |= BitMask;
        }

        CurrentBit += 1;
        BitMask <<= 1;
    }

    Status = STATUS_SUCCESS;

ReadItemDataEnd:

    //
    // Sign extend the item.
    //

    if ((Item->Value & Item->SignBit) != 0) {
        Item->Value |= ~(Item->SignBit - 1);
    }

    return Status;
}

USBHID_API
KSTATUS
UsbhidWriteItemData (
    PUSB_HID_ITEM Item,
    PUCHAR Report,
    ULONG ReportSize
    )

/*++

Routine Description:

    This routine writes the contents from an item into the raw report bytes.

Arguments:

    Item - Supplies a pointer to the item with the value to write.

    Report - Supplies a pointer to the raw report.

    ReportSize - Supplies the size of the report in bytes.

Return Value:

    STATUS_SUCCESS if the data was read.

    STATUS_DATA_LENGTH_MISMATCH if the report is not large enough for the item.

    STATUS_NO_MATCH if this report's ID does not correspond to this item.

--*/

{

    ULONG BitIndex;
    ULONG BitMask;
    PUCHAR Bytes;
    ULONG CurrentBit;
    ULONG FieldEnd;

    if (ReportSize == 0) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (Item->ReportId != 0) {
        Report[0] = Item->ReportId;
        Report += 1;
        ReportSize -= 1;
    }

    Item->PreviousValue = Item->Value;
    FieldEnd = ALIGN_RANGE_UP(Item->BitOffset + Item->Properties.BitSize,
                              BITS_PER_BYTE) / BITS_PER_BYTE;

    if (FieldEnd > ReportSize) {
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    //
    // Try to do a reasonable write.
    //

    if ((Item->BitOffset % BITS_PER_BYTE) == 0) {
        Bytes = Report + Item->BitOffset / BITS_PER_BYTE;
        switch (Item->Properties.BitSize) {
        case BITS_PER_BYTE:
            *Bytes = Item->Value;
            return STATUS_SUCCESS;

        case BITS_PER_BYTE * 2:
            WRITE_UNALIGNED16(Bytes, (USHORT)Item->Value);
            return STATUS_SUCCESS;

        case BITS_PER_BYTE * 4:
            WRITE_UNALIGNED32(Bytes, Item->Value);
            return STATUS_SUCCESS;

        default:
            break;
        }
    }

    //
    // Write the report a bit at a time.
    //

    CurrentBit = Item->BitOffset;
    BitMask = 0x1;
    for (BitIndex = 0; BitIndex < Item->Properties.BitSize; BitIndex += 1) {
        if ((Item->Value & (1 << (CurrentBit % BITS_PER_BYTE))) != 0) {
            Report[CurrentBit / BITS_PER_BYTE] |= BitMask;
        }

        CurrentBit += 1;
        BitMask <<= 1;
    }

    return STATUS_SUCCESS;
}

USBHID_API
ULONG
UsbhidGetReportSize (
    PUSB_HID_PARSER Parser,
    UCHAR ReportId,
    USB_HID_DATA_TYPE Type
    )

/*++

Routine Description:

    This routine determines the length of the USB HID Report of the given type.

Arguments:

    Parser - Supplies a pointer to the parsed HID report.

    ReportId - Supplies the report ID. Supply 0 if the reports have no ID.

    Type - Supplies the type of report to get.

Return Value:

    Returns the number of bytes in the given type of report.

--*/

{

    ULONG ReportIndex;
    ULONG Size;

    for (ReportIndex = 0; ReportIndex < Parser->ReportCount; ReportIndex += 1) {
        if (Parser->ReportSizes[ReportIndex].ReportId == ReportId) {
            Size = Parser->ReportSizes[ReportIndex].Sizes[Type];
            return ALIGN_RANGE_UP(Size, BITS_PER_BYTE) / BITS_PER_BYTE;
        }
    }

    return 0;
}

USBHID_API
PUSB_HID_ITEM
UsbhidFindItem (
    PUSB_HID_PARSER Parser,
    UCHAR ReportId,
    USB_HID_DATA_TYPE Type,
    PUSB_HID_USAGE Usage,
    PUSB_HID_ITEM StartFrom
    )

/*++

Routine Description:

    This routine determines the length of the USB HID Report of the given type.

Arguments:

    Parser - Supplies a pointer to the parsed HID report.

    ReportId - Supplies the report ID. Supply 0 if the reports have no ID, or
        to find an item from any ID.

    Type - Supplies the type of item to search for.

    Usage - Supplies the usage to search for.

    StartFrom - Supplies an item to start the search from, exclusive of the
        item itself. This can be used to find the next occurrence of the
        given usage. Supply NULL to start from the beginning.

Return Value:

    Returns a pointer to the found item on success.

    NULL if the item could not be found.

--*/

{

    PUSB_HID_ITEM Item;
    ULONG ItemIndex;

    //
    // If no usage were specified, just return the first or next item.
    //

    if (Usage == NULL) {
        if (Parser->ItemCount == 0) {
            return NULL;
        }

        if (StartFrom == NULL) {
            return Parser->Items;
        }

        if (StartFrom + 1 >= Parser->Items + Parser->ItemCount) {
            return StartFrom + 1;
        }
    }

    for (ItemIndex = 0; ItemIndex < Parser->ItemCount; ItemIndex += 1) {
        Item = &(Parser->Items[ItemIndex]);
        if ((Item->Properties.Usage.Page == Usage->Page) &&
            (Item->Properties.Usage.Value == Usage->Value) &&
            ((ReportId == 0) || (Item->ReportId == ReportId))) {

            return Item;
        }
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
UsbhidpResetParser (
    PUSB_HID_PARSER Parser
    )

/*++

Routine Description:

    This routine resets a USB hid parser.

Arguments:

    Parser - Supplies a pointer to the HID parser.

Return Value:

    None.

--*/

{

    if (Parser->ReportCount == 0) {
        Parser->ReportCount = 1;
    }

    Parser->StateCount = 1;
    Parser->UsageCount = 0;
    Parser->UsageLimits.Minimum = 0;
    Parser->UsageLimits.Maximum = 0;
    Parser->CollectionPathCount = 0;
    RtlZeroMemory(&(Parser->State[0]), sizeof(Parser->State[0]));
    RtlZeroMemory(&(Parser->ReportSizes[0]), sizeof(Parser->ReportSizes[0]));
    return;
}

KSTATUS
UsbhidpParse (
    PUSB_HID_PARSER Parser,
    PCUCHAR Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine parses a HID report descriptor.

Arguments:

    Parser - Supplies a pointer to the HID parser.

    Data - Supplies a pointer to the report description.

    Length - Supplies the length of the report description in bytes.

Return Value:

    Status code.

--*/

{

    PCUCHAR Bytes;
    PUSB_HID_COLLECTION_PATH CollectionPath;
    PCUCHAR End;
    ULONG Index;
    UCHAR Item;
    ULONG ItemData;
    ULONG ItemIndex;
    PUSB_HID_ITEM NewItem;
    PUSB_HID_COLLECTION_PATH NewPath;
    PUSB_HID_REPORT_SIZES ReportSizes;
    PUSB_HID_STATE State;
    KSTATUS Status;

    Bytes = Data;
    End = Bytes + Length;
    State = &(Parser->State[Parser->StateCount - 1]);
    ReportSizes = &(Parser->ReportSizes[Parser->ReportCount - 1]);
    CollectionPath = NULL;
    if (Parser->CollectionPathCount != 0) {
        CollectionPath =
                    &(Parser->CollectionPath[Parser->CollectionPathCount - 1]);
    }

    //
    // Loop processing local, global, and main elements until a field is
    // found.
    //

    while (Bytes < End) {
        Item = *Bytes;
        Bytes += 1;
        if (Bytes + UsbhidItemSizes[Item & USB_HID_REPORT_ITEM_SIZE_MASK] >
            End) {

            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ParseEnd;
        }

        switch (Item & USB_HID_REPORT_ITEM_SIZE_MASK) {
        case USB_HID_REPORT_ITEM_SIZE_1:
            ItemData = *Bytes;
            break;

        case USB_HID_REPORT_ITEM_SIZE_2:
            ItemData = READ_UNALIGNED16(Bytes);
            break;

        case USB_HID_REPORT_ITEM_SIZE_4:
            ItemData = READ_UNALIGNED32(Bytes);
            break;

        default:
            ItemData = 0;
            break;
        }

        Bytes += UsbhidItemSizes[Item & USB_HID_REPORT_ITEM_SIZE_MASK];

        //
        // Switch on the combination of tag and type.
        //

        switch (Item & USB_HID_ITEM_MASK) {
        case USB_HID_ITEM_USAGE_PAGE:
            if ((Item & USB_HID_REPORT_ITEM_SIZE_MASK) ==
                USB_HID_REPORT_ITEM_SIZE_4) {

                State->Properties.Usage.Page = ItemData >> 16;

            } else {
                State->Properties.Usage.Page = ItemData;
            }

            break;

        case USB_HID_ITEM_LOGICAL_MINIMUM:
            State->Properties.LogicalLimit.Minimum =
                                         UsbhidpSignExtendItem(Item, ItemData);

            break;

        case USB_HID_ITEM_LOGICAL_MAXIMUM:
            State->Properties.LogicalLimit.Maximum =
                                         UsbhidpSignExtendItem(Item, ItemData);

            break;

        case USB_HID_ITEM_PHYSICAL_MINIMUM:
            State->Properties.PhysicalLimit.Minimum =
                                         UsbhidpSignExtendItem(Item, ItemData);

            break;

        case USB_HID_ITEM_PHYSICAL_MAXIMUM:
            State->Properties.PhysicalLimit.Maximum =
                                         UsbhidpSignExtendItem(Item, ItemData);

            break;

        case USB_HID_ITEM_UNIT_EXPONENT:
            State->Properties.Unit.Exponent = ItemData;
            break;

        case USB_HID_ITEM_UNIT:
            State->Properties.Unit.Type = ItemData;
            break;

        case USB_HID_ITEM_REPORT_SIZE:
            State->Properties.BitSize = ItemData;
            break;

        case USB_HID_ITEM_REPORT_ID:
            State->ReportId = ItemData;

            //
            // If this is not the first report ID seen, then switch to the
            // specified report, or create a new one.
            //

            if (Parser->HasReportIds != FALSE) {
                ReportSizes = NULL;
                for (Index = 0; Index < Parser->ReportCount; Index += 1) {
                    if (Parser->ReportSizes[Index].ReportId ==
                        State->ReportId) {

                        ReportSizes = &(Parser->ReportSizes[Index]);
                        break;
                    }
                }

                if (ReportSizes == NULL) {
                    if (Parser->ReportCount >= USB_HID_MAX_REPORT_IDS) {
                        Status = STATUS_BUFFER_OVERRUN;
                        goto ParseEnd;
                    }

                    ReportSizes = &(Parser->ReportSizes[Index]);
                    Parser->ReportCount += 1;
                    RtlZeroMemory(ReportSizes, sizeof(USB_HID_REPORT_SIZES));
                }
            }

            //
            // Save the report ID in the current sizes, which may either be
            // a previously found one, a newly allocated one, or the very
            // first one (in which case it was initialized to offset zero).
            //

            Parser->HasReportIds = TRUE;
            ReportSizes->ReportId = State->ReportId;
            break;

        case USB_HID_ITEM_REPORT_COUNT:
            State->ReportCount = ItemData;
            break;

        case USB_HID_ITEM_PUSH:
            if (Parser->StateCount >= USB_HID_STATE_STACK_SIZE) {
                Status = STATUS_BUFFER_OVERRUN;
                goto ParseEnd;
            }

            RtlCopyMemory(State + 1, State, sizeof(USB_HID_STATE));
            Parser->StateCount += 1;
            State += 1;
            break;

        case USB_HID_ITEM_POP:
            if (Parser->StateCount == 0) {
                Status = STATUS_INVALID_SEQUENCE;
                goto ParseEnd;
            }

            Parser->StateCount -= 1;
            State -= 1;
            break;

        //
        // Usage is local, so push it.
        //

        case USB_HID_ITEM_USAGE:
            if ((Item & USB_HID_REPORT_ITEM_SIZE_MASK) ==
                USB_HID_REPORT_ITEM_SIZE_4) {

                ItemData >>= 16;
            }

            if (Parser->UsageCount >= USB_HID_MAX_USAGE_QUEUE) {
                Status = STATUS_BUFFER_OVERRUN;
                goto ParseEnd;
            }

            Parser->UsageQueue[Parser->UsageCount] = ItemData;
            Parser->UsageCount += 1;
            break;

        case USB_HID_ITEM_USAGE_MINIMUM:
            Parser->UsageLimits.Minimum = ItemData;
            break;

        case USB_HID_ITEM_USAGE_MAXIMUM:
            Parser->UsageLimits.Maximum = ItemData;
            break;

        //
        // Define the main items, starting with collections.
        //

        case USB_HID_ITEM_COLLECTION:
            if (CollectionPath == NULL) {
                NewPath = &(Parser->CollectionPath[0]);
                NewPath->Parent = NULL;

            } else {
                if (Parser->CollectionPathCount >=
                    USB_HID_MAX_COLLECTION_STACK) {

                    Status = STATUS_BUFFER_OVERRUN;
                    goto ParseEnd;
                }

                NewPath =
                        &(Parser->CollectionPath[Parser->CollectionPathCount]);

                NewPath->Parent = CollectionPath;
            }

            Parser->CollectionPathCount += 1;
            NewPath->Type = ItemData;
            NewPath->Usage = State->Properties.Usage;
            CollectionPath = NewPath;

            //
            // Pop the last usage if possible.
            //

            if (Parser->UsageCount != 0) {
                CollectionPath->Usage.Value = Parser->UsageQueue[0];
                for (Index = 1; Index < Parser->UsageCount; Index += 1) {
                    Parser->UsageQueue[Index - 1] = Parser->UsageQueue[Index];
                }

                Parser->UsageCount -= 1;

            } else if (Parser->UsageLimits.Minimum <=
                       Parser->UsageLimits.Maximum) {

                CollectionPath->Usage.Value = Parser->UsageLimits.Minimum;
                Parser->UsageLimits.Minimum += 1;
            }

            break;

        case USB_HID_ITEM_END_COLLECTION:
            if (CollectionPath == NULL) {
                Status = STATUS_INVALID_SEQUENCE;
                goto ParseEnd;
            }

            CollectionPath = CollectionPath->Parent;
            Parser->CollectionPathCount -= 1;
            break;

        //
        // The big enchilada, a data item. This pulls together all the
        // attributes so far into an actual field of some kind.
        //

        case USB_HID_ITEM_INPUT:
        case USB_HID_ITEM_OUTPUT:
        case USB_HID_ITEM_FEATURE:

            //
            // Create data items for each report if this is an array.
            //

            for (ItemIndex = 0;
                 ItemIndex < State->ReportCount;
                 ItemIndex += 1) {

                NewItem = UsbhidpAllocateItem(Parser);
                if (NewItem == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto ParseEnd;
                }

                RtlCopyMemory(&(NewItem->Properties),
                              &(State->Properties),
                              sizeof(USB_HID_ITEM_PROPERTIES));

                NewItem->Flags = ItemData;
                NewItem->CollectionPath = CollectionPath;
                NewItem->ReportId = State->ReportId;

                //
                // Compute the sign extension bit. Originally this was only
                // done if the logical minimum was less than zero, but
                // the VMWare mouse for instance reports a range of 0-32767,
                // and then returns negative data like 65535.
                //

                if ((NewItem->Properties.BitSize <
                     (sizeof(ULONG) * BITS_PER_BYTE)) &&
                    (NewItem->Properties.BitSize > 1)) {

                    NewItem->SignBit = 1 << (NewItem->Properties.BitSize - 1);
                }

                if (Parser->UsageCount != 0) {
                    NewItem->Properties.Usage.Value = Parser->UsageQueue[0];
                    for (Index = 1; Index < Parser->UsageCount; Index += 1) {
                        Parser->UsageQueue[Index - 1] =
                                                     Parser->UsageQueue[Index];
                    }

                    Parser->UsageCount -= 1;

                } else if (Parser->UsageLimits.Minimum <=
                           Parser->UsageLimits.Maximum) {

                    NewItem->Properties.Usage.Value =
                                                   Parser->UsageLimits.Minimum;

                    Parser->UsageLimits.Minimum += 1;
                }

                switch (Item & USB_HID_REPORT_ITEM_TAG_MASK) {
                case USB_HID_ITEM_INPUT:
                    NewItem->Type = UsbhidDataInput;
                    break;

                case USB_HID_ITEM_OUTPUT:
                    NewItem->Type = UsbhidDataOutput;
                    break;

                case USB_HID_ITEM_FEATURE:
                default:
                    NewItem->Type = UsbhidDataFeature;
                    break;
                }

                NewItem->BitOffset = ReportSizes->Sizes[NewItem->Type];
                ReportSizes->Sizes[NewItem->Type] +=
                                                   NewItem->Properties.BitSize;
            }

            break;

        //
        // Skip unsupported long items.
        //

        case USB_HID_ITEM_LONG:
            if (ItemData > End - Bytes) {
                Status = STATUS_BUFFER_OVERRUN;
                goto ParseEnd;
            }

            Bytes += ItemData;
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            goto ParseEnd;
        }

        if ((Item & USB_HID_REPORT_ITEM_TYPE_MASK) ==
            USB_HID_REPORT_ITEM_MAIN) {

            Parser->UsageLimits.Minimum = 0;
            Parser->UsageLimits.Maximum = 0;
            Parser->UsageCount = 0;
        }
    }

    Status = STATUS_SUCCESS;

ParseEnd:
    return Status;
}

PUSB_HID_ITEM
UsbhidpAllocateItem (
    PUSB_HID_PARSER Parser
    )

/*++

Routine Description:

    This routine returns a new item from the HID parser.

Arguments:

    Parser - Supplies a pointer to the HID parser.

Return Value:

    Returns a pointer to the HID item on success.

    NULL on allocation failure.

--*/

{

    PVOID NewBuffer;
    USHORT NewCapacity;

    if (Parser->ItemCount >= Parser->ItemCapacity) {
        if (Parser->ItemCapacity >= USB_HID_MAX_ITEMS) {
            return NULL;
        }

        NewCapacity = Parser->ItemCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = 16;
        }

        NewBuffer = UsbhidReallocate(Parser->Items,
                                     NewCapacity * sizeof(USB_HID_ITEM));

        if (NewBuffer == NULL) {
            return NULL;
        }

        Parser->Items = NewBuffer;
        Parser->ItemCapacity = NewCapacity;
    }

    Parser->ItemCount += 1;
    return &(Parser->Items[Parser->ItemCount - 1]);
}

LONG
UsbhidpSignExtendItem (
    UCHAR Item,
    ULONG Value
    )

/*++

Routine Description:

    This routine potentially sign extends a value depending on if its high bit
    is set.

Arguments:

    Item - Supplies the item size (in report form).

    Value - Supplies the raw value plucked out of the report.

Return Value:

    Returns the potentially sign extended item.

--*/

{

    switch (UsbhidItemSizes[Item & USB_HID_REPORT_ITEM_SIZE_MASK]) {
    case 1:
        if ((Value & 0x80) != 0) {
            Value |= 0xFFFFFF00;
        }

        break;

    case 2:
        if ((Value & 0x8000) != 0) {
            Value |= 0xFFFF0000;
        }

        break;

    default:
        break;
    }

    return (LONG)Value;
}

