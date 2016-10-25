/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    generic.h

Abstract:

    This header contains definitions for the generic netlink protocol.

Author:

    Chris Stevens 10-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define NETLINK_GENERIC_ALLOCATION_TAG 0x65676C4E // 'eglN'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a generic netlink family.

Members:

    TreeNode - Stores a node in the global tree of generic netlink families.

    ReferenceCount - Stores the number of references take on the family.

    MulticastGroupOffset - Stores the offset into the entire generic netlink
        multicast group ID namespace where this family's multicast group IDs
        begin.

    Properties - Stores the generic netlink family properties, including its ID
        name, and interface.

--*/

struct _NETLINK_GENERIC_FAMILY {
    RED_BLACK_TREE_NODE TreeNode;
    volatile ULONG ReferenceCount;
    ULONG MulticastGroupOffset;
    NETLINK_GENERIC_FAMILY_PROPERTIES Properties;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Store the lock and tree for storing the generic netlink families.
//

extern PSHARED_EXCLUSIVE_LOCK NetlinkGenericFamilyLock;
extern RED_BLACK_TREE NetlinkGenericFamilyTree;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
NetlinkpGenericControlInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the built in generic netlink control family.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
NetlinkpGenericControlSendNotification (
    PNETLINK_GENERIC_FAMILY Family,
    UCHAR Command,
    PNETLINK_GENERIC_MULTICAST_GROUP Group
    );

/*++

Routine Description:

    This routine sends a generic netlink control command based on the family
    and or group information.

Arguments:

    Family - Supplies a pointer to the generic netlink family for which the
        command is being sent.

    Command - Supplies the generic netlink control command to be sent.

    Group - Supplies an optional pointers to the multicast group that has
        just arrived or is being deleted.

Return Value:

    Status code.

--*/

PNETLINK_GENERIC_FAMILY
NetlinkpGenericLookupFamilyById (
    ULONG FamilyId
    );

/*++

Routine Description:

    This routine searches the database of registered generic netlink families
    for one with the given ID. If successful, the family is returned with an
    added reference which the caller must release.

Arguments:

    FamilyId - Supplies the ID of the desired family.

Return Value:

    Returns a pointer to the generic netlink family on success or NULL on
    failure.

--*/

PNETLINK_GENERIC_FAMILY
NetlinkpGenericLookupFamilyByName (
    PSTR FamilyName
    );

/*++

Routine Description:

    This routine searches the database of registered generic netlink families
    for one with the given name. If successful, the family is returned with an
    added reference which the caller must release.

Arguments:

    FamilyName - Supplies the name of the desired family.

Return Value:

    Returns a pointer to the generic netlink family on success or NULL on
    failure.

--*/

VOID
NetlinkpGenericFamilyAddReference (
    PNETLINK_GENERIC_FAMILY Family
    );

/*++

Routine Description:

    This routine increments the reference count of a generic netlink family.

Arguments:

    Family - Supplies a pointer to a generic netlink family.

Return Value:

    None.

--*/

VOID
NetlinkpGenericFamilyReleaseReference (
    PNETLINK_GENERIC_FAMILY Family
    );

/*++

Routine Description:

    This routine decrements the reference count of a generic netlink family,
    releasing all of its resources if the reference count drops to zero.

Arguments:

    Family - Supplies a pointer to a generic netlink family.

Return Value:

    None.

--*/

