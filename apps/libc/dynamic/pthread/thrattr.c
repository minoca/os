/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    thrattr.c

Abstract:

    This module implements thread attribute support functions.

Author:

    Evan Green 28-Apr-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"
#include <limits.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
pthread_attr_init (
    pthread_attr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a thread attribute structure.

Arguments:

    Attribute - Supplies a pointer to the attribute to initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = 0;
    AttributeInternal->StackBase = NULL;
    AttributeInternal->StackSize = PTHREAD_DEFAULT_STACK_SIZE;
    AttributeInternal->GuardSize = getpagesize();
    AttributeInternal->SchedulingPriority = 0;
    AttributeInternal->SchedulingPolicy = 0;
    return 0;
}

PTHREAD_API
int
pthread_attr_destroy (
    pthread_attr_t *Attribute
    )

/*++

Routine Description:

    This routine destroys a thread attribute structure.

Arguments:

    Attribute - Supplies a pointer to the attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    memset(Attribute, 0xFE, sizeof(pthread_attr_t));
    return 0;
}

PTHREAD_API
int
pthread_attr_getdetachstate (
    const pthread_attr_t *Attribute,
    int *State
    )

/*++

Routine Description:

    This routine returns the thread detach state for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    State - Supplies a pointer where the state will be returned on success. See
        PTHREAD_CREATE_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *State = PTHREAD_CREATE_JOINABLE;
    if ((AttributeInternal->Flags & PTHREAD_FLAG_DETACHED) != 0) {
        *State = PTHREAD_CREATE_DETACHED;
    }

    return 0;
}

PTHREAD_API
int
pthread_attr_setdetachstate (
    pthread_attr_t *Attribute,
    int State
    )

/*++

Routine Description:

    This routine sets the thread detach state for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    State - Supplies the new detach state to set. See PTHREAD_CREATE_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    if (State == PTHREAD_CREATE_JOINABLE) {
        AttributeInternal->Flags &= ~PTHREAD_FLAG_DETACHED;

    } else if (State == PTHREAD_CREATE_DETACHED) {
        AttributeInternal->Flags |= PTHREAD_FLAG_DETACHED;

    } else {
        return EINVAL;
    }

    return 0;
}

PTHREAD_API
int
pthread_attr_getschedpolicy (
    const pthread_attr_t *Attribute,
    int *Policy
    )

/*++

Routine Description:

    This routine returns the thread scheduling policy for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Policy - Supplies a pointer where the policy will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *Policy = AttributeInternal->SchedulingPolicy;
    return 0;
}

PTHREAD_API
int
pthread_attr_setschedpolicy (
    const pthread_attr_t *Attribute,
    int Policy
    )

/*++

Routine Description:

    This routine sets the thread scheduling policy for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Policy - Supplies the new policy to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    AttributeInternal->SchedulingPolicy = Policy;
    return 0;
}

PTHREAD_API
int
pthread_attr_getschedparam (
    const pthread_attr_t *Attribute,
    int *Parameter
    )

/*++

Routine Description:

    This routine returns the thread scheduling parameter for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Parameter - Supplies a pointer where the scheduling parameter will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *Parameter = AttributeInternal->SchedulingPriority;
    return 0;
}

PTHREAD_API
int
pthread_attr_setschedparam (
    pthread_attr_t *Attribute,
    int Parameter
    )

/*++

Routine Description:

    This routine sets the thread scheduling parameter for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Parameter - Supplies the new parameter to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    AttributeInternal->SchedulingPriority = Parameter;
    return 0;
}

PTHREAD_API
int
pthread_attr_getscope (
    const pthread_attr_t *Attribute,
    int *Scope
    )

/*++

Routine Description:

    This routine returns the thread scheduling scope for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Scope - Supplies a pointer where the thread scope will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    *Scope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}

PTHREAD_API
int
pthread_attr_setscope (
    pthread_attr_t *Attribute,
    int Scope
    )

/*++

Routine Description:

    This routine sets the thread scheduling scope for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Scope - Supplies the new scope to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Status;

    if (Scope == PTHREAD_SCOPE_SYSTEM) {
        Status = 0;

    } else if (Scope == PTHREAD_SCOPE_PROCESS) {
        Status = ENOTSUP;

    } else {
        Status = EINVAL;
    }

    return Status;
}

PTHREAD_API
int
pthread_attr_getstacksize (
    const pthread_attr_t *Attribute,
    size_t *StackSize
    )

/*++

Routine Description:

    This routine returns the thread stack size for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackSize - Supplies a pointer where the stack size will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *StackSize = AttributeInternal->StackSize;
    return 0;
}

PTHREAD_API
int
pthread_attr_setstacksize (
    pthread_attr_t *Attribute,
    size_t StackSize
    )

/*++

Routine Description:

    This routine sets the thread stack size for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackSize - Supplies the desired stack size. This should not be less than
        PTHREAD_STACK_MIN and should be a multiple of the page size.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;
    UINTN PageSize;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    PageSize = sysconf(_SC_PAGE_SIZE);
    if ((StackSize < PTHREAD_STACK_MIN) || (!IS_ALIGNED(StackSize, PageSize))) {
        return EINVAL;
    }

    AttributeInternal->StackSize = StackSize;
    return 0;
}

PTHREAD_API
int
pthread_attr_getstack (
    const pthread_attr_t *Attribute,
    void **StackBase,
    size_t *StackSize
    )

/*++

Routine Description:

    This routine returns the thread stack information for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackBase - Supplies a pointer where the stack base will be returned on
        success.

    StackSize - Supplies a pointer where the stack size will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *StackBase = AttributeInternal->StackBase;
    *StackSize = AttributeInternal->StackSize;
    return 0;
}

PTHREAD_API
int
pthread_attr_setstack (
    pthread_attr_t *Attribute,
    void *StackBase,
    size_t StackSize
    )

/*++

Routine Description:

    This routine sets the thread stack information for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackBase - Supplies the stack base pointer.

    StackSize - Supplies the desired stack size. This should not be less than
        PTHREAD_STACK_MIN.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;
    UINTN PageSize;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    PageSize = sysconf(_SC_PAGE_SIZE);
    if ((StackSize < PTHREAD_STACK_MIN) || (!IS_ALIGNED(StackSize, PageSize))) {
        return EINVAL;
    }

    if (!IS_ALIGNED((UINTN)StackBase, PageSize)) {
        return EINVAL;
    }

    AttributeInternal->StackBase = StackBase;
    AttributeInternal->StackSize = StackSize;
    return 0;
}

PTHREAD_API
int
pthread_attr_getguardsize (
    const pthread_attr_t *Attribute,
    size_t *GuardSize
    )

/*++

Routine Description:

    This routine returns the thread stack guard region size for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    GuardSize - Supplies a pointer where the stack guard region size will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    *GuardSize = AttributeInternal->GuardSize;
    return 0;
}

PTHREAD_API
int
pthread_attr_setguardsize (
    pthread_attr_t *Attribute,
    size_t GuardSize
    )

/*++

Routine Description:

    This routine sets the thread stack guard region size for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    GuardSize - Supplies the desired stack guard region size.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    AttributeInternal->GuardSize = GuardSize;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

