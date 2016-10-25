/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpiext.c

Abstract:

    This module implements ACPI related debugger extensions.

Author:

    Evan Green 27-Nov-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define away KERNEL_API to avoid it being defined as an import or export.
//

#define KERNEL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "../../../drivers/acpi/acpiobj.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define ROOT_NAMESPACE_OBJECT_SYMBOL "acpi!AcpiNamespaceRoot"

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ExtAcpiNamespace (
    PDEBUGGER_CONTEXT Context,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    );

INT
ExtpPrintNamespaceAtRoot (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONGLONG NextSibling,
    ULONG IndentationLevel
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExtAcpi (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine implements the ACPI debugger extension.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

{

    if (Command == NULL) {
        DbgOut("Error: A valid subcommand must be supplied. Try one of these:\n"
               "\t!acpi.ns\n\n");

        return EINVAL;
    }

    if (strcmp(Command, "ns") == 0) {
        ExtAcpiNamespace(Context, ArgumentCount - 1, ArgumentValues + 1);

    } else {
        DbgOut("Error: A valid subcommand must be supplied. Try one of these:\n"
               "\t!acpi.ns\n\n");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ExtAcpiNamespace (
    PDEBUGGER_CONTEXT Context,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine implements the ACPI namespace debugger extension.

Arguments:

    Context - Supplies a pointer to the debugger application context.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    None.

--*/

{

    ULONG AddressSize;
    ULONG ArgumentIndex;
    ULONG BytesRead;
    BOOL Result;
    ULONGLONG RootAddress;
    ULONGLONG RootAddressAddress;
    INT Success;

    RootAddress = 0;
    RootAddressAddress = 0;
    AddressSize = DbgGetTargetPointerSize(Context);

    //
    // If there are no arguments, try to find the root.
    //

    if (ArgumentCount == 0) {
        Result = DbgEvaluate(Context,
                             ROOT_NAMESPACE_OBJECT_SYMBOL,
                             &RootAddressAddress);

        if (Result != 0) {
            DbgOut("Error: Could not evaluate %s\n",
                   ROOT_NAMESPACE_OBJECT_SYMBOL);

            return;
        }

        //
        // Now given the address of the pointer, read the value to get the
        // actual address of the root object.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                RootAddressAddress,
                                AddressSize,
                                &RootAddress,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != AddressSize)) {
            DbgOut("Error: Could not read root object at 0x%I64x.\n",
                   RootAddressAddress);

            return;
        }

        if (RootAddress == (UINTN)NULL) {
            DbgOut("ACPI Object root is NULL.\n");
            return;
        }

        DbgOut("%s: %I64x\n", ROOT_NAMESPACE_OBJECT_SYMBOL, RootAddress);
        ExtpPrintNamespaceAtRoot(Context, RootAddress, NULL, 0);

    } else {

        //
        // Loop through each argument, evaluate the address, and print the
        // namespace tree at that object.
        //

        for (ArgumentIndex = 0;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Result = DbgEvaluate(Context,
                                 ArgumentValues[ArgumentIndex],
                                 &RootAddress);

            if (Result != 0) {
                DbgOut("Failed to evaluate address at \"%s\".\n",
                       ArgumentValues[ArgumentIndex]);
            }

            ExtpPrintNamespaceAtRoot(Context, RootAddress, NULL, 0);
            if (ArgumentIndex != ArgumentCount - 1) {
                DbgOut("\n----");
            }
        }
    }

    return;
}

INT
ExtpPrintNamespaceAtRoot (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONGLONG NextSibling,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out the ACPI namespace rooted at the given object.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the root object to print.

    NextSibling - Supplies an optional pointer where the next sibling pointer
        will be returned.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PTYPE_SYMBOL AcpiObjectType;
    ULONGLONG ChildListHead;
    ULONG ChildListOffset;
    PVOID Data;
    ULONG DataSize;
    ULONG ListEntryOffset;
    ULONG Name;
    INT Status;
    ULONGLONG Value;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return EINVAL;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadTypeByName(Context,
                               Address,
                               "acpi!ACPI_OBJECT",
                               &AcpiObjectType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read object at 0x%I64x: %s.\n",
               Address,
               strerror(Status));

        goto PrintNamespaceAtRootEnd;
    }

    //
    // Print the object.
    //

    Status = DbgReadIntegerMember(Context,
                                  AcpiObjectType,
                                  "Name",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintNamespaceAtRootEnd;
    }

    Name = Value;
    DbgOut("%08I64x %c%c%c%c ",
           Address,
           (UCHAR)Name,
           (UCHAR)(Name >> 8),
           (UCHAR)(Name >> 16),
           (UCHAR)(Name >> 24));

    Status = DbgPrintTypeMember(Context,
                                Address,
                                Data,
                                DataSize,
                                AcpiObjectType,
                                "Type",
                                0,
                                0);

    if (Status != 0) {
        goto PrintNamespaceAtRootEnd;
    }

    DbgOut(" ");
    Status = DbgReadIntegerMember(Context,
                                  AcpiObjectType,
                                  "Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    switch (Value) {
    case AcpiObjectInteger:
        DbgOut("Value: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Integer.Value",
                                    0,
                                    0);

        break;

    case AcpiObjectString:
        DbgOut("Address: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.String.String",
                                    0,
                                    0);

        break;

    case AcpiObjectBuffer:
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Buffer.Buffer",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(" Length: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Buffer.Length",
                                    0,
                                    0);

        break;

    case AcpiObjectPackage:
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Package.Array",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(" Count: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Package.ElementCount",
                                    0,
                                    0);

        break;

    case AcpiObjectFieldUnit:
        DbgOut("OpRegion ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.FieldUnit.OperationRegion",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(" ( ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.FieldUnit.BitOffset",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.FieldUnit.BitLength",
                                    0,
                                    0);

        DbgOut(")");
        break;

    case AcpiObjectMethod:
        DbgOut("Args: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Method.ArgumentCount",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", at ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Method.AmlCode",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(" length ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Method.AmlCodeSize",
                                    0,
                                    0);

        break;

    case AcpiObjectOperationRegion:
        DbgOut("(");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.OperationRegion.Space",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.OperationRegion.Offset",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.OperationRegion.Length",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(")");
        break;

    case AcpiObjectBufferField:
        DbgOut("Destination Object: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.BufferField.DestinationObject",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", Bit Offset: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.BufferField.BitOffset",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        DbgOut(", Bit Length ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.BufferField.BitLength",
                                    0,
                                    0);

        if (Status != 0) {
            break;
        }

        break;

    case AcpiObjectAlias:
        DbgOut("Destination: ");
        Status = DbgPrintTypeMember(Context,
                                    Address,
                                    Data,
                                    DataSize,
                                    AcpiObjectType,
                                    "U.Alias.DestinationObject",
                                    0,
                                    0);

        break;

    default:
        break;
    }

    DbgOut("\n");
    if (Status != 0) {
        goto PrintNamespaceAtRootEnd;
    }

    //
    // Get offsets into the structure for the list head and list entries.
    //

    IndentationLevel += 1;
    Status = DbgGetMemberOffset(AcpiObjectType,
                                "ChildListHead",
                                &ChildListOffset,
                                NULL);

    if (Status != 0) {
        goto PrintNamespaceAtRootEnd;
    }

    ChildListHead = Address + (ChildListOffset / BITS_PER_BYTE);
    Status = DbgGetMemberOffset(AcpiObjectType,
                                "SiblingListEntry",
                                &ListEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintNamespaceAtRootEnd;
    }

    ListEntryOffset /= BITS_PER_BYTE;

    //
    // Read the sibling list entry's next pointer for the caller.
    //

    if (NextSibling != NULL) {
        Status = DbgReadIntegerMember(Context,
                                      AcpiObjectType,
                                      "SiblingListEntry.Next",
                                      Address,
                                      Data,
                                      DataSize,
                                      NextSibling);

        if (Status != 0) {
            goto PrintNamespaceAtRootEnd;
        }
    }

    //
    // Read the first element on the child list.
    //

    Status = DbgReadIntegerMember(Context,
                                  AcpiObjectType,
                                  "ChildListHead.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Address);

    free(Data);
    Data = NULL;

    //
    // Loop printing all children.
    //

    while (Address != ChildListHead) {
        Address -= ListEntryOffset;
        Status = ExtpPrintNamespaceAtRoot(Context,
                                          Address,
                                          &Address,
                                          IndentationLevel);

        if (Status != 0) {
            goto PrintNamespaceAtRootEnd;
        }
    }

PrintNamespaceAtRootEnd:
    if (Data != NULL) {
        free(Data);
    }

    return Status;
}

