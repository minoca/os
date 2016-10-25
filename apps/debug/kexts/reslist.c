/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reslist.c

Abstract:

    This module implements resource list debugger extensions.

Author:

    Evan Green 12-Dec-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/debug/dbgext.h>
#include "../../../kernel/io/arb.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ExtpPrintDeviceResources (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceConfigurationList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceRequirementList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceRequirement (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceAllocationList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceAllocation (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintDeviceArbiters (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintResourceArbiter (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

BOOL
ExtpPrintArbiterEntry (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    );

PSTR
ExtpGetResourceTypeString (
    RESOURCE_TYPE Type
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExtResource (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    )

/*++

Routine Description:

    This routine implements the resource related debugger extension.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies a pointer to the subcommand string.

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

    ULONGLONG Address;
    ULONG ArgumentIndex;
    INT Result;

    Address = 0;
    if (Command == NULL) {
        DbgOut("Error: Supply a subcommand. Valid subcommands are:\n"
               "  !res.dev\n  !res.req\n  !res.reqlist\n  "
               "!res.conflist\n  !res.alloc\n  !res.alloclist\n  "
               "!res.arb\n  !res.devarbs\n  !res.arbentry\n");

        return EINVAL;
    }

    //
    // At least one parameter is required.
    //

    if (ArgumentCount < 2) {
        DbgOut("Error: Supply an address to dump.\n");

    } else {

        //
        // Loop through each argument, evaluate the address, and print the
        // namespace tree at that object.
        //

        for (ArgumentIndex = 1;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Result = DbgEvaluate(Context,
                                 ArgumentValues[ArgumentIndex],
                                 &Address);

            if (Result != 0) {
                DbgOut("Failed to evaluate address at \"%s\".\n",
                       ArgumentValues[ArgumentIndex]);

                return Result;
            }

            if (strcmp(Command, "dev") == 0) {
                ExtpPrintDeviceResources(Context, Address, 0);

            } else if (strcmp(Command, "req") == 0) {
                ExtpPrintResourceRequirement(Context, Address, 0);

            } else if (strcmp(Command, "reqlist") == 0) {
                ExtpPrintResourceRequirementList(Context, Address, 0);

            } else if (strcmp(Command, "conflist") == 0) {
                ExtpPrintResourceConfigurationList(Context, Address, 0);

            } else if (strcmp(Command, "alloc") == 0) {
                ExtpPrintResourceAllocation(Context, Address, 0);

            } else if (strcmp(Command, "alloclist") == 0) {
                ExtpPrintResourceAllocationList(Context, Address, 0);

            } else if (strcmp(Command, "arbentry") == 0) {
                ExtpPrintArbiterEntry(Context, Address, 0);

            } else if (strcmp(Command, "arb") == 0) {
                ExtpPrintResourceArbiter(Context, Address, 0);

            } else if (strcmp(Command, "devarbs") == 0) {
                ExtpPrintDeviceArbiters(Context, Address, 0);

            } else {
                DbgOut("Error: Invalid subcommand. Run !res for detailed"
                       "usage.\n");
            }

            if (ArgumentIndex != ArgumentCount - 1) {
                DbgOut("\n----\n");
            }
        }
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ExtpPrintDeviceResources (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a device's resources.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the devices whose resources should be
        dumped.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG BootResources;
    ULONGLONG BusLocalResources;
    PVOID Data;
    ULONG DataSize;
    ULONGLONG DeviceAddress;
    PTYPE_SYMBOL DeviceType;
    ULONGLONG HeaderType;
    ULONGLONG ProcessorLocalResources;
    ULONGLONG ResourceRequirements;
    BOOL Result;
    ULONGLONG SelectedConfiguration;
    INT Status;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    DeviceAddress = Address;
    DbgOut("Device %I64x:\n", DeviceAddress);
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_DEVICE",
                               &DeviceType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read _DEVICE at 0x%I64x\n", Address);
        goto PrintDeviceResourcesEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "Header.Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &HeaderType);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (HeaderType != ObjectDevice) {
        DbgOut("Object header type %I64d, probably not a device!\n",
               HeaderType);

        Status = EINVAL;
        goto PrintDeviceResourcesEnd;
    }

    IndentationLevel += 1;

    //
    // Print the processor resources.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "ProcessorLocalResources",
                                  Address,
                                  Data,
                                  DataSize,
                                  &ProcessorLocalResources);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (ProcessorLocalResources == 0) {
        DbgOut("No Processor Local Resources.\n");

    } else {
        DbgOut("Processor Local Resources @ %x\n", ProcessorLocalResources);
        ExtpPrintResourceAllocationList(Context,
                                        ProcessorLocalResources,
                                        IndentationLevel);
    }

    //
    // Print the bus local resources.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "BusLocalResources",
                                  Address,
                                  Data,
                                  DataSize,
                                  &BusLocalResources);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (BusLocalResources == 0) {
        DbgOut("No Bus Local Resources.\n");

    } else {
        DbgOut("Bus Local Resources @ %x\n", BusLocalResources);
        ExtpPrintResourceAllocationList(Context,
                                        BusLocalResources,
                                        IndentationLevel);
    }

    //
    // Print the boot resources.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "BootResources",
                                  Address,
                                  Data,
                                  DataSize,
                                  &BootResources);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (BootResources == 0) {
        DbgOut("No Boot Resources.\n");

    } else {
        DbgOut("Boot Resources @ %x\n", BootResources);
        ExtpPrintResourceAllocationList(Context,
                                        BootResources,
                                        IndentationLevel);
    }

    //
    // Print the selected configuration.
    //

    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "SelectedConfiguration",
                                  Address,
                                  Data,
                                  DataSize,
                                  &SelectedConfiguration);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (SelectedConfiguration != 0) {
        DbgOut("%*s", IndentationLevel, "");
        DbgOut("Selected Configuration %x\n", SelectedConfiguration);
    }

    //
    // Print the resource requirements.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "ResourceRequirements",
                                  Address,
                                  Data,
                                  DataSize,
                                  &ResourceRequirements);

    if (Status != 0) {
        goto PrintDeviceResourcesEnd;
    }

    if (ResourceRequirements == 0) {
        DbgOut("No Resource Requirements.\n");

    } else {
        DbgOut("Resource Requirements @ %x\n", ResourceRequirements);
        ExtpPrintResourceConfigurationList(Context,
                                           ResourceRequirements,
                                           IndentationLevel);
    }

    Status = 0;

PrintDeviceResourcesEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceConfigurationList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource configuration list.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the resource configuration list to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    PTYPE_SYMBOL ConfigurationListType;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    PTYPE_SYMBOL ListEntryType;
    ULONG RequirementHeadOffset;
    ULONG RequirementListEntryOffset;
    PTYPE_SYMBOL RequirementListType;
    BOOL Result;
    INT Status;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    DbgOut("Resource Configuration List @ %08I64x\n", Address);
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_CONFIGURATION_LIST",
                               &ConfigurationListType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read configuration list at 0x%I64x.\n",
               Address);

        goto PrintResourceConfigurationListEnd;
    }

    Status = DbgGetMemberOffset(ConfigurationListType,
                                "RequirementListListHead",
                                &RequirementHeadOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceConfigurationListEnd;
    }

    RequirementHeadOffset /= BITS_PER_BYTE;
    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintResourceConfigurationListEnd;
    }

    Status = DbgGetTypeByName(Context,
                              "_RESOURCE_REQUIREMENT_LIST",
                              &RequirementListType);

    if (Status != 0) {
        goto PrintResourceConfigurationListEnd;
    }

    Status = DbgGetMemberOffset(RequirementListType,
                                "ListEntry",
                                &RequirementListEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceConfigurationListEnd;
    }

    RequirementListEntryOffset /= BITS_PER_BYTE;

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = Address + RequirementHeadOffset;
    Status = DbgReadIntegerMember(Context,
                                  ConfigurationListType,
                                  "RequirementListListHead.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &CurrentEntryAddress);

    if (Status != 0) {
        goto PrintResourceConfigurationListEnd;
    }

    free(Data);
    Data = NULL;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        assert(Data == NULL);

        Status = DbgReadType(Context,
                             CurrentEntryAddress,
                             ListEntryType,
                             &Data,
                             &DataSize);

        if (Status != 0) {
            goto PrintResourceConfigurationListEnd;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress - RequirementListEntryOffset;
        Status = ExtpPrintResourceRequirementList(Context,
                                                  ChildObjectAddress,
                                                  IndentationLevel);

        if (Status == FALSE) {
            Status = EINVAL;
            goto PrintResourceConfigurationListEnd;
        }

        //
        // Move to the next child.
        //

        Status = DbgReadIntegerMember(Context,
                                      ListEntryType,
                                      "Next",
                                      CurrentEntryAddress,
                                      Data,
                                      DataSize,
                                      &CurrentEntryAddress);

        if (Status != 0) {
            goto PrintResourceConfigurationListEnd;
        }

        free(Data);
        Data = NULL;
    }

PrintResourceConfigurationListEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceRequirementList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource requirement list.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the resource requirement list to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    PTYPE_SYMBOL ListEntryType;
    ULONG RequirementEntryOffset;
    ULONG RequirementListHeadOffset;
    PTYPE_SYMBOL RequirementListType;
    PTYPE_SYMBOL RequirementType;
    BOOL Result;
    INT Status;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    DbgOut("Resource Requirement List @ %08I64x\n", Address);
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_REQUIREMENT_LIST",
                               &RequirementListType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read requirement list at 0x%I64x.\n",
               Address);

        goto PrintResourceRequirementListEnd;
    }

    Status = DbgGetMemberOffset(RequirementListType,
                                "RequirementListHead",
                                &RequirementListHeadOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceRequirementListEnd;
    }

    RequirementListHeadOffset /= BITS_PER_BYTE;
    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintResourceRequirementListEnd;
    }

    Status = DbgGetTypeByName(Context,
                              "_RESOURCE_REQUIREMENT",
                              &RequirementType);

    if (Status != 0) {
        goto PrintResourceRequirementListEnd;
    }

    Status = DbgGetMemberOffset(RequirementType,
                                "ListEntry",
                                &RequirementEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceRequirementListEnd;
    }

    RequirementEntryOffset /= BITS_PER_BYTE;

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = Address + RequirementListHeadOffset;
    Status = DbgReadIntegerMember(Context,
                                  RequirementListType,
                                  "RequirementListHead.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &CurrentEntryAddress);

    if (Status != 0) {
        goto PrintResourceRequirementListEnd;
    }

    free(Data);
    Data = NULL;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        assert(Data == NULL);

        Status = DbgReadType(Context,
                             CurrentEntryAddress,
                             ListEntryType,
                             &Data,
                             &DataSize);

        if (Status != 0) {
            goto PrintResourceRequirementListEnd;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress - RequirementEntryOffset;
        Result = ExtpPrintResourceRequirement(Context,
                                              ChildObjectAddress,
                                              IndentationLevel);

        if (Result == FALSE) {
            Status = EINVAL;
            goto PrintResourceRequirementListEnd;
        }

        //
        // Move to the next child.
        //

        Status = DbgReadIntegerMember(Context,
                                      ListEntryType,
                                      "Next",
                                      CurrentEntryAddress,
                                      Data,
                                      DataSize,
                                      &CurrentEntryAddress);

        if (Status != 0) {
            goto PrintResourceRequirementListEnd;
        }

        free(Data);
        Data = NULL;
    }

PrintResourceRequirementListEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceRequirement (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource requirement.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the resource requirement to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG AlternativeListEntryOffset;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    ULONGLONG Flags;
    PTYPE_SYMBOL ListEntryType;
    ULONGLONG RequirementDataSize;
    PTYPE_SYMBOL RequirementType;
    PSTR ResourceType;
    BOOL Result;
    INT Status;
    ULONGLONG Value;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_REQUIREMENT",
                               &RequirementType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read requirement at 0x%I64x.\n", Address);
        goto PrintResourceRequirementEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    ResourceType = ExtpGetResourceTypeString(Value);
    DbgOut("%08I64x %16s: Range ", Address, ResourceType);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Minimum",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%08I64x - ", Value);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Maximum",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%08I64x, Len ", Value);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Length",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%08I64x, Align ", Value);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Alignment",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%I64x, Char ", Value);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Characteristics",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%I64x, Flags ", Value);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Flags",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Flags);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    DbgOut("%I64x", Flags);
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "OwningRequirement",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    if (Value != 0) {
        DbgOut(", Owner %I64x", Value);
    }

    if ((Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "Provider",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    if (Value != 0) {
        DbgOut(", Provider %x", Value);
    }

    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "DataSize",
                                  Address,
                                  Data,
                                  DataSize,
                                  &RequirementDataSize);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    if (RequirementDataSize != 0) {
        Status = DbgReadIntegerMember(Context,
                                      RequirementType,
                                      "Data",
                                      Address,
                                      Data,
                                      DataSize,
                                      &Value);

        if (Status != 0) {
            goto PrintResourceRequirementEnd;
        }

        DbgOut(", Data 0x%I64x Size 0x%I64x", Value, RequirementDataSize);
    }

    DbgOut("\n");

    //
    // If the requirement is not linked in, assume it is an alternative and
    // don't try to traverse alternatives.
    //

    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "ListEntry.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    if (Value == 0) {
        Status = 0;
        goto PrintResourceRequirementEnd;
    }

    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    Status = DbgGetMemberOffset(RequirementType,
                                "AlternativeListEntry",
                                &AlternativeListEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    AlternativeListEntryOffset /= BITS_PER_BYTE;

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = Address + AlternativeListEntryOffset;
    Status = DbgReadIntegerMember(Context,
                                  RequirementType,
                                  "AlternativeListEntry.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &CurrentEntryAddress);

    if (Status != 0) {
        goto PrintResourceRequirementEnd;
    }

    free(Data);
    Data = NULL;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        assert(Data == NULL);

        Status = DbgReadType(Context,
                             CurrentEntryAddress,
                             ListEntryType,
                             &Data,
                             &DataSize);

        if (Status != 0) {
            goto PrintResourceRequirementEnd;
        }

        //
        // Print the resource requirement alternative.
        //

        ChildObjectAddress = CurrentEntryAddress - AlternativeListEntryOffset;
        Result = ExtpPrintResourceRequirement(Context,
                                              ChildObjectAddress,
                                              IndentationLevel);

        if (Result == FALSE) {
            Status = EINVAL;
            goto PrintResourceRequirementEnd;
        }

        //
        // Move to the next child.
        //

        Status = DbgReadIntegerMember(Context,
                                      ListEntryType,
                                      "Next",
                                      CurrentEntryAddress,
                                      Data,
                                      DataSize,
                                      &CurrentEntryAddress);

        if (Status != 0) {
            goto PrintResourceRequirementEnd;
        }

        free(Data);
        Data = NULL;
    }

    Status = 0;

PrintResourceRequirementEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceAllocationList (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource allocation list.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the resource allocation list to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG AllocationEntryOffset;
    ULONG AllocationListHeadOffset;
    PTYPE_SYMBOL AllocationListType;
    PTYPE_SYMBOL AllocationType;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    PTYPE_SYMBOL ListEntryType;
    BOOL Result;
    INT Status;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    DbgOut("Resource Allocation List @ %08I64x\n", Address);
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_ALLOCATION_LIST",
                               &AllocationListType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read allocation list at 0x%I64x.\n", Address);
        goto PrintResourceAllocationListEnd;
    }

    Status = DbgGetMemberOffset(AllocationListType,
                                "AllocationListHead",
                                &AllocationListHeadOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceAllocationListEnd;
    }

    AllocationListHeadOffset /= BITS_PER_BYTE;
    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintResourceAllocationListEnd;
    }

    Status = DbgGetTypeByName(Context, "_RESOURCE_ALLOCATION", &AllocationType);
    if (Status != 0) {
        goto PrintResourceAllocationListEnd;
    }

    Status = DbgGetMemberOffset(AllocationType,
                                "ListEntry",
                                &AllocationEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceAllocationListEnd;
    }

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = Address + AllocationListHeadOffset;
    Status = DbgReadIntegerMember(Context,
                                  AllocationListType,
                                  "AllocationListHead.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &CurrentEntryAddress);

    if (Status != 0) {
        goto PrintResourceAllocationListEnd;
    }

    free(Data);
    Data = NULL;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        assert(Data == NULL);

        Status = DbgReadType(Context,
                             CurrentEntryAddress,
                             ListEntryType,
                             &Data,
                             &DataSize);

        if (Status != 0) {
            goto PrintResourceAllocationListEnd;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress - AllocationEntryOffset;
        Result = ExtpPrintResourceAllocation(Context,
                                             ChildObjectAddress,
                                             IndentationLevel);

        if (Result == FALSE) {
            Status = EINVAL;
            goto PrintResourceAllocationListEnd;
        }

        //
        // Move to the next child.
        //

        Status = DbgReadIntegerMember(Context,
                                      ListEntryType,
                                      "Next",
                                      CurrentEntryAddress,
                                      Data,
                                      DataSize,
                                      &CurrentEntryAddress);

        if (Status != 0) {
            goto PrintResourceAllocationListEnd;
        }

        free(Data);
        Data = NULL;
    }

    Status = 0;

PrintResourceAllocationListEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceAllocation (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource allocation.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the resource allocation to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONGLONG AllocationDataSize;
    PTYPE_SYMBOL AllocationType;
    PVOID Data;
    ULONG DataSize;
    PSTR ResourceType;
    BOOL Result;
    INT Status;
    ULONGLONG Value;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_ALLOCATION",
                               &AllocationType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read allocation at 0x%I64x.\n",
               Address);

        goto PrintResourceAllocationEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    ResourceType = ExtpGetResourceTypeString(Value);
    DbgOut("%08I64x %16s: ", Address, ResourceType);
    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Allocation",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    DbgOut("%08I64x, Len ", Value);
    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Length",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    DbgOut("%08I64x, Char ", Value);
    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Characteristics",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    DbgOut("%I64x", Value);
    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "OwningAllocation",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    if (Value != 0) {
        DbgOut(", Owner %I64x", Value);
    }

    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Flags",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    if ((Value & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "Provider",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    if (Value != 0) {
        DbgOut(", Provider %I64x", Value);
    }

    Status = DbgReadIntegerMember(Context,
                                  AllocationType,
                                  "DataSize",
                                  Address,
                                  Data,
                                  DataSize,
                                  &AllocationDataSize);

    if (Status != 0) {
        goto PrintResourceAllocationEnd;
    }

    if (AllocationDataSize != 0) {
        Status = DbgReadIntegerMember(Context,
                                      AllocationType,
                                      "Data",
                                      Address,
                                      Data,
                                      DataSize,
                                      &Value);

        if (Status != 0) {
            goto PrintResourceAllocationEnd;
        }

        DbgOut(", Data %I64x Size 0x%I64x", Value, AllocationDataSize);
    }

    Status = 0;

PrintResourceAllocationEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    DbgOut("\n");
    return Result;
}

BOOL
ExtpPrintDeviceArbiters (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out the resource arbiters associated with a device.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the device whose arbiters should be
        dumped.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArbiterIndex;
    ULONG ArbiterListEntryOffset;
    ULONG ArbiterListHeadOffset;
    PTYPE_SYMBOL ArbiterType;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    ULONGLONG DeviceAddress;
    ULONGLONG DeviceParentAddress;
    PTYPE_SYMBOL DeviceType;
    ULONGLONG HeaderType;
    PVOID ListEntryData;
    ULONG ListEntryDataSize;
    PTYPE_SYMBOL ListEntryType;
    ULONGLONG NextParent;
    ULONGLONG OriginalDeviceAddress;
    ULONGLONG ResourceType;
    BOOL Result;
    INT Status;

    ChildObjectAddress = 0;
    Data = NULL;
    ListEntryData = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");

    //
    // Get the parent of the current device.
    //

    OriginalDeviceAddress = Address;
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_DEVICE",
                               &DeviceType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Failed to read device at %I64x.\n", Address);
        goto PrintDeviceArbitersEnd;
    }

    Status = DbgGetMemberOffset(DeviceType,
                                "ArbiterListHead",
                                &ArbiterListHeadOffset,
                                NULL);

    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    ArbiterListHeadOffset /= BITS_PER_BYTE;
    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    Status = DbgGetTypeByName(Context, "_RESOURCE_ARBITER", &ArbiterType);
    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    Status = DbgGetMemberOffset(ArbiterType,
                                "ListEntry",
                                &ArbiterListEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "Header.Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &HeaderType);

    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    if (HeaderType != ObjectDevice) {
        DbgOut("Object header type %I64d, probably not a device!\n",
               HeaderType);

        return FALSE;
    }

    Status = DbgReadIntegerMember(Context,
                                  DeviceType,
                                  "ParentDevice",
                                  Address,
                                  Data,
                                  DataSize,
                                  &DeviceParentAddress);

    if (Status != 0) {
        goto PrintDeviceArbitersEnd;
    }

    DbgOut("Arbiters for device %I64x (parent %I64x):\n",
           OriginalDeviceAddress,
           DeviceParentAddress);

    free(Data);
    Data = NULL;

    //
    // Attempt to find each arbiter.
    //

    IndentationLevel += 1;
    for (ArbiterIndex = 1; ArbiterIndex < ArbiterTypeCount; ArbiterIndex += 1) {

        //
        // Start at the original device address and look through all its
        // arbiters.
        //

        DeviceAddress = DeviceParentAddress;
        while (TRUE) {

            assert(Data == NULL);

            Status = DbgReadType(Context,
                                 DeviceAddress,
                                 DeviceType,
                                 &Data,
                                 &DataSize);

            if (Status != 0) {
                DbgOut("Failed to read device at %I64x.\n", DeviceAddress);
                goto PrintDeviceArbitersEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          DeviceType,
                                          "Header.Type",
                                          DeviceAddress,
                                          Data,
                                          DataSize,
                                          &HeaderType);

            if (Status != 0) {
                goto PrintDeviceArbitersEnd;
            }

            if (HeaderType != ObjectDevice) {
                DbgOut("Object header type %I64d, probably not a device!\n",
                       HeaderType);

                Status = EINVAL;
                goto PrintDeviceArbitersEnd;
            }

            Status = DbgReadIntegerMember(Context,
                                          DeviceType,
                                          "ParentDevice",
                                          Address,
                                          Data,
                                          DataSize,
                                          &NextParent);

            if (Status != 0) {
                goto PrintDeviceArbitersEnd;
            }

            //
            // Loop through every arbiter in the device.
            //

            ChildListHead = DeviceAddress + ArbiterListHeadOffset;
            Status = DbgReadIntegerMember(Context,
                                          DeviceType,
                                          "ArbiterListHead.Next",
                                          DeviceAddress,
                                          Data,
                                          DataSize,
                                          &CurrentEntryAddress);

            if (Status != 0) {
                goto PrintDeviceArbitersEnd;
            }

            free(Data);
            Data = NULL;
            while (CurrentEntryAddress != ChildListHead) {

                //
                // Read the list entry.
                //

                assert(ListEntryData == NULL);

                Status = DbgReadType(Context,
                                     CurrentEntryAddress,
                                     ListEntryType,
                                     &ListEntryData,
                                     &ListEntryDataSize);

                if (Status != 0) {
                    goto PrintDeviceArbitersEnd;
                }

                //
                // Read in the arbiter.
                //

                ChildObjectAddress =
                                  CurrentEntryAddress - ArbiterListEntryOffset;

                assert(Data == NULL);

                Status = DbgReadType(Context,
                                     ChildObjectAddress,
                                     ArbiterType,
                                     &Data,
                                     &DataSize);

                if (Status != 0) {
                    DbgOut("Error: Could not read arbiter at 0x%I64x.\n",
                           ChildObjectAddress);

                    goto PrintDeviceArbitersEnd;
                }

                Status = DbgReadIntegerMember(Context,
                                              ArbiterType,
                                              "ResourceType",
                                              ChildObjectAddress,
                                              Data,
                                              DataSize,
                                              &ResourceType);

                if (Status != 0) {
                    goto PrintDeviceArbitersEnd;
                }

                free(Data);
                Data = NULL;

                //
                // Stop looking if this arbiter is the right type.
                //

                if (ResourceType == ArbiterIndex) {
                    free(ListEntryData);
                    ListEntryData = NULL;
                    break;
                }

                //
                // Move to the next entry.
                //

                Status = DbgReadIntegerMember(Context,
                                              ListEntryType,
                                              "Next",
                                              CurrentEntryAddress,
                                              ListEntryData,
                                              ListEntryDataSize,
                                              &CurrentEntryAddress);

                if (Status != 0) {
                    goto PrintDeviceArbitersEnd;
                }

                free(ListEntryData);
                ListEntryData = NULL;
            }

            //
            // If an arbiter was found, print it out and stop looking for this
            // arbiter type.
            //

            if (CurrentEntryAddress != ChildListHead) {
                ExtpPrintResourceArbiter(Context,
                                         ChildObjectAddress,
                                         IndentationLevel);

                break;
            }

            //
            // No arbiter was found in this device, so move to the parent
            // device.
            //

            DeviceAddress = NextParent;
            if (DeviceAddress == 0) {
                DbgOut("Could not find %s arbiter.\n",
                       ExtpGetResourceTypeString(ArbiterIndex));

                break;
            }
        }
    }

PrintDeviceArbitersEnd:
    if (Data != NULL) {
        free(Data);
    }

    if (ListEntryData != NULL) {
        free(ListEntryData);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintResourceArbiter (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out a resource arbiter.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the arbiter to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArbiterEntryListHeadOffset;
    ULONG ArbiterEntryOffset;
    PTYPE_SYMBOL ArbiterType;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    PVOID Data;
    ULONG DataSize;
    PTYPE_SYMBOL ListEntryType;
    PSTR ResourceType;
    BOOL Result;
    INT Status;
    ULONGLONG Value;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_RESOURCE_ARBITER",
                               &ArbiterType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Failed to read _RESOURCE_ARBITER at %I64x.\n", Address);
        goto PrintResourceArbiterEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  ArbiterType,
                                  "ResourceType",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    ResourceType = ExtpGetResourceTypeString(Value);
    DbgOut("%s Arbiter @ 0x%I64x owned by device ", ResourceType, Address);
    Status = DbgReadIntegerMember(Context,
                                  ArbiterType,
                                  "OwningDevice",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    DbgOut("0x%I64x\n", Value);
    Status = DbgGetMemberOffset(ArbiterType,
                                "EntryListHead",
                                &ArbiterEntryListHeadOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    ArbiterEntryListHeadOffset /= BITS_PER_BYTE;
    Status = DbgGetMemberOffset(ArbiterType,
                                "ListEntry",
                                &ArbiterEntryOffset,
                                NULL);

    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    ArbiterEntryOffset /= BITS_PER_BYTE;
    Status = DbgGetTypeByName(Context, "LIST_ENTRY", &ListEntryType);
    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    //
    // Print out all entries.
    //

    IndentationLevel += 1;
    ChildListHead = Address + ArbiterEntryListHeadOffset;
    Status = DbgReadIntegerMember(Context,
                                  ArbiterType,
                                  "EntryListHead.Next",
                                  Address,
                                  Data,
                                  DataSize,
                                  &CurrentEntryAddress);

    if (Status != 0) {
        goto PrintResourceArbiterEnd;
    }

    free(Data);
    Data = NULL;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        assert(Data == NULL);

        Status = DbgReadType(Context,
                             CurrentEntryAddress,
                             ListEntryType,
                             &Data,
                             &DataSize);

        if (Status != 0) {
            goto PrintResourceArbiterEnd;
        }

        //
        // Print the arbiter entry.
        //

        ChildObjectAddress = CurrentEntryAddress - ArbiterEntryOffset;
        Result = ExtpPrintArbiterEntry(Context,
                                       ChildObjectAddress,
                                       IndentationLevel);

        if (Result == FALSE) {
            Status = EINVAL;
            goto PrintResourceArbiterEnd;
        }

        //
        // Move to the next entry.
        //

        Status = DbgReadIntegerMember(Context,
                                      ListEntryType,
                                      "Next",
                                      CurrentEntryAddress,
                                      Data,
                                      DataSize,
                                      &CurrentEntryAddress);

        if (Status != 0) {
            goto PrintResourceArbiterEnd;
        }

        free(Data);
        Data = NULL;
    }

    Status = 0;

PrintResourceArbiterEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

BOOL
ExtpPrintArbiterEntry (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    ULONG IndentationLevel
    )

/*++

Routine Description:

    This routine prints out an arbiter entry.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address of the arbiter entry to print.

    IndentationLevel - Supplies the indentation level to print the object at.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PVOID Data;
    ULONG DataSize;
    PTYPE_SYMBOL EntryType;
    BOOL Result;
    PSTR SpaceType;
    INT Status;
    ULONGLONG Value;

    Data = NULL;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    DbgOut("%*s", IndentationLevel, "");
    Status = DbgReadTypeByName(Context,
                               Address,
                               "_ARBITER_ENTRY",
                               &EntryType,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        DbgOut("Error: Could not read entry at 0x%I64x.\n",
               Address);

        goto PrintArbiterEntryEnd;
    }

    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Type",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    switch (Value) {
    case ArbiterSpaceInvalid:
        SpaceType = "Invalid";
        break;

    case ArbiterSpaceFree:
        SpaceType = "Free";
        break;

    case ArbiterSpaceReserved:
        SpaceType = "Reserved";
        break;

    case ArbiterSpaceAllocated:
        SpaceType = "Allocated";
        break;

    default:
        SpaceType = "INVALID";
        break;
    }

    DbgOut("%08I64x %9s: ", Address, SpaceType);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Allocation",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    DbgOut("%08I64x, Len ", Value);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Length",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    DbgOut("%08I64x, Char ", Value);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Characteristics",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    DbgOut("%I64x, Requirement ", Value);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "CorrespondingRequirement",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    DbgOut("0x%I64x, Device ", Value);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Device",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    DbgOut("0x%I64x", Value);
    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "DependentEntry",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    if (Value != 0) {
        DbgOut(", Dependent %I64x", Value);
    }

    Status = DbgReadIntegerMember(Context,
                                  EntryType,
                                  "Flags",
                                  Address,
                                  Data,
                                  DataSize,
                                  &Value);

    if (Status != 0) {
        goto PrintArbiterEntryEnd;
    }

    if ((Value & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    if ((Value & RESOURCE_FLAG_BOOT) != 0) {
        DbgOut(" Boot");
    }

    DbgOut("\n");

PrintArbiterEntryEnd:
    if (Data != NULL) {
        free(Data);
    }

    Result = TRUE;
    if (Status != 0) {
        Result = FALSE;
    }

    return Result;
}

PSTR
ExtpGetResourceTypeString (
    RESOURCE_TYPE Type
    )

/*++

Routine Description:

    This routine returns a string representing the given resource type.

Arguments:

    Type - Supplies the resource type.

Return Value:

    Returns a pointer to a constant read-only string representing the given
    resource type.

--*/

{

    PSTR ResourceType;

    switch (Type) {
    case ResourceTypeInvalid:
        ResourceType = "Invalid";
        break;

    case ResourceTypePhysicalAddressSpace:
        ResourceType = "Physical Address";
        break;

    case ResourceTypeIoPort:
        ResourceType = "I/O Port";
        break;

    case ResourceTypeInterruptLine:
        ResourceType = "Interrupt Line";
        break;

    case ResourceTypeInterruptVector:
        ResourceType = "Interrupt Vector";
        break;

    case ResourceTypeBusNumber:
        ResourceType = "Bus Number";
        break;

    case ResourceTypeDmaChannel:
        ResourceType = "DMA Channel";
        break;

    case ResourceTypeVendorSpecific:
        ResourceType = "Vendor Specific";
        break;

    case ResourceTypeGpio:
        ResourceType = "GPIO";
        break;

    case ResourceTypeSimpleBus:
        ResourceType = "SPB";
        break;

    default:
        ResourceType = "INVALID RESOURCE TYPE";
        break;
    }

    return ResourceType;
}

