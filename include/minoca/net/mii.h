/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mii.h

Abstract:

    This header contains definitions for the Media Independent Interface, a
    register set commonly used on networking PHYs.

Author:

    Evan Green 8-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define MII_PHY_COUNT 32

//
// Define MII Basic Control register bits.
//

#define MII_BASIC_CONTROL_SPEED_1000              0x0040
#define MII_BASIC_CONTROL_COLLISION_TEST          0x0080
#define MII_BASIC_CONTROL_FULL_DUPLEX             0x0100
#define MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION 0x0200
#define MII_BASIC_CONTROL_ISOLATE                 0x0400
#define MII_BASIC_CONTROL_POWER_DOWN              0x0800
#define MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION  0x1000
#define MII_BASIC_CONTROL_SPEED_100               0x2000
#define MII_BASIC_CONTROL_LOOPBACK                0x4000
#define MII_BASIC_CONTROL_RESET                   0x8000

//
// Define MII Basic Status register bits.
//

#define MII_BASIC_STATUS_EXTENDED_CAPABILITY      0x0001
#define MII_BASIC_STATUS_JABBER_DETECTED          0x0002
#define MII_BASIC_STATUS_LINK_STATUS              0x0004
#define MII_BASIC_STATUS_AUTONEGOTIATE_CAPABLE    0x0008
#define MII_BASIC_STATUS_REMOTE_FAULT             0x0010
#define MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE   0x0020
#define MII_BASIC_STATUS_PREAMBLE_SUPPRESSION     0x0040

//
// This bit is set if there is extended status in register 0x0F.
//

#define MII_BASIC_STATUS_EXTENDED_STATUS          0x0100

//
// This bit is set if the PHY can do 100BASE-T2 half-duplex.
//

#define MII_BASIC_STATUS_100_HALF2                0x0200

//
// This bit is set if the PHY can do 100BASE-T2 full-duplex.
//

#define MII_BASIC_STATUS_100_FULL2                0x0400

//
// This bit is set if the PHY can do 10 Mbps half-duplex.
//

#define MII_BASIC_STATUS_10_HALF                  0x0800

//
// This bit is set if the PHY can do 10 Mbps full-duplex.
//

#define MII_BASIC_STATUS_10_FULL                  0x1000

//
// This bit is set if the PHY can do 100 Mbps, half-duplex.
//

#define MII_BASIC_STATUS_100_HALF                 0x2000

//
// This bit is set if the PHY can do 100 Mbps, full-duplex.
//

#define MII_BASIC_STATUS_100_FULL                 0x4000

//
// This bit is set if the PHY can do 100 Mbps with 4k packets.
//

#define MII_BASIC_STATUS_100_BASE4                0x8000

#define MII_BASIC_STATUS_MEDIA_MASK \
    (MII_BASIC_STATUS_100_HALF2 | \
     MII_BASIC_STATUS_100_FULL2 | \
     MII_BASIC_STATUS_10_HALF | \
     MII_BASIC_STATUS_10_FULL | \
     MII_BASIC_STATUS_100_HALF | \
     MII_BASIC_STATUS_100_FULL | \
     MII_BASIC_STATUS_100_BASE4)

//
// Define MII Advertise register bits.
//

#define MII_ADVERTISE_SELECT_MASK            0x001F
#define MII_ADVERTISE_CSMA                   0x0001
#define MII_ADVERTISE_10_HALF                0x0020
#define MII_ADVERTISE_1000X_FULL             0x0020
#define MII_ADVERTISE_10_FULL                0x0040
#define MII_ADVERTISE_1000X_HALF             0x0040
#define MII_ADVERTISE_100_HALF               0x0080
#define MII_ADVERTISE_1000X_PAUSE            0x0080
#define MII_ADVERTISE_100_FULL               0x0100
#define MII_ADVERTISE_1000X_PAUSE_ASYMMETRIC 0x0100
#define MII_ADVERTISE_100_BASE4              0x0200
#define MII_ADVERTISE_PAUSE                  0x0400
#define MII_ADVERTISE_PAUSE_ASYMMETRIC       0x0800
#define MII_ADVERTISE_REMOTE_FAULT           0x2000
#define MII_ADVERTISE_LINK_PARTNER           0x4000
#define MII_ADVERTISE_NEXT_PAGE              0x8000

#define MII_ADVERTISE_FULL \
    (MII_ADVERTISE_100_FULL | MII_ADVERTISE_10_FULL | MII_ADVERTISE_CSMA)

#define MII_ADVERTISE_ALL                               \
    (MII_ADVERTISE_10_HALF | MII_ADVERTISE_10_FULL |    \
     MII_ADVERTISE_100_HALF | MII_ADVERTISE_100_FULL)

//
// Define MII Gigabit control register bits.
//

#define MII_GIGABIT_CONTROL_MANUAL_MASTER       0x1000
#define MII_GIGABIT_CONTROL_ADVANCED_MASTER     0x0800
#define MII_GIGABIT_CONTROL_ADVERTISE_1000_FULL 0x0200
#define MII_GIGABIT_CONTROL_ADVERTISE_1000_HALF 0x0100

//
// Define MII Gigabit status register bits.
//

#define MII_GIGABIT_STATUS_ASYMMETRIC_PAUSE_CAPABLE 0x0200
#define MII_GIGABIT_STATUS_PARTNER_1000_HALF        0x0400
#define MII_GIGABIT_STATUS_PARTNER_1000_FULL        0x0800
#define MII_GIGABIT_STATUS_REMOTE_RX_STATUS         0x1000
#define MII_GIGABIT_STATUS_LOCAL_RX_STATUS          0x2000
#define MII_GIGABIT_STATUS_MASTER                   0x4000
#define MII_GIGABIT_STATUS_MASTER_SLAVE_FAULT       0x8000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MII_REGISTER {
    MiiRegisterBasicControl               = 0x00, // BMCR
    MiiRegisterBasicStatus                = 0x01, // BMSR
    MiiRegisterPhysicalId1                = 0x02, // PHYSID1
    MiiRegisterPhysicalId2                = 0x03, // PHYSID2
    MiiRegisterAdvertise                  = 0x04, // ADVERTISE
    MiiRegisterLinkPartnerAbility         = 0x05, // LPA
    MiiRegisterExpansion                  = 0x06, // EXPANSION
    MiiRegisterGigabitControl             = 0x09, // CTRL1000
    MiiRegisterGigabitStatus              = 0x0A, // STAT1000
    MiiRegisterExtendedStatus             = 0x0F, // ESTATUS
    MiiRegisterDisconnectCounter          = 0x12, // DCOUNTER
    MiiRegisterFalseCarrierCounter        = 0x13, // FCSCOUNTER
    MiiRegisterNWayTest                   = 0x14, // NWAYTEST
    MiiRegisterReceiveErrorCounter        = 0x15, // RERRCOUNTER
    MiiRegisterSiliconRevision            = 0x16, // SREVISION
    MiiRegisterLoopbackReceiveBypassError = 0x18, // LBRERROR
    MiiRegisterPhyAddress                 = 0x19, // PHYADDR
    MiiRegisterTpiStatus                  = 0x1B, // TPISTATUS
    MiiRegisterNetworkConfiguration       = 0x1C, // NCONFIG
} MII_REGISTER, *PMII_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
