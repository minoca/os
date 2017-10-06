/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    utsname.h

Abstract:

    This header contains definitions for getting the system name.

Author:

    Evan Green 17-Jul-2013

--*/

#ifndef _UTSNAME_H
#define _UTSNAME_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <limits.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the maximum length of each of the strings in the utsname structure.
//

#define UTSNAME_STRING_SIZE 80

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the buffer used to name the machine.

Members:

    sysname - Stores a string describing the name of this implementation of
        the operating system.

    nodename - Stores a string describing the name of this node within the
        communications network to which this node is attached, if any.

    release - Stores a string containing the release level of this
        implementation.

    version - Stores a string containing the version level of this release.

    machine - Stores the name of the hardware type on which the system is
        running.

    domainname - Stores the name of the network domain this machine resides in,
        if any.

--*/

struct utsname {
    char sysname[UTSNAME_STRING_SIZE];
    char nodename[HOST_NAME_MAX];
    char release[UTSNAME_STRING_SIZE];
    char version[UTSNAME_STRING_SIZE];
    char machine[UTSNAME_STRING_SIZE];
    char domainname[UTSNAME_STRING_SIZE];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
uname (
    struct utsname *Name
    );

/*++

Routine Description:

    This routine returns the system name and version.

Arguments:

    Name - Supplies a pointer to a name structure to fill out.

Return Value:

    Returns a non-negative value on success.

    -1 on error, and errno will be set to indicate the error.

--*/

#ifdef __cplusplus

}

#endif
#endif

