/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgport.c

Abstract:

    This module attempts to set up the kernel debugging transport.

Author:

    Evan Green 25-Mar-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlib.h"
#include "paging.h"
#include "loader.h"
#include "dbgport.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of uncached memory to allocate for the debug device.
//

#define DEBUG_DEVICE_MEMORY_SIZE 0x2000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopMapDebugPortTable (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters,
    PDEBUG_PORT_TABLE2 Table
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
BoSetUpKernelDebugTransport (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters
    )

/*++

Routine Description:

    This routine attempts to set up the kernel debugger transport.

Arguments:

    KernelParameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None. Failure here is not fatal so it is not reported.

--*/

{

    PDEBUG_PORT_TABLE2 DebugPortTable;
    PDEBUG_PORT_TABLE2 GeneratedDebugPortTable;
    KSTATUS Status;

    GeneratedDebugPortTable = NULL;

    //
    // Go exploring PCI for debug devices. This is done even if there is a
    // debug port table so that legacy interrupts can be found and squelched.
    //

    Status = BopExploreForDebugDevice(&GeneratedDebugPortTable);
    if (!KSUCCESS(Status)) {
        return;
    }

    //
    // Find the debug port table. If there isn't one, use the generated one.
    // Otherwise, free the generated one.
    //

    DebugPortTable = BoGetAcpiTable(DBG2_SIGNATURE, NULL);
    if (DebugPortTable == NULL) {
        DebugPortTable = GeneratedDebugPortTable;

    } else if (GeneratedDebugPortTable != NULL) {
        BoFreeMemory(GeneratedDebugPortTable);
        GeneratedDebugPortTable = NULL;
    }

    if (GeneratedDebugPortTable != NULL) {
        BoAddFirmwareTable(KernelParameters, GeneratedDebugPortTable);
    }

    if (DebugPortTable != NULL) {
        BopMapDebugPortTable(KernelParameters, DebugPortTable);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopMapDebugPortTable (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters,
    PDEBUG_PORT_TABLE2 Table
    )

/*++

Routine Description:

    This routine maps the given debug port table and creates a system resource.

Arguments:

    KernelParameters - Supplies a pointer to the kernel initialization block.

    Table - Supplies a pointer to the debug port table.

Return Value:

    Status code.

--*/

{

    PGENERIC_ADDRESS Address;
    ULONG AddressIndex;
    PULONG AddressSize;
    PDEBUG_DEVICE_INFORMATION Device;
    ULONG DeviceIndex;
    PSYSTEM_RESOURCE_DEBUG_DEVICE Resource;
    KSTATUS Status;

    if (Table->DeviceInformationCount == 0) {
        return STATUS_SUCCESS;
    }

    Resource = NULL;

    //
    // Loop through every debug device.
    //

    Device = (PDEBUG_DEVICE_INFORMATION)((PUCHAR)Table +
                                         Table->DeviceInformationOffset);

    for (DeviceIndex = 0;
         DeviceIndex < Table->DeviceInformationCount;
         DeviceIndex += 1) {

        //
        // Work through every address.
        //

        Address = (PGENERIC_ADDRESS)((PUCHAR)Device +
                       READ_UNALIGNED16(&(Device->BaseAddressRegisterOffset)));

        AddressSize = (PULONG)((PUCHAR)Device +
                               READ_UNALIGNED16(&(Device->AddressSizeOffset)));

        for (AddressIndex = 0;
             AddressIndex < Device->GenericAddressCount;
             AddressIndex += 1) {

            //
            // If it's memory, map it.
            //

            if (Address->AddressSpaceId == AddressSpaceMemory) {
                Resource =
                        BoAllocateMemory(sizeof(SYSTEM_RESOURCE_DEBUG_DEVICE));

                if (Resource == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto MapDebugPortTableEnd;
                }

                RtlZeroMemory(Resource, sizeof(SYSTEM_RESOURCE_DEBUG_DEVICE));
                Resource->Header.Type = SystemResourceDebugDevice;
                Resource->Header.PhysicalAddress =
                                         READ_UNALIGNED64(&(Address->Address));

                Resource->Header.Size = READ_UNALIGNED32(AddressSize);
                Resource->Header.VirtualAddress = (PVOID)-1;
                Status = BoMapPhysicalAddress(
                                      &(Resource->Header.VirtualAddress),
                                      Resource->Header.PhysicalAddress,
                                      Resource->Header.Size,
                                      MAP_FLAG_CACHE_DISABLE | MAP_FLAG_GLOBAL,
                                      MemoryTypeHardware);

                if (!KSUCCESS(Status)) {
                    goto MapDebugPortTableEnd;
                }

                INSERT_BEFORE(&(Resource->Header.ListEntry),
                              &(KernelParameters->SystemResourceListHead));
            }

            //
            // Move to the next address.
            //

            Address += 1;
            AddressSize += 1;
        }

        //
        // Move to the next device.
        //

        Device = (PDEBUG_DEVICE_INFORMATION)((PUCHAR)Device +
                                          READ_UNALIGNED16(&(Device->Length)));
    }

    Status = STATUS_SUCCESS;

MapDebugPortTableEnd:
    if (!KSUCCESS(Status)) {
        if (Resource != NULL) {
            if (Resource->Header.VirtualAddress != NULL) {
                BoUnmapPhysicalAddress(Resource->Header.VirtualAddress,
                                       Resource->Header.Size / MmPageSize());
            }

            BoFreeMemory(Resource);
        }
    }

    return Status;
}

