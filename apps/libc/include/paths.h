/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paths.h

Abstract:

    This header contains standard paths.

Author:

    Evan Green 1-Sep-2016

--*/

#ifndef _PATHS_H
#define _PATHS_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the default search path.
//

#define _PATH_DEFPATH "/usr/bin:/bin:/usr/local/bin"

//
// Define the "all standard utilities" path.
//

#define _PATH_STDPATH \
    "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin"

#define _PATH_BSHELL    "/bin/sh"
#define _PATH_CONSOLE   "/dev/console"
#define _PATH_CSHELL    "/bin/csh"
#define _PATH_DEVDB     "/var/run/dev.db"
#define _PATH_DEVNULL   "/dev/null"
#define _PATH_DEVZERO   "/dev/zero"
#define _PATH_LASTLOG   "/var/log/lastlog"
#define _PATH_LOCALE    "/usr/share/locale"
#define _PATH_MAILDIR   "/var/mail"
#define _PATH_MAN       "/usr/share/man"
#define _PATH_MNTTAB    "/etc/fstab"
#define _PATH_MOUNTED   "/etc/mtab"
#define _PATH_NOLOGIN   "/etc/nologin"
#define _PATH_PRESERVE  "/var/lib"
#define _PATH_RSH       "/usr/bin/rsh"
#define _PATH_SENDMAIL  "/usr/sbin/sendmail"
#define _PATH_SHADOW    "/etc/shadow"
#define _PATH_SHELLS    "/etc/shells"
#define _PATH_TTY       "/dev/tty"
#define _PATH_UTMP      "/var/run/utmp"
#define _PATH_UTMPX     _PATH_UTMP
#define _PATH_WTMP      "/var/log/wtmp"
#define _PATH_WTMPX     _PATH_WTMP

#define _PATH_DEV       "/dev/"
#define _PATH_TMP       "/tmp/"
#define _PATH_VARDB     "/var/db/"
#define _PATH_VARRUN    "/var/run/"
#define _PATH_VARTMP    "/var/tmp/"

//
// Minoca-specific paths.
//

#define _PATH_TZ        "/etc/tz"
#define _PATH_TZALMANAC "/usr/share/tz/tzdata"
#define _PATH_URANDOM   "/dev/urandom"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

