/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    libcp.h

Abstract:

    This header contains internal definitions for the Minoca C Library.

Author:

    Evan Green 4-Mar-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#define LIBC_API __DLLEXPORT

#include <minoca/lib/minocaos.h>
#include <libcbase.h>
#include <minoca/lib/mlibc.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <wchar.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro non-destructively sets the orientation of the given stream.
//

#define ORIENT_STREAM(_Stream, _Orientation)                      \
    if (((_Stream)->Flags & FILE_FLAG_ORIENTATION_MASK) == 0) {   \
        (_Stream)->Flags |= (_Orientation);                        \
    }

//
// This macro zeros memory and ensures that the compiler doesn't optimize away
// the memset.
//

#define SECURITY_ZERO(_Buffer, _Size)                                       \
    {                                                                       \
        memset((_Buffer), 0, (_Size));                                      \
        *(volatile char *)(_Buffer) = *((volatile char *)(_Buffer) + 1);    \
    }

//
// This macro asserts that the file permission bits are equivalent.
//

#define ASSERT_FILE_PERMISSIONS_EQUIVALENT()                \
    assert((FILE_PERMISSION_USER_READ == S_IRUSR) &&        \
           (FILE_PERMISSION_USER_WRITE == S_IWUSR) &&       \
           (FILE_PERMISSION_USER_EXECUTE == S_IXUSR) &&     \
           (FILE_PERMISSION_GROUP_READ == S_IRGRP) &&       \
           (FILE_PERMISSION_GROUP_WRITE == S_IWGRP) &&      \
           (FILE_PERMISSION_GROUP_EXECUTE == S_IXGRP) &&    \
           (FILE_PERMISSION_OTHER_READ == S_IROTH) &&       \
           (FILE_PERMISSION_OTHER_WRITE == S_IWOTH) &&      \
           (FILE_PERMISSION_OTHER_EXECUTE == S_IXOTH) &&    \
           (FILE_PERMISSION_SET_USER_ID == S_ISUID) &&      \
           (FILE_PERMISSION_SET_GROUP_ID == S_ISGID))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define internal file flags.
//

#define FILE_FLAG_UNGET_VALID       0x00000001
#define FILE_FLAG_END_OF_FILE       0x00000002
#define FILE_FLAG_ERROR             0x00000004
#define FILE_FLAG_BYTE_ORIENTED     0x00000008
#define FILE_FLAG_WIDE_ORIENTED     0x00000010
#define FILE_FLAG_READ_LAST         0x00000020
#define FILE_FLAG_DISABLE_LOCKING   0x00000040
#define FILE_FLAG_BUFFER_ALLOCATED  0x00000080
#define FILE_FLAG_STANDARD_IO       0x00000100
#define FILE_FLAG_CAN_READ          0x00000200

#define FILE_FLAG_ORIENTATION_MASK \
    (FILE_FLAG_BYTE_ORIENTED | FILE_FLAG_WIDE_ORIENTED)

//
// Define the maximum size of a passwd or group line/data buffer.
//

#define USER_DATABASE_LINE_MAX 1024

//
// Define the internal signal number used for thread cancellation.
//

#define SIGNAL_PTHREAD 32

//
// Define the internal signal number used for set ID requests.
//

#define SIGNAL_SETID 33

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about an open file stream.

Members:

    ListEntry - Stores pointers to the next and previous streams in the global
        list.

    Descriptor - Stores the file descriptor number.

    OpenFlags - Stores the flags the file was opened with.

    Flags - Stores internal flags. See FILE_FLAG_* definitions.

    Lock - Stores the stream lock.

    BufferMode - Stores the buffering mode for this file. This value is either
        _IOFBF for fully buffered, _IOLBF for line buffered, or _IONBF for
        non-buffered.

    Buffer - Stores a pointer to the file buffer.

    BufferSize - Stores the size of the file buffer in bytes.

    BufferValidSize - Stores the number of bytes in the buffer that actually
        have good data in them.

    BufferNextIndex - Stores the index into the buffer where the next read or
        write will occur.

    UngetCharacter - Stores the unget character.

    Pid - Stores the process ID of the process if the stream was opened with
        the popen routine.

    ShiftState - Stores the current multi-byte shift state.

--*/

typedef struct _FILE {
    LIST_ENTRY ListEntry;
    ULONG Descriptor;
    ULONG OpenFlags;
    ULONG Flags;
    pthread_mutex_t Lock;
    ULONG BufferMode;
    PCHAR Buffer;
    ULONG BufferSize;
    ULONG BufferValidSize;
    ULONG BufferNextIndex;
    WCHAR UngetCharacter;
    pid_t Pid;
    mbstate_t ShiftState;
} *PFILE;

/*++

Structure Description:

    This structure defines a C library type conversion interface.

Members:

    ListEntry - Stores the interfaces list entry into the global list of type
        conversion interfaces.

    Type - Stores the conversion type of the interface.

    Buffer - Stores an opaque pointer to the interface buffer.

    Network - Stores a pointer to the network type conversion interface.

--*/

typedef struct _CL_TYPE_CONVERSION_INTERFACE {
    LIST_ENTRY ListEntry;
    CL_CONVERSION_TYPE Type;
    union {
      PVOID Buffer;
      PCL_NETWORK_CONVERSION_INTERFACE Network;
    } Interface;

} CL_TYPE_CONVERSION_INTERFACE, *PCL_TYPE_CONVERSION_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global list of type conversion interfaces, protected by a global
// lock.
//

extern LIST_ENTRY ClTypeConversionInterfaceList;
extern pthread_mutex_t ClTypeConversionInterfaceLock;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
ClpInitializeSignals (
    VOID
    );

/*++

Routine Description:

    This routine initializes signal handling functionality for the C library.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
ClpInitializeFileIo (
    VOID
    );

/*++

Routine Description:

    This routine initializes the file I/O subsystem of the C library.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ClpInitializeTypeConversions (
    VOID
    );

/*++

Routine Description:

    This routine initializes the type conversion subsystem of the C library.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ClpInitializeEnvironment (
    VOID
    );

/*++

Routine Description:

    This routine initializes the environment variable support in the C library.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ClpLockStream (
    FILE *Stream
    );

/*++

Routine Description:

    This routine locks the file stream.

Arguments:

    Stream - Supplies a pointer to the stream to lock.

Return Value:

    None.

--*/

BOOL
ClpTryToLockStream (
    FILE *Stream
    );

/*++

Routine Description:

    This routine makes a single attempt at locking the file stream. If locking
    is disabled on the stream, this always returns TRUE.

Arguments:

    Stream - Supplies a pointer to the stream to try to lock.

Return Value:

    TRUE if the lock was successfully acquired.

    FALSE if the lock was not successfully acquired.

--*/

VOID
ClpUnlockStream (
    FILE *Stream
    );

/*++

Routine Description:

    This routine unlocks the file stream.

Arguments:

    Stream - Supplies a pointer to the stream to unlock.

Return Value:

    None.

--*/

VOID
ClpFlushAllStreams (
    BOOL AllUnlocked,
    PFILE UnlockedStream
    );

/*++

Routine Description:

    This routine flushes every stream in the application.

Arguments:

    AllUnlocked - Supplies a boolean that if TRUE flushes every stream without
        acquiring the file lock. This is used during aborts.

    UnlockedStream - Supplies a specific stream that when flushed should be
        flushed unlocked.

Return Value:

    None.

--*/

VOID
ClpInitializeTimeZoneSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for time zones.

Arguments:

    None.

Return Value:

    None.

--*/

time_t
ClpConvertSystemTimeToUnixTime (
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts the given system time structure into a time_t time
    value. Fractional seconds are truncated.

Arguments:

    SystemTime - Supplies a pointer to the system time structure.

Return Value:

    Returns the time_t value corresponding to the given system time.

--*/

VOID
ClpConvertUnixTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    time_t UnixTime
    );

/*++

Routine Description:

    This routine converts the given time_t value into a system time structure.
    Fractional seconds in the system time structure are set to zero.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    UnixTime - Supplies the time to set.

Return Value:

    None.

--*/

VOID
ClpConvertTimeValueToSystemTime (
    PSYSTEM_TIME SystemTime,
    const struct timeval *TimeValue
    );

/*++

Routine Description:

    This routine converts the given time value into a system time structure.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    TimeValue - Supplies a pointer to the time value structure to be converted
        to system time.

Return Value:

    None.

--*/

VOID
ClpConvertSpecificTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    const struct timespec *SpecificTime
    );

/*++

Routine Description:

    This routine converts the given specific time into a system time structure.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    SpecificTime - Supplies a pointer to the specific time structure to be
        converted to system time.

Return Value:

    None.

--*/

VOID
ClpConvertCounterToTimeValue (
    ULONGLONG Counter,
    ULONGLONG Frequency,
    struct timeval *TimeValue
    );

/*++

Routine Description:

    This routine converts a tick count at a known frequency into a time value
    structure, rounded up to the nearest microsecond.

Arguments:

    Counter - Supplies the counter value in ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    TimeValue - Supplies a pointer where the time value equivalent will be
        returned.

Return Value:

    None.

--*/

VOID
ClpConvertTimeValueToCounter (
    PULONGLONG Counter,
    ULONGLONG Frequency,
    const struct timeval *TimeValue
    );

/*++

Routine Description:

    This routine converts a time value into a tick count at a known frequency,
    rounded up to the nearest tick.

Arguments:

    Counter - Supplies a pointer that receives the calculated counter value in
        ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    TimeValue - Supplies a pointer to the time value.

Return Value:

    None.

--*/

VOID
ClpConvertCounterToSpecificTime (
    ULONGLONG Counter,
    ULONGLONG Frequency,
    struct timespec *SpecificTime
    );

/*++

Routine Description:

    This routine converts a tick count at a known frequency into a specific
    time structure, rounded up to the nearest nanosecond.

Arguments:

    Counter - Supplies the counter value in ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    SpecificTime - Supplies a pointer where the specific time equivalent will
        be returned.

Return Value:

    None.

--*/

VOID
ClpConvertSpecificTimeToCounter (
    PULONGLONG Counter,
    ULONGLONG Frequency,
    const struct timespec *SpecificTime
    );

/*++

Routine Description:

    This routine converts a specific time into a tick count at a known
    frequency, rounded up to the nearest tick.

Arguments:

    Counter - Supplies a pointer that receives the calculated counter value in
        ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    SpecificTime - Supplies a pointer to the specific time.

Return Value:

    None.

--*/

INT
ClpConvertSpecificTimeoutToSystemTimeout (
    const struct timespec *SpecificTimeout,
    PULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine converts the given specific timeout into a system timeout in
    milliseconds. The specific timeout's seconds and nanoseconds must not be
    negative and the nanoseconds must not be greater than 1 billion (the number
    of nanoseconds in a second). If the specific timeout is NULL, then the
    timeout in milliseconds will be set to an indefinite timeout.

Arguments:

    SpecificTimeout - Supplies an optional pointer to the specific timeout.

    TimeoutInMilliseconds - Supplies a pointer that receives the system timeout
        in milliseconds.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
ClpConvertResourceUsage (
    PRESOURCE_USAGE KernelUsage,
    ULONGLONG Frequency,
    struct rusage *LibraryUsage
    );

/*++

Routine Description:

    This routine converts a kernel resource usage structure into a struct
    rusage.

Arguments:

    KernelUsage - Supplies a a pointer to the structure to convert.

    Frequency - Supplies the frequency of the processor for converting cycle
        counts.

    LibraryUsage - Supplies a pointer where the library usage structure will
        be returned.

Return Value:

    None.

--*/

VOID
ClpSetThreadIdentityOnAllThreads (
    ULONG Fields,
    PTHREAD_IDENTITY Identity
    );

/*++

Routine Description:

    This routine uses a signal to set the thread identity on all threads
    except the current one (which is assumed to have already been set).

Arguments:

    Fields - Supplies the bitfield of identity fields to set. See
        THREAD_IDENTITY_FIELD_* definitions.

    Identity - Supplies a pointer to the thread identity information to set.

Return Value:

    None.

--*/

VOID
ClpSetSupplementaryGroupsOnAllThreads (
    PGROUP_ID GroupIds,
    UINTN GroupIdCount
    );

/*++

Routine Description:

    This routine uses a signal to set the supplementary groups on all threads
    except the current one (which is assumed to have already been set).

Arguments:

    GroupIds - Supplies a pointer to the array of group IDs to set.

    GroupIdCount - Supplies the number of elements in the group ID array.

Return Value:

    None.

--*/

void
ClpUnregisterAtfork (
    void *DynamicObjectHandle
    );

/*++

Routine Description:

    This routine unregisters any at-fork handlers registered with the given
    dynamic object handle.

Arguments:

    DynamicObjectHandle - Supplies the value unique to the dynamic object. All
        handlers registered with this same value will be removed.

Return Value:

    None.

--*/

VOID
ClpRunAtforkPrepareRoutines (
    VOID
    );

/*++

Routine Description:

    This routine calls the prepare routine for any fork handlers.

Arguments:

    None.

Return Value:

    None. This function returns with the at-fork mutex held.

--*/

VOID
ClpRunAtforkChildRoutines (
    VOID
    );

/*++

Routine Description:

    This routine calls the child routine for any fork handlers. This routine
    must obviously be called from a newly forked child. This function assumes
    that the at-fork mutex is held, and re-initializes it.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ClpRunAtforkParentRoutines (
    VOID
    );

/*++

Routine Description:

    This routine calls the child routine for any fork handlers. This routine
    must obviously be called from a newly forked child. This function assumes
    that the at-fork mutex is held, and releases it.

Arguments:

    None.

Return Value:

    None.

--*/

PSTR
ClpGetFqdn (
    VOID
    );

/*++

Routine Description:

    This routine returns a null terminated string containing the fully
    qualified domain name of the machine.

Arguments:

    None.

Return Value:

    Returns a null terminated string containing nodename.domainname on success.
    The caller is responsible for freeing this string.

    NULL on allocation failure.

--*/

int
ClpSetSignalAction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    );

/*++

Routine Description:

    This routine sets a new signal action for the given signal number.

Arguments:

    SignalNumber - Supplies the signal number that will be affected.

    NewAction - Supplies an optional pointer to the new signal action to
        perform upon receiving that signal. If this pointer is NULL, then no
        change will be made to the signal's action.

    OriginalAction - Supplies a pointer where the original signal action will
        be returned.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/
