/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl310.h

Abstract:

    This header contains definitions for the PL-310 L2 cache controller.

Author:

    Chris Stevens 17-Jan-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define constants for the PL310 registers.
//

#define PL310_REGISTER_BASE_ALIGNMENT 0x4
#define PL310_REGISTER_SIZE _4KB

//
// Define the PL310 cache ID register values.
//

#define PL310_CACHE_ID_IMPLEMENTER_MASK  (0xFF << 24)
#define PL310_CACHE_ID_IMPLEMENTER_SHIFT 24
#define PL310_CACHE_ID_CACHE_ID_MASK     (0x3F << 10)
#define PL310_CACHE_ID_CACHE_ID_SHIFT    10
#define PL310_CACHE_ID_PART_NUMBER_MASK  (0xF << 6)
#define PL310_CACHE_ID_PART_NUMBER_SHIFT 6
#define PL310_CACHE_ID_RTL_RELEASE_MASK  (0x3F << 0)
#define PL310_CACHE_ID_RTL_RELEASE_SHIFT 0

//
// Define the PL310 RTL Release values.
//

#define PL310_CACHE_ID_RTL_RELEASE_R0P0 0x00
#define PL310_CACHE_ID_RTL_RELEASE_R1P0 0x02
#define PL310_CACHE_ID_RTL_RELEASE_R2P0 0x04
#define PL310_CACHE_ID_RTL_RELEASE_R3P0 0x05
#define PL310_CACHE_ID_RTL_RELEASE_R3P1 0x06
#define PL310_CACHE_ID_RTL_RELEASE_R3P1_50REL0 0x07
#define PL310_CACHE_ID_RTL_RELEASE_R3P2 0x08
#define PL310_CACHE_ID_RTL_RELEASE_R3P3 0x09

//
// Define PL310 cache type register values.
//

#define PL310_CACHE_TYPE_HARVARD (1 << 24)
#define PL310_CACHE_TYPE_L2_DATA_LINE_SIZE_MASK (0x3 << 12)
#define PL310_CACHE_TYPE_L2_INSTRUCTION_LINE_SIZE_MASK (0x3 << 0)

//
// Define PL310 control register values.
//

#define PL310_CONTROL_L2_CACHE_ENABLED (1 << 0)

//
// Define PL310 auxiliary control register values.
//

#define PL310_AUXILIARY_CONTROL_ASSOCIATIVITY (1 << 16)
#define PL310_AUXILIARY_CONTROL_WAY_SIZE_SHIFT 17
#define PL310_AUXILIARY_CONTROL_WAY_SIZE_MASK (0x7 << 17)
#define PL310_AUXILIARY_CONTROL_WAY_16KB 0x1
#define PL310_AUXILIARY_CONTROL_WAY_32KB 0x2
#define PL310_AUXILIARY_CONTROL_WAY_64KB 0x3
#define PL310_AUXILIARY_CONTROL_WAY_128KB 0x4
#define PL310_AUXILIARY_CONTROL_WAY_256KB 0x5
#define PL310_AUXILIARY_CONTROL_WAY_512KB 0x6

//
// Define the extra shift required to calculate the real way size. The encoded
// way sizes are shift values. When 1 is shifted by the encoded way size and
// then multiplied by 8KB, the real way size is obtained. For example,
// (1 << 0x3) equals 8 and 8 times 8KB is 64KB. Multiplication is easier with
// these numbers, because multiplying by 8KB is just another shift by 13. So
// to obtain the real way size, shift 1 by the encoded size plus the shift for
// 8KB (13).
//

#define PL310_8KB_SHIFT 13

//
// Define the PL310 interrupt register values.
//

#define PL310_INTERRUPT_DECERR (1 << 8)
#define PL310_INTERRUPT_SLVERR (1 << 7)
#define PL310_INTERRUPT_ERRRD (1 << 6)
#define PL310_INTERRUPT_ERRRT (1 << 5)
#define PL310_INTERRUPT_ERRWD (1 << 4)
#define PL310_INTERRUPT_ERRWT (1 << 3)
#define PL310_INTERRUPT_PARRD (1 << 2)
#define PL310_INTERRUPT_PARRT (1 << 1)
#define PL310_INTERRUPT_ECNTR (1 << 0)
#define PL310_INTERRUPT_MASK \
    (PL310_INTERRUPT_DECERR | \
     PL310_INTERRUPT_SLVERR | \
     PL310_INTERRUPT_ERRRD | \
     PL310_INTERRUPT_ERRRT | \
     PL310_INTERRUPT_ERRWD | \
     PL310_INTERRUPT_ERRWT | \
     PL310_INTERRUPT_PARRD | \
     PL310_INTERRUPT_PARRT | \
     PL310_INTERRUPT_ECNTR)

//
// Define PL310 cache maintenance values.
//

#define PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_8 0xFF
#define PL310_CACHE_MAINTENANCE_INVALIDATE_WAY_16 0xFFFF
#define PL310_CACHE_MAINTENANCE_PA_MASK (0x7FFFFFF << 5)
#define PL310_CACHE_MAINTENANCE_WAY_MASK (0xF << 28)
#define PL310_CACHE_MAINTENANCE_WAY_SHIFT 28
#define PL310_CACHE_MAINTENANCE_SET_MASK (0x7FFFFF << 5)
#define PL310_CACHE_MAINTENANCE_SET_SHIFT 5

//
// Define PL310 debug control register values.
//

#define PL310_DEBUG_CONTROL_DISABLE_WRITE_BACK (1 << 1)
#define PL310_DEBUG_CONTROL_DISABLE_CACHE_LINEFILL (1 << 0)

//
// Define PL310 cache line size constants.
//

#define PL310_DATA_CACHE_LINE_SIZE 32
#define PL310_INSTRUCTION_CACHE_LINE_SIZE 32

//
// Define PL310 prefetch control registers.
//

#define PL310_PREFETCH_CONTROL_DOUBLE_LINEFILL_INCREMENT (1 << 23)
#define PL310_PREFETCH_CONTROL_DOUBLE_LINEFILL (1 << 30)

//
// Define the value to write to the cache sync register.
//

#define PL310_CACHE_SYNC_VALUE 0xFFFFFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Register set definition for the PL-310. These are offsets in bytes, not
// words.
// Errata 753970 indicates that using the Cache Sync register at offset 0x730
// prevents further write merging of Normal memory. It suggests using the
// undocumented offset 0x740, which apparently acheives the same effect as the
// cache sync register but without the disasters. The PL310 is one of ARM's
// shining jewels, a real beacon of quality. This only applies to release r3p0.
//

typedef enum _PL310_REGISTER {
    Pl310CacheId                 = 0x0,
    Pl310CacheType               = 0x4,
    Pl310Control                 = 0x100,
    Pl310AuxiliaryControl        = 0x104,
    Pl310TagRamControl           = 0x108,
    Pl310DataRamControl          = 0x10C,
    Pl310EventCounterControl     = 0x200,
    Pl310EventCounter1Config     = 0x204,
    Pl310EventCounter0Config     = 0x208,
    Pl310EventCounter1           = 0x20C,
    Pl310EventCounter0           = 0x210,
    Pl310InterruptMask           = 0x214,
    Pl310InterruptMaskStatus     = 0x218,
    Pl310InterruptRawStatus      = 0x21C,
    Pl310InterruptClear          = 0x220,
    Pl310CacheSync               = 0x730,
    Pl310CacheSyncR3P0           = 0x740,
    Pl310InvalidatePhysical      = 0x770,
    Pl310InvalidateWay           = 0x77C,
    Pl310CleanPhysical           = 0x7B0,
    Pl310CleanIndex              = 0x7B8,
    Pl310CleanWay                = 0x7BC,
    Pl310CleanInvalidatePhysical = 0x7F0,
    Pl310CleanInvalidateIndex    = 0x7F8,
    Pl310CleanInvalidateWay      = 0x7FC,
    Pl310DataLockdown0           = 0x900,
    Pl310InstructionLockdown0    = 0x904,
    Pl310DataLockdown1           = 0x908,
    Pl310InstructionLockdown1    = 0x90C,
    Pl310DataLockdown2           = 0x910,
    Pl310InstructionLockdown2    = 0x914,
    Pl310DataLockdown3           = 0x918,
    Pl310InstructionLockdown3    = 0x91C,
    Pl310DataLockdown4           = 0x920,
    Pl310InstructionLockdown4    = 0x924,
    Pl310DataLockdown5           = 0x928,
    Pl310InstructionLockdown5    = 0x92C,
    Pl310DataLockdown6           = 0x930,
    Pl310InstructionLockdown6    = 0x934,
    Pl310DataLockdown7           = 0x938,
    Pl310InstructionLockdown7    = 0x93C,
    Pl310LockLineEn              = 0x950,
    Pl310UnlockWay               = 0x954,
    Pl310AddressFilteringStart   = 0xC00,
    Pl310AddressFilteringEnd     = 0xC04,
    Pl310DebugControl            = 0xF40,
    Pl310PrefetchOffsetRegister  = 0xF60,
    Pl310PowerControlRegister    = 0xF80,
} PL310_REGISTER, *PPL310_REGISTER;

/*+

Structure Description:

    This structure defines the internal cache context private to a PL-310
    cache controller.

Members:

    CacheSize - Stores the size of the cache in bytes.

    WaySize - Stores the size of each way, in bytes.

    WayCount - Stores the number of ways in the cache.

    CacheRelease - Stores the release version of the cache.

--*/

typedef struct _PL310_CACHE_DATA {
    ULONG CacheSize;
    ULONG WaySize;
    UCHAR WayCount;
    UCHAR CacheRelease;
} PL310_CACHE_DATA, *PPL310_CACHE_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

