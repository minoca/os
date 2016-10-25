/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4chc.c

Abstract:

    This module implements the support for the PrimeCell PL-310 L2 cache
    controller on the TI OMAP4.

Author:

    Chris Stevens 13-Jan-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>
#include "omap4.h"
#include "pl310.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// This macro performs a 32-bit read from the Omap4's PL-310 register. The
// _Register should be a PL310_REGISTER.
//

#define READ_CACHE_REGISTER(_Register) \
    HlReadRegister32(HlOmap4Pl310RegistersBase + (_Register))

//
// This macro performs a 32-bit write to the Omap4's PL-310 register. The
// _Register should be a PL310_REGISTER.
//

#define WRITE_CACHE_REGISTER(_Register, _Value) \
    HlWriteRegister32(HlOmap4Pl310RegistersBase + (_Register), (_Value))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
HlpOmap4SmcCommand (
    ULONG Argument1,
    ULONG Argument2,
    ULONG Command
    );

KSTATUS
HlpOmap4CacheInitialize (
    PVOID Context
    );

VOID
HlpOmap4CacheFlush (
    PVOID Context,
    ULONG Flags
    );

VOID
HlpOmap4CacheFlushRegion (
    PVOID Context,
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    );

KSTATUS
HlpOmap4GetCacheProperties (
    PVOID Context,
    PCACHE_CONTROLLER_PROPERTIES Properties
    );

VOID
HlpOmap4CacheFlushByIndex (
    PPL310_CACHE_DATA CacheData,
    ULONG Register
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the virtual address of OMAP4's mapped PL-310 registers.
//

PVOID HlOmap4Pl310RegistersBase;

//
// Stores a lock that protects access to the registers.
//

HARDWARE_MODULE_LOCK HlOmap4Pl310RegisterLock;

//
// Store the physical address of the OMAP4's PL-310 register base.
//

PHYSICAL_ADDRESS HlOmap4Pl310RegistersPhysicalBase;

//
// Store a boolean indicating whether keeping the OMAP4's PL-310 cache
// controller disabled should be forced.
//

BOOL HlOmap4Pl310ForceDisable = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpOmap4CacheControllerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the OMAP4's Cache controller module.
    Its role is to detect and report the presence of any cache controllers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    CACHE_CONTROLLER_DESCRIPTION CacheController;
    PCACHE_CONTROLLER_PROPERTIES Pl310Data;
    KSTATUS Status;

    if (HlOmap4Pl310ForceDisable != FALSE) {
        goto Omap4CacheControllerModuleEntryEnd;
    }

    //
    // Timers are always initialized before cache controllers, so the OMAP4
    // table should already be set up.
    //

    if (HlOmap4Table == NULL) {
        goto Omap4CacheControllerModuleEntryEnd;
    }

    HlInitializeLock(&HlOmap4Pl310RegisterLock);
    HlOmap4Pl310RegistersPhysicalBase =
                               HlOmap4Table->Pl310RegistersBasePhysicalAddress;

    //
    // Report the physical address space that the PL-310 is occupying.
    //

    HlReportPhysicalAddressUsage(HlOmap4Pl310RegistersPhysicalBase,
                                 PL310_REGISTER_SIZE);

    Pl310Data = HlAllocateMemory(sizeof(PL310_CACHE_DATA),
                                 OMAP4_ALLOCATION_TAG,
                                 FALSE,
                                 NULL);

    if (Pl310Data == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Omap4CacheControllerModuleEntryEnd;
    }

    RtlZeroMemory(Pl310Data, sizeof(PL310_CACHE_DATA));
    RtlZeroMemory(&CacheController, sizeof(CACHE_CONTROLLER_DESCRIPTION));
    CacheController.TableVersion = CACHE_CONTROLLER_DESCRIPTION_VERSION;
    CacheController.FunctionTable.Initialize = HlpOmap4CacheInitialize;
    CacheController.FunctionTable.Flush = HlpOmap4CacheFlush;
    CacheController.FunctionTable.FlushRegion = HlpOmap4CacheFlushRegion;
    CacheController.FunctionTable.GetProperties = HlpOmap4GetCacheProperties;
    CacheController.Context = Pl310Data;
    CacheController.PropertiesVersion = CACHE_CONTROLLER_PROPERTIES_VERSION;
    Status = HlRegisterHardware(HardwareModuleCacheController,
                                &CacheController);

    if (!KSUCCESS(Status)) {
        goto Omap4CacheControllerModuleEntryEnd;
    }

Omap4CacheControllerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpOmap4CacheInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes a cache controller to enable the cache and prepare
    it for clean and invalidate calls.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The cache controller will not be used if a
    failure status code is returned.

--*/

{

    BOOL LockHeld;
    PPL310_CACHE_DATA Pl310CacheData;
    KSTATUS Status;
    ULONG Value;
    ULONG WayMask;
    ULONG WaySizeShift;

    LockHeld = FALSE;
    Pl310CacheData = (PPL310_CACHE_DATA)Context;

    //
    // Map the controller if it has not yet been done.
    //

    if (HlOmap4Pl310RegistersBase == NULL) {
        HlOmap4Pl310RegistersBase = HlMapPhysicalAddress(
                                             HlOmap4Pl310RegistersPhysicalBase,
                                             PL310_REGISTER_SIZE,
                                             TRUE);

        if (HlOmap4Pl310RegistersBase == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Pl310CacheInitializeEnd;
        }
    }

    //
    // Acquire the lock before modifying the registers.
    //

    HlAcquireLock(&HlOmap4Pl310RegisterLock);
    LockHeld = TRUE;

    //
    // Disable the cache.
    //

    Value = READ_CACHE_REGISTER(Pl310Control);
    if ((Value & PL310_CONTROL_L2_CACHE_ENABLED) != 0) {
        Value &= ~PL310_CONTROL_L2_CACHE_ENABLED;
        HlpOmap4SmcCommand(Value,
                           0,
                           OMAP4_SMC_COMMAND_WRITE_L2_CACHE_CONTROL_REGISTER);
    }

    //
    // Determine the RTL release version of the cache.
    //

    Value = READ_CACHE_REGISTER(Pl310CacheId);
    Value = (Value & PL310_CACHE_ID_RTL_RELEASE_MASK) >>
            PL310_CACHE_ID_RTL_RELEASE_SHIFT;

    Pl310CacheData->CacheRelease = (UCHAR)Value;

    //
    // Make sure there isn't anything unexpected set for the cache type.
    //

    Value = READ_CACHE_REGISTER(Pl310CacheType);
    if ((Value & PL310_CACHE_TYPE_HARVARD) != 0) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto Pl310CacheInitializeEnd;
    }

    if ((Value & PL310_CACHE_TYPE_L2_DATA_LINE_SIZE_MASK) != 0) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto Pl310CacheInitializeEnd;
    }

    if ((Value & PL310_CACHE_TYPE_L2_INSTRUCTION_LINE_SIZE_MASK) != 0) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto Pl310CacheInitializeEnd;
    }

    //
    // Determine the way count and the size of each way and record it.
    //

    Value = READ_CACHE_REGISTER(Pl310AuxiliaryControl);
    if ((Value & PL310_AUXILIARY_CONTROL_ASSOCIATIVITY) == 0) {
        WayMask = PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_8;
        Pl310CacheData->WayCount = 8;

    } else {
        WayMask = PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_16;
        Pl310CacheData->WayCount = 16;
    }

    //
    // Get the encoded way shift and then effectively multiply the shifted
    // value by 8KB.
    //

    WaySizeShift = (Value & PL310_AUXILIARY_CONTROL_WAY_SIZE_MASK) >>
                   PL310_AUXILIARY_CONTROL_WAY_SIZE_SHIFT;

    Pl310CacheData->WaySize = 1 << (WaySizeShift + PL310_8KB_SHIFT);
    Pl310CacheData->CacheSize = Pl310CacheData->WaySize *
                                Pl310CacheData->WayCount;

    //
    // Invalidate all entries in the cache.
    //

    WRITE_CACHE_REGISTER(Pl310InvalidateWay, WayMask);
    while (TRUE) {
        Value = READ_CACHE_REGISTER(Pl310InvalidateWay);
        if ((Value & WayMask) == 0) {
            break;
        }
    }

    //
    // Clear any residual raw interrupts.
    //

    WRITE_CACHE_REGISTER(Pl310InterruptClear, PL310_INTERRUPT_MASK);

    //
    // Mask all future interrupts.
    //

    WRITE_CACHE_REGISTER(Pl310InterruptMask, PL310_INTERRUPT_MASK);

    //
    // Enable the L2 cache.
    //

    Value = READ_CACHE_REGISTER(Pl310Control);
    Value |= PL310_CONTROL_L2_CACHE_ENABLED;
    HlpOmap4SmcCommand(Value,
                       0,
                       OMAP4_SMC_COMMAND_WRITE_L2_CACHE_CONTROL_REGISTER);

    Status = STATUS_SUCCESS;

Pl310CacheInitializeEnd:
    if (LockHeld != FALSE) {
        HlReleaseLock(&HlOmap4Pl310RegisterLock);
    }

    return Status;
}

VOID
HlpOmap4CacheFlush (
    PVOID Context,
    ULONG Flags
    )

/*++

Routine Description:

    This routine cleans and/or invalidates the cache owned by the cache
    controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Flags - Supplies a bitmask of flush flags. See CACHE_CONTROLLER_FLUSH_FLAG_*
        for definitions.

Return Value:

    None.

--*/

{

    PPL310_CACHE_DATA Pl310CacheData;
    PL310_REGISTER SyncRegister;
    ULONG Value;
    ULONG WayMask;

    Pl310CacheData = (PPL310_CACHE_DATA)Context;

    //
    // Get which register to use for the cache synchronization.
    //

    SyncRegister = Pl310CacheSync;
    if (Pl310CacheData->CacheRelease == PL310_CACHE_ID_RTL_RELEASE_R3P0) {
        SyncRegister = Pl310CacheSyncR3P0;
    }

    //
    // Acquire the lock before modifying the registers.
    //

    HlAcquireLock(&HlOmap4Pl310RegisterLock);
    if (((Flags & HL_CACHE_FLAG_CLEAN) != 0) &&
        ((Flags & HL_CACHE_FLAG_INVALIDATE) != 0)) {

        HlpOmap4CacheFlushByIndex(Pl310CacheData, Pl310CleanInvalidateIndex);

    } else if ((Flags & HL_CACHE_FLAG_CLEAN) != 0) {
        HlpOmap4CacheFlushByIndex(Pl310CacheData, Pl310CleanIndex);

    } else if ((Flags & HL_CACHE_FLAG_INVALIDATE) != 0) {
        if (Pl310CacheData->WayCount == 8) {
            WayMask = PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_8;

        } else {
            WayMask = PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_16;
        }

        WRITE_CACHE_REGISTER(Pl310InvalidateWay, WayMask);
        while (TRUE) {
            Value = READ_CACHE_REGISTER(Pl310InvalidateWay);
            if ((Value & WayMask) == 0) {
                break;
            }
        }
    }

    //
    // Now synchronize the cache.
    //

    WRITE_CACHE_REGISTER(SyncRegister, PL310_CACHE_SYNC_VALUE);
    HlReleaseLock(&HlOmap4Pl310RegisterLock);
    return;
}

VOID
HlpOmap4CacheFlushRegion (
    PVOID Context,
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    )

/*++

Routine Description:

    This routine cleans and/or invalidates a region of the cache owned by the
    cache controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Address - Supplies the starting physical address of the region to flush. It
        must be aligned to the cache line size.

    SizeInBytes - Supplies the number of bytes to flush.

    Flags - Supplies a bitmask of flush flags. See CACHE_CONTROLLER_FLUSH_FLAG_*
        for definitions.

Return Value:

    None.

--*/

{

    PPL310_CACHE_DATA Pl310CacheData;
    ULONG Register;
    PL310_REGISTER SyncRegister;
    ULONG Value;

    //
    // It will probably be more noticable to refuse to flush an unaligned
    // address than to quietly flush it and potentially corrupt the tip of some
    // other buffer.
    //

    if (IS_ALIGNED(Address, PL310_DATA_CACHE_LINE_SIZE) == FALSE) {
        return;
    }

    Pl310CacheData = (PPL310_CACHE_DATA)Context;
    if (((Flags & HL_CACHE_FLAG_CLEAN) != 0) &&
        ((Flags & HL_CACHE_FLAG_INVALIDATE) != 0)) {

        Register = Pl310CleanInvalidatePhysical;

    } else if ((Flags & HL_CACHE_FLAG_CLEAN) != 0) {
        Register = Pl310CleanPhysical;

    } else if ((Flags & HL_CACHE_FLAG_INVALIDATE) != 0) {
        Register = Pl310InvalidatePhysical;

    } else {
        return;
    }

    //
    // Get which register to use for the cache synchronization.
    //

    SyncRegister = Pl310CacheSync;
    if (Pl310CacheData->CacheRelease == PL310_CACHE_ID_RTL_RELEASE_R3P0) {
        SyncRegister = Pl310CacheSyncR3P0;
    }

    //
    // Acquire the lock before modifying the registers.
    //

    HlAcquireLock(&HlOmap4Pl310RegisterLock);
    Value = 0;
    while (SizeInBytes != 0) {
        Value = Address & PL310_CACHE_MAINTENANCE_PA_MASK;
        WRITE_CACHE_REGISTER(Register, Value);
        SizeInBytes -= PL310_DATA_CACHE_LINE_SIZE;
        Address += PL310_DATA_CACHE_LINE_SIZE;
    }

    //
    // Now synchronize the cache.
    //

    WRITE_CACHE_REGISTER(SyncRegister, PL310_CACHE_SYNC_VALUE);
    HlReleaseLock(&HlOmap4Pl310RegisterLock);
    return;
}

KSTATUS
HlpOmap4GetCacheProperties (
    PVOID Context,
    PCACHE_CONTROLLER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine cleans and invalidates the cache owned by the cache controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Properties - Supplies a pointer that receives the properties of the given
        cache controller (e.g. cache line size).

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PPL310_CACHE_DATA Pl310CacheData;
    KSTATUS Status;

    //
    // Fill out the properties with the minimum shared information between the
    // system's requested version and this module's version. As case statements
    // for higher version numbers are added, there should be no break statement
    // so that they fall through to fill out data for lower version numbers.
    //

    Pl310CacheData = (PPL310_CACHE_DATA)Context;
    switch (Properties->Version) {
    case CACHE_CONTROLLER_PROPERTIES_VERSION:
        Properties->CacheSize = Pl310CacheData->CacheSize;
        Properties->DataCacheLineSize = PL310_DATA_CACHE_LINE_SIZE;
        Properties->InstructionCacheLineSize =
                                             PL310_INSTRUCTION_CACHE_LINE_SIZE;

        Status = STATUS_SUCCESS;
        break;

    //
    // If none of the cases matched, then the system is requesting a cache
    // controller properties version greater than what is supported by this
    // module.
    //

    default:
        Status = STATUS_VERSION_MISMATCH;
        break;
    }

    return Status;
}

VOID
HlpOmap4CacheFlushByIndex (
    PPL310_CACHE_DATA CacheData,
    ULONG Register
    )

/*++

Routine Description:

    This routine either cleans or cleans and invalidates the entire cache by
    index. PL-310 Errata 727915 states that the background clean and clean and
    invalidate by way registers do not work for the r2p0 PL-310, which the
    Omap4 has.

Arguments:

    CacheData - Supplies a pointer to the recorded cache data captured during
        initialization.

    Register - Supplies the PL-310 register to use for the flush.

Return Value:

    None.

--*/

{

    ULONG SetCount;
    ULONG SetIndex;
    ULONG Value;
    UCHAR WayIndex;

    SetCount = CacheData->WaySize / PL310_DATA_CACHE_LINE_SIZE;
    for (WayIndex = 0; WayIndex < CacheData->WayCount; WayIndex += 1) {
        for (SetIndex = 0; SetIndex < SetCount; SetIndex += 1) {
            Value = WayIndex << PL310_CACHE_MAINTENANCE_WAY_SHIFT;
            Value |= (SetIndex << PL310_CACHE_MAINTENANCE_SET_SHIFT);
            WRITE_CACHE_REGISTER(Register, Value);
        }
    }

    return;
}

