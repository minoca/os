/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    errno.h

Abstract:

    This header contains definitions for standard error numbers.

Author:

    Evan Green 11-Mar-2013

--*/

#ifndef _ERRNO_H
#define _ERRNO_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define errno as a macro so that the test for errno being defined succeeds.
//

#define errno errno

//
// Define standard error numbers.
//

//
// Operation not permitted
//

#define EPERM 1

//
// No such file or directory
//

#define ENOFILE 2
#define ENOENT 2

//
// No such process
//

#define ESRCH 3

//
// Interrupted system call
//

#define EINTR 4

//
// I/O error
//

#define EIO 5

//
// No such device or address
//

#define ENXIO 6

//
// Argument list too long
//

#define E2BIG 7

//
// Exec format error
//

#define ENOEXEC 8

//
// Bad file descriptor
//

#define EBADF 9

//
// No child processes
//

#define ECHILD 10

//
// Try again
//

#define EAGAIN 11

//
// Out of memory
//

#define ENOMEM 12

//
// Permission denied
//

#define EACCES 13

//
// Bad address
//

#define EFAULT 14

//
// Block device required
//

#define ENOTBLK 15

//
// Device or resource busy
//

#define EBUSY 16

//
// File exists
//

#define EEXIST 17

//
// Improper cross device link
//

#define EXDEV 18

//
// No such device
//

#define ENODEV 19

//
// Not a directory
//

#define ENOTDIR 20

//
// Is a directory
//

#define EISDIR 21

//
// Invalid argument
//

#define EINVAL 22

//
// Too many open files in the system
//

#define ENFILE 23

//
// Too many open files
//

#define EMFILE 24

//
// Inappropriate I/O control operation (not a typewriter)
//

#define ENOTTY 25

//
// Text file busy
//

#define ETXTBSY 26

//
// File too large
//

#define EFBIG 27

//
// No space left on device
//

#define ENOSPC 28

//
// Illegal seek (on a pipe)
//

#define ESPIPE 29

//
// Read-only file system
//

#define EROFS 30

//
// Too many links
//

#define EMLINK 31

//
// Broken pipe
//

#define EPIPE 32

//
// Numerical argument out of domain
//

#define EDOM 33

//
// Numerical result out of range
//

#define ERANGE 34

//
// Resource deadlock would occur
//

#define EDEADLOCK 35
#define EDEADLK 35

//
// File name too long
//

#define ENAMETOOLONG 36

//
// No record locks available
//

#define ENOLCK 37

//
// Function not implemented
//

#define ENOSYS 38

//
// Directory not empty
//

#define ENOTEMPTY 39

//
// Too many symbolic links encountered
//

#define ELOOP 40

//
// Operation would block
//

#define EWOULDBLOCK EAGAIN

//
// No message of desired type
//

#define ENOMSG 42

//
// Identifier removed
//

#define EIDRM 43

//
// Operation not supported.
//

#define ENOTSUP 44

//
// The previous owner died.
//

#define EOWNERDEAD 45

//
// The state is not recoverable.
//

#define ENOTRECOVERABLE 46

//
// Device not a stream
//

#define ENOSTR 47

//
// No data available
//

#define ENODATA 48

//
// Timer expired
//

#define ETIME 49

//
// Out of streams resources
//

#define ENOSR 50

//
// Link has been severed
//

#define ENOLINK 51

//
// Protocol error
//

#define EPROTO 52

//
// Multihop attempted
//

#define EMULTIHOP 53

//
// Not a data message
//

#define EBADMSG 54

//
// Value too large for defined data type
//

#define EOVERFLOW 55

//
// Illegal byte sequence
//

#define EILSEQ 56

//
// Socket operation on non-socket
//

#define ENOTSOCK 57

//
// Destination address required
//

#define EDESTADDRREQ 58

//
// Message too long
//

#define EMSGSIZE 59

//
// Protocol wrong type for socket
//

#define EPROTOTYPE 60

//
// Protocol not available
//

#define ENOPROTOOPT 61

//
// Protocol not supported
//

#define EPROTONOSUPPORT 62

//
// Operation not supported on transport endpoint
//

#define EOPNOTSUPP 63

//
// Address family not supported by protocol
//

#define EAFNOSUPPORT 64

//
// Address already in use
//

#define EADDRINUSE 65

//
// Cannot assign requested address
//

#define EADDRNOTAVAIL 66

//
// Network is down
//

#define ENETDOWN 67

//
// Network is unreachable
//

#define ENETUNREACH 68

//
// Network dropped connection on reset
//

#define ENETRESET 69

//
// Software caused connection abort
//

#define ECONNABORTED 70

//
// Connection reset
//

#define ECONNRESET 71

//
// No buffer space available
//

#define ENOBUFS 72

//
// Transport endpoint already connected
//

#define EISCONN 73

//
// Transport endpoint is not connected
//

#define ENOTCONN 74

//
// Connection timed out
//

#define ETIMEDOUT 75

//
// Connection refused
//

#define ECONNREFUSED 76

//
// No route to host
//

#define EHOSTUNREACH 77

//
// Operation already in progress
//

#define EALREADY 78

//
// Operation now in progress
//

#define EINPROGRESS 79

//
// Stale NFS handle
//

#define ESTALE 80

//
// Quota exceeded
//

#define EDQUOT 81

//
// Operation canceled
//

#define ECANCELED 82

//
// Protocol family not supported
//

#define EPFNOSUPPORT 83

//
// Cannot send after endpoint shutdown.
//

#define ESHUTDOWN 84

//
// Host is down.
//

#define EHOSTDOWN 85

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the last error number global.
//

LIBC_API extern __THREAD int errno;

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

