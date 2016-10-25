/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    env.c

Abstract:

    This module implements support for environment variables.

Author:

    Evan Green 22-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial capacity of the environments array.
//

#define ENVIRONMENT_ARRAY_INITIAL_CAPACITY 16

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
ClpImGetEnvironmentVariable (
    PSTR Variable
    );

INT
ClpReallocateEnvironment (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the environment.
//

LIBC_API char **environ;

//
// Store a pointer to the last allocated environment and its size.
//

PSTR *ClAllocatedEnvironment = NULL;
ULONG ClAllocatedEnvironmentCapacity = 0;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
getenv (
    const char *Name
    )

/*++

Routine Description:

    This routine returns the value for the environment variable with the
    given name. This function is neither reentrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the null terminated string containing the name
        of the environment variable to get.

Return Value:

    Returns a pointer to the value of the environment variable on success. This
    memory may be destroyed by subsequent calls to getenv, setenv, or
    unsetenv. The caller does not own it and must not modify or free this
    memory.

--*/

{

    ULONG Index;
    ULONG NameLength;

    if ((Name == NULL) || (*Name == '\0') || (environ == NULL)) {
        return NULL;
    }

    Index = 0;
    NameLength = strlen(Name);
    while (environ[Index] != NULL) {
        if ((strncmp(environ[Index], Name, NameLength) == 0) &&
            (environ[Index][NameLength] == '=')) {

            return environ[Index] + NameLength + 1;
        }

        Index += 1;
    }

    return NULL;
}

LIBC_API
int
setenv (
    const char *Name,
    const char *Value,
    int Overwrite
    )

/*++

Routine Description:

    This routine sets the value for the given environment variable. This
    function is neither reentrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the null terminated string containing the name
        of the environment variable to set. The routine will fail if this
        string has an equal in it.

    Value - Supplies a pointer to the null terminated string containing the
        value to set for this variable.

    Overwrite - Supplies an integer that if non-zero will cause an existing
        environment variable with the same name to be overwritten. If this is
        zero and the given name exists, the function will return successfully
        but the value will not be changed.

Return Value:

    0 on success.

    -1 on failure, an errno will be set to contain the error code.

--*/

{

    ULONG AllocationSize;
    ULONG Index;
    ULONG NameLength;
    INT Result;
    PSTR Variable;

    if ((Name == NULL) || (*Name == '\0') || (strchr(Name, '=') != NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (Value == NULL) {
        return unsetenv(Name);
    }

    //
    // Figure out how big this buffer is going to need to be in the form
    // name=value.
    //

    NameLength = strlen(Name);
    AllocationSize = NameLength + strlen(Value) + 2;

    //
    // Look for the environment variable with the given name first. Compare the
    // length of the name and make sure there's an equal coming next in the
    // existing environment string (as in name=value).
    //

    Index = 0;
    if (environ != NULL) {
        while (environ[Index] != NULL) {
            if ((strncmp(environ[Index], Name, NameLength) == 0) &&
                (environ[Index][NameLength] == '=')) {

                break;
            }

            Index += 1;
        }
    }

    //
    // If the environment is not the same as the allocated environment or the
    // allocated one is full, allocate a new environment array.
    //

    if ((environ != ClAllocatedEnvironment) ||
        ((Index + 1) >= ClAllocatedEnvironmentCapacity)) {

        Result = ClpReallocateEnvironment();
        if (Result != 0) {
            errno = Result;
            return -1;
        }
    }

    //
    // If the variable exists, don't replace it if the user doesn't want to.
    //

    if ((environ[Index] != NULL) && (Overwrite == 0)) {
        return 0;
    }

    //
    // Allocate a new variable string.
    //

    Variable = malloc(AllocationSize);
    if (Variable == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snprintf(Variable, AllocationSize, "%s=%s", Name, Value);

    //
    // Replacing is easy (but leaky, unfortunately by necessity).
    //

    if (environ[Index] != NULL) {
        environ[Index] = Variable;
        return 0;
    }

    assert((Index + 1) < ClAllocatedEnvironmentCapacity);

    //
    // Expanding when there's room is also easy.
    //

    environ[Index] = Variable;
    environ[Index + 1] = NULL;
    return 0;
}

LIBC_API
int
putenv (
    char *String
    )

/*++

Routine Description:

    This routine adds the given string to the environment list.

Arguments:

    String - Supplies a pointer to the null terminated string in the form
        "name=value". This string will become part of the environment, if it
        is modified then that modification will be reflected in the
        environment. The memory supplied in this argument is used directly, so
        the argument must not be automatically allocated. If the given string
        contains no equal sign, then the functionality is equivalent to
        unsetenv with the given string.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PSTR Equals;
    ULONG Index;
    ULONG NameLength;
    BOOL Remove;
    INT Result;

    if (String == NULL) {
        errno = EINVAL;
        return -1;
    }

    Remove = FALSE;

    //
    // This function is pretty sketchy, but at least try to replace an
    // existing variable if the input is in the right format.
    //

    NameLength = 0;
    Equals = strchr(String, '=');
    if (Equals != NULL) {
        NameLength = (UINTN)Equals - (UINTN)String;
        if (*(Equals + 1) == '\0') {
            Remove = TRUE;
        }
    }

    if (NameLength == 0) {
        errno = EINVAL;
        return -1;
    }

    //
    // Look for the environment variable with the given name first. Compare the
    // length of the name and make sure there's an equal coming next in the
    // existing environment string (as in name=value).
    //

    Index = 0;
    if (environ != NULL) {
        while (environ[Index] != NULL) {
            if ((strncmp(environ[Index], String, NameLength) == 0) &&
                (environ[Index][NameLength] == '=')) {

                if (Remove != FALSE) {

                    //
                    // Move all the other entries down, removing (and leaking)
                    // this one.
                    //

                    while (TRUE) {
                        environ[Index] = environ[Index + 1];
                        if (environ[Index] == NULL) {
                            break;
                        }

                        Index += 1;
                    }

                } else {

                    //
                    // Replace this entry with the new value. Leak the old value
                    // intentionally, as it's unknown where it came from.
                    //

                    environ[Index] = String;
                }

                return 0;
            }

            Index += 1;
        }
    }

    if (Remove != FALSE) {
        return 0;
    }

    //
    // If the environment is not the same as the allocated environment or the
    // allocated one is full, allocate a new environment array.
    //

    if ((environ != ClAllocatedEnvironment) ||
        ((Index + 1) >= ClAllocatedEnvironmentCapacity)) {

        Result = ClpReallocateEnvironment();
        if (Result != 0) {
            errno = Result;
            return -1;
        }
    }

    //
    // Add the name of the end.
    //

    environ[Index] = String;
    Index += 1;
    environ[Index] = NULL;
    return 0;
}

LIBC_API
int
unsetenv (
    const char *Name
    )

/*++

Routine Description:

    This routine removes the environment variable with the given name from
    the current environment. This routine is neither re-entrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the name of the variable to unset. This
        string must not have an equals '=' in it.

Return Value:

    0 on success (whether or not the environment variable previously existed).

    -1 on failure, and errno will be set to contain more information. Errno is
    commonly set to EINVAL if the argument is a null pointer, an empty string,
    or contains an equals.

--*/

{

    ULONG Index;
    ULONG MoveIndex;
    ULONG NameLength;

    if ((Name == NULL) || (*Name == '\0') || (strchr(Name, '=') != NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (environ == NULL) {
        return 0;
    }

    NameLength = strlen(Name);

    //
    // Loop through looking for the variable.
    //

    Index = 0;
    while (environ[Index] != NULL) {
        if ((strncmp(environ[Index], Name, NameLength) == 0) &&
            (environ[Index][NameLength] == '=')) {

            //
            // Move all other environment variables up one.
            //

            MoveIndex = Index;
            while (environ[MoveIndex] != NULL) {
                environ[MoveIndex] = environ[MoveIndex + 1];
                MoveIndex += 1;
            }

            //
            // Keep looking for this environment variable name, as the user
            // may have edited multiple variables in themselves.
            //

            continue;
        }

        Index += 1;
    }

    return 0;
}

LIBC_API
const char *
getexecname (
    void
    )

/*++

Routine Description:

    This routine returns the path name of the executable.

Arguments:

    None.

Return Value:

    Returns a pointer to the pathname of the executable on success. The caller
    must not alter this memory.

--*/

{

    PPROCESS_ENVIRONMENT Environment;

    Environment = OsGetCurrentEnvironment();
    return Environment->ImageName;
}

VOID
ClpInitializeEnvironment (
    VOID
    )

/*++

Routine Description:

    This routine initializes the environment variable support in the C library.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PPROCESS_ENVIRONMENT Environment;

    OsImGetEnvironmentVariable = ClpImGetEnvironmentVariable;
    Environment = OsGetCurrentEnvironment();
    environ = Environment->Environment;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
ClpImGetEnvironmentVariable (
    PSTR Variable
    )

/*++

Routine Description:

    This routine gets an environment variable value for the image library.

Arguments:

    Variable - Supplies a pointer to a null terminated string containing the
        name of the variable to get.

Return Value:

    Returns a pointer to the value of the environment variable. The image
    library will not free or modify this value.

    NULL if the given environment variable is not set.

--*/

{

    return getenv(Variable);
}

INT
ClpReallocateEnvironment (
    VOID
    )

/*++

Routine Description:

    This routine reallocates the environment variables, either copying the
    existing allocated variables into an expanded buffer or copying in the
    user-supplied variables.

Arguments:

    None.

Return Value:

    0 on success.

    ENOMEM if insufficient memory was available to add the variable.

--*/

{

    ULONG Count;
    PSTR *NewEnvironment;
    ULONG NewEnvironmentCapacity;

    //
    // If there is a user environment that has yet to been copied here, then
    // base the size off the user environment.
    //

    Count = 0;
    if ((environ != NULL) && (environ != ClAllocatedEnvironment)) {

        ASSERT(ClAllocatedEnvironment == NULL);

        while (environ[Count] != NULL) {
            Count += 1;
        }

        NewEnvironmentCapacity = Count * 2;
    }

    //
    // If there was no existing environment, or it was empty, use the initial
    // capacity or base the new size off of the existing size.
    //

    if (Count == 0) {
        if (ClAllocatedEnvironment == NULL) {

            ASSERT(ClAllocatedEnvironmentCapacity == 0);

            NewEnvironmentCapacity = ENVIRONMENT_ARRAY_INITIAL_CAPACITY;

        } else {
            NewEnvironmentCapacity = ClAllocatedEnvironmentCapacity * 2;
        }
    }

    if (NewEnvironmentCapacity < ENVIRONMENT_ARRAY_INITIAL_CAPACITY) {
        NewEnvironmentCapacity = ENVIRONMENT_ARRAY_INITIAL_CAPACITY;
    }

    //
    // The capacity does not include room for the NULL terminator, so always
    // allocate an extra space.
    //

    NewEnvironment = realloc(ClAllocatedEnvironment,
                             (NewEnvironmentCapacity + 1) * sizeof(char *));

    if (NewEnvironment == NULL) {
        return ENOMEM;
    }

    //
    // If there is a user environment, copy said environment and add the
    // null-terminator.
    //

    if ((environ != NULL) && (environ != ClAllocatedEnvironment)) {
        memcpy(NewEnvironment, environ, Count * sizeof(char *));
        NewEnvironment[Count] = NULL;

    //
    // Otherwise, just null terminate if this is the first time the environment
    // has been allocated. If realloc copied an existing environment above then
    // it should have copied the null-terminator along with it.
    //

    } else if (environ == NULL) {
        NewEnvironment[0] = NULL;
    }

    //
    // Update the global variables.
    //

    ClAllocatedEnvironmentCapacity = NewEnvironmentCapacity;
    ClAllocatedEnvironment = NewEnvironment;
    environ = NewEnvironment;
    return 0;
}
