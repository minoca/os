/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    pty.h

Abstract:

    This header contains definitions for manipulating pseudo-terminals in the
    C Library.

Author:

    Evan Green 2-Feb-2015

--*/

#ifndef _PTY_H
#define _PTY_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
openpty (
    int *Master,
    int *Slave,
    char *Name,
    const struct termios *Settings,
    const struct winsize *WindowSize
    );

/*++

Routine Description:

    This routine creates a new pseudo-terminal device.

Arguments:

    Master - Supplies a pointer where a file descriptor to the master will be
        returned on success.

    Slave - Supplies a pointer where a file descriptor to the slave will be
        returned on success.

    Name - Supplies an optional pointer where the name of the slave terminal
        will be returned on success. This buffer must be PATH_MAX size in bytes
        if supplied.

    Settings - Supplies an optional pointer to the settings to apply to the
        new terminal.

    WindowSize - Supplies an optional pointer to the window size to set in the
        new terminal.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

