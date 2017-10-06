/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    limits.h

Abstract:

    This header contains implementation-defined constants for various system
    limits.

Author:

    Evan Green 11-Mar-2013

--*/

#ifndef _LIMITS_H
#define _LIMITS_H

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
// Define the number of bits in a char.
//

#define CHAR_BIT __CHAR_BIT__

//
// Define the minimum and maximum values a signed char can hold.
//

#define SCHAR_MAX __SCHAR_MAX__
#define SCHAR_MIN (-SCHAR_MAX - 1)

//
// Define the maximum value an unsigned char can hold.
//

#define UCHAR_MAX (SCHAR_MAX * 2U + 1U)

//
// Define the minimum and maximum a char can hold.
//

#ifdef __CHAR_UNSIGNED__

#define CHAR_MIN 0U
#define CHAR_MAX UCHAR_MAX

#else

#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

#endif

//
// Define the minimum and maximum values a signed short int can hold.
//

#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-SHRT_MAX - 1)

//
// Define the maximum value an unsigned short int can hold.
//

#define USHRT_MAX (SHRT_MAX * 2U + 1U)

//
// Define the minimum and maximum values a signed int can hold.
//

#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX - 1)

//
// Define the maximum value an unsigned int can hold.
//

#define UINT_MAX (INT_MAX * 2U + 1U)

//
// Define the minimum and maximum values a signed long int can hold.
//

#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1L)

//
// Define the maximum value an unsigned long int can hold.
//

#define ULONG_MAX (LONG_MAX * 2UL + 1UL)

//
// Define the minimum and maximum values a signed long long int can hold.
//

#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define LONG_LONG_MAX __LONG_LONG_MAX__
#define LONG_LONG_MIN (-LONG_LONG_MAX - 1LL)

//
// Define the maximum value of an unsigned long long int.
//

#define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)
#define ULONG_LONG_MAX (LONG_LONG_MAX * 2ULL + 1ULL)

//
// Define the maximum value an ssize_t can hold.
//

#define SSIZE_MAX LONG_MAX

//
// Define the maximum number of bytes in a multibyte character sequence.
//

#define MB_LEN_MAX 16

//
// Define the number of links a single file may have.
//

#define LINK_MAX 127

//
// Define the maximum number of bytes in a terminal canonical line.
//

#define MAX_CANON 511

//
// Define the minimum number of bytes for which space is available in a
// terminal input queue. This is the maximum number of bytes a conforming
// application may require to be typed as input before reading them.
//

#define MAX_INPUT 511

//
// Define the maximum length of a file, not including the null terminator.
//

#define NAME_MAX 255

//
// Define the maximum length of a file path, including the null terminator.
//

#define PATH_MAX 4096

//
// Define the maximum number of bytes that is guaranteed to be atomic when
// writing to a pipe.
//

#define PIPE_BUF 4096

//
// Define the maximum length of a user name, including the null terminator.
//

#define LOGIN_NAME_MAX 256

//
// Define the maximum length of the local host name.
//

#define HOST_NAME_MAX 80

//
// Define the maximum number of simultaneous supplemental group IDs a process
// can have.
//

#define NGROUPS_MAX 65536

//
// Define the maximum number of realtime signals reserved for application use
// in this implementation.
//

#define RTSIG_MAX 32

//
// Define the maximum number of IO vectors that can be passed to the vectored
// IO functions.
//

#define IOV_MAX 1024

//
// Define POSIX minimum requirements.
//

//
// Define the number of I/O operations that can be specified in a list I/O
// call.
//

#define _POSIX_AIO_LISTIO_MAX 2

//
// Define the number of outstanding asynchronous I/O operations.
//

#define _POSIX_AIO_MAX 1

//
// Define the maximum length of an argument to the exec functions, including
// environment data.
//

#define _POSIX_ARG_MAX 4096

//
// Define the maximum number of simultaneous processes per real user ID.
//

#define _POSIX_CHILD_MAX 25

//
// Define the number of timer expiration overruns.
//

#define _POSIX_DELAYTIMER_MAX 32

//
// Define the maximum length of a hostname (not including the null terminator
// as returned from the gethostname function.
//

#define _POSIX_HOST_NAME_MAX 255

//
// Define the maximum number of links to a single file.
//

#define _POSIX_LINK_MAX 8

//
// Define the size of storage required for a login name, in bytes, including
// the null terminator.
//

#define _POSIX_LOGIN_NAME_MAX 9

//
// Define the maximum number of bytes in a terminal canonical input queue.
//

#define _POSIX_MAX_CANON 255

//
// Define the maximum number of bytes allowed in a terminal input queue.
//

#define _POSIX_MAX_INPUT 255

//
// Define the number of message queues that can be open for a single process.
//

#define _POSIX_MQ_OPEN_MAX 8

//
// Define the maximum number of message priorities supported by the
// implementation.
//

#define _POSIX_MQ_PRIO_MAX 32

//
// Define the maximum number of bytes in a file name, not including the null
// terminator.
//

#define _POSIX_NAME_MAX 14

//
// Define the number of simultaneous supplementary group IDs per process.
//

#define _POSIX_NGROUPS_MAX 8

//
// Define the maximum number of files that one process can have open at any
// one time.
//

#define _POSIX_OPEN_MAX 20

//
// Define the maximum number of bytes in a pathname.
//

#define _POSIX_PATH_MAX 255

//
// Define the maximum number of bytes that is guarantted to be atomic when
// writing to a pipe.
//

#define _POSIX_PIPE_BUF 512

//
// Define the number of repeated occurrences of a basic regular expression
// permitted by the regexec and regcomp functions when using the interval
// notation \{m,n\}.
//

#define _POSIX_RE_DUP_MAX 255

//
// Define the number of realtime signals reserved for application use.
//

#define _POSIX_RTSIG_MAX 8

//
// Define the number of semaphores a process may have.
//

#define _POSIX_SEM_NSEMS_MAX 256

//
// Define the maximum value a semaphore may have.
//

#define _POSIX_SEM_VALUE_MAX 32767

//
// Define the number of queued signals that a process may send and have pending
// at the receiver(s) at any time.
//

#define _POSIX_SIGQUEUE_MAX 32

//
// Define the value that can be stored in an object of type ssize_t.
//

#define _POSIX_SSIZE_MAX 32767

//
// Define the number of streams that one process can have open at one time.
//

#define _POSIX_STREAM_MAX 8

//
// Define the number of bytes in a symlink file.
//

#define _POSIX_SYMLINK_MAX 255

//
// Define the numer of symbolic links that can be traversed in the resolution
// of a pathname in the absence of a loop.
//

#define _POSIX_SYMLOOP_MAX 8

//
// Define the per-process number of timers.
//

#define _POSIX_TIMER_MAX 32

//
// Define the size of the storage required for a terminal device name,
// including the null terminator.
//

#define _POSIX_TTY_NAME_MAX 9

//
// Define the maximum number of bytes supported for the name of a timezone
// (not of the TZ variable).
//

#define _POSIX_TZNAME_MAX 6

//
// Define the minimum number of attempts made to destory a thread's
// thread-specific data values on thread exit.
//

#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS 4

//
// Define the maximum number of keys that can be made per process.
//

#define _POSIX_THREAD_KEYS_MAX 128

//
// Define the maximum number of threads per process.
//

#define _POSIX_THREAD_THREADS_MAX 64

//
// Define the maximum number of bytes in a filename.
//

#define _XOPEN_NAME_MAX 255

//
// Define the maximum number of bytes in a pathname.
//

#define _XOPEN_PATH_MAX 1024

//
// Define the maximum "obase" values allowed by the bc utility.
//

#define _POSIX2_BC_BASE_MAX 99

//
// Define the maximum number of elements allowed in an array by the bc utility.
//

#define _POSIX2_BC_DIM_MAX 2048

//
// Define the maximum scale allowed by the bc utility.
//

#define _POSIX2_BC_SCALE_MAX 99

//
// Define the maximum number of bytes in a character class name.
//

#define _POSIX2_CHARCLASS_NAME_MAX 14

//
// Define the maximum length of a string constant accepted by the bc utility.
//

#define _POSIX2_BC_STRING_MAX 1000

//
// Define the maximum number of weights that can be assigned to an entry of the
// LC_COLLAGE order keybowrd in the locale definition file.
//

#define _POSIX2_COLL_WEIGHTS_MAX 2

//
// Define the maximum number of expressions that can be nested within
// parentheses by the expr utility.
//

#define _POSIX2_EXPR_NEST_MAX 32

//
// Define the maximum length of a utility's input line (either standard input
// or another file), when the utility is described as processing text. The
// length includes room for the trailing newline.
//

#define _POSIX2_LINE_MAX 2048

//
// Define the maximum number of repeated occurrences of a regular expression
// permitted when using the interval notation \{m,n\}.
//

#define _POSIX2_RE_DUP_MAX 255

//
// Define reasonable limits for some required definitions. There is no actual
// limit.
//

//
// Define the maximum value of "digit" in calls to printf and scanf functions.
//

#define NL_ARGMAX _POSIX_ARG_MAX

//
// Define the number of bytes in a "lang" name.
//

#define NL_LANGMAX _POSIX2_LINE_MAX

//
// Define the maximum message number.
//

#define NL_MSGMAX INT_MAX

//
// Define the maximum number of bytes in an N-to-1 collation mapping.
//

#define NL_NMAX INT_MAX

//
// Define the maximum set number.
//

#define NL_SETMAX INT_MAX

//
// Define the maximum nubmer of bytes in a message.
//

#define NL_TEXTMAX INT_MAX

//
// Define the default process priority.
//

#define NZERO 20

//
// Define the maximum number of loops attempted to clean thread keys.
//

#define PTHREAD_DESTRUCTOR_ITERATIONS 4

//
// Define the maximum number of thread keys supported.
//

#define PTHREAD_KEYS_MAX 128

//
// Define the maximum number of threads per process.
//

#define PTHREAD_THREADS_MAX 2048

//
// Define the minimum stack size for a new thread.
//

#define PTHREAD_STACK_MIN 0x1000

//
// Define the maximum value of a semaphore.
//

#define SEM_VALUE_MAX 0x3FFFFFFF

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

