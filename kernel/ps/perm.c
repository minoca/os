/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perm.c

Abstract:

    This module implements support routines for thread permission and identity
    management.

Author:

    Evan Green 4-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

//
// This macro evaluates to non-zero if the given user ID matches any of
// the real user ID, effective user ID, or saved user ID of the given identity.
//

#define MATCHES_IDENTITY_USER(_UserId, _Identity)           \
    (((_UserId) == ((_Identity)->RealUserId)) ||            \
     ((_UserId) == ((_Identity)->EffectiveUserId)) ||       \
     ((_UserId) == ((_Identity)->SavedUserId)))

//
// This macro evaluates to non-zero if the given group ID matches any of the
// real group ID, effective group ID, or saved group ID of the given identity.
//

#define MATCHES_IDENTITY_GROUP(_GroupId, _Identity)         \
    (((_GroupId) == ((_Identity)->RealGroupId)) ||          \
     ((_GroupId) == ((_Identity)->EffectiveGroupId)) ||     \
     ((_GroupId) == ((_Identity)->SavedGroupId)))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspSetThreadIdentity (
    ULONG FieldsToSet,
    PTHREAD_IDENTITY Identity
    );

KSTATUS
PspSetThreadPermissions (
    ULONG FieldsToSet,
    PTHREAD_PERMISSIONS Permissions
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
PsCheckPermission (
    ULONG Permission
    )

/*++

Routine Description:

    This routine checks to see if the calling thread currently has the given
    permission.

Arguments:

    Permission - Supplies the permission number to check. See PERMISSION_*
        definitions.

Return Value:

    STATUS_SUCCESS if the current thread has the given permission.

    STATUS_PERMISSION_DENIED if the thread does not have the given permission.

--*/

{

    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    if (PERMISSION_CHECK(Thread->Permissions.Effective, Permission)) {
        return STATUS_SUCCESS;
    }

    return STATUS_PERMISSION_DENIED;
}

BOOL
PsIsUserInGroup (
    GROUP_ID Group
    )

/*++

Routine Description:

    This routine determines if the given group ID matches the effective
    group ID or any of the supplementary group IDs of the calling thread. The
    current thread must not be a kernel thread.

Arguments:

    Group - Supplies the group ID to check against.

Return Value:

    TRUE if the calling thread is a member of the given group.

    FALSE if the calling thread is not a member of the given group.

--*/

{

    UINTN GroupIndex;
    PSUPPLEMENTARY_GROUPS SupplementaryGroups;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    if ((Thread->Flags & THREAD_FLAG_USER_MODE) == 0) {

        ASSERT(FALSE);

        return FALSE;
    }

    if (Thread->Identity.EffectiveGroupId == Group) {
        return TRUE;
    }

    SupplementaryGroups = Thread->SupplementaryGroups;
    while (SupplementaryGroups != NULL) {
        for (GroupIndex = 0;
             GroupIndex < SupplementaryGroups->Count;
             GroupIndex += 1) {

            if (Group == SupplementaryGroups->Groups[GroupIndex]) {
                return TRUE;
            }
        }

        SupplementaryGroups = SupplementaryGroups->Next;
    }

    return FALSE;
}

INTN
PsSysSetThreadIdentity (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the get/set thread identity system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_SET_THREAD_IDENTITY Parameters;
    KSTATUS Status;

    Parameters = SystemCallParameter;
    Status = PspSetThreadIdentity(Parameters->Request.FieldsToSet,
                                  &(Parameters->Request.Identity));

    return Status;
}

INTN
PsSysSetThreadPermissions (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the get/set thread permissions system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_SET_THREAD_PERMISSIONS Parameters;
    KSTATUS Status;

    Parameters = SystemCallParameter;
    Status = PspSetThreadPermissions(Parameters->Request.FieldsToSet,
                                     &(Parameters->Request.Permissions));

    return Status;
}

INTN
PsSysSetSupplementaryGroups (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the get/set supplementary groups system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    UINTN AllocationSize;
    PSUPPLEMENTARY_GROUPS Block;
    UINTN BlockCapacity;
    UINTN BlockIndex;
    UINTN Count;
    PSUPPLEMENTARY_GROUPS NewBlock;
    PSYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS Parameters;
    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;

    NewBlock = NULL;
    Thread = KeGetCurrentThread();
    Process = PsGetCurrentProcess();
    Parameters = SystemCallParameter;
    if (Parameters->Set != FALSE) {

        //
        // Set an (arbitrary) cap.
        //

        if (Parameters->Count > SUPPLEMENTARY_GROUP_MAX) {
            Status = STATUS_INVALID_PARAMETER;
            goto SysSetSupplementaryGroupsEnd;
        }

        //
        // Ensure the caller has the privileges to do this.
        //

        Status = PsCheckPermission(PERMISSION_SET_GROUP_ID);
        if (!KSUCCESS(Status)) {
            goto SysSetSupplementaryGroupsEnd;
        }

        //
        // Count the current capacity.
        //

        Count = 0;
        Block = Thread->SupplementaryGroups;
        while (Block != NULL) {
            Count += Block->Capacity;
            Block = Block->Next;
        }

        //
        // Allocate a new block if needed.
        //

        if (Count < Parameters->Count) {
            BlockCapacity = Parameters->Count - Count;
            BlockCapacity = ALIGN_RANGE_UP(BlockCapacity,
                                           SUPPLEMENTARY_GROUP_MIN);

            AllocationSize = sizeof(SUPPLEMENTARY_GROUPS) +
                             (BlockCapacity * sizeof(GROUP_ID));

            NewBlock = MmAllocatePagedPool(AllocationSize,
                                           PS_GROUP_ALLOCATION_TAG);

            if (NewBlock == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SysSetSupplementaryGroupsEnd;
            }

            //
            // Initialize to all ones instead of zeros to avoid
            // accidents that assign groups with IDs of root.
            //

            RtlSetMemory(NewBlock, -1, AllocationSize);
            NewBlock->Capacity = BlockCapacity;
            NewBlock->Groups = (PGROUP_ID)(NewBlock + 1);
            NewBlock->Count = 0;
        }

        KeAcquireQueuedLock(Process->QueuedLock);
        Status = STATUS_SUCCESS;
        Count = 0;
        Block = Thread->SupplementaryGroups;
        while (Block != NULL) {

            //
            // Set each entry in the block, up to the total count.
            //

            for (BlockIndex = 0;
                 BlockIndex < Block->Capacity;
                 BlockIndex += 1) {

                if (Count == Parameters->Count) {
                    break;
                }

                Status = MmCopyFromUserMode(&(Block->Groups[BlockIndex]),
                                            &(Parameters->Groups[Count]),
                                            sizeof(GROUP_ID));

                if (!KSUCCESS(Status)) {
                    break;
                }

                Count += 1;
            }

            //
            // Set the number of valid entries to be however many were filled
            // in.
            //

            Block->Count = BlockIndex;
            if (!KSUCCESS(Status)) {
                break;
            }

            Block = Block->Next;
        }

        //
        // Add the remainder of the groups to the brand new block.
        //

        if (Count < Parameters->Count) {

            ASSERT((NewBlock != NULL) &&
                   ((Parameters->Count - Count) <= NewBlock->Capacity));

            BlockIndex = 0;
            while (Count < Parameters->Count) {
                Status = MmCopyFromUserMode(&(NewBlock->Groups[BlockIndex]),
                                            &(Parameters->Groups[Count]),
                                            sizeof(GROUP_ID));

                if (!KSUCCESS(Status)) {
                    break;
                }

                Count += 1;
                BlockIndex += 1;
            }

            //
            // Only add the new block if it worked, otherwise memory could
            // accumulate via user mode calls with bad pointers.
            //

            NewBlock->Count = BlockIndex;
            if (KSUCCESS(Status)) {
                NewBlock->Next = Thread->SupplementaryGroups;
                Thread->SupplementaryGroups = NewBlock;
                NewBlock = NULL;
            }
        }

        KeReleaseQueuedLock(Process->QueuedLock);

    //
    // Just get the groups.
    //

    } else {
        Count = 0;
        Block = Thread->SupplementaryGroups;
        Status = STATUS_SUCCESS;
        while (Block != NULL) {
            for (BlockIndex = 0; BlockIndex < Block->Count; BlockIndex += 1) {
                if ((Count < Parameters->Count) && (KSUCCESS(Status))) {
                    Status = MmCopyToUserMode(&(Parameters->Groups[Count]),
                                              &(Block->Groups[BlockIndex]),
                                              sizeof(GROUP_ID));
                }

                Count += 1;
            }

            Block = Block->Next;
        }

        Parameters->Count = Count;
    }

SysSetSupplementaryGroupsEnd:
    if (NewBlock != NULL) {
        MmFreePagedPool(NewBlock);
    }

    return Status;
}

INTN
PsSysSetResourceLimit (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call that gets or sets a resource limit
    for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    RESOURCE_LIMIT NewValue;
    PSYSTEM_CALL_SET_RESOURCE_LIMIT Parameters;
    KSTATUS Status;
    PKTHREAD Thread;
    RESOURCE_LIMIT_TYPE Type;

    Thread = KeGetCurrentThread();
    Parameters = SystemCallParameter;
    Type = Parameters->Type;
    if ((Type >= ResourceLimitCount) || (Type < 0)) {
        Status = STATUS_INVALID_PARAMETER;
        goto SysSetResourceLimitEnd;
    }

    //
    // Copy the values to potentially set into a local, and copy the current
    // values to be returned.
    //

    NewValue.Current = Parameters->Value.Current;
    NewValue.Max = Parameters->Value.Max;
    Parameters->Value.Current = Thread->Limits[Type].Current;
    Parameters->Value.Max = Thread->Limits[Type].Max;

    //
    // If not setting, that's all there is to do.
    //

    if (Parameters->Set == FALSE) {
        Status = STATUS_SUCCESS;
        goto SysSetResourceLimitEnd;
    }

    //
    // The caller wants to set new limits. Make sure current isn't greater than
    // max.
    //

    if (NewValue.Current > NewValue.Max) {
        Status = STATUS_INVALID_PARAMETER;
        goto SysSetResourceLimitEnd;
    }

    //
    // If trying to raise the max, the caller had better have the appropriate
    // permissions.
    //

    if (NewValue.Max > Parameters->Value.Max) {
        Status = PsCheckPermission(PERMISSION_RESOURCES);
        if (!KSUCCESS(Status)) {
            goto SysSetResourceLimitEnd;
        }

        //
        // Don't allow the file count go beyond what the kernel can handle.
        //

        if ((Type == ResourceLimitFileCount) &&
            (NewValue.Max > OB_MAX_HANDLES)) {

            Status = STATUS_PERMISSION_DENIED;
            goto SysSetResourceLimitEnd;
        }
    }

    Thread->Limits[Type].Max = NewValue.Max;
    Thread->Limits[Type].Current = NewValue.Current;

    //
    // Attempt to set the new stack size now, and silently ignore failures.
    //

    if (Type == ResourceLimitStack) {
        if ((Thread->Flags & THREAD_FLAG_FREE_USER_STACK) != 0) {
            PspSetThreadUserStackSize(Thread, NewValue.Current);
        }
    }

    Status = STATUS_SUCCESS;

SysSetResourceLimitEnd:
    return Status;
}

VOID
PspPerformExecutePermissionChanges (
    PIO_HANDLE ExecutableHandle
    )

/*++

Routine Description:

    This routine fixes up the user identity and potentially permissions in
    preparation for executing an image.

Arguments:

    ExecutableHandle - Supplies an open file handle to the executable image.

Return Value:

    None.

--*/

{

    BOOL FileEffective;
    PERMISSION_SET FileInheritable;
    PERMISSION_SET FilePermitted;
    FILE_PROPERTIES FileProperties;
    PERMISSION_SET NewPermitted;
    BOOL SetRoot;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    FileEffective = FALSE;
    FileInheritable = PERMISSION_SET_EMPTY;
    FilePermitted = PERMISSION_SET_EMPTY;

    //
    // Always clear the keep capabilities bit.
    //

    Thread->Permissions.Behavior &= ~PERMISSION_BEHAVIOR_KEEP_PERMISSIONS;
    Status = IoGetFileInformation(ExecutableHandle, &FileProperties);
    if (!KSUCCESS(Status)) {

        //
        // Save the effective user and group IDs into the saved user and group
        // IDs.
        //

        Thread->Identity.SavedUserId = Thread->Identity.EffectiveUserId;
        Thread->Identity.SavedGroupId = Thread->Identity.EffectiveGroupId;
        return;
    }

    //
    // TODO: Return immediately if the mount flags specify no-setuid.
    //

    //
    // If the set-group-id bit is set in the file permissions, then change the
    // effective group ID to that of the file.
    //

    if ((FileProperties.Permissions & FILE_PERMISSION_SET_GROUP_ID) != 0) {
        Thread->Identity.EffectiveGroupId = FileProperties.GroupId;
    }

    //
    // If the set-user-id bit is set in the file permissions, then change the
    // effective user ID to that of the file.
    //

    SetRoot = FALSE;
    if ((FileProperties.Permissions & FILE_PERMISSION_SET_USER_ID) != 0) {
        Thread->Identity.EffectiveUserId = FileProperties.UserId;
        if (FileProperties.UserId == USER_ID_ROOT) {
            SetRoot = TRUE;
        }
    }

    //
    // Initialize the saved user and group IDs to be equal to the effective
    // ones.
    //

    Thread->Identity.SavedUserId = Thread->Identity.EffectiveUserId;
    Thread->Identity.SavedGroupId = Thread->Identity.EffectiveGroupId;
    if ((Thread->Permissions.Behavior & PERMISSION_BEHAVIOR_NO_ROOT) == 0) {

        //
        // If it's a set-user-id-root program, or the real user ID is root, and
        // the user hasn't set the no-root flag, then adjust the permissions
        // mask.
        //

        if ((SetRoot != FALSE) ||
            (Thread->Identity.RealUserId == USER_ID_ROOT)) {

            FilePermitted = PERMISSION_SET_FULL;
            FileInheritable = PERMISSION_SET_FULL;
        }

        //
        // If the new effective user is root, either by setuid methods or just
        // because they were before, then the file effective bit is set so that
        // they have these permissions on startup.
        //

        if (Thread->Identity.EffectiveUserId == USER_ID_ROOT) {
            FileEffective = TRUE;
        }
    }

    //
    // Modify the permission sets for the execution. The new permitted mask is
    // (OldPermitted & FileInheritable) | (FilePermited & Limit). The effective
    // permissions are set to the permitted permissions if the file "effective"
    // bit is set, or just wiped otherwise.
    //

    NewPermitted = Thread->Permissions.Inheritable;
    PERMISSION_AND(NewPermitted, FileInheritable);
    PERMISSION_AND(FilePermitted, Thread->Permissions.Limit);
    PERMISSION_OR(NewPermitted, FilePermitted);
    Thread->Permissions.Permitted = NewPermitted;
    if (FileEffective != FALSE) {
        Thread->Permissions.Effective = NewPermitted;

    } else {
        Thread->Permissions.Effective = PERMISSION_SET_EMPTY;
    }

    return;
}

KSTATUS
PspCopyThreadCredentials (
    PKTHREAD NewThread,
    PKTHREAD ThreadToCopy
    )

/*++

Routine Description:

    This routine copies the credentials of a thread onto a new yet-to-be-run
    thread.

Arguments:

    NewThread - Supplies a pointer to the new thread to initialize.

    ThreadToCopy - Supplies a pointer to the thread to copy identity and
        permissions from.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

{

    UINTN AllocationSize;
    PSUPPLEMENTARY_GROUPS Block;
    UINTN Count;
    PSUPPLEMENTARY_GROUPS NewBlock;
    KSTATUS Status;

    //
    // Just copy the identity, permissions, and limits straight over.
    //

    RtlCopyMemory(&(NewThread->Identity),
                  &(ThreadToCopy->Identity),
                  sizeof(THREAD_IDENTITY));

    RtlCopyMemory(&(NewThread->Permissions),
                  &(ThreadToCopy->Permissions),
                  sizeof(THREAD_PERMISSIONS));

    RtlCopyMemory(&(NewThread->Limits),
                  &(ThreadToCopy->Limits),
                  sizeof(NewThread->Limits));

    //
    // Count up the old thread supplementary group count so it can be allocated
    // in a single block.
    //

    Count = 0;
    Block = ThreadToCopy->SupplementaryGroups;
    while (Block != NULL) {
        Count += Block->Count;
        Block = Block->Next;
    }

    if (Count != 0) {
        Count = ALIGN_RANGE_UP(Count, SUPPLEMENTARY_GROUP_MIN);
        AllocationSize = sizeof(SUPPLEMENTARY_GROUPS) +
                         (Count * sizeof(GROUP_ID));

        NewBlock = MmAllocatePagedPool(AllocationSize, PS_GROUP_ALLOCATION_TAG);
        if (NewBlock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyThreadCredentialsEnd;
        }

        NewBlock->Capacity = Count;
        NewBlock->Count = 0;
        NewBlock->Groups = (PGROUP_ID)(NewBlock + 1);
        NewBlock->Next = NULL;

        //
        // Now copy all the blocks over into the new biggy block.
        //

        Block = ThreadToCopy->SupplementaryGroups;
        Count = 0;
        while (Block != NULL) {
            if (Block->Count != 0) {
                RtlCopyMemory(&(NewBlock->Groups[Count]),
                              Block->Groups,
                              Block->Count * sizeof(GROUP_ID));

                Count += Block->Count;
            }

            Block = Block->Next;
        }

        ASSERT(Count <= NewBlock->Capacity);

        NewBlock->Count = Count;
        NewThread->SupplementaryGroups = NewBlock;
    }

    Status = STATUS_SUCCESS;

CopyThreadCredentialsEnd:
    return Status;
}

VOID
PspDestroyCredentials (
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine destroys credentials associated with a dying thread.

Arguments:

    Thread - Supplies a pointer to the thread being terminated.

Return Value:

    None.

--*/

{

    PSUPPLEMENTARY_GROUPS Groups;
    PSUPPLEMENTARY_GROUPS Next;

    Groups = Thread->SupplementaryGroups;
    Thread->SupplementaryGroups = NULL;
    while (Groups != NULL) {
        Next = Groups->Next;
        MmFreePagedPool(Groups);
        Groups = Next;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspSetThreadIdentity (
    ULONG FieldsToSet,
    PTHREAD_IDENTITY Identity
    )

/*++

Routine Description:

    This routine gets or sets the current thread's identity.

Arguments:

    FieldsToSet - Supplies the bitmask of fields to set. Supply 0 to simply get
        the current thread identity. See the THREAD_IDENTITY_FIELD_*
        definitions.

    Identity - Supplies a pointer that on input contains the new identity to
        set. Only the fields specified in the fields to set mask will be
        examined. On output, contains the complete new thread identity
        information.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PERMISSION_DENIED if the caller does not have appropriate
        permissions (set user ID and/or set group ID permissions).

--*/

{

    PTHREAD_IDENTITY CurrentIdentity;
    BOOL Match;
    KSTATUS Status;
    PKTHREAD Thread;
    BOOL WasRoot;

    Thread = KeGetCurrentThread();
    CurrentIdentity = &(Thread->Identity);
    if (FieldsToSet == 0) {
        Status = STATUS_SUCCESS;
        goto SetThreadIdentityEnd;
    }

    //
    // Before making any changes, ensure the caller isn't overstepping
    // permissions. If changing a user ID, a caller without the set user ID
    // permission can set any user ID to any of its existing user IDs.
    //

    Status = STATUS_SUCCESS;
    if ((FieldsToSet & THREAD_IDENTITY_FIELDS_USER) != 0) {
        Status = PsCheckPermission(PERMISSION_SET_USER_ID);
        if (!KSUCCESS(Status)) {
            Status = STATUS_SUCCESS;
            if ((FieldsToSet & THREAD_IDENTITY_FIELD_REAL_USER_ID) != 0) {
                Match = MATCHES_IDENTITY_USER(Identity->RealUserId,
                                              CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }

            if ((FieldsToSet & THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID) != 0) {
                Match = MATCHES_IDENTITY_USER(Identity->EffectiveUserId,
                                              CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }

            if ((FieldsToSet & THREAD_IDENTITY_FIELD_SAVED_USER_ID) != 0) {
                Match = MATCHES_IDENTITY_USER(Identity->SavedUserId,
                                              CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }
        }

        if (!KSUCCESS(Status)) {
            goto SetThreadIdentityEnd;
        }
    }

    //
    // Check permissions on the group IDs being set. If the special permission
    // isn't there, then the caller can set a group ID to one of its existing
    // group IDs.
    //

    if ((FieldsToSet & THREAD_IDENTITY_FIELDS_GROUP) != 0) {
        Status = PsCheckPermission(PERMISSION_SET_GROUP_ID);
        if (!KSUCCESS(Status)) {
            Status = STATUS_SUCCESS;
            if ((FieldsToSet & THREAD_IDENTITY_FIELD_REAL_GROUP_ID) != 0) {
                Match = MATCHES_IDENTITY_GROUP(Identity->RealGroupId,
                                               CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }

            if ((FieldsToSet & THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID) != 0) {
                Match = MATCHES_IDENTITY_GROUP(Identity->EffectiveGroupId,
                                               CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }

            if ((FieldsToSet & THREAD_IDENTITY_FIELD_SAVED_GROUP_ID) != 0) {
                Match = MATCHES_IDENTITY_GROUP(Identity->SavedGroupId,
                                               CurrentIdentity);

                if (Match == FALSE) {
                    Status = STATUS_PERMISSION_DENIED;
                }
            }
        }

        if (!KSUCCESS(Status)) {
            goto SetThreadIdentityEnd;
        }
    }

    //
    // Determine if any of the original user IDs were root.
    //

    WasRoot = FALSE;
    if ((CurrentIdentity->RealUserId == USER_ID_ROOT) ||
        (CurrentIdentity->EffectiveUserId == USER_ID_ROOT) ||
        (CurrentIdentity->SavedUserId == USER_ID_ROOT)) {

        WasRoot = TRUE;
    }

    //
    // The permissions all check out, write the new IDs.
    //

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_REAL_USER_ID) != 0) {
        CurrentIdentity->RealUserId = Identity->RealUserId;
    }

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID) != 0) {

        //
        // Unless the "no fixup" flag is set, modify the permissions if the
        // effective user ID is moving to or from the traditional root user.
        //

        if ((Thread->Permissions.Behavior &
             PERMISSION_BEHAVIOR_NO_SETUID_FIXUP) == 0) {

            //
            // If the effective user ID goes from zero to non-zero, clear all
            // effective permissions.
            //

            if ((CurrentIdentity->EffectiveUserId == USER_ID_ROOT) &&
                (Identity->EffectiveUserId != USER_ID_ROOT)) {

                Thread->Permissions.Effective = PERMISSION_SET_EMPTY;
            }

            //
            // If the effective user ID goes from non-zero to zero, then copy
            // the permitted permissions to the effective permissions.
            //

            if ((CurrentIdentity->EffectiveUserId != USER_ID_ROOT) &&
                (Identity->EffectiveUserId == USER_ID_ROOT)) {

                Thread->Permissions.Effective = Thread->Permissions.Permitted;
            }
        }

        CurrentIdentity->EffectiveUserId = Identity->EffectiveUserId;
    }

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_SAVED_USER_ID) != 0) {
        CurrentIdentity->SavedUserId = Identity->SavedUserId;
    }

    //
    // If at least one of the real, effective, or saved user IDs was zero and
    // all three are now non-zero, then all permissions are cleared from the
    // permitted and effective sets.
    //

    if ((WasRoot != FALSE) &&
        (CurrentIdentity->RealUserId != USER_ID_ROOT) &&
        (CurrentIdentity->EffectiveUserId != USER_ID_ROOT) &&
        (CurrentIdentity->SavedUserId != USER_ID_ROOT)) {

        if ((Thread->Permissions.Behavior &
             PERMISSION_BEHAVIOR_KEEP_PERMISSIONS) == 0) {

            Thread->Permissions.Permitted = PERMISSION_SET_EMPTY;
            Thread->Permissions.Effective = PERMISSION_SET_EMPTY;
        }
    }

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_REAL_GROUP_ID) != 0) {
        CurrentIdentity->RealGroupId = Identity->RealGroupId;
    }

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID) != 0) {
        CurrentIdentity->EffectiveGroupId = Identity->EffectiveGroupId;
    }

    if ((FieldsToSet & THREAD_IDENTITY_FIELD_SAVED_GROUP_ID) != 0) {
        CurrentIdentity->SavedGroupId = Identity->SavedGroupId;
    }

SetThreadIdentityEnd:
    RtlCopyMemory(Identity, CurrentIdentity, sizeof(THREAD_IDENTITY));
    return Status;
}

KSTATUS
PspSetThreadPermissions (
    ULONG FieldsToSet,
    PTHREAD_PERMISSIONS Permissions
    )

/*++

Routine Description:

    This routine gets or sets the current thread's permission masks.

Arguments:

    FieldsToSet - Supplies the bitmask of fields to set. Supply 0 to simply get
        the current thread identity. See the THREAD_PERMISSION_FIELD_*
        definitions.

    Permissions - Supplies a pointer that on input contains the new
        permissions to set. Only the fields specified in the fields to set
        mask will be examined. On output, contains the complete new thread
        permission masks.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PERMISSION_DENIED if the caller does not have appropriate
        permissions (potentially the permission to add permissions).

--*/

{

    PTHREAD_PERMISSIONS CurrentPermissions;
    ULONG DifferentBits;
    PERMISSION_SET IllegalBits;
    PERMISSION_SET InheritablePlusLimit;
    PERMISSION_SET InheritablePlusPermitted;
    ULONG SameMask;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    CurrentPermissions = &(Thread->Permissions);
    if (FieldsToSet == 0) {
        Status = STATUS_SUCCESS;
        goto SetThreadPermissionsEnd;
    }

    //
    // If the thread does not have permission to set more permissions, then
    // additional rules apply.
    //

    Status = PsCheckPermission(PERMISSION_SET_PERMISSIONS);
    if (!KSUCCESS(Status)) {
        Status = STATUS_SUCCESS;

        //
        // The "set permissions" permission is required to change the behavior
        // mask and the limit set.
        //

        if ((FieldsToSet &
            (THREAD_PERMISSION_FIELD_BEHAVIOR |
             THREAD_PERMISSION_FIELD_LIMIT)) != 0) {

            Status = STATUS_PERMISSION_DENIED;
        }

        //
        // The new inheritable mask must only have permissions from the
        // inheritable and permitted sets.
        //

        if ((FieldsToSet & THREAD_PERMISSION_FIELD_INHERITABLE) != 0) {
            InheritablePlusPermitted = CurrentPermissions->Inheritable;
            PERMISSION_OR(InheritablePlusPermitted,
                          CurrentPermissions->Permitted);

            IllegalBits = Permissions->Inheritable;
            PERMISSION_REMOVE_SET(IllegalBits, InheritablePlusPermitted);
            if (!PERMISSION_IS_EMPTY(IllegalBits)) {
                Status = STATUS_PERMISSION_DENIED;
            }
        }
    }

    //
    // Bits can never be added to the limit set.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_LIMIT) != 0) {
        IllegalBits = Permissions->Limit;
        PERMISSION_REMOVE_SET(IllegalBits, CurrentPermissions->Limit);
        if (!PERMISSION_IS_EMPTY(IllegalBits)) {
            Status = STATUS_PERMISSION_DENIED;
        }
    }

    //
    // The lock bits are like fuses, once they're blown they can no longer
    // be changed. For each lock bit that is set, if either the lock bit
    // or the bit itself is different, then fail.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_BEHAVIOR) != 0) {
        if ((Permissions->Behavior & ~PERMISSION_BEHAVIOR_VALID_MASK) != 0) {
            Status = STATUS_INVALID_PARAMETER;
        }

        SameMask = 0;
        if ((CurrentPermissions->Behavior &
             PERMISSION_BEHAVIOR_KEEP_PERMISSIONS_LOCKED) != 0) {

            SameMask |= PERMISSION_BEHAVIOR_KEEP_PERMISSIONS_LOCKED |
                        PERMISSION_BEHAVIOR_KEEP_PERMISSIONS;
        }

        if ((CurrentPermissions->Behavior &
             PERMISSION_BEHAVIOR_NO_SETUID_FIXUP_LOCKED) != 0) {

            SameMask |= PERMISSION_BEHAVIOR_NO_SETUID_FIXUP_LOCKED |
                        PERMISSION_BEHAVIOR_NO_SETUID_FIXUP;
        }

        if ((CurrentPermissions->Behavior &
             PERMISSION_BEHAVIOR_NO_ROOT_LOCKED) != 0) {

            SameMask |= PERMISSION_BEHAVIOR_NO_ROOT_LOCKED |
                        PERMISSION_BEHAVIOR_NO_ROOT;
        }

        //
        // If any of the bits that are required to be the same are actually
        // different, then the request is denied.
        //

        DifferentBits = Permissions->Behavior ^ CurrentPermissions->Behavior;
        if ((DifferentBits & SameMask) != 0) {
            Status = STATUS_PERMISSION_DENIED;
        }
    }

    //
    // The new inheritable set must be a subset of the existing inheritable
    // set plus the limit.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_INHERITABLE) != 0) {
        InheritablePlusLimit = CurrentPermissions->Inheritable;
        PERMISSION_OR(InheritablePlusLimit, CurrentPermissions->Limit);
        IllegalBits = Permissions->Inheritable;
        PERMISSION_REMOVE_SET(IllegalBits, InheritablePlusLimit);
        if (!PERMISSION_IS_EMPTY(IllegalBits)) {
            Status = STATUS_PERMISSION_DENIED;
        }
    }

    //
    // Bits cannot be added to the permitted set.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_PERMITTED) != 0) {
        IllegalBits = Permissions->Permitted;
        PERMISSION_REMOVE_SET(IllegalBits, CurrentPermissions->Permitted);
        if (!PERMISSION_IS_EMPTY(IllegalBits)) {
            Status = STATUS_PERMISSION_DENIED;
        }
    }

    //
    // The effective set is limited to the permitted set.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_EFFECTIVE) != 0) {
        IllegalBits = Permissions->Effective;
        PERMISSION_REMOVE_SET(IllegalBits, CurrentPermissions->Permitted);
        if (!PERMISSION_IS_EMPTY(IllegalBits)) {
            Status = STATUS_PERMISSION_DENIED;
        }
    }

    //
    // If any of those conditions tripped a failure, don't change any settings.
    //

    if (!KSUCCESS(Status)) {
        goto SetThreadPermissionsEnd;
    }

    //
    // All the checks passed, set the desired fields.
    //

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_BEHAVIOR) != 0) {
        CurrentPermissions->Behavior = Permissions->Behavior;
    }

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_LIMIT) != 0) {
        CurrentPermissions->Limit = Permissions->Limit;
    }

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_INHERITABLE) != 0) {
        CurrentPermissions->Inheritable = Permissions->Inheritable;
    }

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_PERMITTED) != 0) {
        CurrentPermissions->Permitted = Permissions->Permitted;
    }

    if ((FieldsToSet & THREAD_PERMISSION_FIELD_EFFECTIVE) != 0) {
        CurrentPermissions->Effective = Permissions->Effective;
    }

    Status = STATUS_SUCCESS;

SetThreadPermissionsEnd:
    RtlCopyMemory(Permissions, CurrentPermissions, sizeof(THREAD_PERMISSIONS));
    return Status;
}

