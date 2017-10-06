/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    syslog.h

Abstract:

    This header contains definitions for the system logger facilities in the C
    Library

Author:

    Evan Green 22-Jan-2015

--*/

#ifndef _SYSLOG_H
#define _SYSLOG_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stdarg.h>
#include <sys/types.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro extracts the priority portion of a log ID.
//

#define LOG_PRI(_Priority) ((_Priority) & LOG_PRIMASK)

//
// This macro extracts the facility portion of a log ID.
//

#define LOG_FAC(_Facility) (((_Facility) & LOG_FACMASK) >> 3)

//
// This macro combines the facility and priority into a single 32-bit quantity.
//

#define LOG_MAKEPRI(_Facility, _Priority) ((_Facility) | (_Priority))

//
// This macro converts a single priority value into a log mask for setlogmask.
//

#define LOG_MASK(_Priority) (1 << (_Priority))

//
// This macro evaluates to a log mask for setlogmask of the given priority
// value and all priorities of increasing importance.
//

#define LOG_UPTO(_Priority) ((1 << ((_Priority) + 1)) - 1)

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define log priorities.
//

//
// Emergency: System is unusable
//

#define LOG_EMERG 0

//
// Alert: Action must be taken immediately.
//

#define LOG_ALERT 1

//
// Critical conditions.
//

#define LOG_CRIT 2

//
// Error conditions
//

#define LOG_ERR 3

//
// Warning conditions
//

#define LOG_WARNING 4

//
// Notice: Normal but significant condition
//

#define LOG_NOTICE 5

//
// Informational messages
//

#define LOG_INFO 6

//
// Debug-level messages
//

#define LOG_DEBUG 7

//
// Define the mask used to extract the priority.
//

#define LOG_PRIMASK 0x0007

//
// Define logger facilities.
//

//
// Kernel messages
//

#define LOG_KERN (0 << 3)

//
// Miscellaneous user mode messages
//

#define LOG_USER (1 << 3)

//
// Mail system
//

#define LOG_MAIL (2 << 3)

//
// System daemons
//

#define LOG_DAEMON (3 << 3)

//
// Security/authorization messages
//

#define LOG_AUTH (4 << 3)

//
// Messages generated internally by the system log daemon
//

#define LOG_SYSLOG (5 << 3)

//
// Line printer subsystem
//

#define LOG_LPR (6 << 3)

//
// Network news subsystem
//

#define LOG_NEWS (7 << 3)

//
// UUCP subsystem
//

#define LOG_UUCP (8 << 3)

//
// Cron daemon
//

#define LOG_CRON (9 << 3)

//
// Private security/authorization messages
//

#define LOG_AUTHPRIV (10 << 3)

//
// FTP daemon
//

#define LOG_FTP (11 << 3)

//
// Codes 12 through 15 are reserved for system use.
//

//
// Local facilities
//

#define LOG_LOCAL0 (16 << 3)
#define LOG_LOCAL1 (17 << 3)
#define LOG_LOCAL2 (18 << 3)
#define LOG_LOCAL3 (19 << 3)
#define LOG_LOCAL4 (20 << 3)
#define LOG_LOCAL5 (21 << 3)
#define LOG_LOCAL6 (22 << 3)
#define LOG_LOCAL7 (23 << 3)

//
// Define the current number of logging facilities
//

#define LOG_NFACILITIES 24

//
// Define the mask used to extract the facility portion of the combined value.
//

#define LOG_FACMASK 0x03F8

//
// Define options for openlog.
//

//
// Set this option to log the process ID with every message.
//

#define LOG_PID 0x00000001

//
// Set this option to log to the console if there were errors sending the log.
//

#define LOG_CONS 0x00000002

//
// Set this option to delay open until the first syslog call. This is the
// default.
//

#define LOG_ODELAY 0x00000004

//
// Set this option to open the log file immediately (no delay).
//

#define LOG_NDELAY 0x00000008

//
// Set this option to not wait for child processes.
//

#define LOG_NOWAIT 0x00000010

//
// Set this option to log to standard error as well.
//

#define LOG_PERROR 0x00000020

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
void
openlog (
    const char *Identifier,
    int Options,
    int Facility
    );

/*++

Routine Description:

    This routine sets process attributes that affect subsequent calls to the
    syslog function.

Arguments:

    Identifier - Supplies an identifier that is prepended to every message.

    Options - Supplies a mask of logging options. See LOG_* definitions.

    Facility - Supplies the default facility to be assigned to all messages
        that don't already have a facility. The initial default facility is
        LOG_USER.

Return Value:

    None.

--*/

LIBC_API
int
setlogmask (
    int PriorityMask
    );

/*++

Routine Description:

    This routine sets the log priority mask for the current process, and
    returns the previous mask. Calls to syslog with a priority not set in the
    given mask will be silently rejected. The default mask allows all
    priorities to be logged. A call to openlog is not requred prior to calling
    this function.

Arguments:

    PriorityMask - Supplies the mask of priority bits to log. Use LOG_MASK and
        LOG_UPTO macros to create this value. If this value is zero, the
        current mask is returned but is not changed.

Return Value:

    Returns the original mask before the potential change.

--*/

LIBC_API
void
vsyslog (
    int Priority,
    const char *Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine sends a message to an implementation-defined logging facility,
    which may log it to an implementation-defined system log, write it to the
    console, forward it over the network, or simply ignore it. The message
    header contains at least a timestamp and tag string.

Arguments:

    Priority - Supplies the priority and facility of the message.

    Format - Supplies the printf-style format string to print.

    ArgumentList - Supplies the remaining arguments, dictated by the format
        string.

Return Value:

    None.

--*/

LIBC_API
void
syslog (
    int Priority,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine sends a message to an implementation-defined logging facility,
    which may log it to an implementation-defined system log, write it to the
    console, forward it over the network, or simply ignore it. The message
    header contains at least a timestamp and tag string.

Arguments:

    Priority - Supplies the priority and facility of the message.

    Format - Supplies the printf-style format string to print.

    ... - Supplies the remaining arguments, dictated by the format string.

Return Value:

    None.

--*/

LIBC_API
void
closelog (
    void
    );

/*++

Routine Description:

    This routine shuts down system logging facilities. They may be reopened by
    a subsequent call to openlog or syslog.

Arguments:

    None.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

