/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/rtl.h>
#include "../../drivers/acpi/acpiobj.h"
#include "dbgext.h"

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

    ULONG ArgumentIndex;
    ULONG BytesRead;
    BOOL Result;
    ULONGLONG RootAddress;
    ULONGLONG RootAddressAddress;
    INT Success;

    RootAddress = 0;
    RootAddressAddress = 0;

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
                                sizeof(PACPI_OBJECT),
                                &RootAddress,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(PACPI_OBJECT))) {
            DbgOut("Error: Could not read root object at 0x%I64x.\n",
                   RootAddressAddress);

            return;
        }

        if (RootAddress == (UINTN)NULL) {
            DbgOut("ACPI Object root is NULL.\n");
            return;
        }

        DbgOut("%s: %I64x\n", ROOT_NAMESPACE_OBJECT_SYMBOL, RootAddress);
        ExtpPrintNamespaceAtRoot(Context, RootAddress, 0);

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

            ExtpPrintNamespaceAtRoot(Context, RootAddress, 0);
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
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out the ACPI namespace rooted at the given object.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the root object to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    PSTR Name;
    ACPI_OBJECT Object;
    PSTR Space;
    INT Status;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return EINVAL;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    Status = DbgReadMemory(Context,
                           TRUE,
                           Address,
                           sizeof(ACPI_OBJECT),
                           &Object,
                           &BytesRead);

    if ((Status != 0) || (BytesRead != sizeof(ACPI_OBJECT))) {
        DbgOut("Error: Could not read object at 0x%I64x.\n", Address);
        if (Status == 0) {
            Status = EINVAL;
        }

        return Status;
    }

    //
    // Print the object.
    //

    Name = (PSTR)(&(Object.Name));
    DbgOut("%08I64x %c%c%c%c ", Address, Name[0], Name[1], Name[2], Name[3]);
    Status = DbgPrintType(Context,
                          "ACPI_OBJECT_TYPE",
                          &(Object.Type),
                          sizeof(ACPI_OBJECT_TYPE));

    if (Status != 0) {
        DbgOut("OBJECTTYPE(%x)", Object.Type);
        return Status;
    }

    DbgOut(" ");
    switch (Object.Type) {
    case AcpiObjectInteger:
        DbgOut("Value: 0x%I64x", Object.U.Integer.Value);
        break;

    case AcpiObjectString:
        DbgOut("Address: %x", Object.U.String.String);
        break;

    case AcpiObjectBuffer:
        DbgOut("Buffer: %x Length: 0x%x",
               Object.U.Buffer.Buffer,
               Object.U.Buffer.Length);

        break;

    case AcpiObjectPackage:
        DbgOut("Array: %x ElementCount: 0x%x",
               Object.U.Package.Array,
               Object.U.Package.ElementCount);

        break;

    case AcpiObjectFieldUnit:
        DbgOut("OpRegion: %x (%I64x, %I64x)",
               Object.U.FieldUnit.OperationRegion,
               Object.U.FieldUnit.BitOffset,
               Object.U.FieldUnit.BitLength);

        break;

    case AcpiObjectMethod:
        DbgOut("%d Args, at %x length 0x%x",
               Object.U.Method.ArgumentCount,
               Object.U.Method.AmlCode,
               Object.U.Method.AmlCodeSize);

        break;

    case AcpiObjectOperationRegion:
        switch (Object.U.OperationRegion.Space) {
        case OperationRegionSystemMemory:
            Space = "SystemMemory";
            break;

        case OperationRegionSystemIo:
            Space = "SystemIO";
            break;

        case OperationRegionPciConfig:
            Space = "PCIConfig";
            break;

        case OperationRegionEmbeddedController:
            Space = "EmbeddedController";
            break;

        case OperationRegionSmBus:
            Space = "SMBus";
            break;

        case OperationRegionCmos:
            Space = "CMOS";
            break;

        case OperationRegionPciBarTarget:
            Space = "PCIBarTarget";
            break;

        case OperationRegionIpmi:
            Space = "IPMI";
            break;

        default:
            Space = "Unknown space";
            break;
        }

        DbgOut("(%s, 0x%I64x, 0x%I64x)",
               Space,
               Object.U.OperationRegion.Offset,
               Object.U.OperationRegion.Length);

        break;

    case AcpiObjectBufferField:
        DbgOut("Destination Object: %x, Bit Offset: 0x%I64x, Bit "
               "Length 0x%I64x",
               Object.U.BufferField.DestinationObject,
               Object.U.BufferField.BitOffset,
               Object.U.BufferField.BitLength);

        break;

    case AcpiObjectAlias:
        DbgOut("Destination: %x", Object.U.Alias.DestinationObject);
        break;

    default:
        break;
    }

    DbgOut("\n", Object.ReferenceCount);

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = Address + FIELD_OFFSET(ACPI_OBJECT, ChildListHead);
    CurrentEntryAddress = (UINTN)Object.ChildListHead.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               CurrentEntryAddress,
                               sizeof(LIST_ENTRY),
                               &CurrentEntryValue,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            if (Status == 0) {
                Status = EINVAL;
            }

            return Status;
        }

        //
        // Recurse down the tree.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(ACPI_OBJECT, SiblingListEntry);

        Status = ExtpPrintNamespaceAtRoot(Context,
                                          ChildObjectAddress,
                                          IndentationLevel);

        if (Status != 0) {
            return Status;
        }

        //
        // Move to the next child.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return 0;
}

