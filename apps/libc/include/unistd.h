/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    unistd.h

Abstract:

    This header contains standard system types and definitions.

Author:

    Evan Green 5-Mar-2013

--*/

#ifndef _UNISTD_H
#define _UNISTD_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the POSIX version this implementation (mostly) conforms to.
//

#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L
#define _XOPEN_VERSION 700

//
// Define the POSIX constants for options and option groups.
//

#define _POSIX_BARRIERS 200809L
#define _POSIX_CHOWN_RESTRICTED 1
#define _POSIX_CLOCK_SELECTION 200809L
#define _POSIX_CPUTIME -1
#define _POSIX_FSYNC 200809L
#define _POSIX_IPV6 200809L
#define _POSIX_JOB_CONTROL 1
#define _POSIX_MAPPED_FILES 200809L
#define _POSIX_MEMLOCK -1
#define _POSIX_MEMLOCK_RANGE -1
#define _POSIX_MEMORY_PROTECTION 200809L
#define _POSIX_MESSAGE_PASSING -1
#define _POSIX_MONOTONIC_CLOCK 200809L
#define _POSIX_NO_TRUNC 0
#define _POSIX_PRIORITIZED_IO -1
#define _POSIX_PRIORITY_SCHEDULING -1
#define _POSIX_RAW_SOCKETS 200809L
#define _POSIX_READER_WRITER_LOCKS 200809L
#define _POSIX_REALTIME_SIGNALS 200809L
#define _POSIX_REGEXP 1
#define _POSIX_SAVED_IDS 1
#define _POSIX_SEMAPHORES 200809L
#define _POSIX_SHARED_MEMORY_OBJECTS 200809L
#define _POSIX_SHELL 1
#define _POSIX_SPAWN 200809L
#define _POSIX_SPIN_LOCKS -1
#define _POSIX_SPORADIC_SERVER -1
#define _POSIX_SYNCHRONIZED_IO 200890L
#define _POSIX_THREAD_ATTR_STACKADDR 200809L
#define _POSIX_THREAD_ATTR_STACKSIZE 200809L
#define _POSIX_THREAD_CPUTIME -1
#define _POSIX_THREAD_PRIO_INHERIT -1
#define _POSIX_THREAD_PRIO_PROTECT -1
#define _POSIX_THREAD_PRIORITY_SCHEDULING -1
#define _POSIX_THREAD_PROCESS_SHARED 200809L
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT -1
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT -1
#define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
#define _POSIX_THREAD_SPORADIC_SERVER -1
#define _POSIX_THREADS 200809L
#define _POSIX_TIMEOUTS 200809L
#define _POSIX_TIMERS 200809L
#define _POSIX_TRACE -1
#define _POSIX_TRACE_EVENT_FILTER -1
#define _POSIX_TRACE_INHERIT -1
#define _POSIX_TRACE_LOG -1
#define _POSIX_TYPED_MEMORY_OBJECTS -1

#if (__SIZEOF_POINTER__ == 8)

#define _POSIX_V6_ILP32_OFF32 -1
#define _POSIX_V6_ILP32_OFFBIG -1
#define _POSIX_V6_LP64_OFF64 1
#define _POSIX_V6_LPBIG_OFFBIG -1
#define _POSIX_V7_ILP32_OFF32 -1
#define _POSIX_V7_ILP32_OFFBIG -1
#define _POSIX_V7_LP64_OFF64 1
#define _POSIX_V7_LPBIG_OFFBIG -1

#else

#define _POSIX_V6_ILP32_OFF32 -1
#define _POSIX_V6_ILP32_OFFBIG 1
#define _POSIX_V6_LP64_OFF64 -1
#define _POSIX_V6_LPBIG_OFFBIG -1
#define _POSIX_V7_ILP32_OFF32 -1
#define _POSIX_V7_ILP32_OFFBIG 1
#define _POSIX_V7_LP64_OFF64 -1
#define _POSIX_V7_LPBIG_OFFBIG -1

#endif

#define _POSIX2_C_BIND 200809L
#define _POSIX2_C_DEV 200809L
#define _POSIX2_CHAR_TERM 1
#define _POSIX2_FORT_DEV -1
#define _POSIX2_FORT_RUN -1
#define _POSIX2_LOCALEDEF 200809L
#define _POSIX2_PBS -1
#define _POSIX2_PBS_ACCOUNTING -1
#define _POSIX2_PBS_CHECKPOINT -1
#define _POSIX2_PBS_LOCATE -1
#define _POSIX2_PBS_MESSAGE -1
#define _POSIX2_PBS_TRACK -1
#define _POSIX2_SW_DEV 200809L
#define _POSIX2_UPE 200809L
#define _XOPEN_CRYPT 1
#define _XOPEN_ENH_I18N 1
#define _XOPEN_REALTIME -1
#define _XOPEN_REALTIME_THREADS -1
#define _XOPEN_SHM 1
#define _XOPEN_STREAMS -1
#define _XOPEN_UNIX 1
#define _XOPEN_UUCP -1

//
// Define all libcrypt API functions to be imports unless otherwise specified.
//

#ifndef LIBCRYPT_API

#define LIBCRYPT_API __DLLIMPORT

#endif

//
// Define the value of a character that shall disable terminal special
// character handling.
//

#define _POSIX_VDISABLE '\0'

//
// Define the standard file descriptors.
//

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

//
// Define flags used in calls to "access" that specify whether a given path
// is readable, writable, or executable. Also define a flag indicating if the
// file exists at all. Note that these constants match up with the
// EFFECTIVE_ACCESS_* definitions used by the kernel.
//

#define F_OK 0x00000000
#define X_OK 0x00000001
#define W_OK 0x00000002
#define R_OK 0x00000004

//
// Define sysconf constants.
//

//
// Determine the number of clock ticks per second for the clock_t type.
//

#define _SC_CLK_TCK 1

//
// Determine the size of a page on this system.
//

#define _SC_PAGE_SIZE 2
#define _SC_PAGESIZE _SC_PAGE_SIZE

//
// Determine the maximum length of arguments to the exec functions.
//

#define _SC_ARG_MAX 3

//
// Determine the number of simultaneous processes per user ID.
//

#define _SC_CHILD_MAX 4

//
// Determine the maximum length of a hostname, not including the null
// terminator, as returned by gethostname.
//

#define _SC_HOST_NAME_MAX 5

//
// Determine the maximum length of a login name, including the null terminator.
//

#define _SC_LOGIN_NAME_MAX 6

//
// Determine the maximum number of files that a process can have open at any
// time.
//

#define _SC_OPEN_MAX 7

//
// Determine the maximum number of repeated occurrences of a BRE permitted by
// regexec and regcomp.
//

#define _SC_RE_DUP_MAX 8

//
// Determine the maximum number of streams that a process can have open at any
// time.
//

#define _SC_STREAM_MAX 9

//
// Determine the maximum number of symbolic links seen in a pathname before
// resolution returns ELOOP.
//

#define _SC_SYMLOOP_MAX 10

//
// Determine the maximum length of a terminal device name, including the null
// terminator.
//

#define _SC_TTY_NAME_MAX 11

//
// Determine the maximum number of bytes in a timezone name.
//

#define _SC_TZNAME_MAX 12

//
// Determine the year and month the POSIX.1 standard was approved in the
// format YYYYMML. The value 199009 indicates the September 1990 revision.
//

#define _SC_VERSION 13

//
// Determine the maximum obase value accepted by the bc utility.
//

#define _SC_BC_BASE_MAX 14

//
// Determine the maximum value of elements permitted in an array by the bc
// utility.
//

#define _SC_BC_DIM_MAX 15

//
// Determine the maximum scale value allowed by the bc utility.
//

#define _SC_BC_SCALE_MAX 16

//
// Determine the maximum length of a string accepted by the bc utility.
//

#define _SC_BC_STRING_MAX 17

//
// Determine the maximum number of weights that can be assigned to an entry
// of the LC_COLLATE order keyword in the locale definition file.
//

#define _SC_COLL_WEIGHTS_MAX 18

//
// Determine the maximum number of expressions which can be nested within
// parentheses by the expr utility.
//

#define _SC_EXPR_NEST_MAX 19

//
// Determine the maximum length of a utility's input line, either from standard
// input or from a file. This includes space for a trailing newline.
//
//

#define _SC_LINE_MAX 20

//
// Determine the version of the POSIX.2 standard in the format of YYYYMML.
//

#define _SC_2_VERSION 21

//
// Determine whether the POSIX.2 C language development facilities are
// supported.
//

#define _SC_2_C_DEV 22

//
// Determine whether the POSIX.2 FORTRAN development utilities are supported.
//

#define _SC_2_FORT_DEV 23

//
// Determine whether the POSIX.2 FORTRAN runtime utilities are supported.
//

#define _SC_2_FORT_RUN 24

//
// Determine whether the POSIX.2 creation of locales via localedef is supported.
//

#define _SC_2_LOCALEDEF 25

//
// Determine whether the POSIX.2 software development utilities option is
// supported.
//

#define _SC_2_SW_DEV 26

//
// Determine the total number of physical pages in the system.
//

#define _SC_PHYS_PAGES 27

//
// Determine the total number of physical pages in the system that are
// currently allocated.
//

#define _SC_AVPHYS_PAGES 28

//
// Determine the number of processors configured.
//

#define _SC_NPROCESSORS_CONF 29

//
// Determine the number of processors online.
//

#define _SC_NPROCESSORS_ONLN 30

//
// Define the maximum size of getgrgid_r and getgrnam_r data buffers.
//

#define _SC_GETGR_R_SIZE_MAX 31

//
// Define the maximum size of getpwuid_r and getpwnam_r data buffers.
//

#define _SC_GETPW_R_SIZE_MAX 32

//
// Define the maximum number of groups in the system.
//

#define _SC_NGROUPS_MAX 33

//
// Determine whether POSIX thread barriers are supported.
//

#define _SC_BARRIERS 34

//
// Determine whether POSIX clock selection is supported.
//

#define _SC_CLOCK_SELECTION 35

//
// Determine whether the Process CPU-Time Clocks option is supported.
//

#define _SC_CPUTIME 36

//
// Determine whether the File Synchronization option is supported.
//

#define _SC_FSYNC 37

//
// Determine whether the IPv6 option is supported.
//

#define _SC_IPV6 38

//
// Determine whether job control is supported.
//

#define _SC_JOB_CONTROL 39

//
// Determine whether memory mapped files are supported.
//

#define _SC_MAPPED_FILES 40

//
// Determine whether the Process Memory Locking option is supported.
//

#define _SC_MEMLOCK 41

//
// Determine whether the Range Memory Locking option is supported.
//

#define _SC_MEMLOCK_RANGE 42

//
// Determine whether memory protection is supported.
//

#define _SC_MEMORY_PROTECTION 43

//
// Determine whether the Message Passing option is supported.
//

#define _SC_MESSAGE_PASSING 44

//
// Determine whether the the Monotonic Clock option is supported.
//

#define _SC_MONOTONIC_CLOCK 45

//
// Determine whether the Prioritized Input and Output option is supported.
//

#define _SC_PRIORITIZED_IO 46

//
// Determine whether the Process Scheduling opion is supported.
//

#define _SC_PRIORITY_SCHEDULING 47

//
// Determine whether the Raw Sockets options is supported.
//

#define _SC_RAW_SOCKETS 48

//
// Determine whether read-write locks are supported.
//

#define _SC_READER_WRITER_LOCKS 49

//
// Determine whether realtime signals are supported.
//

#define _SC_REALTIME_SIGNALS 50

//
// Determine whether regular expression handling is supported.
//

#define _SC_REGEXP 51

//
// Determine whether each process has a saved set-user-ID and a saved
// set-group-ID.
//

#define _SC_SAVED_IDS 52

//
// Determine whether semaphores are supported.
//

#define _SC_SEMAPHORES 53

//
// Determine whether the Shared Memory Objects option is supported.
//

#define _SC_SHARED_MEMORY_OBJECTS 54

//
// Determine whether the POSIX shell is supported.
//

#define _SC_SHELL 55

//
// Determine whetherthe Spawn option is supported.
//

#define _SC_SPAWN 56

//
// Determine whether the Spin Locks options is supported.
//

#define _SC_SPIN_LOCKS 57

//
// Determine whether the Process Sporadic Server option is supported.
//

#define _SC_SPORADIC_SERVER 58

//
// Determine whether the Synchronized Input and Output option is supported.
//

#define _SC_SYNCHRONIZED_IO 59

//
// Determine whether the Thread Stack Address Attribute option is supported.
//

#define _SC_THREAD_ATTR_STACKADDR 60

//
// Determine whether the Thread Stack Size Attribute option is supported.
//

#define _SC_THREAD_ATTR_STACKSIZE 61

//
// Determine whether the Thread CPU-Time Clocks option is supported.
//

#define _SC_THREAD_CPUTIME 62

//
// Determine whether the Non-Robust Mutex Priority Inheritance option is
// supported.
//

#define _SC_THREAD_PRIO_INHERIT 63

//
// Determine whether the Non-Robust Mutex Priority Protection option is
// supported.
//

#define _SC_THREAD_PRIO_PROTECT 64

//
// Determine whether the Thread Execution Scheduilng option is supported.
//

#define _SC_THREAD_PRIORITY_SCHEDULING 65

//
// Determine whether the Thread Process-Shared Synchronization option is
// supported.
//

#define _SC_THREAD_PROCESS_SHARED 66

//
// Determine whether the Robust Mutex Priority Inheritance option is supported.
//

#define _SC_THREAD_ROBUST_PRIO_INHERIT 67

//
// Determine whether the  Robust Mutex Priority Protection option is supported.
//

#define _SC_THREAD_ROBUST_PRIO_PROTECT 68

//
// Determine whether thread-safe functions are supported.
//

#define _SC_THREAD_SAFE_FUNCTIONS 69

//
// Determine whether the Thread Sporadic Server option is supported.
//

#define _SC_THREAD_SPORADIC_SERVER 70

//
// Determine whether threads are supported.
//

#define _SC_THREADS 71

//
// Determine whether timeouts are supported.
//

#define _SC_TIMEOUTS 72

//
// Determine whether timers are supported.
//

#define _SC_TIMERS 73

//
// Determine whether the Trace option is supported.
//

#define _SC_TRACE 74

//
// Determine whether the Trace Event Filter option is supported.
//

#define _SC_TRACE_EVENT_FILTER 75

//
// Determine whether the Trace Inherit option is supported.
//

#define _SC_TRACE_INHERIT 76

//
// Determine whether the Trace Log option is supported.
//

#define _SC_TRACE_LOG 77

//
// Determine whether the Typed Memory Objects option is supported.
//

#define _SC_TYPED_MEMORY_OBJECTS 78

//
// Determine whether the C-language compilation environment has 32-bit int,
// long, pointer, and off_t types.
//

#define _SC_V6_ILP32_OFF32 79

//
// Determine whether the C-language compilation environment has 32-bit int,
// long, and pointer types, and an off_t type of at least 64-bits.
//

#define _SC_V6_ILP32_OFFBIG 80

//
// Determine whether the C-language compilation environment has 32-bit int and
// 64-bit long, pointer, and off_t types.
//

#define _SC_V6_LP64_OFF64 81

//
// Determine whether the C-language compilation environment has an int type of
// at least 32-bits and long, pointer, and off_t types of at least 64-bits.
//

#define _SC_V6_LPBIG_OFFBIG 82

//
// Determine whether the C-language compilation environment has 32-bit int,
// long, pointer, and off_t types.
//

#define _SC_V7_ILP32_OFF32 83

//
// Determine whether the C-language compilation environment has 32-bit int,
// long, and pointer types, and an off_t type of at least 64-bits.
//

#define _SC_V7_ILP32_OFFBIG 84

//
// Determine whether the C-language compilation environment has 32-bit int and
// 64-bit long, pointer, and off_t types.
//

#define _SC_V7_LP64_OFF64 85

//
// Determine whether the C-language compilation environment has an int type of
// at least 32-bits and long, pointer, and off_t types of at least 64-bits.
//

#define _SC_V7_LPBIG_OFFBIG 86

//
// Determine whether the C-Language Binding option is supported.
//

#define _SC_2_C_BIND 87

//
// Determine whether the Terminal Characteristics option is supported.
//

#define _SC_2_CHAR_TERM 88

//
// Determine whether the Batch Environment Services and Utilities option is
// supported.
//

#define _SC_2_PBS 89

//
// Determine whether the Batch Accounting option is supported.
//

#define _SC_2_PBS_ACCOUNTING 90

//
// Determine whether the Batch Checkpoint/Restart option is supported.
//

#define _SC_2_PBS_CHECKPOINT 91

//
// Determine whether the Locate Batch Job Request option is supported.
//

#define _SC_2_PBS_LOCATE 92

//
// Determine whether the Batch Job Message Request option is supported.
//

#define _SC_2_PBS_MESSAGE 93

//
// Determine whether the Track Batch Job Request option is supported.
//

#define _SC_2_PBS_TRACK 94

//
// Determine whether the User Portability Utilities option is supported.
//

#define _SC_2_UPE 95

//
// Determine whether the X/Open Encryption Option Group is supported.
//

#define _SC_XOPEN_CRYPT 96

//
// Determine whether the Issue 4, Version 2 Enhanced Internationalization
// Option Group is supported.
//

#define _SC_XOPEN_ENH_I18N 97

//
// Determine whether the X/Open Realtime Option Group is supported.
//

#define _SC_XOPEN_REALTIME 98

//
// Determine whether the X/Open Realtime Threads Option Group is supported.
//

#define _SC_XOPEN_REALTIME_THREADS 99

//
// Determine whether the Issue 4, Version 2 Shared Memory Option Group is
// supported.
//

#define _SC_XOPEN_SHM 100

//
// Determine whether the XSI STREAMS Option Group is supported.
//

#define _SC_XOPEN_STREAMS 101

//
// Determine whether the XSI option is supported.
//

#define _SC_XOPEN_UNIX 102

//
// Determine whether the UUCP Utilities option is supported.
//

#define _SC_XOPEN_UUCP 103

//
// Determine the version of the X/Open Portability Guide.
//

#define _SC_XOPEN_VERSION 104

//
// Define pathconf and fpathconf constants.
//

//
// When referring to a directory, the system supports the creation of symbolic
// links within that directory.
//

#define _PC_2_SYMLINKS 1

//
// Get the minimum number of bytes of storage actually allocated for any
// portion of a file.
//

#define _PC_ALLOC_SIZE_MIN 2

//
// Determine whether asynchronous I/O operations may be performed for the
// associated file.
//

#define _PC_ASYNC_IO 3

//
// Determine whether the use of chown and fchown is restricted to a process
// with appropriate privileges, and to changing the group ID of a file only to
// the effective group ID of the process or to one of its supplementary group
// IDs.
//

#define _PC_CHOWN_RESTRICTED 4

//
// Determine the minimum number of bits needed to represent, as a signed
// integer, the maximum size of a regular file allowed in the specified
// directory.
//

#define _PC_FILESIZEBITS 5

//
// Determine the maximum number of links to a single file.
//

#define _PC_LINK_MAX 6

//
// Determine the maximum number of bytes in a terminal canonical input line.
//

#define _PC_MAX_CANON 7

//
// Determine the minimum number of bytes for which space is available in the
// terminal input queue. This is also the maximum number of bytes a conforming
// application may require to be typed as input before reading them.
//

#define _PC_MAX_INPUT 8

//
// Determine the maximum number of bytes in a file name, not including the
// null terminator.
//

#define _PC_NAME_MAX 9

//
// Determine whether pathname components longer than NAME_MAX generate an error.
//

#define _PC_NO_TRUNC 10

//
// Determine the minimum number of bytes in a path name, including the null
// terminator.
//

#define _PC_PATH_MAX 11

//
// Determine the maximum number of bytes that is guaranteed to be atomic when
// writing to a pipe.
//

#define _PC_PIPE_BUF 12

//
// Determine whether prioritized input or output operations may be performed
// for the associated file.
//

#define _PC_PRIO_IO 13

//
// Determine the recommended increment for file transfer sizes between
// POSIX_REC_MIN_XFER_SIZE and POSIX_REC_MAX_XFER_SIZE.
//

#define _PC_REC_INCR_XFER_SIZE 14

//
// Determine the minimum recommended transfer size.
//

#define _PC_REC_MIN_XFER_SIZE 15

//
// Determine the recommended transfer alignment.
//

#define _PC_REC_XFER_ALIGN 16

//
// Determine the maximum number of bytes in a symbolic link.
//

#define _PC_SYMLINK_MAX 17

//
// Determine whether synchronized input or output operations may be performed
// for the associated file.
//

#define _PC_SYNC_IO 18

//
// Determine the value of a character that shall disable terminal special
// character handling.
//

#define _PC_VDISABLE 19

//
// Define function values that can be passed to the lockf function.
//

//
// Use this function to unlock a previously locked region.
//

#define F_ULOCK 0

//
// Use this function to lock a region for exclusive use.
//

#define F_LOCK 1

//
// Use this function to test and lock a region for exclusive use, without
// blocking.
//

#define F_TLOCK 2

//
// Use this function to test a region for locks held by other processes.
//

#define F_TEST 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the environment.
//

LIBC_API extern char **environ;

//
// Define some globals from getopt.h. See getopt.h for full descriptions of
// each of these variables. The header getopt.h is not included in this file
// because some declarations (like struct option) should not be included here.
//

LIBC_API extern char *optarg;
LIBC_API extern int optind;
LIBC_API extern int opterr;
LIBC_API extern int optopt;
LIBC_API extern int optreset;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
brk (
    void *Address
    );

/*++

Routine Description:

    This routine sets the current program break to the specified address.

    New programs should use malloc and free in favor of this deprecated
    legacy function. This function is likely to fail if any other memory
    functions such as malloc or free are used. Other functions, including the
    C library, may use malloc and free silently. This function is neither
    thread-safe nor reentrant.

Arguments:

    Address - Supplies the new address of the program break.

Return Value:

    0 on success.

    -1 on failure, and errno is set to indicate the error.

--*/

LIBC_API
void *
sbrk (
    intptr_t Increment
    );

/*++

Routine Description:

    This routine increments the current program break by the given number of
    bytes. If the value is negative, the program break is decreased.

    New programs should use malloc and free in favor of this deprecated
    legacy function. This function is likely to fail if any other memory
    functions such as malloc or free are used. Other functions, including the
    C library, may use malloc and free silently. This function is neither
    thread-safe nor reentrant.

Arguments:

    Increment - Supplies the amount to add or remove from the program break.

Return Value:

    Returns the original program break address before this function changed it
    on success.

    (void *)-1 on failure, and errno is set to indicate the error.

--*/

LIBC_API
int
execl (
    const char *Path,
    const char *Argument0,
    ...
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Argument0 - Supplies the first argument to execute, usually the same as
        the command name.

    ... - Supplies the arguments to the program. The argument list must be
        terminated with a NULL.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execv (
    const char *Path,
    char *const Arguments[]
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execle (
    const char *Path,
    const char *Argument0,
    ...
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Argument0 - Supplies the first argument to the program, usually the same
        as the program name.

    ... - Supplies the arguments to the program. The argument list must be
        terminated with a NULL. After the NULL an array of strings representing
        the environment is expected (think of it like a final argument after
        the NULL in the form const char *envp[]).

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execve (
    const char *Path,
    char *const Arguments[],
    char *const Environment[]
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.

Arguments:

    Path - Supplies a pointer to a string containing the fully specified path
        of the file to execute.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

    Environment - Supplies an array of pointers to strings containing the
        environment variables to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execlp (
    const char *File,
    const char *Argument0,
    ...
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image. If the
    given file is found but of an unrecognized binary format, then a shell
    interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Argument0 - Supplies the first argument to the program. Additional arguments
        follow in the ellipses. The argument list must be terminated with a
        NULL.

    ... - Supplies any remaining arguments.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execvp (
    const char *File,
    char *const Arguments[]
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image. If the
    given file is found but of an unrecognized binary format, then a shell
    interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
int
execvpe (
    const char *File,
    char *const Arguments[],
    char *const Environment[]
    );

/*++

Routine Description:

    This routine replaces the current process image with a new image. The
    parameters to this function also include the environment variables to use.
    If the given file is found but of an unrecognized binary format, then a
    shell interpreter will be launched and passed the file.

Arguments:

    File - Supplies a pointer to a string containing the name of the executable,
        which will be searched for on the PATH if the string does not contain a
        slash.

    Arguments - Supplies an array of pointers to strings containing the
        arguments to pass to the program.

    Environment - Supplies an array of pointers to strings containing the
        environment variables to pass to the program.

Return Value:

    Does not return on success, the current process is gutted and replaced with
    the specified image.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBC_API
__NO_RETURN
void
_exit (
    int Status
    );

/*++

Routine Description:

    This routine terminates the current process. It does not call any routines
    registered to run upon exit.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

LIBC_API
int
close (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine closes a file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to close.

Return Value:

    0 on success.

    -1 if the file could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

LIBC_API
int
closefrom (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine closes all file descriptors with a value greater than or
    equal to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the minimum file descriptor number.

Return Value:

    0 on success.

    -1 if a file descriptor could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

LIBC_API
ssize_t
read (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine attempts to read the specifed number of bytes from the given
    open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer where the read bytes will be
        returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes successfully read from the file.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
ssize_t
pread (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount,
    off_t Offset
    );

/*++

Routine Description:

    This routine attempts to read the specifed number of bytes from the given
    open file descriptor at a given offset. It does not change the current
    file pointer.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer where the read bytes will be
        returned.

    ByteCount - Supplies the number of bytes to read.

    Offset - Supplies the offset from the start of the file to read from.

Return Value:

    Returns the number of bytes successfully read from the file.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
ssize_t
write (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer containing the bytes to be
        written.

    ByteCount - Supplies the number of bytes to write.

Return Value:

    Returns the number of bytes successfully written to the file.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
ssize_t
pwrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount,
    off_t Offset
    );

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor at a given offset. It does not update the current
    file position.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer containing the bytes to be
        written.

    ByteCount - Supplies the number of bytes to write.

    Offset - Supplies the offset from the start of the file to write to.

Return Value:

    Returns the number of bytes successfully written to the file.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
int
fsync (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine flushes all the data associated with the open file descriptor
    to its corresponding backing device. It does not return until the data has
    been flushed.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
fdatasync (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine flushes all the data associated with the open file descriptor
    to its corresponding backing device. It does not return until the data has
    been flushed. It is similar to fsync but does not flush modified metadata
    if that metadata is unimportant to retrieving the file later. For example,
    last access and modified times wouldn't require a metadata flush, but file
    size change would.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
void
sync (
    void
    );

/*++

Routine Description:

    This routine schedules a flush for all file system related data that is in
    memory. Upon return, it is not guaranteed that the writing of the data is
    complete.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
off_t
lseek (
    int FileDescriptor,
    off_t Offset,
    int Whence
    );

/*++

Routine Description:

    This routine sets the file offset for the open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Offset - Supplies the offset from the reference location given in the
        Whence argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

LIBC_API
int
ftruncate (
    int FileDescriptor,
    off_t NewSize
    );

/*++

Routine Description:

    This routine sets the file size of the given file descriptor. If the new
    size is smaller than the original size, then the remaining data will be
    discarded. If the new size is larger than the original size, then the
    extra space will be filled with zeroes.

Arguments:

    FileDescriptor - Supplies the file descriptor whose size should be
        modified.

    NewSize - Supplies the new size of the file descriptor in bytes.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
truncate (
    const char *Path,
    off_t NewSize
    );

/*++

Routine Description:

    This routine sets the file size of the given file path. If the new size is
    smaller than the original size, then the remaining data will be discarded.
    If the new size is larger than the original size, then the extra space will
    be filled with zeroes.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the file whose size should be changed.

    NewSize - Supplies the new size of the file descriptor in bytes.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
pipe (
    int FileDescriptors[2]
    );

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
pipe2 (
    int FileDescriptors[2],
    int Flags
    );

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

    Flags - Supplies a bitfield of open flags governing the behavior of the new
        descriptors. Only O_NONBLOCK and O_CLOEXEC are honored.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
symlink (
    const char *LinkTarget,
    const char *LinkName
    );

/*++

Routine Description:

    This routine creates a symbolic link with the given name pointed at the
    supplied target path.

Arguments:

    LinkTarget - Supplies the location that the link points to.

    LinkName - Supplies the path where the link should be created.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
symlinkat (
    const char *LinkTarget,
    int Directory,
    const char *LinkName
    );

/*++

Routine Description:

    This routine creates a symbolic link with the given name pointed at the
    supplied target path.

Arguments:

    LinkTarget - Supplies the location that the link points to.

    Directory - Supplies an optional file descriptor. If the given path to the
        link name is a relative path, the directory referenced by this
        descriptor will be used as a starting point for path resolution. Supply
        AT_FDCWD to use the working directory for relative paths.

    LinkName - Supplies the path where the link should be created.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
ssize_t
readlink (
    const char *Path,
    char *LinkDestinationBuffer,
    size_t LinkDestinationBufferSize
    );

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Path - Supplies a pointer to the symbolic link path.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned. A null terminator is not
        written.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

Return Value:

    Returns the number of bytes placed into the buffer on success.

    -1 on failure. The errno variable will be set to indicate the error, and
    the buffer will remain unchanged.

--*/

LIBC_API
ssize_t
readlinkat (
    int Directory,
    const char *Path,
    char *LinkDestinationBuffer,
    size_t LinkDestinationBufferSize
    );

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned. A null terminator is not
        written.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

Return Value:

    Returns the number of bytes placed into the buffer on success.

    -1 on failure. The errno variable will be set to indicate the error, and
    the buffer will remain unchanged.

--*/

LIBC_API
int
link (
    const char *ExistingFile,
    const char *LinkPath
    );

/*++

Routine Description:

    This routine creates a hard link to the given file.

Arguments:

    ExistingFile - Supplies a pointer to a null-terminated string containing
        the path to the file that already exists.

    LinkPath - Supplies a pointer to a null-terminated string containing the
        path of the new link to create.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
linkat (
    int ExistingFileDirectory,
    const char *ExistingFile,
    int LinkPathDirectory,
    const char *LinkPath,
    int Flags
    );

/*++

Routine Description:

    This routine creates a hard link to the given file.

Arguments:

    ExistingFileDirectory - Supplies an optional file descriptor. If the given
        existing file path is a relative path, the directory referenced by this
        descriptor will be used as a starting point for path resolution. Supply
        AT_FDCWD to use the working directory for relative paths.

    ExistingFile - Supplies a pointer to a null-terminated string containing
        the path to the file that already exists.

    LinkPathDirectory - Supplies an optional file descriptor. If the given new
        link is a relative path, the directory referenced by this descriptor
        will be used as a starting point for path resolution. Supply AT_FDCWD
        to use the working directory for relative paths.

    LinkPath - Supplies a pointer to a null-terminated string containing the
        path of the new link to create.

    Flags - Supplies AT_SYMLINK_FOLLOW if the routine should link to the
        destination of the symbolic link if the existing file path is a
        symbolic link. Supply 0 to create a link to the symbolic link itself.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
unlink (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the entry to remove.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

LIBC_API
int
unlinkat (
    int Directory,
    const char *Path,
    int Flags
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to a null terminated string containing the path
        of the entry to remove.

    Flags - Supplies a bitfield of flags. Supply AT_REMOVEDIR to attempt to
        remove a directory (and only a directory). Supply zero to attempt to
        remove a non-directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

LIBC_API
int
dup (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine duplicates the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

Return Value:

    Returns the new file descriptor which represents a copy of the original
    file descriptor.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
dup2 (
    int FileDescriptor,
    int CopyDescriptor
    );

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
dup3 (
    int FileDescriptor,
    int CopyDescriptor,
    int Flags
    );

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy. If this
        is equal to the original file descriptor, then the call fails with
        EINVAL.

    Flags - Supplies O_* open flags governing the new descriptor. Only
        O_CLOEXEC is permitted.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
lockf (
    int FileDescriptor,
    int Function,
    off_t Size
    );

/*++

Routine Description:

    This routine locks or unlocks sections of a file with advisory-mode locks.
    All locks for a process are removed when the process terminates. Record
    locking is supported at least for regular files, and may be supported for
    other file types.

Arguments:

    FileDescriptor - Supplies the file descriptor to query. To establish a
        lock, the given file descriptor must be opened with O_WRONLY or O_RDWR.

    Function - Supplies the action to be taken. Valid values are:
        F_ULOCK - Unlocks a locked section.
        F_LOCK - Locks a section for exclusive use (blocking if already locked).
        F_TLOCK - Test and lock for exclusive use, not blocking.
        F_TEST - Test for a section of locks by other processes.

    Size - Supplies the number of contiguous bytes to be locked or unlocked.
        The section to be locked or unlocked starts at the current offset in
        the file and extends forward for a positve size or backwards for a
        negative size (the preceding bytes up to but not including the current
        offset). If size is 0, the section from the current offset through the
        largest possible offset shall be locked. Locks may exist past the
        current end of file.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information. The errno
    variable may be set to the following values:

    EACCES or EAGAIN if the function argument is F_TLOCK or F_TEST and the
    section is already locked by another process.

    EDEADLK if the function argument is F_LOCK and a deadlock is detected.

    EINVAL if the function is valid or the size plus the current offset is less
    than zero.

    EOVERFLOW if the range cannot properly be represented in an off_t.

--*/

LIBC_API
int
isatty (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine determines if the given file descriptor is backed by an
    interactive terminal device or not.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

Return Value:

    1 if the given file descriptor is backed by a terminal device.

    0 on error or if the file descriptor is not a terminal device. On error,
    the errno variable will be set to give more details.

--*/

LIBC_API
int
rmdir (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to unlink a directory. The directory must be empty or
    the operation will fail.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the directory to remove.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

LIBC_API
char *
getcwd (
    char *Buffer,
    size_t BufferSize
    );

/*++

Routine Description:

    This routine returns a pointer to a null terminated string containing the
    path to the current working directory.

Arguments:

    Buffer - Supplies a pointer to a buffer where the string should be returned.
        If NULL is supplied, then malloc will be used to allocate a buffer of
        the appropriate size, and it is therefore the caller's responsibility
        to free this memory.

    BufferSize - Supplies the size of the buffer, in bytes.

Return Value:

    Returns a pointer to a string containing the current working directory on
    success.

    NULL on failure. Errno will contain more information.

--*/

LIBC_API
int
chdir (
    const char *Path
    );

/*++

Routine Description:

    This routine changes the current working directory (the starting point for
    all paths that don't begin with a path separator).

Arguments:

    Path - Supplies a pointer to the null terminated string containing the
        path of the new working directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current working directory will not be changed.

--*/

LIBC_API
int
fchdir (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine changes the current working directory (the starting point for
    all paths that don't begin with a path separator) using an already open
    file descriptor to that directory.

Arguments:

    FileDescriptor - Supplies the open file handle to the directory to change
        to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current working directory will not be changed.

--*/

LIBC_API
int
chroot (
    const char *Path
    );

/*++

Routine Description:

    This routine changes the current root directory. The working directory is
    not changed. The caller must have sufficient privileges to change root
    directories.

Arguments:

    Path - Supplies a pointer to the null terminated string containing the
        path of the new root directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current root directory will not be changed. Errno may be set to the
    following values, among others:

    EACCES if search permission is denied on a component of the path prefix.

    EPERM if the caller has insufficient privileges.

--*/

LIBC_API
int
fchroot (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine changes the current root directory using an already open file
    descriptor to that directory. The caller must have sufficient privileges
    to change root directories.

Arguments:

    FileDescriptor - Supplies the open file handle to the directory to change
        to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. On failure,
    the current root directory will not be changed.

--*/

LIBC_API
int
fchown (
    int FileDescriptor,
    uid_t Owner,
    gid_t Group
    );

/*++

Routine Description:

    This routine sets the file owner and group of the file opened with the
    given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor whose owner and group should
        be modified.

    Owner - Supplies the new owner of the file.

    Group - Supplies the new group of the file.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
chown (
    const char *Path,
    uid_t Owner,
    gid_t Group
    );

/*++

Routine Description:

    This routine sets the file owner of the given path.

Arguments:

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
lchown (
    const char *Path,
    uid_t Owner,
    gid_t Group
    );

/*++

Routine Description:

    This routine sets the file owner of the given path. The only difference
    between this routine and chown is that if the path given to this routine
    refers to a symbolic link, the operation will be done on the link itself
    (as opposed to the destination of the link, which is what chown would
    operate on).

Arguments:

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fchownat (
    int Directory,
    const char *Path,
    uid_t Owner,
    gid_t Group,
    int Flags
    );

/*++

Routine Description:

    This routine sets the file owner of the given path.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

    Flags - Supplies AT_SYMLINK_NOFOLLOW if the routine should modify
        information for the symbolic link itself, or 0 if the call should
        follow a symbolic link at the destination.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
pid_t
getpid (
    void
    );

/*++

Routine Description:

    This routine returns the current process identifier.

Arguments:

    None.

Return Value:

    Returns the process identifier.

--*/

LIBC_API
pid_t
getppid (
    void
    );

/*++

Routine Description:

    This routine returns the current process' parent process identifier.

Arguments:

    None.

Return Value:

    Returns the parent process identifier.

--*/

LIBC_API
pid_t
getpgid (
    pid_t ProcessId
    );

/*++

Routine Description:

    This routine returns the process group identifier of the process with
    the given ID, or the calling process.

Arguments:

    ProcessId - Supplies the process ID to return the process group for. Supply
        0 to return the process group ID of the calling process.

Return Value:

    Returns the process group ID of the given process (or the current process).

    (pid_t)-1 and errno will be set to EPERM if the desired process is out of
    this session and the implementation doesn't allow cross session requests,
    ESRCH if no such process exists, or EINVAL if the pid argument is invalid.

--*/

LIBC_API
pid_t
getpgrp (
    void
    );

/*++

Routine Description:

    This routine returns the process group identifier of the calling process.

Arguments:

    None.

Return Value:

    Returns the process group ID of the calling process.

--*/

LIBC_API
int
setpgid (
    pid_t ProcessId,
    pid_t ProcessGroupId
    );

/*++

Routine Description:

    This routine joins an existing process group or creates a new process group
    within the session of the calling process. The process group ID of a
    session leader will not change.

Arguments:

    ProcessId - Supplies the process ID of the process to put in a new process
        group. Supply 0 to use the current process.

    ProcessGroupId - Supplies the new process group to put the process in.
        Supply zero to set the process group ID to the same numerical value as
        the specified process ID.

Return Value:

    0 on success.

    -1 on failure and errno will be set to contain more information.

--*/

LIBC_API
pid_t
setpgrp (
    void
    );

/*++

Routine Description:

    This routine sets the process group ID of the calling process to the
    process ID of the calling process. This routine has no effect if the
    calling process is a session leader.

Arguments:

    None.

Return Value:

    Returns the process group ID of the calling process.

--*/

LIBC_API
pid_t
getsid (
    pid_t ProcessId
    );

/*++

Routine Description:

    This routine returns the process group ID of the process that is the
    session leader of the given process. If the given parameter is 0, then
    the current process ID is used as the parameter.

Arguments:

    ProcessId - Supplies a process ID of the process whose session leader
        should be returned.

Return Value:

    Returns the process group ID of the session leader of the specified process.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
pid_t
setsid (
    void
    );

/*++

Routine Description:

    This routine creates a new session if the calling process is not a
    process group leader. The calling process will be the session leader of
    the new session, and will be the process group leader of a new process
    group, and will have no controlling terminal. The process group ID of the
    calling process will be set equal to the process ID of the calling process.
    The calling process will be the only process in the new process group and
    the only process in the new session.

Arguments:

    None.

Return Value:

    Returns the value of the new process group ID of the calling process.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
pid_t
fork (
    void
    );

/*++

Routine Description:

    This routine creates a new process by copying the existing process.

Arguments:

    None.

Return Value:

    Returns 0 to the child process.

    Returns the process ID of the child process to the parent process.

    Returns -1 to the parent process on error, and the errno variable will be
    set to provide more information about the error.

--*/

LIBC_API
uid_t
getuid (
    void
    );

/*++

Routine Description:

    This routine returns the current real user ID.

Arguments:

    None.

Return Value:

    Returns the real user ID.

--*/

LIBC_API
gid_t
getgid (
    void
    );

/*++

Routine Description:

    This routine returns the current real group ID.

Arguments:

    None.

Return Value:

    Returns the real group ID.

--*/

LIBC_API
uid_t
geteuid (
    void
    );

/*++

Routine Description:

    This routine returns the current effective user ID, which represents the
    privilege level with which this process can perform operations. Normally
    this is the same as the real user ID, but binaries with the setuid
    permission bit set the effective user ID to their own when they're run.

Arguments:

    None.

Return Value:

    Returns the effective user ID.

--*/

LIBC_API
gid_t
getegid (
    void
    );

/*++

Routine Description:

    This routine returns the current effective group ID, which represents the
    privilege level with which this process can perform operations. Normally
    this is the same as the real group ID, but binaries with the setgid
    permission bit set the effective group ID to their own when they're run.

Arguments:

    None.

Return Value:

    Returns the effective group ID.

--*/

LIBC_API
int
setuid (
    uid_t UserId
    );

/*++

Routine Description:

    This routine sets the real user ID, effective user ID, and saved
    set-user-ID of the calling process to the given user ID. This only occurs
    if the process has the appropriate privileges to do this. If the process
    does not have appropriate privileges but the given user ID is equal to the
    real user ID or the saved set-user-ID, then this routine sets the effective
    user ID to the given user ID; the real user ID and saved set-user-ID remain
    unchanged.

Arguments:

    UserId - Supplies the user ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real user ID or saved set-user-ID.

--*/

LIBC_API
int
setgid (
    gid_t GroupId
    );

/*++

Routine Description:

    This routine sets the real group ID, effective group ID, and saved
    set-group-ID of the calling process to the given group ID. This only occurs
    if the process has the appropriate privileges to do this. If the process
    does not have appropriate privileges but the given group ID is equal to the
    real group ID or the saved set-group-ID, then this routine sets the
    effective group ID to the given group ID; the real group ID and saved
    set-group-ID remain unchanged.

Arguments:

    GroupId - Supplies the group ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real group ID or saved set-group-ID.

--*/

LIBC_API
int
seteuid (
    uid_t UserId
    );

/*++

Routine Description:

    This routine sets the effective user ID of the calling process to the given
    user ID. The real user ID and saved set-user-ID remain unchanged. This only
    occurs if the process has appropriate privileges, or if the real user ID
    is equal to the saved set-user-ID.

Arguments:

    UserId - Supplies the effective user ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real user ID or saved set-user-ID.

--*/

LIBC_API
int
setegid (
    gid_t GroupId
    );

/*++

Routine Description:

    This routine sets the effective group ID of the calling process to the
    given group ID. The real group ID and saved set-group-ID remain unchanged.
    This only occurs if the process has appropriate privileges, or if the real
    group ID is equal to the saved set-group-ID.

Arguments:

    GroupId - Supplies the effective group ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real group ID or saved set-group-ID.

--*/

LIBC_API
int
setreuid (
    uid_t RealUserId,
    uid_t EffectiveUserId
    );

/*++

Routine Description:

    This routine sets the real and/or effective user IDs of the current process
    to the given values. This only occurs if the process has appropriate
    privileges. Unprivileged processes may only set the effective user ID to
    the real or saved user IDs. Unprivileged users may only set the real
    group ID to the saved or effective user IDs. If the real user ID is being
    set, or the effective user ID is being set to something other than the
    previous real user ID, then the saved user ID is also set to the new
    effective user ID.

Arguments:

    RealUserId - Supplies the real user ID to change to. If -1 is supplied, the
        real user ID will not be changed.

    EffectiveUserId - Supplies the effective user ID to change to. If -1 is
        supplied, the effective user ID will not be changed.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if a user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given effective ID
    does not match the real user ID or saved set-user-ID.

--*/

LIBC_API
int
setregid (
    gid_t RealGroupId,
    gid_t EffectiveGroupId
    );

/*++

Routine Description:

    This routine sets the real and/or effective group IDs of the current process
    to the given values. This only occurs if the process has appropriate
    privileges. Unprivileged processes may only set the effective group ID to
    the real or saved group IDs. Unprivileged users may only set the real
    group ID to the saved or effective group IDs. If the real group ID is being
    set, or the effective group ID is being set to something other than the
    previous real group ID, then the saved group ID is also set to the new
    effective group ID.

Arguments:

    RealGroupId - Supplies the real group ID to change to. If -1 is supplied,
        the real group ID will not be changed.

    EffectiveGroupId - Supplies the effective group ID to change to. If -1 is
        supplied, the effective group ID will not be changed.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if a group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given effective ID
    does not match the real group ID or saved set-group-ID.

--*/

LIBC_API
int
setresuid (
    uid_t RealUserId,
    uid_t EffectiveUserId,
    uid_t SavedUserId
    );

/*++

Routine Description:

    This routine sets the real, effective, and saved user IDs of the calling
    thread. A unprivileged process may set each one of these to one of the
    current real, effective, or saved user ID. A process with the setuid
    permission may set these to any values.

Arguments:

    RealUserId - Supplies the real user ID to set, or -1 to leave the value
        unchanged.

    EffectiveUserId - Supplies the effective user ID to set, or -1 to leave the
        value unchanged.

    SavedUserId - Supplies the saved user ID to set, or -1 to leave the value
        unchanged.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. This may
    fail even for root, so the return value must always be checked.

--*/

LIBC_API
int
setresgid (
    gid_t RealGroupId,
    gid_t EffectiveGroupId,
    gid_t SavedGroupId
    );

/*++

Routine Description:

    This routine sets the real, effective, and saved group IDs of the calling
    thread. A unprivileged process may set each one of these to one of the
    current real, effective, or saved group ID. A process with the setuid
    permission may set these to any values.

Arguments:

    RealGroupId - Supplies the real group ID to set, or -1 to leave the value
        unchanged.

    EffectiveGroupId - Supplies the effective group ID to set, or -1 to leave
        the value unchanged.

    SavedGroupId - Supplies the saved group ID to set, or -1 to leave the value
        unchanged.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. This may
    fail even for root, so the return value must always be checked.

--*/

LIBC_API
int
getgroups (
    int ElementCount,
    gid_t GroupList[]
    );

/*++

Routine Description:

    This routine returns the array of supplementary groups that the current
    user belongs to.

Arguments:

    ElementCount - Supplies the size (in elements) of the supplied group list
        buffer.

    GroupList - Supplies a buffer where the user's supplementary groups will
        be returned.

Return Value:

    Returns the number of supplementary groups that the current user belongs to.
    The full count is returned even if the element count is less than that so
    that the caller can regroup (get it) and try again if the buffer allocated
    was too small.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
char *
getlogin (
    void
    );

/*++

Routine Description:

    This routine returns a pointer to a string containing the user name
    associated by the login activity with the controlling terminal of the
    current process. This routine is neither reentrant nor thread safe.

Arguments:

    None.

Return Value:

    Returns a pointer to a buffer containing the name of the logged in user.
    This data may be overwritten by a subsequent call to this function.

    NULL on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
getlogin_r (
    char *Buffer,
    size_t BufferSize
    );

/*++

Routine Description:

    This routine returns a pointer to a string containing the user name
    associated by the login activity with the controlling terminal of the
    current process. This routine is thread safe and reentrant.

Arguments:

    Buffer - Supplies a pointer to the buffer where the login name will be
        returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
int
pause (
    void
    );

/*++

Routine Description:

    This routine suspends execution until a signal is caught and handled by the
    application. This routine is known to be frought with timing problems, as
    the most common use for it involves checking if a signal has occurred, and
    calling pause if not. Unfortunately that doesn't work as a signal can come
    in after the check but before the call to pause. Pause is really only
    useful if the entirety of the application functionality is implemented
    inside signal handlers.

Arguments:

    None.

Return Value:

    -1 always. A return rather negatively is thought of as a failure of the
    function. The errno variable will be set to indicate the "error".

--*/

LIBC_API
long
sysconf (
    int Variable
    );

/*++

Routine Description:

    This routine gets the system value for the given variable index. These
    variables are not expected to change within a single invocation of a
    process, and therefore need only be queried once per process.

Arguments:

    Variable - Supplies the variable to get. See _SC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

LIBC_API
long
fpathconf (
    int FileDescriptor,
    int Variable
    );

/*++

Routine Description:

    This routine gets the current value of a configurable limit or option
    that is associated with the given open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to query the value for.

    Variable - Supplies the variable to get. See _PC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

LIBC_API
long
pathconf (
    const char *Path,
    int Variable
    );

/*++

Routine Description:

    This routine gets the current value of a configurable limit or option
    that is associated with the given file or directory path.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the file
        or directory path to get the configuration limit for.

    Variable - Supplies the variable to get. See _PC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

LIBC_API
int
getdtablesize (
    void
    );

/*++

Routine Description:

    This routine returns the maximum number of file descriptors that are
    supported.

Arguments:

    None.

Return Value:

    Returns the maximum number of file descriptors that one process can have
    open.

--*/

LIBC_API
int
getpagesize (
    void
    );

/*++

Routine Description:

    This routine returns the number of bytes in the basic unit of memory
    allocation on the current machine, the page. This routine is provided for
    historical reasons, new applications should use sysconf(_SC_PAGESIZE)
    (which is in fact exactly what this routine turns around and does).

Arguments:

    None.

Return Value:

    Returns the number of bytes in a memory page.

--*/

LIBC_API
unsigned int
alarm (
    unsigned int Seconds
    );

/*++

Routine Description:

    This routine converts causes the system to generate a SIGALRM signal for
    the process after the number of realtime seconds specified by the given
    parameter have elapsed. Processor scheduling delays may prevent the process
    from handling the signal as soon as it is generated. Alarm requests are not
    stacked; only one SIGALRM generation can be scheduled in this manner. If
    the SIGALRM signal has not yet been generated, the call shall result in
    rescheduling the time at which the SIGALRM signal is generated.

Arguments:

    Seconds - Supplies the number of seconds from now that the alarm should
        fire in. If this value is 0, then a pending alarm request, if any, is
        canceled.

Return Value:

    If there is a previous alarm request with time remaining, then the
    (non-zero) number of seconds until the alarm would have signaled is
    returned.

    0 otherwise. The specification for this function says that it cannot fail.
    In reality, it might, and errno should be checked if 0 is returned.

--*/

LIBC_API
unsigned
sleep (
    unsigned Seconds
    );

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given number of realtime seconds has elapsed or a signal was delivered.

Arguments:

    Seconds - Supplies the number of seconds to sleep for.

Return Value:

    None.

--*/

LIBC_API
int
usleep (
    useconds_t Microseconds
    );

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given number of realtime microseconds has elapsed or a signal was delivered.

Arguments:

    Microseconds - Supplies the number of microseconds to sleep for.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
access (
    const char *Path,
    int Mode
    );

/*++

Routine Description:

    This routine checks the given path for accessibility using the real user
    ID.

Arguments:

    Path - Supplies the path string of the file to get the accessibility
        information for.

    Mode - Supplies the mode bits the caller is interested in. Valid values are
        F_OK to check if the file exists, R_OK to check if the file is readable,
        W_OK to check if the file is writable, and X_OK to check if the file is
        executable.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
faccessat (
    int Directory,
    const char *Path,
    int Mode,
    int Flags
    );

/*++

Routine Description:

    This routine checks the given path for accessibility using the real user
    ID and real group ID rather than the effective user and group IDs.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies the path string of the file to get the accessibility
        information for.

    Mode - Supplies the mode bits the caller is interested in. Valid values are
        F_OK to check if the file exists, R_OK to check if the file is readable,
        W_OK to check if the file is writable, and X_OK to check if the file is
        executable.

    Flags - Supplies a bitfield of flags. Supply AT_EACCESS if the checks for
        accessibility should be performed using the effective user and group
        ID rather than the real user and group ID.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
char *
ttyname (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine returns the null-terminated pathname of the terminal
    associated with the given file descriptor. This function is neither
    reentrant nor thread safe.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

Return Value:

    Returns a pointer to the supplied buffer on success. This buffer may be
    overwritten by subsequent calls to this routine.

    NULL on failure and errno will be set to contain more information. Common
    error values are:

    EBADF if the file descriptor is not valid.

    ENOTTY if the file descriptor is not a terminal.

    ENOMEM if not enough memory was available.

--*/

LIBC_API
char *
ttyname_r (
    int FileDescriptor,
    char *Name,
    size_t NameSize
    );

/*++

Routine Description:

    This routine returns the null-terminated pathname of the terminal
    associated with the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

    Name - Supplies a pointer to the buffer where the name will be returned
        on success.

    NameSize - Supplies the size of the name buffer in bytes.

Return Value:

    Returns a pointer to the supplied buffer on success.

    NULL on failure and errno will be set to contain more information. Common
    error values are:

    EBADF if the file descriptor is not valid.

    ENOTTY if the file descriptor is not a terminal.

    ERANGE if the supplied buffer was not large enough.

--*/

LIBC_API
int
tcsetpgrp (
    int FileDescriptor,
    pid_t ProcessGroupId
    );

/*++

Routine Description:

    This routine sets the foreground process group ID associated with the
    given terminal file descriptor. The application shall ensure that the file
    associated with the given descriptor is the controlling terminal of the
    calling process and the controlling terminal is currently associated with
    the session of the calling process. The application shall ensure that the
    given process group ID is led by a process in the same session as the
    calling process.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

    ProcessGroupId - Supplies the process group ID to set for the terminal.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

LIBC_API
pid_t
tcgetpgrp (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine returns the value of the process group ID of the foreground
    process associated with the given terminal. If ther is no foreground
    process group, this routine returns a value greater than 1 that does not
    match the process group ID of any existing process group.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

Return Value:

    Returns the process group ID of the foreground process associated with the
    terminal on success.

    -1 on failure, and errno will be set to contain more information. Possible
    values of errno are:

    EBADF if the file descriptor is invalid.

    ENOTTY if the calling process does not having a controlling terminal, or
    the file is not the controlling terminal.

--*/

LIBC_API
int
gethostname (
    char *Name,
    size_t NameLength
    );

/*++

Routine Description:

    This routine returns the network host name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
int
getdomainname (
    char *Name,
    size_t NameLength
    );

/*++

Routine Description:

    This routine returns the network domain name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
int
sethostname (
    const char *Name,
    size_t Size
    );

/*++

Routine Description:

    This routine sets the network host name for the current machine.

Arguments:

    Name - Supplies a pointer to the new name to set.

    Size - Supplies the size of the name, not including a null terminator.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
int
setdomainname (
    const char *Name,
    size_t Size
    );

/*++

Routine Description:

    This routine sets the network domain name for the current machine.

Arguments:

    Name - Supplies a pointer to the new name to set.

    Size - Supplies the size of the name, not including a null terminator.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
void
swab (
    const void *Source,
    void *Destination,
    ssize_t ByteCount
    );

/*++

Routine Description:

    This routine copies bytes from a source buffer to a destination, exchanging
    adjacent bytes. The source and destination buffers should not overlap.

Arguments:

    Source - Supplies the source buffer.

    Destination - Supplies the destination buffer (which should not overlap
        with the source buffer).

    ByteCount - Supplies the number of bytes to copy. This should be even. If
        it is odd, the byte count will be truncated down to an even boundary,
        so the last odd byte will not be copied.

Return Value:

    None.

--*/

LIBCRYPT_API
char *
crypt (
    const char *Key,
    const char *Salt
    );

/*++

Routine Description:

    This routine encrypts a user's password using various encryption/hashing
    standards. The default is DES, which is fairly weak and subject to
    dictionary attacks.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies a two character salt to use to perterb the results. If this
        string starts with a $ and a number, alternate hashing algorithms are
        selected. The format is $id$salt$encrypted. ID can be 1 for MD5, 5 for
        SHA-256, or 6 for SHA-512.

Return Value:

    Returns a pointer to the encrypted password (plus ID and salt information
    in cases where an alternate mechanism is used). This is a static buffer,
    which may be overwritten by subsequent calls to crypt.

--*/

LIBC_API
char *
getpass (
    const char *Prompt
    );

/*++

Routine Description:

    This routine reads outputs the given prompt, and reads in a line of input
    without echoing it. This routine attempts to use the process' controlling
    terminal, or stdin/stderr otherwise.

Arguments:

    Prompt - Supplies a pointer to the prompt to print.

Return Value:

    Returns a pointer to the entered input on success. If this is a password,
    the caller should be sure to clear this buffer out as soon as possible.

    NULL on failure.

--*/

LIBC_API
char *
getusershell (
    void
    );

/*++

Routine Description:

    This routine returns the next permitted user shell in the database of
    valid shells. This opens the file if necessary. This routine is neither
    thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the next shell on success. This
    buffer may be overwritten by subsequent calls to getusershell.

    NULL on failure or end-of-database.

--*/

LIBC_API
void
setusershell (
    void
    );

/*++

Routine Description:

    This routine rewinds the user shells database back to the beginning.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
endusershell (
    void
    );

/*++

Routine Description:

    This routine closes the permitted user shells database.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
int
getopt (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options
    );

/*++

Routine Description:

    This routine parses command line arguments, successively returning each
    passed in argument. This routine is neither reentrant nor thread safe.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    Options - Supplies a pointer to a null terminated string containing the set
        of accepted options. Each character represents an allowed option. If
        a character is followed by a colon ':', then that option takes an
        argument. If an option with an argument is found and there are more
        characters in the current string, then the remainder of that string
        will be returned. Otherwise, the next argument will be returned. If
        there is no next argument, that's considered an error.

Return Value:

    Returns the next option character on success. The global variable optind
    will be updated to reflect the index of the next argument to be processed.
    It will be initialied by the system to 1. If the option takes an argument,
    the global variable optarg will be set to point at that argument.

    Returns '?' if the option found is not in the recognized option string. The
    optopt global variable will be set to the unrecognized option that resulted
    in this condition. The '?' character is also returned if the options string
    does not begin with a colon ':' and a required argument is not found. If the
    opterr global variable has not been set to 0 by the user, then an error
    will be printed to standard error.

    Returns ':' if the options string begins with a colon and a required
    argument is missing. If the opterr global variable has not been set to 0 by
    the user, then an error will be printed to standard error.

    -1 if a non-option was encountered. In this case the optind global variable
    will be set to the first non-option argument.

--*/

LIBC_API
int
nice (
    int Increment
    );

/*++

Routine Description:

    This routine adds the given value to the current process' nice value. A
    process' nice value is a non-negative number for which a more positive
    value results in less favorable scheduling. Valid nice values are between
    0 and 2 * NZERO - 1.

Arguments:

    Increment - Supplies the increment to add to the current nice value.

Return Value:

    Returns the new nice value minus NZERO. Note that this can result in a
    successful return value of -1. Callers checking for errors should set
    errno to 0 before calling this function, then check errno after.

    -1 on failure, and errno will be set to indicate more information. This may
    fail with EPERM if the increment is negative and the caller does not have
    appropriate privileges.

--*/

#ifdef __cplusplus

}

#endif
#endif

