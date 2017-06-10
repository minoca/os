/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osp.h

Abstract:

    This header contains definitions for the Chalk OS module.

Author:

    Evan Green 28-Aug-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

#ifdef _WIN32

#include "oswin32.h"

#else

#include <sys/utsname.h>

#endif

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef CK_IS_UNIX
#define CK_IS_UNIX 1
#endif

//
// Define any O_* open flags that might not exist on all systems.
//

#ifndef O_APPEND
#define O_APPEND 0x0
#endif

#ifndef O_EXEC
#define O_EXEC 0x0
#endif

#ifndef O_SEARCH
#define O_SEARCH 0x0
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0x0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x0
#endif

#ifndef O_SYNC
#define O_SYNC 0x0
#endif

#ifndef O_DSYNC
#define O_DSYNC 0x0
#endif

#ifndef O_RSYNC
#define O_RSYNC 0x0
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0x0
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0
#endif

#ifndef O_ASYNC
#define O_ASYNC 0x0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0x0
#endif

#ifndef O_NOATIME
#define O_NOATIME 0x0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0x0
#endif

#ifndef O_PATH
#define O_PATH 0x0
#endif

#ifndef O_TEXT
#define O_TEXT 0x0
#endif

#ifndef O_BINARY
#define O_BINARY 0x0
#endif

#ifndef S_ISLNK
#define S_ISLNK(_Mode) 0
#endif

#ifndef S_ISUID
#define S_ISUID 0
#endif

#ifndef S_ISGID
#define S_ISGID 0
#endif

#ifndef S_ISVTX
#define S_ISVTX 0
#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern CK_VARIABLE_DESCRIPTION CkOsErrnoValues[];
extern CK_VARIABLE_DESCRIPTION CkOsIoModuleValues[];

//
// -------------------------------------------------------- Function Prototypes
//

VOID
CkpOsModuleInit (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine populates the OS module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

VOID
CkpOsRaiseError (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine raises an error associated with the current errno value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

VOID
CkpOsInitializeInfo (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine initializes the OS information functions and globals.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

