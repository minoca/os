/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include "dbgext.h"
#include "../../kernel/io/arb.h"
#include "../../kernel/io/iop.h"

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

    ULONG BytesRead;
    DEVICE Device;
    ULONGLONG DeviceAddress;
    ULONG IndentIndex;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    DeviceAddress = Address;
    DbgOut("Device %I64x:\n", DeviceAddress);
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(DEVICE),
                            &Device,
                            &BytesRead);

    if ((Success != 0) ||
        (BytesRead != sizeof(DEVICE))) {

        DbgOut("Error: Could not read device at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    if (Device.Header.Type != ObjectDevice) {
        DbgOut("Object header type %d, probably not a device!\n",
                      Device.Header.Type);

        return FALSE;
    }

    IndentationLevel += 1;

    //
    // Print the processor resources.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    if (Device.ProcessorLocalResources == NULL) {
        DbgOut("No Processor Local Resources.\n");

    } else {
        DbgOut("Processor Local Resources @ %x\n",
                      Device.ProcessorLocalResources);

        ExtpPrintResourceAllocationList(Context,
                                        (UINTN)Device.ProcessorLocalResources,
                                        IndentationLevel);
    }

    //
    // Print the bus local resources.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    if (Device.BusLocalResources == NULL) {
        DbgOut("No Bus Local Resources.\n");

    } else {
        DbgOut("Bus Local Resources @ %x\n", Device.BusLocalResources);
        ExtpPrintResourceAllocationList(Context,
                                        (UINTN)Device.BusLocalResources,
                                        IndentationLevel);
    }

    //
    // Print the boot resources.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    if (Device.BootResources == NULL) {
        DbgOut("No Boot Resources.\n");

    } else {
        DbgOut("Boot Resources @ %x\n", Device.BootResources);
        ExtpPrintResourceAllocationList(Context,
                                        (UINTN)Device.BootResources,
                                        IndentationLevel);
    }

    //
    // Print the selected configuration.
    //

    if (Device.SelectedConfiguration != NULL) {
        for (IndentIndex = 0;
             IndentIndex < IndentationLevel;
             IndentIndex += 1) {

            DbgOut("  ");
        }

        DbgOut("Selected Configuration %x\n", Device.SelectedConfiguration);
    }

    //
    // Print the resource requirements.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    if (Device.ResourceRequirements == NULL) {
        DbgOut("No Resource Requirements.\n");

    } else {
        DbgOut("Resource Requirements @ %x\n", Device.ResourceRequirements);
        ExtpPrintResourceConfigurationList(Context,
                                           (UINTN)Device.ResourceRequirements,
                                           IndentationLevel);
    }

    return TRUE;
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

    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    RESOURCE_CONFIGURATION_LIST ConfigurationList;
    ULONGLONG ConfigurationListAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    ConfigurationListAddress = Address;
    DbgOut("Resource Configuration List @ %08I64x\n",
           ConfigurationListAddress);

    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_CONFIGURATION_LIST),
                            &ConfigurationList,
                            &BytesRead);

    if ((Success != 0) ||
        (BytesRead != sizeof(RESOURCE_CONFIGURATION_LIST))) {

        DbgOut("Error: Could not read configuration list at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = ConfigurationListAddress +
                    FIELD_OFFSET(RESOURCE_CONFIGURATION_LIST,
                                 RequirementListListHead);

    CurrentEntryAddress = (UINTN)ConfigurationList.RequirementListListHead.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                CurrentEntryAddress,
                                sizeof(LIST_ENTRY),
                                &CurrentEntryValue,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            return FALSE;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(RESOURCE_REQUIREMENT_LIST, ListEntry);

        Success = ExtpPrintResourceRequirementList(Context,
                                                   ChildObjectAddress,
                                                   IndentationLevel);

        if (Success == FALSE) {
            return FALSE;
        }

        //
        // Move to the next child.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return TRUE;
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

    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    RESOURCE_REQUIREMENT_LIST RequirementList;
    ULONGLONG RequirementListAddress;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    RequirementListAddress = Address;
    DbgOut("Resource Requirement List @ %08I64x\n",
                  RequirementListAddress);

    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_REQUIREMENT_LIST),
                            &RequirementList,
                            &BytesRead);

    if ((Success != 0) ||
        (BytesRead != sizeof(RESOURCE_REQUIREMENT_LIST))) {

        DbgOut("Error: Could not read requirement list at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = RequirementListAddress +
                    FIELD_OFFSET(RESOURCE_REQUIREMENT_LIST,
                                 RequirementListHead);

    CurrentEntryAddress = (UINTN)RequirementList.RequirementListHead.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                CurrentEntryAddress,
                                sizeof(LIST_ENTRY),
                                &CurrentEntryValue,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            return FALSE;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(RESOURCE_REQUIREMENT, ListEntry);

        Success = ExtpPrintResourceRequirement(Context,
                                               ChildObjectAddress,
                                               IndentationLevel);

        if (Success == FALSE) {
            return FALSE;
        }

        //
        // Move to the next child.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return TRUE;
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

    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    RESOURCE_REQUIREMENT Requirement;
    ULONGLONG RequirementAddress;
    PSTR ResourceType;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    RequirementAddress = Address;
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_REQUIREMENT),
                            &Requirement,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(RESOURCE_REQUIREMENT))) {
        DbgOut("Error: Could not read requirement at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    ResourceType = ExtpGetResourceTypeString(Requirement.Type);
    DbgOut("%08I64x %16s: Range %08I64x - %08I64x, Len %08I64x, "
           "Align %I64x, Char %I64x, Flags %x",
           RequirementAddress,
           ResourceType,
           Requirement.Minimum,
           Requirement.Maximum,
           Requirement.Length,
           Requirement.Alignment,
           Requirement.Characteristics,
           Requirement.Flags);

    if (Requirement.OwningRequirement != NULL) {
        DbgOut(", Owner %x", Requirement.OwningRequirement);
    }

    if ((Requirement.Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    if (Requirement.Provider != NULL) {
        DbgOut(", Provider %x", Requirement.Provider);
    }

    if (Requirement.DataSize != 0) {
        DbgOut(", Data %x Size 0x%x", Requirement.Data, Requirement.DataSize);
    }

    DbgOut("\n");

    //
    // If the requirement is not linked in, assume it is an alternative and
    // don't try to traverse alternatives.
    //

    if (Requirement.ListEntry.Next == NULL) {
        return TRUE;
    }

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = RequirementAddress +
                    FIELD_OFFSET(RESOURCE_REQUIREMENT,
                                 AlternativeListEntry);

    CurrentEntryAddress = (UINTN)Requirement.AlternativeListEntry.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                CurrentEntryAddress,
                                sizeof(LIST_ENTRY),
                                &CurrentEntryValue,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            return FALSE;
        }

        //
        // Print the resource requirement alternative.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(RESOURCE_REQUIREMENT,
                                          AlternativeListEntry);

        Success = ExtpPrintResourceRequirement(Context,
                                               ChildObjectAddress,
                                               IndentationLevel);

        if (Success == FALSE) {
            return FALSE;
        }

        //
        // Move to the next child.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return TRUE;
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

    RESOURCE_ALLOCATION_LIST AllocationList;
    ULONGLONG AllocationListAddress;
    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    AllocationListAddress = Address;
    DbgOut("Resource Allocation List @ %08I64x\n", AllocationListAddress);
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_ALLOCATION_LIST),
                            &AllocationList,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(RESOURCE_ALLOCATION_LIST))) {
        DbgOut("Error: Could not read allocation list at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    //
    // Print out all children.
    //

    IndentationLevel += 1;
    ChildListHead = AllocationListAddress +
                    FIELD_OFFSET(RESOURCE_ALLOCATION_LIST,
                                 AllocationListHead);

    CurrentEntryAddress = (UINTN)AllocationList.AllocationListHead.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                CurrentEntryAddress,
                                sizeof(LIST_ENTRY),
                                &CurrentEntryValue,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            return FALSE;
        }

        //
        // Print the resource requirement list.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(RESOURCE_ALLOCATION, ListEntry);

        Success = ExtpPrintResourceAllocation(Context,
                                              ChildObjectAddress,
                                              IndentationLevel);

        if (Success == FALSE) {
            return FALSE;
        }

        //
        // Move to the next child.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return TRUE;
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

    RESOURCE_ALLOCATION Allocation;
    ULONGLONG AllocationAddress;
    ULONG BytesRead;
    ULONG IndentIndex;
    PSTR ResourceType;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    AllocationAddress = Address;
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_ALLOCATION),
                            &Allocation,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(RESOURCE_ALLOCATION))) {
        DbgOut("Error: Could not read allocation at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    ResourceType = ExtpGetResourceTypeString(Allocation.Type);
    DbgOut("%08I64x %16s: %08I64x, Len %08I64x, Char %I64x",
           AllocationAddress,
           ResourceType,
           Allocation.Allocation,
           Allocation.Length,
           Allocation.Characteristics);

    if (Allocation.OwningAllocation != NULL) {
        DbgOut(", Owner %x", Allocation.OwningAllocation);
    }

    if ((Allocation.Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    if (Allocation.Provider != NULL) {
        DbgOut(", Provider %x", Allocation.Provider);
    }

    if (Allocation.DataSize != 0) {
        DbgOut(", Data %x Size 0x%x", Allocation.Data, Allocation.DataSize);
    }

    DbgOut("\n");
    return TRUE;
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

    RESOURCE_ARBITER Arbiter;
    ULONG ArbiterIndex;
    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    DEVICE Device;
    ULONGLONG DeviceAddress;
    ULONGLONG DeviceParentAddress;
    ULONG IndentIndex;
    ULONGLONG OriginalDeviceAddress;
    INT Success;

    ChildObjectAddress = 0;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    //
    // Get the parent of the current device.
    //

    OriginalDeviceAddress = Address;
    Success = DbgReadMemory(Context,
                            TRUE,
                            OriginalDeviceAddress,
                            sizeof(DEVICE),
                            &Device,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(DEVICE))) {
        DbgOut("Failed to read device at %I64x.\n",
               OriginalDeviceAddress);

        return FALSE;
    }

    if (Device.Header.Type != ObjectDevice) {
        DbgOut("Object header type %d, probably not a device!\n",
               Device.Header.Type);

        return FALSE;
    }

    DeviceParentAddress = (UINTN)Device.ParentDevice;
    DbgOut("Arbiters for device %I64x (parent %I64x):\n",
           OriginalDeviceAddress,
           DeviceParentAddress);

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
            Success = DbgReadMemory(Context,
                                    TRUE,
                                    DeviceAddress,
                                    sizeof(DEVICE),
                                    &Device,
                                    &BytesRead);

            if ((Success != 0) || (BytesRead != sizeof(DEVICE))) {
                DbgOut("Failed to read device at %I64x.\n", DeviceAddress);
                return FALSE;
            }

            if (Device.Header.Type != ObjectDevice) {
                DbgOut("Object header type %d, probably not a device!\n",
                       Device.Header.Type);

                return FALSE;
            }

            //
            // Loop through every arbiter in the device.
            //

            ChildListHead = DeviceAddress +
                            FIELD_OFFSET(DEVICE, ArbiterListHead);

            CurrentEntryAddress = (UINTN)Device.ArbiterListHead.Next;
            while (CurrentEntryAddress != ChildListHead) {

                //
                // Read the list entry.
                //

                Success = DbgReadMemory(Context,
                                        TRUE,
                                        CurrentEntryAddress,
                                        sizeof(LIST_ENTRY),
                                        &CurrentEntryValue,
                                        &BytesRead);

                if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
                    DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                           CurrentEntryAddress);

                    return FALSE;
                }

                //
                // Read in the arbiter.
                //

                ChildObjectAddress = CurrentEntryAddress -
                                     FIELD_OFFSET(RESOURCE_ARBITER, ListEntry);

                Success = DbgReadMemory(Context,
                                        TRUE,
                                        ChildObjectAddress,
                                        sizeof(RESOURCE_ARBITER),
                                        &Arbiter,
                                        &BytesRead);

                if ((Success != 0) || (BytesRead != sizeof(RESOURCE_ARBITER))) {
                    DbgOut("Error: Could not read arbiter at 0x%I64x.\n",
                           ChildObjectAddress);

                    return FALSE;
                }

                //
                // Stop looking if this arbiter is the right type.
                //

                if (Arbiter.ResourceType == ArbiterIndex) {
                    break;
                }

                //
                // Move to the next entry.
                //

                CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
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

            DeviceAddress = (UINTN)Device.ParentDevice;
            if (Device.ParentDevice == NULL) {
                DbgOut("Could not find %s arbiter.\n",
                       ExtpGetResourceTypeString(ArbiterIndex));

                break;
            }
        }
    }

    return TRUE;
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

    RESOURCE_ARBITER Arbiter;
    ULONGLONG ArbiterAddress;
    ULONG BytesRead;
    ULONGLONG ChildListHead;
    ULONGLONG ChildObjectAddress;
    ULONGLONG CurrentEntryAddress;
    LIST_ENTRY CurrentEntryValue;
    ULONG IndentIndex;
    PSTR ResourceType;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    ArbiterAddress = Address;
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(RESOURCE_ARBITER),
                            &Arbiter,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(RESOURCE_ARBITER))) {
        DbgOut("Error: Could not read arbiter at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    ResourceType = ExtpGetResourceTypeString(Arbiter.ResourceType);
    DbgOut("%s Arbiter @ %I64x owned by device %x\n",
           ResourceType,
           ArbiterAddress,
           Arbiter.OwningDevice);

    //
    // Print out all entries.
    //

    IndentationLevel += 1;
    ChildListHead = ArbiterAddress +
                    FIELD_OFFSET(RESOURCE_ARBITER,
                                 EntryListHead);

    CurrentEntryAddress = (UINTN)Arbiter.EntryListHead.Next;
    while (CurrentEntryAddress != ChildListHead) {

        //
        // Read the list entry.
        //

        Success = DbgReadMemory(Context,
                                TRUE,
                                CurrentEntryAddress,
                                sizeof(LIST_ENTRY),
                                &CurrentEntryValue,
                                &BytesRead);

        if ((Success != 0) || (BytesRead != sizeof(LIST_ENTRY))) {
            DbgOut("Error: Could not read LIST_ENTRY at 0x%I64x.\n",
                   CurrentEntryAddress);

            return FALSE;
        }

        //
        // Print the arbiter entry.
        //

        ChildObjectAddress = CurrentEntryAddress -
                             FIELD_OFFSET(ARBITER_ENTRY, ListEntry);

        Success = ExtpPrintArbiterEntry(Context,
                                        ChildObjectAddress,
                                        IndentationLevel);

        if (Success == FALSE) {
            return FALSE;
        }

        //
        // Move to the next entry.
        //

        CurrentEntryAddress = (UINTN)CurrentEntryValue.Next;
    }

    return TRUE;
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

    ULONG BytesRead;
    ARBITER_ENTRY Entry;
    ULONGLONG EntryAddress;
    ULONG IndentIndex;
    PSTR SpaceType;
    INT Success;

    //
    // Bail out if the indentation seems too deep.
    //

    if (IndentationLevel > 50) {
        return FALSE;
    }

    //
    // Print out the indentation.
    //

    for (IndentIndex = 0; IndentIndex < IndentationLevel; IndentIndex += 1) {
        DbgOut("  ");
    }

    EntryAddress = Address;
    Success = DbgReadMemory(Context,
                            TRUE,
                            Address,
                            sizeof(ARBITER_ENTRY),
                            &Entry,
                            &BytesRead);

    if ((Success != 0) || (BytesRead != sizeof(ARBITER_ENTRY))) {
        DbgOut("Error: Could not read entry at 0x%I64x.\n",
               Address);

        return FALSE;
    }

    switch (Entry.Type) {
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

    DbgOut("%08I64x %9s: %08I64x, Len %08I64x, Char %I64x, Requirement "
           "%x, Device %x",
           EntryAddress,
           SpaceType,
           Entry.Allocation,
           Entry.Length,
           Entry.Characteristics,
           Entry.CorrespondingRequirement,
           Entry.Device);

    if (Entry.DependentEntry != NULL) {
        DbgOut(", Dependent %x", Entry.DependentEntry);
    }

    if ((Entry.Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        DbgOut(" NotShared");
    }

    if ((Entry.Flags & RESOURCE_FLAG_BOOT) != 0) {
        DbgOut(" Boot");
    }

    DbgOut("\n");
    return TRUE;
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

    case ResourceTypeDmaLine:
        ResourceType = "DMA Line";
        break;

    case ResourceTypeVendorSpecific:
        ResourceType = "Vendor Specific";
        break;

    case ResourceTypeGpio:
        ResourceType = "GPIO";
        break;

    default:
        ResourceType = "INVALID RESOURCE TYPE";
        break;
    }

    return ResourceType;
}

