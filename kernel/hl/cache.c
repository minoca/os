/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cache.c

Abstract:

    This module implements cache support for the hardware library.

Author:

    Chris Stevens 13-Jan-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "hlp.h"
#include "cache.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a cache controller that has been registered with the
    system.

Members:

    ListEntry - Stores pointers to the next and previous cache controllers in
        the system.

    FunctionTable - Stores pointers to functions implemented by the hardware
        module abstracting this cache controller.

    PrivateContext - Stores a pointer to the hardware module's private context.

    Identifier - Stores the unique hardware identifier of the cache controller.

    Flags - Stores a bitmaks of cache controller flags.
        See CACHE_CONTROLLER_FLAG_* for definitions.

--*/

typedef struct _CACHE_CONTROLLER {
    LIST_ENTRY ListEntry;
    CACHE_CONTROLLER_FUNCTION_TABLE FunctionTable;
    PVOID PrivateContext;
    ULONG Identifier;
    ULONG Flags;
} CACHE_CONTROLLER, *PCACHE_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of registered cache controllers.
//

LIST_ENTRY HlCacheControllers;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlFlushCache (
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the cache for every registered cache controller.

Arguments:

    Flags - Supplies a bitmask of cache flush flags. See HL_CACHE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    PCACHE_CONTROLLER CurrentCache;
    PLIST_ENTRY CurrentEntry;
    PCACHE_CONTROLLER_FLUSH Flush;

    //
    // Iterate over each cache and flush it.
    //

    CurrentEntry = HlCacheControllers.Next;
    while (CurrentEntry != &HlCacheControllers) {
        CurrentCache = LIST_VALUE(CurrentEntry, CACHE_CONTROLLER, ListEntry);
        Flush = CurrentCache->FunctionTable.Flush;
        Flush(CurrentCache->PrivateContext, Flags);
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

VOID
HlFlushCacheRegion (
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the given cache region for every registered cache
    controller.

Arguments:

    Address - Supplies the starting physical address of the region to flush. It
        must be aligned to the cache line size.

    SizeInBytes - Supplies the number of bytes to flush.

    Flags - Supplies a bitmask of cache flush flags. See HL_CACHE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    PCACHE_CONTROLLER CurrentCache;
    PLIST_ENTRY CurrentEntry;
    PCACHE_CONTROLLER_FLUSH_REGION FlushRegion;

    //
    // Iterate over each cache and flush the region.
    //

    CurrentEntry = HlCacheControllers.Next;
    while (CurrentEntry != &HlCacheControllers) {
        CurrentCache = LIST_VALUE(CurrentEntry, CACHE_CONTROLLER, ListEntry);
        FlushRegion = CurrentCache->FunctionTable.FlushRegion;
        FlushRegion(CurrentCache->PrivateContext, Address, SizeInBytes, Flags);
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

ULONG
HlGetDataCacheLineSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the maximum data cache line size out of all registered
    cache controllers.

Arguments:

    None.

Return Value:

    Returns the maximum data cache line size out of all registered cache
    controllers in bytes.

--*/

{

    PCACHE_CONTROLLER CurrentCache;
    PLIST_ENTRY CurrentEntry;
    PCACHE_CONTROLLER_GET_PROPERTIES GetProperties;
    ULONG MaxDataCacheLineSize;
    CACHE_CONTROLLER_PROPERTIES Properties;
    KSTATUS Status;

    MaxDataCacheLineSize = 0;
    Properties.Version = CACHE_CONTROLLER_PROPERTIES_VERSION;

    //
    // Iterate over each cache and find the biggest data cache line size.
    //

    CurrentEntry = HlCacheControllers.Next;
    while (CurrentEntry != &HlCacheControllers) {
        CurrentCache = LIST_VALUE(CurrentEntry, CACHE_CONTROLLER, ListEntry);
        GetProperties = CurrentCache->FunctionTable.GetProperties;
        Status = GetProperties(CurrentCache->PrivateContext, &Properties);
        if (KSUCCESS(Status)) {

            ASSERT(POWER_OF_2(Properties.DataCacheLineSize) != FALSE);
            ASSERT(Properties.Version == CACHE_CONTROLLER_PROPERTIES_VERSION);

            if (Properties.DataCacheLineSize > MaxDataCacheLineSize) {
                MaxDataCacheLineSize = Properties.DataCacheLineSize;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return MaxDataCacheLineSize;
}

KSTATUS
HlpInitializeCacheControllers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the cache subsystem.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = STATUS_SUCCESS;
    if (KeGetCurrentProcessorNumber() == 0) {
        INITIALIZE_LIST_HEAD(&HlCacheControllers);

        //
        // Perform architecture-specific initialization.
        //

        Status = HlpArchInitializeCacheControllers();
        if (!KSUCCESS(Status)) {
            goto InitializeCachesEnd;
        }
    }

InitializeCachesEnd:
    return Status;
}

KSTATUS
HlpCacheControllerRegisterHardware (
    PCACHE_CONTROLLER_DESCRIPTION CacheDescription
    )

/*++

Routine Description:

    This routine is called to register a new cache controller with the system.

Arguments:

    CacheDescription - Supplies a pointer to a structure describing the new
        cache controller.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PCACHE_CONTROLLER CacheController;
    PCACHE_CONTROLLER_INITIALIZE Initialize;
    KSTATUS Status;

    CacheController = NULL;

    //
    // Check the table version.
    //

    if (CacheDescription->TableVersion < CACHE_CONTROLLER_DESCRIPTION_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto CacheControllerRegisterHardwareEnd;
    }

    //
    // Check the properties version.
    //

    if (CacheDescription->PropertiesVersion <
        CACHE_CONTROLLER_PROPERTIES_VERSION) {

        Status = STATUS_VERSION_MISMATCH;
        goto CacheControllerRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if ((CacheDescription->FunctionTable.Flush == NULL) ||
        (CacheDescription->FunctionTable.FlushRegion == NULL) ||
        (CacheDescription->FunctionTable.GetProperties == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CacheControllerRegisterHardwareEnd;
    }

    //
    // Allocate the new controller object.
    //

    AllocationSize = sizeof(CACHE_CONTROLLER);
    CacheController = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (CacheController == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CacheControllerRegisterHardwareEnd;
    }

    RtlZeroMemory(CacheController, AllocationSize);

    //
    // Initialize the new cache controller based on the description.
    //

    RtlCopyMemory(&(CacheController->FunctionTable),
                  &(CacheDescription->FunctionTable),
                  sizeof(CACHE_CONTROLLER_FUNCTION_TABLE));

    CacheController->Identifier = CacheDescription->Identifier;
    CacheController->PrivateContext = CacheDescription->Context;

    //
    // Insert the cache controller into the list.
    //

    INSERT_BEFORE(&(CacheController->ListEntry), &HlCacheControllers);

    //
    // Initialize the new cache controller immediately.
    //

    Status = STATUS_SUCCESS;
    Initialize = CacheController->FunctionTable.Initialize;
    if (Initialize != NULL) {
        Status = Initialize(CacheController->PrivateContext);
    }

    if (!KSUCCESS(Status)) {
        CacheController->Flags |= CACHE_CONTROLLER_FLAG_FAILED;

    } else {
        CacheController->Flags |= CACHE_CONTROLLER_FLAG_INITIALIZED;
    }

    Status = STATUS_SUCCESS;

CacheControllerRegisterHardwareEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

