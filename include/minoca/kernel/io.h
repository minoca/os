/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    io.h

Abstract:

    This header contains definitions for the I/O Subsystem.

Author:

    Evan Green 16-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/devres.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro sets a driver specific error code on a device, automatically
// generating the source file and line number parameters. The first parameter
// is a PDEVICE, then second parameter is a PDRIVER, the third parameter is a
// KSTATUS and the fourth parameter is a driver specific error code.
//

#define IoSetDeviceDriverError(_Device, _Driver, _Status, _DriverError) \
    IoSetDeviceDriverErrorEx((_Device),                                 \
                             (_Status),                                 \
                             (_Driver),                                 \
                             (_DriverError),                            \
                             __FILE__,                                  \
                             __LINE__)                                  \

//
// This macro initializes a path point structure.
//

#define IO_INITIALIZE_PATH_POINT(_PathPoint)    \
    (_PathPoint)->PathEntry = NULL;             \
    (_PathPoint)->MountPoint = NULL;

//
// This macro determines if two path points are equal. Both the path entry and
// mount point must match.
//

#define IO_ARE_PATH_POINTS_EQUAL(_PathPoint1, _PathPoint2)     \
    (((_PathPoint1)->PathEntry == (_PathPoint2)->PathEntry) && \
     ((_PathPoint1)->MountPoint == (_PathPoint2)->MountPoint))

//
// This macro adds a reference to both the path entry and mount point of a path
// point.
//

#define IO_PATH_POINT_ADD_REFERENCE(_PathPoint)         \
    IoPathEntryAddReference((_PathPoint)->PathEntry);   \
    IoMountPointAddReference((_PathPoint)->MountPoint);

//
// This macros releases a reference from both the path entry and mount point of
// a path point.
//

#define IO_PATH_POINT_RELEASE_REFERENCE(_PathPoint)         \
    IoPathEntryReleaseReference((_PathPoint)->PathEntry);   \
    IoMountPointReleaseReference((_PathPoint)->MountPoint);

//
// This macro copies the original path point to the copy.
//

#define IO_COPY_PATH_POINT(_Copy, _Original)       \
    (_Copy)->PathEntry = (_Original)->PathEntry;   \
    (_Copy)->MountPoint = (_Original)->MountPoint;

//
// ---------------------------------------------------------------- Definitions
//

#define DEVICE_STATE_HISTORY 10

//
// Define the current version number of the driver function table.
//

#define DRIVER_FUNCTION_TABLE_VERSION 1

//
// Define the name of the local terminal.
//

#define LOCAL_TERMINAL_PATH "/Terminal/Slave0"

//
// Define standard device class IDs.
//

#define DISK_CLASS_ID "Disk"
#define PARTITION_CLASS_ID "Partition"
#define CHARACTER_CLASS_ID "Character"

//
// Define maximum string lengths for drivers and device IDs. Strings will be
// truncated at these lengths.
//

#define MAX_DRIVER_NAME 256
#define MAX_DEVICE_ID 1024

//
// Define the maximum number of symbolic links that can be encountered
// recursively during path resolution.
//

#define MAX_SYMBOLIC_LINK_RECURSION 32

//
// Define the delimiter character for the compatible ID string.
//

#define COMPATIBLE_ID_DELIMITER ';'

#define PATH_SEPARATOR '/'

//
// Define the current version of the IO_CONNECT_INTERRUPT_PARAMETERS structure.
//

#define IO_CONNECT_INTERRUPT_PARAMETERS_VERSION 1

//
// Set this bit to grant execute permissions to the given I/O handle.
//

#define IO_ACCESS_EXECUTE 0x00000001

//
// Set this bit to grant write permissions to the given I/O handle.
//

#define IO_ACCESS_WRITE 0x00000002

//
// Set this bit to grant read permissions to the given I/O handle.
//

#define IO_ACCESS_READ  0x00000004

#define IO_ACCESS_MASK \
    (IO_ACCESS_EXECUTE | IO_ACCESS_WRITE | IO_ACCESS_READ)

//
// Set this flag if the file (or object) should be created if it does not exist.
//

#define OPEN_FLAG_CREATE 0x00000001

//
// Set this flag if the file should be truncated to zero size.
//

#define OPEN_FLAG_TRUNCATE 0x00000002

//
// Set this flag to only create the file, failing if it already exists.
//

#define OPEN_FLAG_FAIL_IF_EXISTS 0x00000004

//
// Set this flag to have every write to the file append to the end of it.
//

#define OPEN_FLAG_APPEND 0x00000008

//
// Set this flag if attempting to open a directory.
//

#define OPEN_FLAG_DIRECTORY 0x00000010

//
// Set this flag to make any I/O return immediately if the call would have
// otherwise blocked.
//

#define OPEN_FLAG_NON_BLOCKING 0x00000020

//
// Set this flag if attempting to open a shared memory object.
//

#define OPEN_FLAG_SHARED_MEMORY 0x00000040

//
// Set this flag to fail if the final component of the path to open is a
// symbolic link.
//

#define OPEN_FLAG_NO_SYMBOLIC_LINK 0x00000080

//
// Set this flag to cause calls to write not to return until the data has been
// written to the underlying medium.
//

#define OPEN_FLAG_SYNCHRONIZED 0x00000100

//
// Set this flag when opening a terminal to prevent it from becoming the
// controlling terminal of the process.
//

#define OPEN_FLAG_NO_CONTROLLING_TERMINAL 0x00000200

//
// Set this flag to avoid updating the last access time of the file when it is
// read.
//

#define OPEN_FLAG_NO_ACCESS_TIME 0x00000400

//
// Set this flag to receive signals whenever the descriptor is ready for read
// or write. This does not take effect immediately, as the signal owner still
// needs to be set.
//

#define OPEN_FLAG_ASYNCHRONOUS 0x00000800

//
// Set this flag if mount points should not be followed on the final component.
//

#define OPEN_FLAG_NO_MOUNT_POINT 0x08000000

//
// Set this flag if trying to open a symbolic link itself.
//

#define OPEN_FLAG_SYMBOLIC_LINK 0x10000000

//
// This flag is reserved for use only by the I/O manager. It indicates that the
// given file or device will bypass the page cache for all I/O operations.
//

#define OPEN_FLAG_NO_PAGE_CACHE 0x20000000

//
// This flag is reserved for use only by the I/O manager. It indicates that the
// given device will be used as a paging device.
//

#define OPEN_FLAG_PAGING_DEVICE 0x40000000

//
// This flag is reserved for use only by the memory manager. System crashes
// will result if any other entities set this flag. It indicates that the given
// file will be used as a page file.
//

#define OPEN_FLAG_PAGE_FILE 0x80000000

//
// Set this flag if attempting to delete a shared memory object.
//

#define DELETE_FLAG_SHARED_MEMORY 0x00000001

//
// Set this flag if attempting to delete a directory.
//

#define DELETE_FLAG_DIRECTORY 0x00000002

//
// This flag is reserved for use only by the memory manager. It indicates that
// the I/O operation is to be performed in a no-allocate code path.
//

#define IO_FLAG_NO_ALLOCATE 0x80000000

//
// This flag is reserved for use only by the memory manager. It indicates that
// the I/O operation was initiated to satisfy a page fault. Device drivers
// need not perform any different behavior here, this is only used for updating
// internal accounting numbers.
//

#define IO_FLAG_SERVICING_FAULT 0x40000000

//
// This flag is reserved for use by the page cache. It indicates that a write
// I/O operation should preserve the data because the page cache is about to
// release its copy of the data. The backing device may have previously been
// acknowledging writes without actually writing to persistant storage.
//

#define IO_FLAG_HARD_FLUSH 0x20000000

//
// This flag is reserved for use by the page cache thread. It indicates that
// hard flushes are allowed. Normal threads cannot perform hard flushes because
// a hard flush may fail (e.g. there is no page file to back a shared memory
// object). This failure should not be reported back to user mode, as it is
// non-fatal.
//

#define IO_FLAG_HARD_FLUSH_ALLOWED 0x10000000

//
// This flag indicates that a write I/O operation should flush all the file
// data provided before returning.
//

#define IO_FLAG_DATA_SYNCHRONIZED 0x00000002

//
// This flag, along with the data synchronized flag, indicates that the
// file data and metadata should be flushed. It is illegal to set this flag
// without also setting the data synchronized flag.
//

#define IO_FLAG_METADATA_SYNCHRONIZED 0x00000004

//
// This flag indicates that this request represents the file system fetching
// data or metadata to service a request. It allows the physical memory to be
// shared with the file layer request.
//

#define IO_FLAG_FS_DATA 0x00000008

//
// This flag indicates that this request represents the file system fetching
// metadata to service a request. the FS data flag must also be set.
//

#define IO_FLAG_FS_METADATA 0x00000010

//
// Set this flag if the IRP needs to execute in a no-allocate code path. As a
// result none of the data or code it touches can be pagable.
//

#define IRP_CREATE_FLAG_NO_ALLOCATE 0x00000001

//
// Set this flag if the flush operation should flush all data.
//

#define FLUSH_FLAG_ALL 0x00000001

//
// Set this flag if the flush operation should flush unread data. This only
// applies to some file object types, like terminals.
//

#define FLUSH_FLAG_READ 0x00000002

//
// Set this flag if the flush operation should flush unwritten data. This only
// applies to some file object types, like terminals.
//

#define FLUSH_FLAG_WRITE 0x00000004

//
// Set this flag to discard unflushed data instead of waiting for it to be
// written. This only applies to some file object types, like terminals.
//

#define FLUSH_FLAG_DISCARD 0x00000008

//
// Set this flag if the flush operation should flush all cacheable data in the
// entire system and not return until the data is written to disk.
//

#define FLUSH_FLAG_ALL_SYNCHRONOUS 0x80000000

//
// Define the mount flags.
//

#define MOUNT_FLAG_BIND      0x00000001
#define MOUNT_FLAG_RECURSIVE 0x00000002
#define MOUNT_FLAG_DETACH    0x00000004
#define MOUNT_FLAG_LINKED    0x00000008

//
// Define file permission bits.
//

#define FILE_PERMISSION_OTHER_EXECUTE 0x00000001
#define FILE_PERMISSION_OTHER_WRITE   0x00000002
#define FILE_PERMISSION_OTHER_READ    0x00000004
#define FILE_PERMISSION_OTHER_ALL       \
    (FILE_PERMISSION_OTHER_EXECUTE |    \
     FILE_PERMISSION_OTHER_WRITE |      \
     FILE_PERMISSION_OTHER_READ)

#define FILE_PERMISSION_GROUP_EXECUTE 0x00000008
#define FILE_PERMISSION_GROUP_WRITE   0x00000010
#define FILE_PERMISSION_GROUP_READ    0x00000020
#define FILE_PERMISSION_GROUP_ALL       \
    (FILE_PERMISSION_GROUP_EXECUTE |    \
     FILE_PERMISSION_GROUP_WRITE |      \
     FILE_PERMISSION_GROUP_READ)

#define FILE_PERMISSION_USER_EXECUTE  0x00000040
#define FILE_PERMISSION_USER_WRITE    0x00000080
#define FILE_PERMISSION_USER_READ     0x00000100
#define FILE_PERMISSION_USER_ALL        \
    (FILE_PERMISSION_USER_EXECUTE |     \
     FILE_PERMISSION_USER_WRITE |       \
     FILE_PERMISSION_USER_READ)

#define FILE_PERMISSION_ALL_EXECUTE     \
    (FILE_PERMISSION_USER_EXECUTE |     \
     FILE_PERMISSION_GROUP_EXECUTE |    \
     FILE_PERMISSION_OTHER_EXECUTE)

#define FILE_PERMISSION_ALL             \
    (FILE_PERMISSION_OTHER_ALL |        \
     FILE_PERMISSION_GROUP_ALL |        \
     FILE_PERMISSION_USER_ALL)

#define FILE_PERMISSION_NONE 0

#define FILE_PERMISSION_RESTRICTED    0x00000200
#define FILE_PERMISSION_SET_GROUP_ID  0x00000400
#define FILE_PERMISSION_SET_USER_ID   0x00000800

#define FILE_PERMISSION_MASK 0x00000FFF

#define FILE_PERMISSION_ACCESS_MASK 0x00000007
#define FILE_PERMISSION_OTHER_SHIFT 0
#define FILE_PERMISSION_GROUP_SHIFT 3
#define FILE_PERMISSION_USER_SHIFT 6

//
// Define file property fields that can be set.
//

#define FILE_PROPERTY_FIELD_USER_ID             0x00000001
#define FILE_PROPERTY_FIELD_GROUP_ID            0x00000002
#define FILE_PROPERTY_FIELD_PERMISSIONS         0x00000004
#define FILE_PROPERTY_FIELD_ACCESS_TIME         0x00000008
#define FILE_PROPERTY_FIELD_MODIFIED_TIME       0x00000010
#define FILE_PROPERTY_FIELD_STATUS_CHANGE_TIME  0x00000020
#define FILE_PROPERTY_FIELD_FILE_SIZE           0x00000040

//
// Define the set of properties that only the file owner or a privileged
// user can change.
//

#define FILE_PROPERTY_OWNER_OWNED_FIELDS        \
    (FILE_PROPERTY_FIELD_PERMISSIONS |          \
     FILE_PROPERTY_FIELD_ACCESS_TIME |          \
     FILE_PROPERTY_FIELD_MODIFIED_TIME |        \
     FILE_PROPERTY_FIELD_STATUS_CHANGE_TIME)

//
// Define file descriptor flags.
//

#define FILE_DESCRIPTOR_CLOSE_ON_EXECUTE 0x00000001

//
// Define input control flags.
//

//
// Set this flag to ignore break conditions.
//

#define TERMINAL_INPUT_IGNORE_BREAK 0x00000001

//
// Set this flag to signal an interrupt on break.
//

#define TERMINAL_INPUT_SIGNAL_ON_BREAK 0x00000002

//
// Set this flag to ignore characters with parity errors.
//

#define TERMINAL_INPUT_IGNORE_PARITY_ERRORS 0x0000004

//
// Set this flag to mark parity errors.
//

#define TERMINAL_INPUT_MARK_PARITY_ERRORS 0x00000008

//
// Set this flag to enable input parity checking.
//

#define TERMINAL_INPUT_ENABLE_PARITY_CHECK 0x00000010

//
// Set this flag to strip characters.
//

#define TERMINAL_INPUT_STRIP 0x00000020

//
// Set this flag to map newlines (\n) to carriage returns (\r) on input.
//

#define TERMINAL_INPUT_NEWLINE_TO_CR 0x00000040

//
// Set this flag to ignore carriage returns.
//

#define TERMINAL_INPUT_IGNORE_CR 0x00000080

//
// Set this flag to map carriage return (\r) characters to newlines (\n) on
// input.
//

#define TERMINAL_INPUT_CR_TO_NEWLINE 0x00000100

//
// Set this flag to enable start/stop output control.
//

#define TERMINAL_INPUT_ENABLE_OUTPUT_FLOW_CONTROL 0x00000200

//
// Set this flag to enable start/stop input control.
//

#define TERMINAL_INPUT_ENABLE_INPUT_FLOW_CONTROL 0x00000400

//
// Set this flag to enable any character to restart output.
//

#define TERMINAL_INPUT_ANY_CHARACTER_RESTARTS_OUTPUT 0x00000800

//
// Set this flag to cause a bell character to be sent to the output if the
// input buffer is full. If this flag is not set and a new character is
// received when the input queue is full, then the entire current input and
// output queue is discarded.
//

#define TERMINAL_INPUT_MAX_BELL 0x00001000

//
// Define terminal output control flags.
//

//
// Set this flag to post-process output.
//

#define TERMINAL_OUTPUT_POST_PROCESS 0x00000001

//
// Set this flag to map newlines (\n) or CR-NL (\r\n) on output.
//

#define TERMINAL_OUTPUT_NEWLINE_TO_CRLF 0x00000002

//
// Set this flag to map carriage returns (\r) to newlines (\n) on output.
//

#define TERMINAL_OUTPUT_CR_TO_NEWLINE 0x00000004

//
// Set this flag to avoid carriage return output at column 0.
//

#define TERMINAL_OUTPUT_NO_CR_AT_COLUMN_ZERO 0x00000008

//
// Set this flag to have newline perform carriage return functionality.
//

#define TERMINAL_OUTPUT_NEWLINE_IS_CR 0x00000010

//
// Set this flag to use fill characters for delay.
//

#define TERMINAL_OUTPUT_USE_FILL_CHARACTERS 0x00000020

//
// Set this flag to enable newline delays, which lasts 0.1 seconds.
//

#define TERMINAL_OUTPUT_NEWLINE_DELAY 0x00000040

//
// Set this flag to select carriage return delays, types 0 through 3.
// Type 1 delays for an amount dependent on column position. Type 2 is about
// 0.1 seconds, and type 3 is about 0.15 seconds. If OFILL is set, type 1
// transmits two fill characters and type 2 transmits four fill characters.
//

#define TERMINAL_OUTPUT_CR_DELAY_MASK 0x00000180
#define TERMINAL_OUTPUT_CR_DELAY_1 0x00000080
#define TERMINAL_OUTPUT_CR_DELAY_2 0x00000100
#define TERMINAL_OUTPUT_CR_DELAY_3 0x00000180

//
// Set this flag to enable tab delays, types 0 through 3.
// Type 1 is dependent on column positions, type 2 is 0.1 seconds, and type 3
// is "expand tabs to spaces". If OFILL is set, any delay transmits two fill
// characters.
//

#define TERMINAL_OUTPUT_TAB_DELAY_MASK 0x00000600
#define TERMINAL_OUTPUT_TAB_DELAY_1 0x00000200
#define TERMINAL_OUTPUT_TAB_DELAY_2 0x00000400
#define TERMINAL_OUTPUT_TAB_DELAY_3 0x00000600

//
// Set this flag to enable backspace delays, which lasts 0.05 seconds or one
// fill character.
//

#define TERMINAL_OUTPUT_BACKSPACE_DELAY 0x00000800

//
// Set this flag to enable vertical tab delays, which last two seconds.
//

#define TERMINAL_OUTPUT_VERTICAL_TAB_DELAY 0x00001000

//
// Set this flag to enable form feed delays, which last two seconds.
//

#define TERMINAL_OUTPUT_FORM_FEED_DELAY 0x00002000

//
// Set this flag to fill with DEL (127) characters. If not set, the fill
// character is NUL (0).
//

#define TERMINAL_OUTPUT_FILL_DEL 0x00004000

//
// Define terminal control mode flags.
//

//
// Set this field to set the number of bits per character.
//

#define TERMINAL_CONTROL_CHARACTER_SIZE_MASK 0x00000003
#define TERMINAL_CONTROL_5_BITS_PER_CHARACTER 0x00000000
#define TERMINAL_CONTROL_6_BITS_PER_CHARACTER 0x00000001
#define TERMINAL_CONTROL_7_BITS_PER_CHARACTER 0x00000002
#define TERMINAL_CONTROL_8_BITS_PER_CHARACTER 0x00000003

//
// Set this bit to send two stop bits (without it set one stop bit is sent).
//

#define TERMINAL_CONTROL_2_STOP_BITS 0x00000004

//
// Set this bit to enable the receiver.
//

#define TERMINAL_CONTROL_ENABLE_RECEIVE 0x00000008

//
// Set this bit to enable a parity bit.
//

#define TERMINAL_CONTROL_ENABLE_PARITY 0x00000010

//
// Set this bit to enable odd parity (without this bit set even parity is used).
//

#define TERMINAL_CONTROL_ODD_PARITY 0x00000020

//
// Set this bit to send a hangup signal when the terminal is closed.
//

#define TERMINAL_CONTROL_HANGUP_ON_CLOSE 0x00000040

//
// Set this bit to ignore modem status lines (and do not send a hangup signal).
//

#define TERMINAL_CONTROL_NO_HANGUP 0x00000080

//
// Define terminal local mode bits.
//

//
// Set this bit to enable echo of terminal input directly to its output.
//

#define TERMINAL_LOCAL_ECHO 0x00000001

//
// Set this bit to enable echoing erase characters as BS-SP-BS. Otherwise, the
// character erased is echoed to show what happened (suitable for a printer).
//

#define TERMINAL_LOCAL_ECHO_ERASE 0x00000002

//
// Set this bit to enable echoing the kill character and moving to a new line.
// If this bit is not set, the kill character is simply echoed, which produces
// no output, so the user must remember the line data is killed.
//

#define TERMINAL_LOCAL_ECHO_KILL_NEWLINE 0x00000004

//
// Set this bit to enable echoing the newline character.
//

#define TERMINAL_LOCAL_ECHO_NEWLINE 0x00000008

//
// Set this bit to enable canonical input (erase and kill processing).
//

#define TERMINAL_LOCAL_CANONICAL 0x00000010

//
// Set this bit to enable extended processing. With extended processing,
// the erase, kill, and end of file characters can be preceded by a backslash
// to remove their special meaning.
//

#define TERMINAL_LOCAL_EXTENDED 0x00000020

//
// Set this bit to enable signals to be generated to the controlling process
// group when signal characters are seen at the input.
//

#define TERMINAL_LOCAL_SIGNALS 0x00000040

//
// Set this bit to disable flushing after an interrupt or quit.
//

#define TERMINAL_LOCAL_NO_FLUSH 0x00000080

//
// Set this bit to send a SIGTTOU signal when processes in the background try
// to write to the terminal.
//

#define TERMINAL_LOCAL_STOP_BACKGROUND_WRITES 0x00000100

//
// Set this bit to enable visually erasing the current line when the kill
// character comes in. If this bit is not set, the "echo kill" flag dictates
// what is echoed when a kill character comes in.
//

#define TERMINAL_LOCAL_ECHO_KILL_EXTENDED 0x00000200

//
// Set this bit to enable echoing control characters as '^' followed by their
// text equivalent.
//

#define TERMINAL_LOCAL_ECHO_CONTROL 0x00000400

//
// Define the flags for each field that are currnetly unimplemented.
//

#define TERMINAL_UNIMPLEMENTED_INPUT_FLAGS          \
    (TERMINAL_INPUT_ENABLE_PARITY_CHECK |           \
     TERMINAL_INPUT_MARK_PARITY_ERRORS)

#define TERMINAL_UNIMPLEMENTED_OUTPUT_FLAGS         \
    (TERMINAL_OUTPUT_NO_CR_AT_COLUMN_ZERO |         \
     TERMINAL_OUTPUT_USE_FILL_CHARACTERS |          \
     TERMINAL_OUTPUT_VERTICAL_TAB_DELAY |           \
     TERMINAL_OUTPUT_FORM_FEED_DELAY)

#define TERMINAL_UNIMPLEMENTED_CONTROL_FLAGS        \
    (TERMINAL_CONTROL_2_STOP_BITS |                 \
     TERMINAL_CONTROL_ENABLE_PARITY)

//
// Define the number of control characters in the old terminal settings
// (termio).
//

#define TERMINAL_SETTINGS_OLD_CONTROL_COUNT 8

//
// Define the default create permissions for a terminal device.
//

#define TERMINAL_DEFAULT_PERMISSIONS                            \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE |   \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_WRITE)

//
// Define the default atomic write size for pipes.
//

#define PIPE_ATOMIC_WRITE_SIZE 4096

//
// Define I/O test hook bits.
//

//
// Set this bit to fail one attempt to queue a device work item.
//

#define IO_FAIL_QUEUE_DEVICE_WORK 0x1

//
// Define the file offsets used for reporting the relative directory entries
// dot and dot-dot.
//

#define DIRECTORY_OFFSET_DOT 0
#define DIRECTORY_OFFSET_DOT_DOT 1
#define DIRECTORY_CONTENTS_OFFSET 2

//
// Set this flag in lookup if the device's data should not be stored in the
// page cache.
//

#define LOOKUP_FLAG_NO_PAGE_CACHE 0x00000001

//
// Set this flag if the file's I/O state should be allocated from non-paged
// pool. This is useful if the I/O state needs to be signaled from a DPC.
//

#define LOOKUP_FLAG_NON_PAGED_IO_STATE 0x00000002

//
// Define the version number for the I/O cache statistics.
//

#define IO_CACHE_STATISTICS_VERSION 0x1
#define IO_CACHE_STATISTICS_MAX_VERSION 0x10000000

//
// Define the version number for the global cache statistics.
//

#define IO_GLOBAL_STATISTICS_VERSION 0x1
#define IO_GLOBAL_STATISTICS_MAX_VERSION 0x10000000

//
// Define the device ID given to the object manager.
//

#define OBJECT_MANAGER_DEVICE_ID 1

//
// Define the invalid interrupt line. This can be supplied to the interrupt
// connection routine if only the vector needs connecting.
//

#define INVALID_INTERRUPT_LINE (-1ULL)
#define INVALID_INTERRUPT_VECTOR (-1ULL)

//
// Define the offset to use to specify the current file offset.
//

#define IO_OFFSET_NONE (-1LL)

//
// Define the maximum I/O offset.
//

#define IO_OFFSET_MAX MAX_LONGLONG

//
// Define the set of flags used for read/write IRP preparation and completion.
//

#define IRP_READ_WRITE_FLAG_PHYSICALLY_CONTIGUOUS 0x00000001
#define IRP_READ_WRITE_FLAG_WRITE                 0x00000002
#define IRP_READ_WRITE_FLAG_DMA                   0x00000004
#define IRP_READ_WRITE_FLAG_POLLED                0x00000008

//
// Define the set of flags describing an I/O request's saved I/O buffer state.
//

#define IRP_IO_BUFFER_STATE_FLAG_LOCKED_COPY 0x00000001

//
// Define the current loaded file structure version.
//

#define LOADED_FILE_VERSION 1

//
// Define shared memory properties, stored in the permissions field of the
// permissions structure.
//

//
// Set this flag if the shared memory object is unlinked and will be destroyed
// when the last reference is closed. This lines up with SHM_DEST in the C
// library.
//

#define SHARED_MEMORY_PROPERTY_UNLINKED 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG FILE_PERMISSIONS, *PFILE_PERMISSIONS;
typedef ULONGLONG FILE_ID, *PFILE_ID;
typedef ULONGLONG DEVICE_ID, *PDEVICE_ID;
typedef struct _PATH_ENTRY PATH_ENTRY, *PPATH_ENTRY;
typedef struct _MOUNT_POINT MOUNT_POINT, *PMOUNT_POINT;
typedef struct _VOLUME VOLUME, *PVOLUME;
typedef struct _DRIVER DRIVER, *PDRIVER;
typedef struct _IRP IRP, *PIRP;
typedef struct _STREAM_BUFFER STREAM_BUFFER, *PSTREAM_BUFFER;
typedef struct _IO_HANDLE IO_HANDLE, *PIO_HANDLE;
typedef struct _PAGE_CACHE_ENTRY PAGE_CACHE_ENTRY, *PPAGE_CACHE_ENTRY;

typedef enum _SEEK_COMMAND {
    SeekCommandInvalid,
    SeekCommandNop,
    SeekCommandFromBeginning,
    SeekCommandFromCurrentOffset,
    SeekCommandFromEnd,
} SEEK_COMMAND, *PSEEK_COMMAND;

typedef enum _TERMINAL_CONTROL_CHARACTER {
    TerminalCharacterEndOfFile,
    TerminalCharacterEndOfLine,
    TerminalCharacterErase,
    TerminalCharacterInterrupt,
    TerminalCharacterKill,
    TerminalCharacterFlushCount,
    TerminalCharacterQuit,
    TerminalCharacterStart,
    TerminalCharacterStop,
    TerminalCharacterSuspend,
    TerminalCharacterFlushTime,
    TerminalCharacterCount
} TERMINAL_CONTROL_CHARACTER, *PTERMINAL_CONTROL_CHARACTER;

typedef enum _TERMINAL_CHANGE_BEHAVIOR {
    TerminalChangeNone,
    TerminalChangeNow,
    TerminalChangeAfterOutput,
    TerminalChangeAfterOutputFlushInput
} TERMINAL_CHANGE_BEHAVIOR, *PTERMINAL_CHANGE_BEHAVIOR;

//
// Define terminal user control (IOCTL) codes. These must line up with what's
// defined in sys/ioctl.h in the C library.
//

typedef enum _TERMINAL_USER_CONTROL_CODE {
    TerminalControlGetAttributes             = 0x7401,
    TerminalControlSetAttributes             = 0x7402,
    TerminalControlSetAttributesDrain        = 0x7403,
    TerminalControlSetAttributesFlush        = 0x7404,
    TerminalControlGetAttributesOld          = 0x7405,
    TerminalControlSetAttributesOld          = 0x7406,
    TerminalControlSetAttributesDrainOld     = 0x7407,
    TerminalControlSetAttributesFlushOld     = 0x7408,
    TerminalControlSendBreak                 = 0x7409,
    TerminalControlFlowControl               = 0x740A,
    TerminalControlFlush                     = 0x740B,
    TerminalControlSetExclusive              = 0x740C,
    TerminalControlClearExclusive            = 0x740D,
    TerminalControlSetControllingTerminal    = 0x740E,
    TerminalControlGetProcessGroup           = 0x740F,
    TerminalControlSetProcessGroup           = 0x7410,
    TerminalControlGetOutputQueueSize        = 0x7411,
    TerminalControlInsertInInputQueue        = 0x7412,
    TerminalControlGetWindowSize             = 0x7413,
    TerminalControlSetWindowSize             = 0x7414,
    TerminalControlGetModemStatus            = 0x7415,
    TerminalControlOrModemStatus             = 0x7416,
    TerminalControlClearModemStatus          = 0x7417,
    TerminalControlSetModemStatus            = 0x7418,
    TerminalControlGetSoftCarrier            = 0x7419,
    TerminalControlSetSoftCarrier            = 0x741A,
    TerminalControlGetInputQueueSize         = 0x741B,
    TerminalControlRedirectLocalConsole      = 0x741D,
    TerminalControlSetPacketMode             = 0x7420,
    TerminalControlGiveUpControllingTerminal = 0x7422,
    TerminalControlSendBreakPosix            = 0x7425,
    TerminalControlStartBreak                = 0x7427,
    TerminalControlStopBreak                 = 0x7428,
    TerminalControlGetCurrentSessionId       = 0x7429,
} TERMINAL_USER_CONTROL_CODE, *PTERMINAL_USER_CONTROL_CODE;

//
// Define file I/O user control (IOCTL) codes. These must line up with what's
// defined in sys/ioctl.h in the C library.
//

typedef enum _FILE_IO_USER_CONTROL_CODE {
    FileIoControlNonBlocking = 0x7421,
    FileIoControlAsync       = 0x7452,
} FILE_IO_USER_CONTROL_CODE, *PFILE_IO_USER_CONTROL_CODE;

typedef enum _CRASH_DRIVER_ERROR_CODE {
    DriverErrorInvalid,
    DriverErrorRemovingEnumeratedDevice,
} CRASH_DRIVER_ERROR_CODE, *PCRASH_DRIVER_ERROR_CODE;

typedef enum _IO_INFORMATION_TYPE {
    IoInformationInvalid,
    IoInformationBoot,
    IoInformationMountPoints,
    IoInformationCacheStatistics,
} IO_INFORMATION_TYPE, *PIO_INFORMATION_TYPE;

typedef enum _SHARED_MEMORY_COMMAND {
    SharedMemoryCommandInvalid,
    SharedMemoryCommandUnlink,
    SharedMemoryCommandSet,
    SharedMemoryCommandStat
} SHARED_MEMORY_COMMAND, *PSHARED_MEMORY_COMMAND;

/*++

Structure Description:

    This structure defines a terminal configuration. Note that this structure
    must line up offset for offset with struct termios in the C library to
    support terminal IOCTLs.

Members:

    InputFlags - Stores the terminal input flags. See TERMINAL_INPUT_*
        definitions.

    OutputFlags - Stores the terminal output flags. See TERMINAL_OUTPUT_*
        definitions.

    ControlFlags - Stores the terminal control flags. See TERMINAL_CONTROL_*
        definitions.

    LocalFlags - Stores the terminal local behavior flags. See TERMINAL_LOCAL_*
        definitions.

    ControlCharacters - Stores the recognized control characters.

    InputSpeed - Stores the baud rate for input going to the slave.

    OutputSpeed - Stores the baud rate for output coming from the slave.

--*/

typedef struct _TERMINAL_SETTINGS {
    LONG InputFlags;
    LONG OutputFlags;
    LONG ControlFlags;
    LONG LocalFlags;
    CHAR ControlCharacters[TerminalCharacterCount];
    INT InputSpeed;
    INT OutputSpeed;
} TERMINAL_SETTINGS, *PTERMINAL_SETTINGS;

/*++

Structure Description:

    This structure defines the old structure for terminal settings. This lines
    up byte for byte with struct termio in the C library.

Members:

    InputFlags - Stores the terminal input flags. See TERMINAL_INPUT_*
        definitions.

    OutputFlags - Stores the terminal output flags. See TERMINAL_OUTPUT_*
        definitions.

    ControlFlags - Stores the terminal control flags. See TERMINAL_CONTROL_*
        definitions.

    LocalFlags - Stores the terminal local behavior flags. See TERMINAL_LOCAL_*
        definitions.

    LineDiscipline - Stores the line discipline. Set to zero to indicate TTY
        line discipline.

    ControlCharacters - Stores the recognized control characters.

--*/

typedef struct _TERMINAL_SETTINGS_OLD {
    USHORT InputFlags;
    USHORT OutputFlags;
    USHORT ControlFlags;
    USHORT LocalFlags;
    UCHAR LineDiscipline;
    CHAR ControlCharacters[TERMINAL_SETTINGS_OLD_CONTROL_COUNT];
} TERMINAL_SETTINGS_OLD, *PTERMINAL_SETTINGS_OLD;

/*++

Structure Description:

    This structure defines the terminal window size structure passed back and
    forth in the window size user control (ioctl) messages. Note that this
    structure must line up with struct winsize for the ioctls to function in a
    compliant manner.

Members:

    Rows - Stores the number of rows in the terminal.

    Columns - Stores the number of columns in the terminal.

    PixelsX - Stores the number of pixels in the horizontal direction. This may
        be unused.

    PixelsY - Stores the number of pixels in the vertical direction. This may
        be unused.

--*/

typedef struct _TERMINAL_WINDOW_SIZE {
    USHORT Rows;
    USHORT Columns;
    USHORT PixelsX;
    USHORT PixelsY;
} TERMINAL_WINDOW_SIZE, *PTERMINAL_WINDOW_SIZE;

typedef
VOID
(*PIRP_COMPLETION_ROUTINE) (
    PIRP Irp,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called when an IRP completes. The routine is supplied by
    the sender of the IRP.

Arguments:

    Irp - Supplies a pointer to the IRP that just completed.

    Context - Supplies a pointer supplied by the sender of the IRP. Presumably
        it contains state information relating to the reason this IRP was sent.

Return Value:

    None.

--*/

typedef
VOID
(*PDRIVER_UNLOAD) (
    PVOID Driver
    );

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PDRIVER_ADD_DEVICE) (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

/*++

Routine Description:

    This routine is called when a device is detected that a given driver
    supports. The driver should attach itself to the device stack at this point.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success. The driver should return success in most cases,
    even if it chooses not to attach itself to the stack.

    Failure code if the driver encountered an error such as a resource
    allocation failure.

--*/

typedef
VOID
(*PDRIVER_DISPATCH) (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

/*++

Routine Description:

    This routine is called whenever an IRP is sent to a device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    DeviceContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PDRIVER_CREATE_IRP) (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID *IrpContext,
    ULONG Flags
    );

/*++

Routine Description:

    This routine is called when an IRP is being created. It gives the driver a
    chance to allocate any additional state it may need to associate with the
    IRP.

Arguments:

    Irp - Supplies a pointer to the I/O request packet. The only variables the
        driver can count on staying constant are the device and the IRP Major
        Code. All other fields are subject to change throughout the lifetime of
        the IRP.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    DeviceContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

    Flags - Supplies a bitmask of IRP creation flags. See IRP_FLAG_* for
        definitions.

Return Value:

    None.

--*/

typedef
VOID
(*PINTERFACE_NOTIFICATION_CALLBACK) (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

/*++

Routine Description:

    This routine is called to notify listeners that an interface has arrived
    or departed.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a table of function pointers that the system uses to
    interact with drivers.

Members:

    Version - Stores the version number of the table. Set this to
        DRIVER_FUNCTION_TABLE_VERSION.

    Unload - Stores a pointer to a function called before the driver is
        unloaded from system memory.

    AddDevice - Stores a pointer to the routine used to connect a driver with
        a device.

    CreateIrp - Stores an optional pointer to the routine to be called whenever
        an IRP is allocated for a device in which the driver is involved.

    DestroyIrp - Stores an optional pointer to the routine to be called
        whenever an IRP is destroyed for a device in which the driver is
        involved. If the CreateIrp function is non-NULL, then this routine is
        required.

    DispatchStateChange - Stores a pointer to the routine used to dispatch
        state changing IRPs.

    DispatchOpen - Stores a pointer to the routine used to dispatch Open IRPs.

    DispatchClose - Stores a pointer to the routine used to dispatch Close IRPs.

    DispatchIo - Stores a pointer to the routine used to dispatch I/O IRPs.

    DispatchSystemControl - Stores a pointer to the routine used to dispatch
        system control IRPs.

    DispatchUserControl - Stores a pointer to the routine used to dispatch
        user control IRPs.

--*/

typedef struct _DRIVER_FUNCTION_TABLE {
    ULONG Version;
    PDRIVER_UNLOAD Unload;
    PDRIVER_ADD_DEVICE AddDevice;
    PDRIVER_CREATE_IRP CreateIrp;
    PDRIVER_DISPATCH DestroyIrp;
    PDRIVER_DISPATCH DispatchStateChange;
    PDRIVER_DISPATCH DispatchOpen;
    PDRIVER_DISPATCH DispatchClose;
    PDRIVER_DISPATCH DispatchIo;
    PDRIVER_DISPATCH DispatchSystemControl;
    PDRIVER_DISPATCH DispatchUserControl;
} DRIVER_FUNCTION_TABLE, *PDRIVER_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the parameters to the IoConnectInterrupt function.

Members:

    Version - Stores the table version.

    Device - Stores a pointer to the device whose interrupt is being connected.

    LineNumber - Stores the Global System Interrupt number of the interrupt to
        connect. The device must have this line in its resources.

    Vector - Stores the software interrupt vector number to wire the interrupt
        to. The device must have this vector it its resources.

    InterruptServiceRoutine - Stores an optional pointer to a routine called
        at an interrupt runlevel. This routine should be used simply to
        query and quiesce the device. Actual processing of the interrupt should
        be relegated to a lower level service routine. If this routine is not
        supplied, then the lower level routines will be called back
        automatically.

    DispatchServiceRoutine - Stores an optional pointer to a routine to be
        called at dispatch level to service the interrupt.

    LowLevelServiceRoutine - Stores an optional pointer to a routine to be
        called at low runlevel to service the interrupt. This routine will be
        called from a work item on the system work queue, and therefore cannot
        block on functions that wait for system work items to complete.

    Context - Stores a context pointer that will be passed to each of the
        service routines.

    Interrupt - Stores a pointer where a handle will be returned on success.

--*/

typedef struct _IO_CONNECT_INTERRUPT_PARAMETERS {
    ULONG Version;
    PDEVICE Device;
    ULONGLONG LineNumber;
    ULONGLONG Vector;
    PINTERRUPT_SERVICE_ROUTINE InterruptServiceRoutine;
    PINTERRUPT_SERVICE_ROUTINE DispatchServiceRoutine;
    PINTERRUPT_SERVICE_ROUTINE LowLevelServiceRoutine;
    PVOID Context;
    PHANDLE Interrupt;
} IO_CONNECT_INTERRUPT_PARAMETERS, *PIO_CONNECT_INTERRUPT_PARAMETERS;

typedef
KSTATUS
(*PDRIVER_ENTRY) (
    PDRIVER Driver
    );

/*++

Routine Description:

    This routine is called when a driver is first loaded before any devices
    have attached to it. It normally registers its dispatch routines with the
    system, and performs any driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object. This structure should
        not be modified by the driver directly.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver encountered an error such as a resource
    allocation failure. If a driver fails this routine, it will get unloaded.

--*/

typedef enum _IO_OBJECT_TYPE {
    IoObjectInvalid,
    IoObjectRegularDirectory,
    IoObjectRegularFile,
    IoObjectBlockDevice,
    IoObjectCharacterDevice,
    IoObjectPipe,
    IoObjectObjectDirectory,
    IoObjectSocket,
    IoObjectTerminalMaster,
    IoObjectTerminalSlave,
    IoObjectSharedMemoryObject,
    IoObjectSymbolicLink,
    IoObjectTypeCount
} IO_OBJECT_TYPE, *PIO_OBJECT_TYPE;

/*++

Structure Description:

    This structure defines a directory entry, the listing of one file within a
    directory. The null terminated name of the entry immediately follows this
    structure.

Members:

    FileId - Stores the serial number of the file.

    NextOffset - Stores the file offset to the next directory entry. The dot
        and dot-dot entries always occupy offsets 0 and 1, so the first offset
        passed to a driver is DIRECTORY_CONTENTS_OFFSET.

    Size - Stores the size of the entire entry, including the size of this
        structure plus the size of the null-terminated name after it, including
        the null terminator byte.

    Type - Stores the type of the directory entry. This is of type
        IO_OBJECT_TYPE. Other flags may be added to this field in the future.

--*/

typedef struct _DIRECTORY_ENTRY {
    FILE_ID FileId;
    ULONGLONG NextOffset;
    USHORT Size;
    UCHAR Type;
} PACKED DIRECTORY_ENTRY, *PDIRECTORY_ENTRY;

/*++

Structure Description:

    This structure stores properties and characteristics of a file object.

Members:

    DeviceId - Stores the device number on which this file exists.

    FileId - Stores a unique number representing this file on this volume.
        Generally this is the starting block on the disk where the file data
        resides.

    Type - Stores the type of file (regular file, directory, etc).

    Permissions - Stores the file permissions.

    HardLinkCount - Stores the number of hard links that exist for this file.

    UserId - Stores the user ID of the file owner.

    GroupId - Stores the group ID of the file owner.

    RelatedDevice - Stores the device ID of the related device for certain
        special device types.

    Size - Stores the size of this file entity, in bytes.

    AccessTime - Stores the last time this file was accessed.

    ModifiedTime - Stores the last time this file was written to or truncated.
        This is not updated for status updates like change of ownership,
        permissions, hard link count.

    StatusChangeTime - Stores the last time this file's status was changed.
        This includes a change in the file's ownership, permissions, or
        hard link count.

    CreationTime - Stores the file creation time.

    BlockSize - Stores the size of a block on this file system.

    BlockCount - Stores the number of blocks allocated for this file.

    Flags - Stores user defined flags.

    Generation - Stores the file generation number.

--*/

typedef struct _FILE_PROPERTIES {
    DEVICE_ID DeviceId;
    FILE_ID FileId;
    IO_OBJECT_TYPE Type;
    FILE_PERMISSIONS Permissions;
    LONG HardLinkCount;
    USER_ID UserId;
    GROUP_ID GroupId;
    DEVICE_ID RelatedDevice;
    IO_OFFSET Size;
    SYSTEM_TIME AccessTime;
    SYSTEM_TIME ModifiedTime;
    SYSTEM_TIME StatusChangeTime;
    SYSTEM_TIME CreationTime;
    IO_OFFSET BlockSize;
    IO_OFFSET BlockCount;
    ULONG Flags;
    ULONG Generation;
} FILE_PROPERTIES, *PFILE_PROPERTIES;

/*++

Structure Description:

    This structure defines the parameters for a request to set file information.

Members:

    FieldsToSet - Supplies the bitmask of fields to set. See
        FILE_PROPERY_FIELD_* definitions. If this value is zero, then all the
        fields will be retrieved rather than any being set.

    FileProperties - Stores a pointer to the file properties to get or set.

--*/

typedef struct _SET_FILE_INFORMATION {
    ULONG FieldsToSet;
    PFILE_PROPERTIES FileProperties;
} SET_FILE_INFORMATION, *PSET_FILE_INFORMATION;

/*++

Structure Description:

    This structure defines a link between an I/O object state and a particular
    file descriptor that has signed up for asynchronous signals.

Members:

    ListEntry - Stores pointers to the next and previous receivers in the I/O
        state list.

    Descriptor - Stores the descriptor number that signed up for notifications.

    ProcessId - Stores the identifier of the process that signed up for
        asynchronous I/O.

--*/

typedef struct _ASYNC_IO_RECEIVER {
    LIST_ENTRY ListEntry;
    HANDLE Descriptor;
    PROCESS_ID ProcessId;
} ASYNC_IO_RECEIVER, *PASYNC_IO_RECEIVER;

/*++

Structure Description:

    This structure defines the asynchronous state associated with an I/O
    object.

Members:

    Owner - Stores the owning process of this IO object state. This is the
        process that receives signals when the IO object state changes.

    SetterUserId - Stores the real user ID of the user that set the owner.

    SetterEffectiveUserId - Stores the effective user ID of the user that set
        the owner.

    SetterPermissions - Stores the permission set of the process that set the
        owner.

    Signal - Stores the signal to send to the owner. If zero, then asynchronous
        signaling is not enabled.

    ReceiverList - Stores the head of the list of I/O handles that have agreed
        to get asynchronous signals.

    Lock - Stores a pointer to the lock protecting the list.

--*/

typedef struct _IO_ASYNC_STATE {
    PROCESS_ID Owner;
    USER_ID SetterUserId;
    USER_ID SetterEffectiveUserId;
    PERMISSION_SET SetterPermissions;
    ULONG Signal;
    LIST_ENTRY ReceiverList;
    PQUEUED_LOCK Lock;
} IO_ASYNC_STATE, *PIO_ASYNC_STATE;

/*++

Structure Description:

    This structure defines generic state associated with an I/O object.

Members:

    ReadEvent - Stores a pointer to an event signaled when the I/O handle can
        be read from without blocking.

    ReadHighPriorityEvent - Stores a pointer to an event signaled when high
        priority data can be read from the handle without blocking.

    WriteEvent - Stores a pointer to an event signaled when the I/O handle can
        be written to without blocking.

    WriteHighPriorityEvent - Stores a pointer to an event signaled when high
        priority data can be written to the I/O handle without blocking.

    ErrorEvent - Stores a pointer to an event signaled when there is an error
        regarding the I/O handle.

    Events - Stores a bitmask of events that have occurred for the I/O handle.
        See POLL_EVENT_* for definitions.

    Async - Stores an optional pointer to the asynchronous object state.

--*/

typedef struct _IO_OBJECT_STATE {
    PKEVENT ReadEvent;
    PKEVENT ReadHighPriorityEvent;
    PKEVENT WriteEvent;
    PKEVENT WriteHighPriorityEvent;
    PKEVENT ErrorEvent;
    volatile ULONG Events;
    PIO_ASYNC_STATE Async;
} IO_OBJECT_STATE, *PIO_OBJECT_STATE;

typedef enum _IRP_MAJOR_CODE {
    IrpMajorInvalid,
    IrpMajorStateChange,
    IrpMajorOpen,
    IrpMajorClose,
    IrpMajorIo,
    IrpMajorSystemControl,
    IrpMajorUserControl
} IRP_MAJOR_CODE, *PIRP_MAJOR_CODE;

typedef enum _IRP_MINOR_CODE {
    IrpMinorInvalid,
    IrpMinorStateChangeInvalid = 0x1000,
    IrpMinorQueryResources,
    IrpMinorStartDevice,
    IrpMinorQueryChildren,
    IrpMinorQueryInterface,
    IrpMinorRemoveDevice,
    IrpMinorIdle,
    IrpMinorSuspend,
    IrpMinorResume,
    IrpMinorOpenInvalid = 0x2000,
    IrpMinorOpen,
    IrpMinorCloseInvalid = 0x3000,
    IrpMinorClose,
    IrpMinorIoInvalid = 0x4000,
    IrpMinorIoRead,
    IrpMinorIoWrite,
    IrpMinorSystemControlInvalid = 0x5000,
    IrpMinorSystemControlLookup,
    IrpMinorSystemControlCreate,
    IrpMinorSystemControlWriteFileProperties,
    IrpMinorSystemControlUnlink,
    IrpMinorSystemControlRename,
    IrpMinorSystemControlTruncate,
    IrpMinorSystemControlDelete,
    IrpMinorSystemControlDeviceInformation,
    IrpMinorSystemControlGetBlockInformation,
    IrpMinorSystemControlSynchronize,
} IRP_MINOR_CODE, *PIRP_MINOR_CODE;

typedef enum _IRP_DIRECTION {
    IrpDirectionInvalid,
    IrpDown,
    IrpUp
} IRP_DIRECTION, *PIRP_DIRECTION;

/*++

Structure Description:

    This structure defines a query resources request in an IRP.

Members:

    ResourceRequirements - Stores a pointer to a list of possible resource
        configurations. If this pointer is not filled in, the system assumes
        the device needs no resources.

    BootAllocation - Stores an optional pointer to the resources the device
        has been assigned by the BIOS.

--*/

typedef struct _IRP_QUERY_RESOURCES {
    PRESOURCE_CONFIGURATION_LIST ResourceRequirements;
    PRESOURCE_ALLOCATION_LIST BootAllocation;
} IRP_QUERY_RESOURCES, *PIRP_QUERY_RESOURCES;

/*++

Structure Description:

    This structure defines a start device request in an IRP.

Members:

    ProcessorLocalResources - Stores the resources assigned to the device,
        as seen from the perspective of the CPU complex. This will most
        likely be used by the functional driver to interact with the device.

    BusLocalResources - Stores the resources assigned to the device, as
        seen from the perspective of the bus that enumerated the device.
        This will most likely be used by the bus driver to program any
        bus-specific aspects of the device.

--*/

typedef struct _IRP_START_DEVICE {
    PRESOURCE_ALLOCATION_LIST ProcessorLocalResources;
    PRESOURCE_ALLOCATION_LIST BusLocalResources;
} IRP_START_DEVICE, *PIRP_START_DEVICE;

/*++

Structure Description:

    This structure defines a query children request in an IRP.

Members:

    Children - Stores the address of an array of device pointers. This is
        the list of children reported by the bus. This pointer is expected
        to be allocated from paged pool, and will be freed by the I/O
        manager itself.

    ChildCount - Stores the number of device pointers that are in the list.

--*/

typedef struct _IRP_QUERY_CHILDREN {
    PDEVICE *Children;
    ULONG ChildCount;
} IRP_QUERY_CHILDREN, *PIRP_QUERY_CHILDREN;

/*++

Structure Description:

    This structure defines a query interface request in an IRP.

Members:

    Interface - Stores a pointer to the interface UUID being requested. The
        caller sets this parameter.

    InterfaceBuffer - Stores a pointer to a buffer allocated by the caller
        where the interface will be returned on success. Note that the
        entity requesting the interface is responsible for allocating and
        managing this buffer.

    InterfaceBufferSize - Stores the size of the interface buffer allocated.
        This is set up by the entity requesting the interface.

--*/

typedef struct _IRP_QUERY_INTERFACE {
    PUUID Interface;
    PVOID InterfaceBuffer;
    ULONG InterfaceBufferSize;
} IRP_QUERY_INTERFACE, *PIRP_QUERY_INTERFACE;

/*++

Structure Description:

    This structure defines the parameters for an idle request IRP.

Members:

    ExpectedDuration - Stores the expected duration of the idle period, in
        time counter ticks.

--*/

typedef struct _IRP_IDLE {
    ULONGLONG ExpectedDuration;
} IRP_IDLE, *PIRP_IDLE;

/*++

Structure Description:

    This structure defines an open file or device request in an IRP.

Members:

    FileProperties - Stores a pointer to the properties of the file to open.

    IoState - Stores an optional pointer to the I/O object state for the file.
        For the same device and file ID this will always be the same.

    DesiredAccess - Stores the desired access flags. See IO_ACCESS_*
        definitions.

    OpenFlags - Stores additional flags about how the file or device should be
        opened. See OPEN_FLAG_* definitions.

    DeviceContext - Stores a pointer where the device driver can store a
        pointer of context associated with this open operation. This
        pointer will be passed to the device to uniquely identify it for
        reads, writes, closes, and other operations.

    IoHandle - Stores a pointer to the I/O handle being initialized with this
        open.

--*/

typedef struct _IRP_OPEN {
    PFILE_PROPERTIES FileProperties;
    PIO_OBJECT_STATE IoState;
    ULONG DesiredAccess;
    ULONG OpenFlags;
    PVOID DeviceContext;
    PIO_HANDLE IoHandle;
} IRP_OPEN, *PIRP_OPEN;

/*++

Structure Description:

    This structure defines a close file or device request in an IRP.

Members:

    DeviceContext - Stores a pointer to the device context supplied by the
        device driver upon opening the device. This is used to uniquely
        identify the open file.

--*/

typedef struct _IRP_CLOSE {
    PVOID DeviceContext;
} IRP_CLOSE, *PIRP_CLOSE;

/*++

Structure Description:

    This structure defines an I/O request's saved I/O buffer state.

Members:

    IoBuffer - Stores a pointer to the saved I/O buffer.

    Flags - Stores a bitmask of flags describing the type of I/O buffer saved.
        See IRP_IO_BUFFER_STATE_FLAG_* for definitions.

--*/

typedef struct _IRP_IO_BUFFER_STATE {
    PIO_BUFFER IoBuffer;
    ULONG Flags;
} IRP_IO_BUFFER_STATE, *PIRP_IO_BUFFER_STATE;

/*++

Structure Description:

    This structure defines an I/O request in an IRP.

Members:

    DeviceContext - Stores a pointer to the device context supplied by the
        device driver upon opening the device. This is used to uniquely
        identify the open file.

    IoBuffer - Stores a pointer to the read or write buffer supplied by the
        caller.

    IoBufferState - Stoers a pointer to the internal state used for bounce
        buffering.

    IoFlags - Stores flags governing the behavior of the I/O. See
        IO_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    IoOffset - Stores the offset, in bytes, from the beginning of the file
        where the read or write should be performed.

    IoSizeInBytes - Stores the size of the I/O operation, in bytes.

    IoBytesCompleted - Stores the number of bytes of I/O actually performed.
        This number is to be filled out by the entity completing the IRP.

    NewIoOffset - Stores the new current file position. Normally this is
        just the offset plus the bytes completed, but sometimes (especially
        for directories) it doesn't have to map that way.

    FileProperties - Stores a pointer to the properties of the file on which
        the I/O is to be completed.

--*/

typedef struct _IRP_READ_WRITE {
    PVOID DeviceContext;
    PIO_BUFFER IoBuffer;
    IRP_IO_BUFFER_STATE IoBufferState;
    ULONG IoFlags;
    ULONG TimeoutInMilliseconds;
    IO_OFFSET IoOffset;
    UINTN IoSizeInBytes;
    UINTN IoBytesCompleted;
    IO_OFFSET NewIoOffset;
    PFILE_PROPERTIES FileProperties;
} IRP_READ_WRITE, *PIRP_READ_WRITE;

/*++

Structure Description:

    This structure defines a system control request in an IRP.

Members:

    SystemContext - Stores a pointer to the system context storing the
        information corresponding to the given IRP minor code.

--*/

typedef struct _IRP_SYSTEM_CONTROL {
    PVOID SystemContext;
} IRP_SYSTEM_CONTROL, *PIRP_SYSTEM_CONTROL;

/*++

Structure Description:

    This structure defines a user control request in an IRP.

Members:

    FromKernelMode - Supplies a boolean indicating if the request comes from
        user mode (FALSE) or kernel mode (TRUE). If it comes from user mode,
        the driver must not access the user buffer directly, but instead use
        MM copy routines to copy to and from user mode.

    DeviceContext - Stores a pointer to the device context supplied by the
        device driver upon opening the device. This is used to uniquely
        identify the open file.

    UserBuffer - Supplies a pointer to the buffer containing the context for
        the user control IRP. This will be a user mode pointer and must be
        treated with caution.

    UserBufferSize - Supplies the size of the buffer as reported by user mode.
        Again, this must be treated with suspicion by drivers.

--*/

typedef struct _IRP_USER_CONTROL {
    BOOL FromKernelMode;
    PVOID DeviceContext;
    PVOID UserBuffer;
    UINTN UserBufferSize;
} IRP_USER_CONTROL, *PIRP_USER_CONTROL;

/*++

Structure Description:

    This structure defines an I/O Request Packet (IRP).

Members:

    Header - Stores the standard object manager header.

    Device - Stores a pointer to the device this IRP relates to.

    MajorCode - Stores the major action code of the IRP.

    MinorCode - Stores the minor action code of the IRP.

    Direction - Stores the direction the IRP is travelling. "Down" means the
        IRP is heading towards low-level drivers (the bus driver). "Up" means
        the IRP has been completed and is heading back up towards higher level
        functional drivers.

    Status - Stores the completion status of the IRP.

    CompletionRoutine - Stores a pointer to a routine to call once the IRP is
        complete.

    CompletionContext - Stores an opaque pointer that the sender of the IRP can
        use to store context for the completion callback routine. It will be
        passed to the completion routine.

    ListEntry - Stores a list entry that the current driver processing the IRP
        can use to put the IRP on a list. Drivers that have just gained control
        of the IRP in either direction should not make assumptions about its
        state.

    QueryResources - Stores the results from a Query Resources IRP.

    StartDevice - Stores the parameters to a Start Device IRP.

    QueryChildren - Stores the results from a Query Children IRP.

    QueryInterface - Stores the parameters and results from a Query Interface
        IRP.

    Idle - Stores the parameters for an Idle IRP.

    Open - Stores the parameters and results from an Open IRP.

    Close - Stores the parameters from a Close IRP.

    ReadWrite - Stores the parameters for a Read or Write IRP.

    SystemControl - Stores the parameters for a System Control IRP.

    UserControl - Stores the parameters form a User Control IRP (with pointers
        directly from user mode).

--*/

struct _IRP {
    OBJECT_HEADER Header;
    PDEVICE Device;
    IRP_MAJOR_CODE MajorCode;
    IRP_MINOR_CODE MinorCode;
    IRP_DIRECTION Direction;
    KSTATUS Status;
    PIRP_COMPLETION_ROUTINE CompletionRoutine;
    PVOID CompletionContext;
    LIST_ENTRY ListEntry;
    union {
        IRP_QUERY_RESOURCES QueryResources;
        IRP_START_DEVICE StartDevice;
        IRP_QUERY_CHILDREN QueryChildren;
        IRP_QUERY_INTERFACE QueryInterface;
        IRP_IDLE Idle;
        IRP_OPEN Open;
        IRP_CLOSE Close;
        IRP_READ_WRITE ReadWrite;
        IRP_SYSTEM_CONTROL SystemControl;
        IRP_USER_CONTROL UserControl;
    } U;
};

/*++

Structure Description:

    This structure describes a block I/O device.

Members:

    DeviceToken - Stores an opaque pointer that uniquely identifies this device.

    BlockSize - Stores the native block size, in bytes, of this device.

    BlockCount - Stores the number of blocks contained in this device.

--*/

typedef struct _BLOCK_DEVICE_PARAMETERS {
    PVOID DeviceToken;
    ULONG BlockSize;
    ULONGLONG BlockCount;
} BLOCK_DEVICE_PARAMETERS, *PBLOCK_DEVICE_PARAMETERS;

/*++

Structure Description:

    This structure defines the information sent to a file system when the
    system requests that the file system look up the ID of a file.

Members:

    Root - Stores a boolean indicating if the system would like to look up the
        root entry for this device. If so, the directory file ID, file name,
        and file name size should be ignored.

    Flags - Stores a bitmask of flags returned by lookup. See LOOKUP_FLAGS_*
        for definitions.

    MapFlags - Supplies a bitmask of additional map flags to apply when mapping
        physical addresses returned from doing I/O on this file object. See
        MAP_FLAG_* definitions.

    DirectoryProperties - Stores a pointer to the properties of the directory
        file that is to be searched.

    FileName - Stores a pointer to the name of the file, which may not be
        null terminated.

    FileNameSize - Stores the size of the file name buffer including space
        for a null terminator (which may be a null terminator or may be a
        garbage character).

    Properties - Stores a pointer where the file properties are returned by the
        driver upon success.

--*/

typedef struct _SYSTEM_CONTROL_LOOKUP {
    BOOL Root;
    ULONG Flags;
    ULONG MapFlags;
    PFILE_PROPERTIES DirectoryProperties;
    PCSTR FileName;
    ULONG FileNameSize;
    PFILE_PROPERTIES Properties;
} SYSTEM_CONTROL_LOOKUP, *PSYSTEM_CONTROL_LOOKUP;

/*++

Structure Description:

    This structure defines the information sent to a file system for
    flushing file properties and unlink.

Members:

    File - Stores a pointer to the properties of the target file.

    DeviceContext - Stores a pointer to the open device context for the file if
        there is one. This is filled in for some operations (like truncate),
        but not all.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

--*/

typedef struct _SYSTEM_CONTROL_FILE_OPERATION {
    PFILE_PROPERTIES FileProperties;
    PVOID DeviceContext;
    ULONG Flags;
} SYSTEM_CONTROL_FILE_OPERATION, *PSYSTEM_CONTROL_FILE_OPERATION;

/*++

Structure Description:

    This structure defines the information sent to a file system when the
    system requests that the file system create a new file or directory.

Members:

    Directory - Stores a pointer to the file properties of the directory.

    DirectorySize - Stores the extent of the directory written to create the
        new file. This may be less than the actual directory size but the
        system will only update the directory size if it is larger than the
        currently recorded size.

    Name - Stores a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Stores the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a garbage
        character).

    Properties - Stores the file properties of the created file on success. The
        permissions, object type, user ID, group ID, and access times are all
        valid from the system.

--*/

typedef struct _SYSTEM_CONTROL_CREATE {
    PFILE_PROPERTIES DirectoryProperties;
    ULONGLONG DirectorySize;
    PCSTR Name;
    ULONG NameSize;
    FILE_PROPERTIES FileProperties;
} SYSTEM_CONTROL_CREATE, *PSYSTEM_CONTROL_CREATE;

/*++

Structure Description:

    This structure defines the information sent to a file system when the
    system requests that the file system unlink a directory entry.

Members:

    Directory - Stores a pointer to the file properties of the directory that
        contains the entry to unlink.

    File - Stores a pointer to the file properties of the file that is being
        unlinked.

    Name - Stores a pointer to the name of the file or directory to unlink,
        which may not be null terminated.

    NameSize - Stores the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a garbage
        character).

    Unlinked - Stores a boolean indicating whether or not the directory entry
        was successfully unlinked. An unlink can occur even in some failure
        cases.

--*/

typedef struct _SYSTEM_CONTROL_UNLINK {
    PFILE_PROPERTIES DirectoryProperties;
    PFILE_PROPERTIES FileProperties;
    PCSTR Name;
    ULONG NameSize;
    BOOL Unlinked;
} SYSTEM_CONTROL_UNLINK, *PSYSTEM_CONTROL_UNLINK;

/*++

Structure Description:

    This structure defines the information sent to a file system when the
    system requests that the file system rename a file or directory.

Members:

    SourceDirectoryProperties - Stores a pointer to the file properties of the
        directory containing the file to rename.

    SourceFileProperties - Stores a pointer to the file properties of the file
        to rename.

    DestinationDirectoryProperties - Stores a pointer to the file properties of
        the directory where the named file will reside.

    DestinationFileProperties - Stores an optional pointer to the file
        properties of the file currently sitting at the destination (that will
        need to be unlinked). If there is no file or directory at the
        destination, then this will be NULL.

    DestinationDirectorySize - Stores the extent of the directory written to
        create the new file. This may be less than the actual directory size
        but the system will only update the directory size if it is larger than
        the currently recorded size.

    SourceFileHardLinkDelta - Stores a value indicating a delta (if any) in
        hard links to the source file that was a result of the rename
        operation. Callers should observe this value especially when the call
        fails.

    DestinationFileUnlinked - Stores a boolean indicating whether or not the
        destination file (if any) was unlinked during the process of this
        rename operation. Callers should observe this value especially when the
        call fails.

    Name - Stores a pointer to the string containing the destination
        file/directory name, which may not be null terminated.

    NameSize - Stores the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a garbage
        character).

--*/

typedef struct _SYSTEM_CONTROL_RENAME {
    PFILE_PROPERTIES SourceDirectoryProperties;
    PFILE_PROPERTIES SourceFileProperties;
    PFILE_PROPERTIES DestinationDirectoryProperties;
    PFILE_PROPERTIES DestinationFileProperties;
    ULONGLONG DestinationDirectorySize;
    ULONG SourceFileHardLinkDelta;
    BOOL DestinationFileUnlinked;
    PSTR Name;
    ULONG NameSize;
} SYSTEM_CONTROL_RENAME, *PSYSTEM_CONTROL_RENAME;

/*++

Structure Description:

    This structure defines the information sent to a file system for a truncate
    operation.

Members:

    File - Stores a pointer to the properties of the target file.

    DeviceContext - Stores a pointer to the open device context for the file if
        there is one. This is filled in for some operations (like truncate),
        but not all.

    NewSize - Stores the new size to truncate the file to.

--*/

typedef struct _SYSTEM_CONTROL_TRUNCATE {
    PFILE_PROPERTIES FileProperties;
    PVOID DeviceContext;
    ULONGLONG NewSize;
} SYSTEM_CONTROL_TRUNCATE, *PSYSTEM_CONTROL_TRUNCATE;

/*++

Structure Description:

    This structure defines a device information result returned as an array
    from an enumeration.

Members:

    Uuid - Stores the universally unique identifier of the device information
        type.

    Device - Stores the device ID of the device that enumerates this
        information type.

--*/

typedef struct _DEVICE_INFORMATION_RESULT {
    UUID Uuid;
    DEVICE_ID DeviceId;
} DEVICE_INFORMATION_RESULT, *PDEVICE_INFORMATION_RESULT;

/*++

Structure Description:

    This structure defines a device information header.

Members:

    Uuid - Stores the universally unique identifier of the device information
        type.

    Data - Stores a pointer where the device information will be returned on
        success for "get" operations or to the data to set for "set" operations.

    DataSize - Stores a value that upon input contains the size of the data
        buffer in bytes. On output, returns the required size of the data
        buffer in bytes.

    Set - Stores a boolean indicating whether to retrieve device information
        (FALSE) or set device information (TRUE).

--*/

typedef struct _SYSTEM_CONTROL_DEVICE_INFORMATION {
    UUID Uuid;
    PVOID Data;
    UINTN DataSize;
    BOOL Set;
} SYSTEM_CONTROL_DEVICE_INFORMATION, *PSYSTEM_CONTROL_DEVICE_INFORMATION;

/*++

Structure Description:

    This structure defines a run of contiguous blocks for a file or partition.

Members:

    ListEntry - Stores pointers to the next and previous runs of contiguous
        blocks.

    Address - Stores the logical block start address of the run.

    Count - Stores the number of blocks in the run.

--*/

typedef struct _FILE_BLOCK_ENTRY {
    LIST_ENTRY ListEntry;
    ULONGLONG Address;
    ULONGLONG Count;
} FILE_BLOCK_ENTRY, *PFILE_BLOCK_ENTRY;

/*++

Structure Description:

    This structure defines block information that can be retrieved for a file
    or partition.

Members:

    BlockList - Stores the head of a list of contiguous disk blocks that
        comprise the file or partition.

--*/

typedef struct _FILE_BLOCK_INFORMATION {
    LIST_ENTRY BlockList;
} FILE_BLOCK_INFORMATION, *PFILE_BLOCK_INFORMATION;

/*++

Structure Description:

    This structure defines a block information request.

Members:

    FileProperties - Supplies a pointer to the file properties of the file or
        partition whose block information is being requested.

    FileBlockInformation - Supplies a pointer that receives a block information
        structure for the file or partition.

--*/

typedef struct _SYSTEM_CONTROL_GET_BLOCK_INFORMATION {
    PFILE_PROPERTIES FileProperties;
    PFILE_BLOCK_INFORMATION FileBlockInformation;
} SYSTEM_CONTROL_GET_BLOCK_INFORMATION, *PSYSTEM_CONTROL_GET_BLOCK_INFORMATION;

/*++

Structure Description:

    This structure defines the information necessary to direct disk block-level
    I/O to a file.

Members:

    DiskToken - Stores an opaque token to disk device context.

    BlockSize - Stores the size of each block on disk, in bytes.

    BlockCount - Stores the total number of blocks on the disk.

    BlockIoReset - Stores a pointer to a routine that allows the device to
        reset any I/O paths in preparation for imminent block I/O.

    BlockRead - Stores a pointer to a routine that can do direct block-level
        reads from the disk.

    BlockWrite - Stores a pointer to a routine that can do direct block-level
        writes to a disk.

    FileBlockInformation - Stores a pointer to the block information for the
        file that is being read or written. This includes a list of contiguous
        block runs.

--*/

typedef struct _FILE_BLOCK_IO_CONTEXT {
    PVOID DiskToken;
    ULONG BlockSize;
    ULONGLONG BlockCount;
    PVOID BlockIoReset;
    PVOID BlockIoRead;
    PVOID BlockIoWrite;
    PFILE_BLOCK_INFORMATION FileBlockInformation;
} FILE_BLOCK_IO_CONTEXT, *PFILE_BLOCK_IO_CONTEXT;

/*++

Structure Description:

    This structure defines an entry in an array of mount points.

Members:

    Flags - Stores the flags associated with the mount point.

    MountPointPathOffset - Stores the location of the mount point path string
        as an offset from this structures base address.

    TargetPathOffset - Stores the location of the target path string as an
        offset from this structures base address.

--*/

typedef struct _MOUNT_POINT_ENTRY {
    ULONG Flags;
    UINTN MountPointPathOffset;
    UINTN TargetPathOffset;
} MOUNT_POINT_ENTRY, *PMOUNT_POINT_ENTRY;

/*++

Structure Description:

    This structure defines a set of I/O cache statistics.

Members:

    Version - Stores the version information for this structure. Set this to
        IO_CACHE_STATISTICS_VERSION.

    HeadroomPagesTrigger - Stores the number of free physical pages in the
        system below which the page count will begin evicting entries to
        conserve memory.

    HeadroomPagesRetreat - Stores the number of free physical pages in the
        system the page cache will shoot for once it begins a headroom-based
        eviction of pages.

    MinimumPagesTarget - Stores the target minimum size of the page cache. The
        page cache makes an effort to maintain this minimum by requesting other
        pages be paged out when it falls below this size.

    MinimumPages - Stores the size below which the page cache will not attempt
        to shrink.

    PhysicalPageCount - Stores the current number of physical pages consumed by
        the cache.

    DirtyPageCount - Stores the number of physical pages in the cache that are
        currently dirty.

    LastCleanTime - Stores a time counter value for the last time the page
        cache was cleaned.

--*/

typedef struct _IO_CACHE_STATISTICS {
    ULONG Version;
    UINTN HeadroomPagesTrigger;
    UINTN HeadroomPagesRetreat;
    UINTN MinimumPagesTarget;
    UINTN MinimumPages;
    UINTN PhysicalPageCount;
    UINTN DirtyPageCount;
    ULONGLONG LastCleanTime;
} IO_CACHE_STATISTICS, *PIO_CACHE_STATISTICS;

/*++

Structure Description:

    This structure defines a set of I/O cache statistics.

Members:

    Version - Stores the version information for this structure. Set this to
        IO_GLOBAL_STATISTICS_VERSION.

    BytesRead - Stores the total number of bytes read in.

    BytesWritten - Stores the total number of bytes written out.

    PagingBytesRead - Stores the number of bytes read in from the page file.

    PagingBytesWritten - Stores the number of bytes written to the page file.

--*/

typedef struct _IO_GLOBAL_STATISTICS {
    ULONG Version;
    ULONGLONG BytesRead;
    ULONGLONG BytesWritten;
    ULONGLONG PagingBytesRead;
    ULONGLONG PagingBytesWritten;
} IO_GLOBAL_STATISTICS, *PIO_GLOBAL_STATISTICS;

/*++

Structure Description:

    This structure defines system boot information.

Members:

    SystemDiskIdentifier - Stores the identifier of the disk the running system
        is located on.

    SystemPartitionIdentifier - Stores the identifier of the partition the
        running system is located on.

    BootTime - Stores the time the system was booted.

--*/

typedef struct _IO_BOOT_INFORMATION {
    UCHAR SystemDiskIdentifier[16];
    UCHAR SystemPartitionIdentifier[16];
    SYSTEM_TIME BootTime;
} IO_BOOT_INFORMATION, *PIO_BOOT_INFORMATION;

/*++

Structure Description:

    This structure defines a path in the context of its mount point.

Members:

    PathEntry - Stores a pointer to a path entry.

    MountPoint - Stores a pointer to the mount point that governs the path
        entry.

--*/

typedef struct _PATH_POINT {
    PPATH_ENTRY PathEntry;
    PMOUNT_POINT MountPoint;
} PATH_POINT, *PPATH_POINT;

/*++

Structure Description:

    This structure defines a loaded file.

Members:

    Version - Stores the structure version. Future revisions will be backwards
        compatible. Set to LOADED_FILE_VERSION.

    IoHandle - Stores an open handle to the loaded file.

    Length - Stores the length of the loaded file.

    IoBuffer - Stores a pointer to an I/O buffer that stores the loaded file's
        data.

--*/

typedef struct _LOADED_FILE {
    ULONG Version;
    PIO_HANDLE IoHandle;
    UINTN Length;
    PIO_BUFFER IoBuffer;
} LOADED_FILE, *PLOADED_FILE;

typedef
VOID
(*PLOAD_FILE_COMPLETION_ROUTINE) (
    PVOID Context,
    PLOADED_FILE File
    );

/*++

Routine Description:

    This routine is called when a file load has completed.

Arguments:

    Context - Supplies the context supplied by the caller who initiated the
        file load.

    File - Supplies a pointer to the loaded file.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the permission set for a shared memory object. This
    lines up with struct ipc_perm in the C library.

Members:

    OwnerUserId - Stores the user ID of the owner.

    OwnerGroupId - Stores the group ID of the owner.

    CreatorUserId - Stores the user ID of the creator.

    CreatorGroupId - Stores the group ID of the creator.

    Permissions - Stores the permission set for this object.

--*/

typedef struct _SHARED_MEMORY_PERMISSIONS {
    USER_ID OwnerUserId;
    GROUP_ID OwnerGroupId;
    USER_ID CreatorUserId;
    GROUP_ID CreatorGroupId;
    ULONG Permissions;
} SHARED_MEMORY_PERMISSIONS, *PSHARED_MEMORY_PERMISSIONS;

/*++

Structure Description:

    This structure defines the properties of a shared memory object. This
    structure lines up with struct shmid_ds in the C library.

Members:

    Permissions - Stores the permissions information for the object.

    Size - Stores the size of the shared memory object in bytes.

    AttachTime - Stores the last time an attach occurred.

    DetachTime - Stores the last time a detach occurred.

    ChangeTime - Stores the last time the object was changed (via a set).

    CreatorPid - Stores the process ID of the process that created this object.

    LastPid - Stores the process ID of the last process to operate on this
        object.

    AttachCount - Stores the number of active attachments.

--*/

typedef struct _SHARED_MEMORY_PROPERTIES {
    SHARED_MEMORY_PERMISSIONS Permissions;
    IO_OFFSET Size;
    SYSTEM_TIME AttachTime;
    SYSTEM_TIME DetachTime;
    SYSTEM_TIME ChangeTime;
    PROCESS_ID CreatorPid;
    PROCESS_ID LastPid;
    UINTN AttachCount;
} SHARED_MEMORY_PROPERTIES, *PSHARED_MEMORY_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
KSTATUS
IoCreateDevice (
    PDRIVER BusDriver,
    PVOID BusDriverContext,
    PDEVICE ParentDevice,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PDEVICE *NewDevice
    );

/*++

Routine Description:

    This routine creates a new device in the system. This device can be used in
    subsequent calls to Query Children.

Arguments:

    BusDriver - Supplies a pointer to the driver reporting this device.

    BusDriverContext - Supplies the context pointer that will be passed to the
        bus driver when IRPs are sent to the device.

    ParentDevice - Supplies a pointer to the device enumerating this device.
        Most devices are enumerated off of a bus, so this parameter will
        contain a pointer to that bus device. For unenumerable devices, this
        parameter can be NULL, in which case the device will be enumerated off
        of the root device.

    DeviceId - Supplies a pointer to a null terminated string identifying the
        device. This memory does not have to be retained, a copy of it will be
        created during this call.

    ClassId - Supplies a pointer to a null terminated string identifying the
        device class. This memory does not have to be retained, a copy of it
        will be created during this call.

    CompatibleIds - Supplies a semicolon-delimited list of device IDs that this
        device is compatible with.

    NewDevice - Supplies a pointer where the new device will be returned on
        success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoRemoveUnreportedDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine removes a device that was created but never reported. Devices
    created on enumerable busses must be removed by not reporting them in
    a query children request. This routine must only be called on devices whose
    parent device is the root.

Arguments:

    Device - Supplies a pointer to the device to remove.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
IoDeviceAddReference (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine increments the reference count on a device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoDeviceReleaseReference (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine decrements the reference count on a device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoSetTargetDevice (
    PDEVICE Device,
    PDEVICE TargetDevice
    );

/*++

Routine Description:

    This routine sets the target device for a given device. IRPs flow through
    a device and then through its target device (if not completed by an
    earlier driver). Target devices allow the piling of stacks on one another.
    Target device relations must be set either before the device is reported
    by the bus, or during AddDevice. They cannot be changed after that. This
    routine is not thread safe, as it's only expected to be called by drivers
    on the device during early device initialization.

Arguments:

    Device - Supplies a pointer to the device to set a target device for.

    TargetDevice - Supplies a pointer to the target device.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the device is already too far through its initialization
    to have a target device added to it.

--*/

KERNEL_API
PDEVICE
IoGetTargetDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns the target device for the given device, if any.

Arguments:

    Device - Supplies a pointer to the device to get the target device for.

Return Value:

    Returns a pointer to the target device.

    NULL if there is no target device.

--*/

PDEVICE
IoGetDiskDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns the underlying disk device for a given device.

Arguments:

    Device - Supplies a pointer to a device.

Return Value:

    Returns a pointer to the disk device that backs the device.

    NULL if the given device does not have a disk backing it.

--*/

KERNEL_API
VOID
IoSetDeviceMountable (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine indicates that the given device is mountable. A device cannot
    be unmarked as mountable. This routine is not thread safe.

Arguments:

    Device - Supplies a pointer to the device that the system could potentially
        mount.

Return Value:

    None.

--*/

KERNEL_API
BOOL
IoAreDeviceIdsEqual (
    PCSTR DeviceIdOne,
    PCSTR DeviceIdTwo
    );

/*++

Routine Description:

    This routine determines if the given device IDs match. This routine always
    truncates the given device IDs at the last '#' character, if it exists. If
    one of the supplied device IDs naturally has a '#' character within it,
    then caller should append a second '#' character to the device ID.

Arguments:

    DeviceIdOne - Supplies the first device ID.

    DeviceIdTwo - Supplies the second device ID.

Return Value:

    Returns TRUE if the given device IDs match. FALSE otherwise.

--*/

KERNEL_API
PCSTR
IoGetDeviceId (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns the device ID of the given system device.

Arguments:

    Device - Supplies the device to get the ID of.

Return Value:

    Returns a pionter to a string representing the device's Identifier.

--*/

KERNEL_API
PCSTR
IoGetCompatibleDeviceIds (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns a semilcolon-delimited list of device IDs that this
    device is compatible with.

Arguments:

    Device - Supplies the device to get the compatible IDs of.

Return Value:

    Returns a pointer to a semicolon-delimited string of device IDs that this
    device is compatible with, not including the actual device ID itself.

    NULL if the compatible ID list is empty.

--*/

KERNEL_API
PCSTR
IoGetDeviceClassId (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns the class ID of the given device.

Arguments:

    Device - Supplies the device to get the class ID of.

Return Value:

    Returns the class ID of the given device.

    NULL if the device was not created with a class ID.

--*/

KERNEL_API
BOOL
IoIsDeviceIdInCompatibleIdList (
    PCSTR DeviceId,
    PDEVICE Device
    );

/*++

Routine Description:

    This routine determines if the given device ID is present in the semicolon-
    delimited list of compatible device IDs of the given device, or matches
    the device ID itself.

    This routine must be called at Low level.

Arguments:

    DeviceId - Supplies the device ID in question.

    Device - Supplies the device whose compatible IDs should be queried.

Return Value:

    TRUE if the given device ID string is present in the device's compatible ID
        list.

    FALSE if the device ID string is not present in the compatible ID list.

--*/

KERNEL_API
DEVICE_ID
IoGetDeviceNumericId (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine gets the numeric device ID for the given device.

Arguments:

    Device - Supplies a pointer to the device whose numeric ID is being queried.

Return Value:

    Returns the numeric device ID for the device.

--*/

KERNEL_API
PDEVICE
IoGetDeviceByNumericId (
    DEVICE_ID DeviceId
    );

/*++

Routine Description:

    This routine looks up a device given its numeric device ID. This routine
    will increment the reference count of the device returned, it is the
    caller's responsibility to release that reference. Only devices that are in
    the started state will be returned. This routine must be called at low
    level.

Arguments:

    DeviceId - Supplies the numeric device ID of the device to get.

Return Value:

    Returns a pointer to the device with the given numeric device ID on
    success. This routine will add a reference to the device returned, it is
    the caller's responsibility to release this reference when finished with
    the device.

--*/

KERNEL_API
KSTATUS
IoMergeChildArrays (
    PIRP QueryChildrenIrp,
    PDEVICE *Children,
    ULONG ChildCount,
    ULONG AllocationTag
    );

/*++

Routine Description:

    This routine merges a device's enumerated children with the array that is
    already present in the Query Children IRP. If needed, a new array containing
    the merged list will be created and stored in the IRP, and the old list
    will be freed. If the IRP has no list yet, a copy of the array passed in
    will be created and set in the IRP.

Arguments:

    QueryChildrenIrp - Supplies a pointer to the Query Children IRP.

    Children - Supplies a pointer to the device children. This array will not be
        used in the IRP, so this array can be temporarily allocated.

    ChildCount - Supplies the number of elements in the pointer array.

    AllocationTag - Supplies the allocate tag to use for the newly created
        array.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the new array could not be allocated.

--*/

KERNEL_API
KSTATUS
IoNotifyDeviceTopologyChange (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine notifies the system that the device topology has changed for
    the given device. This routine is meant to be called by a device driver
    when it notices a child device is missing or when a new device arrives.

Arguments:

    Device - Supplies a pointer to the device whose topology has changed.

Return Value:

    None.

--*/

KERNEL_API
BOOL
IoIsDeviceStarted (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns whether or not the device is in the started state.

Arguments:

    Device - Supplies a pointer to the device in question.

Return Value:

    Returns TRUE if the device is in the started state, or FALSE otherwise.

--*/

KERNEL_API
VOID
IoSetDeviceDriverErrorEx (
    PDEVICE Device,
    KSTATUS Status,
    PDRIVER Driver,
    ULONG DriverCode,
    PCSTR SourceFile,
    ULONG LineNumber
    );

/*++

Routine Description:

    This routine sets a driver specific error code on a given device. This
    problem is preventing a device from making forward progress. Avoid calling
    this function directly, use the non-Ex version.

Arguments:

    Device - Supplies a pointer to the device with the error.

    Status - Supplies the failure status generated by the error.

    Driver - Supplies a pointer to the driver reporting the error.

    DriverError - Supplies an optional driver specific error code.

    SourceFile - Supplies a pointer to the source file where the problem
        occurred. This is usually automatically generated by the compiler.

    LineNumber - Supplies the line number in the source file where the problem
        occurred. This is usually automatically generated by the compiler.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoClearDeviceProblem (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine clears any problem code associated with a device, and attempts
    to start the device if it is not already started.

Arguments:

    Device - Supplies a pointer to the device to clear.

Return Value:

    STATUS_SUCCESS if the problem was successfully cleared and the start device
    work item was successfully queued (if needed).

    Error code if the start work item needed to be queued and had a problem.

--*/

KERNEL_API
KSTATUS
IoRegisterDriverFunctions (
    PDRIVER Driver,
    PDRIVER_FUNCTION_TABLE FunctionTable
    );

/*++

Routine Description:

    This routine is called by a driver to register its function pointers with
    the system. Drivers cannot be attached to the system until this is
    complete. This routine is usually called by a driver in its entry point.
    This routine should only be called once during the lifetime of a driver.

Arguments:

    Driver - Supplies a pointer to the driver whose routines are being
        registered.

    FunctionTable - Supplies a pointer to the function pointer table containing
        the drivers dispatch routines.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

--*/

KERNEL_API
KSTATUS
IoAttachDriverToDevice (
    PDRIVER Driver,
    PDEVICE Device,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called by a driver to attach itself to a device. Once
    attached, the driver will participate in all IRPs that go through to the
    device. This routine can only be called during a driver's AddDevice routine.

Arguments:

    Driver - Supplies a pointer to the driver attaching to the device.

    Device - Supplies a pointer to the device to attach to.

    Context - Supplies an optional context pointer that will be passed to the
        driver each time it is called in relation to this device.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_EARLY or STATUS_TOO_LATE if the routine was called outside of a
    driver's AddDevice routine.

    STATUS_INSUFFICIENT_RESOURCES if allocations failed.

--*/

KERNEL_API
VOID
IoDriverAddReference (
    PDRIVER Driver
    );

/*++

Routine Description:

    This routine increments the reference count on a driver.

Arguments:

    Driver - Supplies a pointer to the driver.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoDriverReleaseReference (
    PDRIVER Driver
    );

/*++

Routine Description:

    This routine decrements the reference count on a driver. This routine
    must be balanced by a previous call to add a reference on the driver.

Arguments:

    Driver - Supplies a pointer to the driver.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoGetIrpStatus (
    PIRP Irp
    );

/*++

Routine Description:

    This routine returns the IRP's completion status.

Arguments:

    Irp - Supplies a pointer to the IRP to query.

Return Value:

    Returns the IRP completion status. If no driver has completed the IRP,
    STATUS_NOT_HANDLED will be returned (the initialization value put into the
    IRP).

--*/

KERNEL_API
VOID
IoUpdateIrpStatus (
    PIRP Irp,
    KSTATUS StatusCode
    );

/*++

Routine Description:

    This routine updates the IRP's completion status if the current completion
    status indicates success.

Arguments:

    Irp - Supplies a pointer to the IRP to query.

    StatusCode - Supplies a status code to associate with the completed IRP.
        This will be returned back to the caller requesting the IRP.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoCompleteIrp (
    PDRIVER Driver,
    PIRP Irp,
    KSTATUS StatusCode
    );

/*++

Routine Description:

    This routine is called by a driver to mark an IRP as completed. This
    function can only be called from a driver's dispatch routine when the
    driver owns the IRP. When the dispatch routine returns, the system will not
    continue to move down the driver stack, but will switch directions and
    move up the stack. Only one driver in the stack should complete the IRP.
    This routine must be called at or below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver completing the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to mark as
        completed.

    StatusCode - Supplies a status code to associated with the completed IRP.
        This will be returned back to the caller requesting the IRP.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoPendIrp (
    PDRIVER Driver,
    PIRP Irp
    );

/*++

Routine Description:

    This routine is called by a driver to mark an IRP as pending. This function
    can only be called from a driver's dispatch routine when the driver owns
    the IRP. When the dispatch routine returns, the system will not move to the
    next stack location: the driver will continue to own the IRP until it
    marks it completed or continues the IRP. This routine must be called at or
    below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver pending the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to mark as pending.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoContinueIrp (
    PDRIVER Driver,
    PIRP Irp
    );

/*++

Routine Description:

    This routine is called by a driver to continue processing an IRP that was
    previously marked pending. The driver that pended the IRP will get called
    a second time in the same directon for this IRP. This routine must be
    called at or below dispatch level.

Arguments:

    Driver - Supplies a pointer to the driver unpending the IRP.

    Irp - Supplies a pointer to the IRP owned by the driver to continue
        processing.

Return Value:

    None.

--*/

KERNEL_API
PIRP
IoCreateIrp (
    PDEVICE Device,
    IRP_MAJOR_CODE MajorCode,
    ULONG Flags
    );

/*++

Routine Description:

    This routine creates and initializes an IRP. This routine must be called
    at or below dispatch level.

Arguments:

    Device - Supplies a pointer to the device the IRP will be sent to.

    MajorCode - Supplies the major code of the IRP, which cannot be changed
        once an IRP is allocated (or else disaster ensues).

    Flags - Supplies a bitmask of IRP creation flags. See IRP_FLAG_* for
        definitions.

Return Value:

    Returns a pointer to the newly allocated IRP on success, or NULL on
    failure.

--*/

KERNEL_API
VOID
IoDestroyIrp (
    PIRP Irp
    );

/*++

Routine Description:

    This routine destroys an IRP, freeing all memory associated with it. This
    routine must be called at or below dispatch level.

Arguments:

    Irp - Supplies a pointer to the IRP to free.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoInitializeIrp (
    PIRP Irp
    );

/*++

Routine Description:

    This routine initializes an IRP and prepares it to be sent to a device.
    This routine does not mean that IRPs can be allocated randomly from pool
    and initialized here; IRPs must still be allocated from IoCreateIrp. This
    routine just resets an IRP back to its initialized state.

Arguments:

    Irp - Supplies a pointer to the initialized IRP to initialize. This IRP
        must already have a valid object header.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoSendSynchronousIrp (
    PIRP Irp
    );

/*++

Routine Description:

    This routine sends an initialized IRP down the device stack and does not
    return until the IRP completed. This routine must be called at or below
    dispatch level.

Arguments:

    Irp - Supplies a pointer to the initialized IRP to send. All parameters
        should already be filled out and ready to go.

Return Value:

    STATUS_SUCCESS if the IRP was actually sent properly. This says nothing of
    the completion status of the IRP, which may have failed spectacularly.

    STATUS_INVALID_PARAMETER if the IRP was not properly initialized.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

--*/

KERNEL_API
KSTATUS
IoPrepareReadWriteIrp (
    PIRP_READ_WRITE IrpReadWrite,
    UINTN Alignment,
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    ULONG Flags
    );

/*++

Routine Description:

    This routine prepares the given read/write IRP context for I/O based on the
    given physical address, physical alignment, and flag requirements. It will
    ensure that the IRP's I/O buffer is sufficient and preform any necessary
    flushing that is needed to prepare for the I/O.

Arguments:

    IrpReadWrite - Supplies a pointer to the IRP read/write context that needs
        to be prepared for data transfer.

    Alignment - Supplies the required physical alignment of the I/O buffer, in
        bytes.

    MinimumPhysicalAddress - Supplies the minimum allowed physical address for
        the I/O buffer.

    MaximumPhysicalAddress - Supplies the maximum allowed physical address for
        the I/O buffer.

    Flags - Supplies a bitmask of flags for the preparation. See
        IRP_READ_WRITE_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoCompleteReadWriteIrp (
    PIRP_READ_WRITE IrpReadWrite,
    ULONG Flags
    );

/*++

Routine Description:

    This routine handles read/write IRP completion. It will perform any
    necessary flushes based on the type of I/O (as indicated by the flags) and
    destroy any temporary I/O buffers created for the operation during the
    prepare step.

Arguments:

    IrpReadWrite - Supplies a pointer to the read/write context for the
        completed IRP.

    Flags - Supplies a bitmask of flags for the completion. See
        IRP_READ_WRITE_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoCreateInterface (
    PUUID InterfaceUuid,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize
    );

/*++

Routine Description:

    This routine creates a device interface. Interfaces start out disabled. The
    Interface/device pair must be unique, there cannot be two interfaces for
    the same UUID and device.

Arguments:

    Interface - Supplies a pointer to the UUID identifying the interface.

    Device - Supplies a pointer to the device exposing the interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer.

    InterfaceBufferSize - Supplies the size of the interface buffer, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the interface or device were not specified.

    STATUS_NO_MEMORY if allocations could not be made.

    STATUS_DUPLICATE_ENTRY if an interface already exists for this device.

--*/

KERNEL_API
KSTATUS
IoDestroyInterface (
    PUUID InterfaceUuid,
    PDEVICE Device,
    PVOID InterfaceBuffer
    );

/*++

Routine Description:

    This routine destroys a previously created interface. All parties
    registered for notifications on this interface will be notified that the
    interface is going down.

Arguments:

    InterfaceUuid - Supplies a pointer to the UUID identifying the interface.

    Device - Supplies a pointer to the device supplied on registration.

    InterfaceBuffer - Supplies a pointer to the interface buffer for the
        specific interface to tear down. This needs to match the interface
        buffer used when the interface was created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the interface or device is invalid.

    STATUS_NOT_FOUND if the interface could not be found.

--*/

KERNEL_API
KSTATUS
IoRegisterForInterfaceNotifications (
    PUUID Interface,
    PINTERFACE_NOTIFICATION_CALLBACK CallbackRoutine,
    PDEVICE Device,
    PVOID Context,
    BOOL NotifyForExisting
    );

/*++

Routine Description:

    This routine registers the given handler to be notified when the given
    interface arrives or disappears. Callers are notified of both events.
    Callers will be notified for all interface arrivals and removals of the
    given interface.

Arguments:

    Interface - Supplies a pointer to the UUID identifying the interface.

    CallbackRoutine - Supplies a pointer to the callback routine to notify
        when an interface arrives or is removed.

    Device - Supplies an optional pointer to a device. If this device is
        non-NULL, then interface notifications will be restricted to the given
        device.

    Context - Supplies an optional pointer to a context that will be passed
        back to the caller during notifications.

    NotifyForExisting - Supplies TRUE if the caller would like an arrival
        notification for every pre-existing interface, or FALSE if the caller
        only wants to be notified about future arrivals. The caller will be
        notified about ALL removals no matter the state of this flag.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid interface or callback routine was
        supplied.

    STATUS_NO_MEMORY if memory could not be allocated to satisfy the request.

    STATUS_INSUFFICIENT_RESOURCES if general resources could not be allocated to
        satisfy the request.

    STATUS_DUPLICATE_ENTRY if the given routine is already registered for
        notification on the device.

--*/

KERNEL_API
KSTATUS
IoUnregisterForInterfaceNotifications (
    PUUID Interface,
    PINTERFACE_NOTIFICATION_CALLBACK CallbackRoutine,
    PDEVICE Device,
    PVOID Context
    );

/*++

Routine Description:

    This routine de-registers the given handler from receiving device interface
    notifications. Once this routine returns, the given handler will not
    receive notifications for the given interface.

Arguments:

    Interface - Supplies a pointer to the UUID identifying the interface.

    CallbackRoutine - Supplies a pointer to the callback routine supplied on
        registration for notifications.

    Device - Supplies a pointer to the device supplied upon registration.

    Context - Supplies a pointer to the context supplied upon registration.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid interface or callback routine was
        supplied.

    STATUS_NOT_FOUND if no interface listener was found for the given callback
        routine on the given UUID.

--*/

KERNEL_API
KSTATUS
IoRegisterFileSystem (
    PDRIVER Driver
    );

/*++

Routine Description:

    This routine registers the given driver as a file system driver.

Arguments:

    Driver - Supplies a pointer to the driver registering the file system
        support.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoOpen (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a file, device, pipe, or other I/O object.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies a pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions to apply for a created file.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoOpenDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle,
    PULONG IoOffsetAlignment,
    PULONG IoSizeAlignment,
    PULONGLONG IoCapacity
    );

/*++

Routine Description:

    This routine opens a device. If the given device is the device meant to
    hold the page file, this routine does not prepare the returned I/O handle
    for paging operations.

Arguments:

    Device - Supplies a pointer to a device to open.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer that receives the open I/O handle.

    IoOffsetAlignment - Supplies a pointer where the alignment requirement in
        bytes will be returned for all I/O offsets.

    IoSizeAlignment - Supplies a pointer where the alignment requirement for
        the size of all transfers (the block size) will be returned for all
        I/O requests.

    IoCapacity - Supplies the device's total size, in bytes.

Return Value:

    Status code.

--*/

KERNEL_API
BOOL
IoIsPagingDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine determines whether or not paging is enabled on the given
    device.

Arguments:

    Device - Supplies a pointer to a device.

Return Value:

    Returns TRUE if paging is enabled on the device, or FALSE otherwise.

--*/

KERNEL_API
KSTATUS
IoClose (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine closes a file or device.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle returned when the file was
        opened.

Return Value:

    Status code. Close operations can fail if their associated flushes to
    the file system fail.

--*/

KERNEL_API
KSTATUS
IoRead (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine reads from an I/O object.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

KERNEL_API
KSTATUS
IoWrite (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine writes to an I/O object.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer containing the data to write.

    SizeInBytes - Supplies the number of bytes to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        written will be returned.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

KERNEL_API
KSTATUS
IoReadAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    );

/*++

Routine Description:

    This routine reads from an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer where the read data will be
        returned on success.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

KERNEL_API
KSTATUS
IoWriteAtOffset (
    PIO_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PUINTN BytesCompleted,
    PIRP Irp
    );

/*++

Routine Description:

    This routine writes to an I/O object at a specific offset.

Arguments:

    Handle - Supplies the open I/O handle.

    IoBuffer - Supplies a pointer to an I/O buffer containing the data to write.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        written will be returned.

    Irp - Supplies a pointer to the IRP to use for this I/O. This is required
        when doing operations on the page file.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value to find out how much occurred.

--*/

KERNEL_API
KSTATUS
IoFlush (
    PIO_HANDLE Handle,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes I/O data to its appropriate backing device.

Arguments:

    Handle - Supplies an open I/O handle. This parameters is not required if
        the FLUSH_FLAG_ALL flag is set.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies flags regarding the flush operation. See FLUSH_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSeek (
    PIO_HANDLE Handle,
    SEEK_COMMAND SeekCommand,
    IO_OFFSET Offset,
    PIO_OFFSET NewOffset
    );

/*++

Routine Description:

    This routine seeks to the given position in a file. This routine is only
    relevant for normal file or block based devices.

Arguments:

    Handle - Supplies the open I/O handle.

    SeekCommand - Supplies the reference point for the seek offset. Usual
        reference points are the beginning of the file, current file position,
        and the end of the file.

    Offset - Supplies the offset from the reference point to move in bytes.

    NewOffset - Supplies an optional pointer where the file position after the
        move will be returned on success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoGetFileSize (
    PIO_HANDLE Handle,
    PULONGLONG FileSize
    );

/*++

Routine Description:

    This routine returns the current size of the given file or block device.

Arguments:

    Handle - Supplies the open file handle.

    FileSize - Supplies a pointer where the file size will be returned on
        success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoGetFileInformation (
    PIO_HANDLE Handle,
    PFILE_PROPERTIES FileProperties
    );

/*++

Routine Description:

    This routine gets the file properties for the given I/O handle.

Arguments:

    Handle - Supplies the open file handle.

    FileProperties - Supplies a pointer where the file properties will be
        returned on success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSetFileInformation (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PSET_FILE_INFORMATION Request
    );

/*++

Routine Description:

    This routine sets the file properties for the given I/O handle.
    Only some properties can be set by this routine.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request
        originated from user mode (FALSE) or kernel mode (TRUE). Kernel mode
        requests bypass permission checks.

    Handle - Supplies the open file handle.

    Request - Supplies a pointer to the get/set information request.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoDelete (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file object or shared memory object, its hard link count is decremented.
    If the hard link count reaches zero and no processes have the object open,
    the contents of the object are destroyed. If processes have open handles to
    the object, the destruction of the object contents are deferred until the
    last handle to the old file is closed. If the path points to a symbolic
    link, the link itself is removed and not the destination. The removal of
    the entry from the directory is immediate.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies a pointer to the path to delete.

    PathSize - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoRename (
    BOOL FromKernelMode,
    PIO_HANDLE SourceStartDirectory,
    PCSTR SourcePath,
    ULONG SourcePathSize,
    PIO_HANDLE DestinationStartDirectory,
    PCSTR DestinationPath,
    ULONG DestinationPathSize
    );

/*++

Routine Description:

    This routine attempts to rename the object at the given path. This routine
    operates on symbolic links themselves, not the destinations of symbolic
    links. If the source and destination paths are equal, this routine will do
    nothing and return successfully. If the source path is not a directory, the
    destination path must not be a directory. If the destination file exists,
    it will be deleted. The caller must have write access in both the old and
    new directories. If the source path is a directory, the destination path
    must not exist or be an empty directory. The destination path must not have
    a path prefix of the source (ie it's illegal to move /my/path into
    /my/path/stuff).

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    SourceStartDirectory - Supplies an optional pointer to a handle to the
        directory to start at for relative source paths. If the source path is
        not relative, this parameter is ignored. If this is not supplied, then
        the current working directory of the process is used.

    SourcePath - Supplies a pointer to the path of the file to rename.

    SourcePathSize - Supplies the length of the source path buffer in bytes,
        including the null terminator.

    DestinationStartDirectory - Supplies an optional pointer to the directory
        to start at for relative destination paths. If the destination path is
        not relative, this parameter is ignored. If this is not supplied, then
        the current working directory of the process is used.

    DestinationPath - Supplies a pointer to the path to rename the file to.

    DestinationPathSize - Supplies the size of the destination path buffer in
        bytes, including the null terminator.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoCreateSymbolicLink (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR LinkName,
    ULONG LinkNameSize,
    PSTR LinkTarget,
    ULONG LinkTargetSize
    );

/*++

Routine Description:

    This routine attempts to create a new symbolic link at the given path.
    The target of the symbolic link is not required to exist. The link path
    must not already exist.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    LinkName - Supplies a pointer to the path of the new link to create.

    LinkNameSize - Supplies the length of the link name buffer in bytes,
        including the null terminator.

    LinkTarget - Supplies a pointer to the target of the link, the location the
        link points to.

    LinkTargetSize - Supplies the size of the link target buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoReadSymbolicLink (
    PIO_HANDLE Handle,
    ULONG AllocationTag,
    PSTR *LinkTarget,
    PULONG LinkTargetSize
    );

/*++

Routine Description:

    This routine reads the destination of a given open symbolic link, and
    returns the information in a newly allocated buffer. It is the caller's
    responsibility to free this memory from paged pool.

Arguments:

    Handle - Supplies the open file handle to the symbolic link itself.

    AllocationTag - Supplies the paged pool tag to use when creating the
        allocation.

    LinkTarget - Supplies a pointer where a newly allocated string will be
        returned on success containing the target the link is pointing at.

    LinkTargetSize - Supplies a pointer where the size of the link target in
        bytes (including the null terminator) will be returned.

Return Value:

    STATUS_SUCCESS if the link target was successfully returned.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_NOT_READY if the contents of the symbolic link are not valid.

    Other status codes on other failures.

--*/

KERNEL_API
KSTATUS
IoUserControl (
    PIO_HANDLE Handle,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

/*++

Routine Description:

    This routine performs a user control operation.

Arguments:

    Handle - Supplies the open file handle.

    MinorCode - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoMount (
    BOOL FromKernelMode,
    PCSTR MountPointPath,
    ULONG MountPointPathSize,
    PCSTR TargetPath,
    ULONG TargetPathSize,
    ULONG MountFlags,
    ULONG AccessFlags
    );

/*++

Routine Description:

    This routine attempts to mount the given target on the given mount point.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    MountPointPath - Supplies a pointer to the string containing the path to
        where the target is to be mounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Supplies a pointer to the string containing the path to the
        target file, directory, volume, pipe, socket, or device that is to be
        mounted.

    TargetPathSize - Supplies the size of the target path string in bytes,
        including the null terminator.

    MountFlags - Supplies the flags associated with the mount operation. See
        MOUNT_FLAG_* for definitions.

    AccessFlags - Supplies the flags associated with the mount point's access
        permissions. See IO_ACCESS_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoUnmount (
    BOOL FromKernelMode,
    PCSTR MountPointPath,
    ULONG MountPointPathSize,
    ULONG MountFlags,
    ULONG AccessFlags
    );

/*++

Routine Description:

    This routine attempts to remove a mount point at the given path.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    MountPointPath - Supplies a pointer to the string containing the path to
        where the unmount should take place.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    MountFlags - Supplies the flags associated with the mount operation. See
        MOUNT_FLAG_* for definitions.

    AccessFlags - Supplies the flags associated with the mount point's access
        permissions. See IO_ACCESS_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoGetMountPoints (
    PVOID Buffer,
    PUINTN BufferSize
    );

/*++

Routine Description:

    This routine returns the list of mount points for the current process,
    filling the supplied buffer with the data.

Arguments:

    Buffer - Supplies a pointer to a buffer that receives the mount point data.

    BufferSize - Supplies a pointer to the size of the buffer. Upon return this
        either holds the number of bytes actually used or if the buffer is
        to small, it receives the expected buffer size.

Return Value:

    Status code.

--*/

VOID
IoMountPointAddReference (
    PMOUNT_POINT MountPoint
    );

/*++

Routine Description:

    This routine increments the reference count for the given mount point.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    None.

--*/

VOID
IoMountPointReleaseReference (
    PMOUNT_POINT MountPoint
    );

/*++

Routine Description:

    This routine decrements the reference count for the given mount point.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoGetDevice (
    PIO_HANDLE Handle,
    PDEVICE *Device
    );

/*++

Routine Description:

    This routine returns the actual device backing the given I/O object. Not
    all I/O objects are actually backed by a single device. For file and
    directory objects, this routine will return a pointer to the volume.

Arguments:

    Handle - Supplies the open file handle.

    Device - Supplies a pointer where the underlying I/O device will be
        returned.

Return Value:

    Status code.

--*/

KERNEL_API
BOOL
IoIsPageFileAccessSupported (
    PIO_HANDLE Handle
    );

/*++

Routine Description:

    This routine determines whether or not page file access is supported on the
    given handle.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

Return Value:

    Returns TRUE if the handle supports page file I/O, or FALSE otherwise.

--*/

KERNEL_API
KSTATUS
IoGetGlobalStatistics (
    PIO_GLOBAL_STATISTICS Statistics
    );

/*++

Routine Description:

    This routine returns a snap of the global I/O statistics counters.

Arguments:

    Statistics - Supplies a pointer to the global I/O statistics.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the version is less than
    IO_GLOBAL_STATISTICS_VERSION.

--*/

KERNEL_API
KSTATUS
IoGetFileBlockInformation (
    PIO_HANDLE Handle,
    PFILE_BLOCK_INFORMATION *FileBlockInformation
    );

/*++

Routine Description:

    This routine gets a list of logical block offsets for the given file or
    partition, comprising the runs of contiguous disk space taken up by the
    file or partition.

Arguments:

    Handle - Supplies an I/O handle for the file or partition.

    FileBlockInformation - Supplies a pointer that receives a pointer to the
        block information for the file or partition. If this is non-null and a
        partition is queried, then the partition will update the offsets in the
        block information to be logical block offsets for the parent disk.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
IoDestroyFileBlockInformation (
    PFILE_BLOCK_INFORMATION FileBlockInformation
    );

/*++

Routine Description:

    This routine destroys file block information for a file or partition.

Arguments:

    FileBlockInformation - Supplies a pointer to file block information to be
        destroyed.

Return Value:

    Status code.

--*/

KSTATUS
IoWriteFileBlocks (
    PFILE_BLOCK_IO_CONTEXT FileContext,
    PIO_BUFFER IoBuffer,
    ULONGLONG Offset,
    UINTN SizeInBytes,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine write data directly to a file's disk blocks, bypassing the
    filesystem. It is meant for critical code paths, such as writing out the
    crash dump file during a system failure.

Arguments:

    FileContext - Supplies a pointer to the file block context, including the
        file's block information, the device's block level I/O routines and
        block information.

    IoBuffer - Supplies a pointer to an I/O buffer with the data to write.

    Offset - Supplies the offset, in bytes, into the file where the data is to
        be written.

    SizeInBytes - Supplies the size of the data to write, in bytes.

    BytesCompleted - Supplies a pointer that receives the total number of bytes
        written to the disk.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoLoadFile (
    PCSTR Path,
    ULONG PathLength,
    PLOAD_FILE_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    );

/*++

Routine Description:

    This routine asynchronously loads the file at the given path. The path can
    either be absolute or relative. For the kernel process, relative paths are
    in relative to the system volume's drivers directory. The supplied
    completion routine is invoked when the load finishes.

Arguments:

    Path - Supplies a pointer to the path to the file. It can either be an
        absolute path or a path relative to the system partition's drivers
        directory.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    CompletionRoutine - Supplies a pointer to the callback routine to notify
        when the load is complete.

    CompletionContext - Supplies a pointer to an opaque context that will be
        passed to the completion routine along with the loaded file.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
IoUnloadFile (
    PLOADED_FILE File
    );

/*++

Routine Description:

    This routine unloads the given file.

Arguments:

    File - Supplies a pointer to the file to unload.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoCreatePipe (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG OpenFlags,
    FILE_PERMISSIONS CreatePermissions,
    PIO_HANDLE *ReadHandle,
    PIO_HANDLE *WriteHandle
    );

/*++

Routine Description:

    This routine creates and opens a new pipe.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (and should use the root path as a base)
        or user mode.

    Directory - Supplies an optional pointer to an open handle to a directory
        for relative paths. Supply NULL to use the current working directory.

    Path - Supplies an optional pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    OpenFlags - Supplies the open flags for the pipe. See OPEN_FLAG_*
        definitions. OPEN_FLAG_CREATE and OPEN_FLAG_FAIL_IF_EXISTS are
        automatically applied.

    CreatePermissions - Supplies the permissions to apply to the created pipe.

    ReadHandle - Supplies a pointer where a handle to the read side of the pipe
        will be returned.

    WriteHandle - Supplies a pointer where a handle to the write side of the
        pipe will be returned.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoCreateTerminal (
    BOOL FromKernelMode,
    PIO_HANDLE MasterDirectory,
    PIO_HANDLE SlaveDirectory,
    PCSTR MasterPath,
    UINTN MasterPathLength,
    PCSTR SlavePath,
    UINTN SlavePathLength,
    ULONG MasterAccess,
    ULONG MasterOpenFlags,
    FILE_PERMISSIONS MasterCreatePermissions,
    FILE_PERMISSIONS SlaveCreatePermissions,
    PIO_HANDLE *MasterHandle
    );

/*++

Routine Description:

    This routine creates and opens a new terminal master.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (and should use the root path as a base)
        or user mode.

    MasterDirectory - Supplies an optional pointer to an open handle to a
        directory for relative paths when creating the master. Supply NULL to
        use the current working directory.

    SlaveDirectory - Supplies an optional pointer to an open handle to a
        directory for relative paths when creating the slave. Supply NULL to
        use the current working directory.

    MasterPath - Supplies an optional pointer to the path to create for the
        master.

    MasterPathLength - Supplies the length of the master side path buffer in
        bytes, including the null terminator.

    SlavePath - Supplies an optional pointer to the path to create for the
        master.

    SlavePathLength - Supplies the length of the slave side path buffer in
        bytes, including the null terminator.

    MasterAccess - Supplies the desired access permissions to the master side
        handle. See IO_ACCESS_* definitions.

    MasterOpenFlags - Supplies the open flags to use when opening the master.

    MasterCreatePermissions - Supplies the permissions to apply to the created
        master side.

    SlaveCreatePermissions - Supplies the permission to apply to the created
        slave side.

    MasterHandle - Supplies a pointer where a handle to the master side of the
        new terminal will be returned.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoOpenLocalTerminalMaster (
    PIO_HANDLE *TerminalMaster
    );

/*++

Routine Description:

    This routine opens the master side of the local console terminal. This
    routine is intended to be used by the input and output devices that
    actually service the local console (the user input driver and video
    console driver).

Arguments:

    TerminalMaster - Supplies a pointer where the I/O handle representing the
        master side of the local console terminal will be returned.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoOpenControllingTerminal (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine attempts to open the current process' controlling terminal.

Arguments:

    IoHandle - Supplies a pointer to an already open or opening I/O handle. The
        contents of that handle will be replaced with the controlling terminal.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSetTerminalSettings (
    PIO_HANDLE TerminalHandle,
    PTERMINAL_SETTINGS NewSettings,
    PTERMINAL_SETTINGS OriginalSettings,
    TERMINAL_CHANGE_BEHAVIOR When
    );

/*++

Routine Description:

    This routine gets or sets the current terminal settings.

Arguments:

    TerminalHandle - Supplies a pointer to the I/O handle of the terminal to
        change.

    NewSettings - Supplies an optional pointer to the new terminal settings.
        If this pointer is NULL, then the current settings will be retrieved
        but no changes will be made.

    OriginalSettings - Supplies an optional pointer where the current
        settings will be returned.

    When - Supplies when the new change should occur.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoTerminalSetDevice (
    PIO_HANDLE TerminalMaster,
    PIO_HANDLE DeviceHandle
    );

/*++

Routine Description:

    This routine associates or disassociates a terminal object with a device.
    Writes to the terminal slave will be forwarded to the associated terminal,
    as will changes to the terminal settings. If a device is being associated
    with the terminal, then the new settings will be sent to the device
    immediately in this routine.

Arguments:

    TerminalMaster - Supplies a handle to the terminal master.

    DeviceHandle - Supplies a handle to the terminal hardware device. Any
        previously associated handle will be closed. Supply NULL to
        disassociate the terminal with a device. Upon success, this routine
        takes ownership of this device handle, and the caller should not close
        it manually.

Return Value:

    Status code.

--*/

VOID
IoTerminalDisassociate (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine is called when a session leader dies to disassociate the
    terminal from the rest of the session.

Arguments:

    Process - Supplies a pointer to the session leader that has exited.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoLocateDeviceInformation (
    PUUID Uuid,
    PDEVICE Device,
    PDEVICE_ID DeviceId,
    PDEVICE_INFORMATION_RESULT Results,
    PULONG ResultCount
    );

/*++

Routine Description:

    This routine returns instances of devices enumerating information. Callers
    can get all devices enumerating the given information type, or all
    information types enumerated by a given device. This routine must be called
    at low level.

Arguments:

    Uuid - Supplies an optional pointer to the information identifier to
        filter on. If NULL, any information type will match.

    Device - Supplies an optional pointer to the device to match against. If
        NULL (and the device ID parameter is NULL), then any device will match.

    DeviceId - Supplies an optional pointer to the device ID to match against.
        If NULL (and the device ID parameter is NULL), then any device will
        match.

    Results - Supplies a pointer to a caller allocated buffer where the
        results will be returned.

    ResultCount - Supplies a pointer that upon input contains the size of the
        buffer in information result elements. On output, returns the number
        of elements in the query, even if the provided buffer was too small.
        Do note however that the number of results can change between two
        successive searches.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was not large enough to
    contain all the results. The result count will contain the required number
    of elements to contain the results.

--*/

KERNEL_API
KSTATUS
IoGetSetDeviceInformation (
    DEVICE_ID DeviceId,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets device information.

Arguments:

    DeviceId - Supplies the device ID of the device to get or set information
        for.

    Uuid - Supplies a pointer to the identifier of the device information type
        to get or set.

    Data - Supplies a pointer to the data buffer that either contains the
        information to set or will contain the information to get on success.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output contains the actual size of the data.

    Set - Supplies a boolean indicating whether to get information (FALSE) or
        set information (TRUE).

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoRegisterDeviceInformation (
    PDEVICE Device,
    PUUID Uuid,
    BOOL Register
    );

/*++

Routine Description:

    This routine registers or deregisters a device to respond to information
    requests of the given universally unique identifier. This routine must be
    called at low level.

Arguments:

    Device - Supplies a pointer to the device.

    Uuid - Supplies a pointer to the device information identifier.

    Register - Supplies a boolean indcating if the device is registering (TRUE)
        or de-registering (FALSE) for the information type.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

INTN
IoSysOpen (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine opens a file or other I/O object on behalf of a user mode
    application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysOpenDevice (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine opens a direct handle to a device on behalf of a user mode
    application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysClose (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine closes an I/O handle opened in user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the user mode handle returned during the
        open system call. It is passed to the kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysPerformIo (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine performs I/O for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of bytes completed (a positive integer) on
    success.

    Error status code (a negative integer) on failure.

--*/

INTN
IoSysPerformVectoredIo (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine performs vectored I/O for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of bytes completed (a positive integer) on
    success.

    Error status code (a negative integer) on failure.

--*/

INTN
IoSysFlush (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine flushes data to its backing device for user mode.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysCreatePipe (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine creates a pipe on behalf of a user mode application.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysGetCurrentDirectory (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call requesting the path of the current
    working directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysChangeDirectory (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call requesting to change the current
    working directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysPoll (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the poll system call, which waits on several I/O
    handles.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the number of descriptors selected (a positive integer)
    on success.

    Error status code (a negative integer) on failure.

--*/

INTN
IoSysDuplicateHandle (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for duplicating a file handle.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysFileControl (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the file control system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysGetSetFileInformation (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the get/set file information system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSeek (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the file seek system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysCreateSymbolicLink (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysReadSymbolicLink (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine reads and returns the destination of a symbolic link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysCreateHardLink (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine creates a hard link.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysDelete (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine deletes an entry from a directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysRename (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine renames a file or directory.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysUserControl (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the user control system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysMountOrUnmount (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine mounts or unmounts a file, directory, volume, or device.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysGetEffectiveAccess (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for getting the current user's
    access permission to a given path.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysCreateTerminal (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for creating and opening a new
    terminal.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketCreatePair (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that creates a pair of connected
    sockets.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketCreate (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that creates a new socket.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketBind (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine attempts to bind a socket to a local address.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketListen (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that makes a socket listen and become
    eligible to accept new incoming connections.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketAccept (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that accepts a new incoming
    connection on a socket and spins it off into another socket.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketConnect (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that reaches and and attempts to
    connect with another socket.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketPerformIo (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that sends a packet to a specific
    destination or receives data from a destination. Sockets may also use the
    generic perform I/O operations if the identity of the remote address is
    either already known or not needed.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketPerformVectoredIo (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine handles the system call that performs socket I/O using I/O
    vectors.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketGetSetInformation (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for getting or setting socket
    information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysSocketShutdown (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for shutting down communication to
    a socket.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysLoadDriver (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine loads a driver into the kernel's address space.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysLocateDeviceInformation (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the user mode system call for locating device
    information registrations by UUID or device ID.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
IoSysGetSetDeviceInformation (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the user mode system call for getting and setting
    device information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

VOID
IoIoHandleAddReference (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine increments the reference count on an I/O handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

KSTATUS
IoIoHandleReleaseReference (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine decrements the reference count on an I/O handle. If the
    reference count becomes zero, the I/O handle will be destroyed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

Return Value:

    None.

--*/

PIMAGE_SECTION_LIST
IoGetImageSectionListFromIoHandle (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine gets the image section list for the given I/O handle.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns a pointer to the I/O handles image section list or NULL on failure.

--*/

KERNEL_API
ULONG
IoGetIoHandleAccessPermissions (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine returns the access permissions for the given I/O handle. For
    directories, no access is always returned.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the access permissions for the given I/O handle.

--*/

KERNEL_API
ULONG
IoGetIoHandleOpenFlags (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine returns the current open flags for a given I/O handle. Some
    of these flags can change.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

Return Value:

    Returns the current open flags for the I/O handle.

--*/

BOOL
IoIoHandleIsCacheable (
    PIO_HANDLE IoHandle,
    PULONG MapFlags
    );

/*++

Routine Description:

    This routine determines whether or not data for the I/O object specified by
    the given handle is cached in the page cache.

Arguments:

    IoHandle - Supplies a pointer to an I/O handle.

    MapFlags - Supplies an optional pointer where any additional map flags
        needed when mapping sections from this handle will be returned.
        See MAP_FLAG_* definitions.

Return Value:

    Returns TRUE if the I/O handle's object uses the page cache, FALSE
    otherwise.

--*/

KSTATUS
IoCloseProcessHandles (
    PKPROCESS Process,
    HANDLE MinimumHandle
    );

/*++

Routine Description:

    This routine closes all remaining open handles in the given process.

Arguments:

    Process - Supplies a pointer to the process being terminated.

    MinimumHandle - Supplies the lowest handle to clean up to, inclusive.
        Handles below this one will not be closed.

Return Value:

    Status code.

--*/

KSTATUS
IoCopyProcessHandles (
    PKPROCESS SourceProcess,
    PKPROCESS DestinationProcess
    );

/*++

Routine Description:

    This routine copies all handles in the source process to the destination
    process. This is used during process forking.

Arguments:

    SourceProcess - Supplies a pointer to the process being copied.

    DestinationProcess - Supplies a pointer to the fledgling destination
        process. This process' handle hables must be empty.

Return Value:

    Status code.

--*/

KSTATUS
IoCloseHandlesOnExecute (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine closes any handles marked for "close on execute".

Arguments:

    Process - Supplies a pointer to the process undergoing the execution
        transformation.

Return Value:

    Status code.

--*/

KSTATUS
IoOpenPageFile (
    PCSTR Path,
    ULONG PathSize,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle,
    PULONGLONG FileSize
    );

/*++

Routine Description:

    This routine opens a page file. This routine is to be used only
    internally by MM.

Arguments:

    Path - Supplies a pointer to the string containing the file path to open.

    PathSize - Supplies the length of the path buffer in bytes, including
        the null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

    FileSize - Supplies a pointer where the file size in bytes will be returned
        on success.

Return Value:

    Status code.

--*/

KSTATUS
IoNotifyFileMapping (
    PIO_HANDLE Handle,
    BOOL Mapping
    );

/*++

Routine Description:

    This routine is called to notify a file object that it is being mapped
    into memory or unmapped.

Arguments:

    Handle - Supplies the handle being mapped.

    Mapping - Supplies a boolean indicating if a new mapping is being created
        (TRUE) or an old mapping is being destroyed (FALSE).

Return Value:

    Status code.

--*/

KSTATUS
IoPathAppend (
    PCSTR Prefix,
    ULONG PrefixSize,
    PCSTR Component,
    ULONG ComponentSize,
    ULONG AllocationTag,
    PSTR *AppendedPath,
    PULONG AppendedPathSize
    );

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    Prefix - Supplies the initial path string. This can be null.

    PrefixSize - Supplies the size of the prefix string in bytes including the
        null terminator.

    Component - Supplies a pointer to the component string to add.

    ComponentSize - Supplies the size of the component string in bytes
        including a null terminator.

    AllocationTag - Supplies the tag to use for the combined allocation.

    AppendedPath - Supplies a pointer where the new path will be returned. The
        caller is responsible for freeing this memory.

    AppendedPathSize - Supplies a pointer where the size of the appended bath
        buffer in bytes including the null terminator will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

PPATH_POINT
IoGetPathPoint (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine returns the path point for the given handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle to get the path point of.

Return Value:

    Returns a pointer to the path point corresponding to the given handle.

--*/

VOID
IoPathEntryAddReference (
    PPATH_ENTRY Entry
    );

/*++

Routine Description:

    This routine increments the reference count of the given path entry.

Arguments:

    Entry - Supplies a pointer to the path entry.

Return Value:

    None.

--*/

VOID
IoPathEntryReleaseReference (
    PPATH_ENTRY Entry
    );

/*++

Routine Description:

    This routine decrements the reference count of the given path entry. If the
    reference count drops to zero, the path entry will be destroyed.

Arguments:

    Entry - Supplies a pointer to the path entry.

Return Value:

    None.

--*/

KSTATUS
IoGetCurrentDirectory (
    BOOL FromKernelMode,
    BOOL Root,
    PSTR *Path,
    PUINTN PathSize
    );

/*++

Routine Description:

    This routine gets either the current working directory or the path of the
    current chroot environment.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not a kernel mode
        caller is requesting the directory information. This dictates how the
        given path buffer is treated.

    Root - Supplies a boolean indicating whether to get the path to the current
        working directory (FALSE) or to get the path of the current chroot
        environment (TRUE). If the caller does not have permission to escape a
        changed root, or the root has not been changed, then / is returned in
        the path argument.

    Path - Supplies a pointer to a buffer that will contain the desired path on
        output. If the call is from kernel mode and the pointer is NULL, then
        a buffer will be allocated.

    PathSize - Supplies a pointer to the size of the path buffer on input. On
        output it stores the required size of the path buffer. This includes
        the null terminator.

Return Value:

    Status code.

--*/

KSTATUS
IoLoadDriver (
    PCSTR DriverName,
    PDRIVER *DriverOut
    );

/*++

Routine Description:

    This routine loads a driver into memory. This routine must be called at low
    level.

Arguments:

    DriverName - Supplies the name of the driver to load.

    DriverOut - Supplies an optional pointer where the pointer to the newly
        loaded driver can be returned.

Return Value:

    Status code.

--*/

KSTATUS
IoAddDeviceDatabaseEntry (
    PCSTR DeviceId,
    PCSTR DriverName
    );

/*++

Routine Description:

    This routine adds a mapping between a device and a driver. Only one device
    to driver mapping can exist in the database at once.

Arguments:

    DeviceId - Supplies the device ID of the device to associate with a driver.
        This memory does not need to be retained, a copy of this string will
        be created.

    DriverName - Supplies the name of the driver corresponding to the device.
        This memory does not need to be retained, a copy of this string will be
        created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

    STATUS_INSUFFICIENT_RESOURCE on allocation failure.

    STATUS_DUPLICATE_ENTRY if the device ID already exists in the database.

--*/

KSTATUS
IoAddDeviceClassDatabaseEntry (
    PCSTR ClassId,
    PCSTR DriverName
    );

/*++

Routine Description:

    This routine adds a mapping between a device class and a driver. Only one
    device class to driver mapping can exist in the database at once.

Arguments:

    ClassId - Supplies the device class identifier of the device to associate
        with a driver. This memory does not need to be retained, a copy of this
        string will be created.

    DriverName - Supplies the name of the driver corresponding to the device
        class. This memory does not need to be retained, a copy of this string
        will be created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a required parameter or function was not
        supplied.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_DUPLICATE_ENTRY if the device ID already exists in the database.

--*/

KSTATUS
IoCreateDriverStructure (
    PVOID LoadedImage
    );

/*++

Routine Description:

    This routine is called to create a new driver structure for a loaded image.
    This routine should only be called internally by the system.

Arguments:

    LoadedImage - Supplies a pointer to the image associated with the driver.

Return Value:

    Status code.

--*/

VOID
IoDestroyDriverStructure (
    PVOID LoadedImage
    );

/*++

Routine Description:

    This routine is called to destroy a driver structure in association with
    a driver being torn down. This routine should only be called internally by
    the system.

Arguments:

    LoadedImage - Supplies a pointer to the image being destroyed.

Return Value:

    None.

--*/

KSTATUS
IoCreateVolume (
    PDEVICE Device,
    PVOLUME *Volume
    );

/*++

Routine Description:

    This routine creates a new volume to be mounted by a file system.

Arguments:

    Device - Supplies a pointer to the physical device upon which the file
        system should be mounted.

    Volume - Supplies a pointer that receives a pointer to the newly created
        volume.

Return Value:

    Status code.

--*/

VOID
IoVolumeAddReference (
    PVOLUME Volume
    );

/*++

Routine Description:

    This routine increments a volume's reference count.

Arguments:

    Volume - Supplies a pointer to a volume device.

Return Value:

    None.

--*/

VOID
IoVolumeReleaseReference (
    PVOLUME Volume
    );

/*++

Routine Description:

    This routine decrements a volume's reference count.

Arguments:

    Volume - Supplies a pointer to a volume device.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoCreateResourceArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    );

/*++

Routine Description:

    This routine creates a resource arbiter for the given bus device between
    a system resource and the device's children. This function is needed for
    any device whose children access system resources (like physical address
    space) through a window set up by the parent.

Arguments:

    Device - Supplies a pointer to the parent bus device that provides
        resources.

    ResourceType - Supplies the type of resource that the device provides.

Return Value:

    STATUS_SUCCESS if the new arbiter was created.

    STATUS_INVALID_PARAMETER if an invalid resource type was specified.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_ALREADY_INITIALIZED if the device has already has a resource arbiter
    of this type.

--*/

KERNEL_API
KSTATUS
IoDestroyResourceArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    );

/*++

Routine Description:

    This routine destroys a resource arbiter for the given bus device and type.

Arguments:

    Device - Supplies a pointer to the device that owns resource arbitration.

    ResourceType - Supplies the type of resource arbiter that is to be
        destroyed.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoAddFreeSpaceToArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType,
    ULONGLONG FreeSpaceBegin,
    ULONGLONG FreeSpaceLength,
    ULONGLONG FreeSpaceCharacteristics,
    PRESOURCE_ALLOCATION SourcingAllocation,
    ULONGLONG TranslationOffset
    );

/*++

Routine Description:

    This routine adds a regions of allocatable space to a previously created
    resource arbiter.

Arguments:

    Device - Supplies a pointer to the device that owns the arbiter (and the
        free space).

    ResourceType - Supplies the resource type that the arbiter can dole out.
        An arbiter of this type must have been created by the device.

    FreeSpaceBegin - Supplies the first address of the free region.

    FreeSpaceLength - Supplies the length of the free region.

    FreeSpaceCharacteristics - Supplies the characteristics of the free
        region.

    SourcingAllocation - Supplies a pointer to the parent resource allocation
        that makes this range possible. This pointer is optional. Supplying
        NULL here implies that the given resource is fixed in nature and
        cannot be expanded.

    TranslationOffset - Supplies the offset that has to be added to all
        doled out allocations on the secondary side to get an address in the
        source allocation space (primary side).
        To recap: SecondaryAddress + TranslationOffset = PrimaryAddress, where
        PrimaryAddress is closer to the CPU complex.

Return Value:

    Status code.

--*/

KERNEL_API
PRESOURCE_ALLOCATION_LIST
IoGetProcessorLocalResources (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine returns the given device's processor local resources.

Arguments:

    Device - Supplies a pointer to the device that owns the resources.

Return Value:

    Returns a pointer to the processor local resource allocation list.

--*/

//
// Interrupt management routines.
//

KERNEL_API
KSTATUS
IoConnectInterrupt (
    PIO_CONNECT_INTERRUPT_PARAMETERS Parameters
    );

/*++

Routine Description:

    This routine connects a device's interrupt.

Arguments:

    Parameters - Supplies a pointer to a versioned table containing the
        parameters of the connection.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_READY if the device has no resources or is not started.

    STATUS_RESOURCE_IN_USE if the device attempts to connect to an interrupt
    it does not own.

    Other errors on failure.

--*/

KERNEL_API
VOID
IoDisconnectInterrupt (
    HANDLE InterruptHandle
    );

/*++

Routine Description:

    This routine disconnects a device's interrupt. The device must not
    generate interrupts when this routine is called, as the interrupt line
    may remain open to service other devices connected to the line.

Arguments:

    InterruptHandle - Supplies the handle to the interrupt, returned when the
        interrupt was connected.

Return Value:

    None.

--*/

KERNEL_API
RUNLEVEL
IoRaiseToInterruptRunLevel (
    HANDLE InterruptHandle
    );

/*++

Routine Description:

    This routine raises the current run level to that of the given connected
    interrupt. Callers should use KeLowerRunLevel to return from the run level
    raised to here.

Arguments:

    InterruptHandle - Supplies the handle to the interrupt, returned when the
        interrupt was connected.

Return Value:

    Returns the run level of the current processor immediately before it was
    raised by this function.

--*/

KERNEL_API
RUNLEVEL
IoGetInterruptRunLevel (
    PHANDLE Handles,
    UINTN HandleCount
    );

/*++

Routine Description:

    This routine determines the highest runlevel between all of the
    connected interrupt handles given.

Arguments:

    Handles - Supplies an pointer to an array of connected interrupt handles.

    HandleCount - Supplies the number of elements in the array.

Return Value:

    Returns the highest runlevel between all connected interrupts. This is
    the runlevel to synchronize to if trying to synchronize a device with
    multiple interrupts.

--*/

PSTREAM_BUFFER
IoCreateStreamBuffer (
    PIO_OBJECT_STATE IoState,
    ULONG Flags,
    ULONG BufferSize,
    ULONG AtomicWriteSize
    );

/*++

Routine Description:

    This routine allocates and initializes a new stream buffer.

Arguments:

    IoState - Supplies an optional pointer to the I/O state structure to use
        for this stream buffer.

    Flags - Supplies a bitfield of flags governing the behavior of the stream
        buffer. See STREAM_BUFFER_FLAG_* definitions.

    BufferSize - Supplies the size of the buffer. Supply zero to use a default
        system value.

    AtomicWriteSize - Supplies the number of bytes that can always be written
        to the stream atomically (without interleaving).

Return Value:

    Returns a pointer to the buffer on success.

    NULL on invalid parameter or allocation failure.

--*/

VOID
IoDestroyStreamBuffer (
    PSTREAM_BUFFER StreamBuffer
    );

/*++

Routine Description:

    This routine destroys an allocated stream buffer. It assumes there are no
    waiters on the events.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to tear down.

Return Value:

    None.

--*/

KSTATUS
IoReadStreamBuffer (
    PSTREAM_BUFFER StreamBuffer,
    PIO_BUFFER IoBuffer,
    UINTN ByteCount,
    ULONG TimeoutInMilliseconds,
    BOOL NonBlocking,
    PUINTN BytesRead
    );

/*++

Routine Description:

    This routine reads from a stream buffer. This routine must be called at low
    level, unless the stream was set up to be read at dispatch.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to read from.

    IoBuffer - Supplies a pointer to the I/O buffer where the read data will be
        returned on success.

    ByteCount - Supplies the number of bytes to read.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    NonBlocking - Supplies a boolean indicating if this read should avoid
        blocking.

    BytesRead - Supplies a pointer where the number of bytes actually read will
        be returned.

Return Value:

    Status code. If a failing status code is returned, then check the number of
    bytes read to see if any valid data was returned.

--*/

KSTATUS
IoWriteStreamBuffer (
    PSTREAM_BUFFER StreamBuffer,
    PIO_BUFFER IoBuffer,
    UINTN ByteCount,
    ULONG TimeoutInMilliseconds,
    BOOL NonBlocking,
    PUINTN BytesWritten
    );

/*++

Routine Description:

    This routine writes to a stream buffer. This routine must be called at low
    level, unless the stream was set up to be written at dispatch.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer to write to.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write to the stream buffer.

    ByteCount - Supplies the number of bytes to writes.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    NonBlocking - Supplies a boolean indicating if this write should avoid
        blocking.

    BytesWritten - Supplies a pointer where the number of bytes actually written
        will be returned.

Return Value:

    Status code. If a failing status code is returned, then check the number of
    bytes read to see if any valid data was written.

--*/

KSTATUS
IoStreamBufferConnect (
    PSTREAM_BUFFER StreamBuffer
    );

/*++

Routine Description:

    This routine resets the I/O object state when someone connects to a stream
    buffer.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer.

Return Value:

    Status code.

--*/

PIO_OBJECT_STATE
IoStreamBufferGetIoObjectState (
    PSTREAM_BUFFER StreamBuffer
    );

/*++

Routine Description:

    This routine returns the I/O state for a stream buffer.

Arguments:

    StreamBuffer - Supplies a pointer to the stream buffer.

Return Value:

    Returns a pointer to the stream buffer's I/O object state.

--*/

KSTATUS
IoGetCacheStatistics (
    PIO_CACHE_STATISTICS Statistics
    );

/*++

Routine Description:

    This routine collects the cache statistics and returns them to the caller.

Arguments:

    Statistics - Supplies a pointer that receives the cache statistics. The
        caller should zero this buffer beforehand and set the version member to
        IO_CACHE_STATISTICS_VERSION. Failure to zero the structure beforehand
        may result in uninitialized data when a driver built for a newer OS is
        run on an older OS.

Return Value:

    Status code.

--*/

KERNEL_API
ULONG
IoGetCacheEntryDataSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of data stored in each cache entry.

Arguments:

    None.

Return Value:

    Returns the size of the data stored in each cache entry.

--*/

VOID
IoPageCacheEntryAddReference (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine increments the reference count on the given page cache entry.
    It is assumed that either the lock for the file object associated with the
    page cache entry is held or the caller already has a reference on the given
    page cache entry.

Arguments:

    Entry - Supplies a pointer to the page cache entry whose reference count
        should be incremented.

Return Value:

    None.

--*/

VOID
IoPageCacheEntryReleaseReference (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine decrements the reference count on the given page cache entry.

Arguments:

    Entry - Supplies a pointer to the page cache entry whose reference count
        should be incremented.

Return Value:

    None.

--*/

PHYSICAL_ADDRESS
IoGetPageCacheEntryPhysicalAddress (
    PPAGE_CACHE_ENTRY Entry,
    PULONG MapFlags
    );

/*++

Routine Description:

    This routine returns the physical address of the page cache entry.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

    MapFlags - Supplies an optional pointer to the additional mapping flags
        mandated by the underlying file object.

Return Value:

    Returns the physical address of the given page cache entry.

--*/

PVOID
IoGetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine gets the given page cache entry's virtual address.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the virtual address of the given page cache entry.

--*/

BOOL
IoSetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY Entry,
    PVOID VirtualAddress
    );

/*++

Routine Description:

    This routine attempts to set the virtual address in the given page cache
    entry. It is assumed that the page cache entry's physical address is mapped
    at the given virtual address.

Arguments:

    Entry - Supplies as pointer to the page cache entry.

    VirtualAddress - Supplies the virtual address to set in the page cache
        entry.

Return Value:

    Returns TRUE if the set succeeds or FALSE if another virtual address is
    already set for the page cache entry.

--*/

VOID
IoMarkPageCacheEntryDirty (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine marks the given page cache entry as dirty.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoSetTestHook (
    ULONG TestHookMask
    );

/*++

Routine Description:

    This routine sets the provided test hook mask in the test hook bitmask.

Arguments:

    TestHookMask - Supplies the test hook mask that is to be added to the test
        hook bitmask.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoClearTestHook (
    ULONG TestHookMask
    );

/*++

Routine Description:

    This routine unsets the provided test hook mask from the test hook bitmask.

Arguments:

    TestHookMask - Supplies the test hook mast hat is to be removed from the
        test hook bitmask.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoSetIoObjectState (
    PIO_OBJECT_STATE IoState,
    ULONG Events,
    BOOL Set
    );

/*++

Routine Description:

    This routine sets or clears one or more events in the I/O object state.

Arguments:

    IoState - Supplies a pointer to the I/O object state to change.

    Events - Supplies a mask of poll events to change. See POLL_EVENT_*
        definitions.

    Set - Supplies a boolean indicating if the event(s) should be set (TRUE) or
        cleared (FALSE).

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
IoWaitForIoObjectState (
    PIO_OBJECT_STATE IoState,
    ULONG Events,
    BOOL Interruptible,
    ULONG TimeoutInMilliseconds,
    PULONG ReturnedEvents
    );

/*++

Routine Description:

    This routine waits for the given events to trigger on the I/O object state.

Arguments:

    IoState - Supplies a pointer to the I/O object state to wait on.

    Events - Supplies a mask of poll events to wait for. See POLL_EVENT_*
        definitions. Errors are non-maskable and will always be returned.

    Interruptible - Supplies a boolean indicating whether or not the wait can
        be interrupted if a signal is sent to the process on which this thread
        runs. If TRUE is supplied, the caller must check the return status
        code to find out if the wait was really satisfied or just interrupted.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

    ReturnedEvents - Supplies an optional pointer where the poll events that
        satisfied the wait will be returned on success. If the wait was
        interrupted this will return 0.

Return Value:

    Status code.

--*/

KERNEL_API
PIO_OBJECT_STATE
IoCreateIoObjectState (
    BOOL HighPriority,
    BOOL NonPaged
    );

/*++

Routine Description:

    This routine creates a new I/O object state structure with a reference
    count of one.

Arguments:

    HighPriority - Supplies a boolean indicating whether or not the I/O object
        state should be prepared for high priority events.

    NonPaged - Supplies a boolean indicating whether or not the I/O object
        state should be allocated from non-paged pool. Default is paged pool
        (FALSE).

Return Value:

    Returns a pointer to the new state structure on success.

    NULL on allocation failure.

--*/

KERNEL_API
VOID
IoDestroyIoObjectState (
    PIO_OBJECT_STATE State,
    BOOL NonPaged
    );

/*++

Routine Description:

    This routine destroys the given I/O object state.

Arguments:

    State - Supplies a pointer to the I/O object state to destroy.

    NonPaged - Supplies a boolean indicating whether or not the I/O object
        was allocated from non-paged pool. Default is paged pool (FALSE).

Return Value:

    None.

--*/

PVOID
IoReferenceFileObjectForHandle (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine returns an opaque pointer to the file object opened by the
    given handle. It also adds a reference to the file object, which the caller
    is responsible for freeing.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle whose underlying file
        object should be referenced.

Return Value:

    Returns an opaque pointer to the file object, with an incremented reference
    count. The caller is responsible for releasing this reference.

--*/

VOID
IoFileObjectReleaseReference (
    PVOID FileObject
    );

/*++

Routine Description:

    This routine releases an external reference on a file object taken by
    referencing the file object for a handle.

Arguments:

    FileObject - Supplies the opaque pointer to the file object.

Return Value:

    None. The caller should not count on this pointer remaining unique after
    this call returns.

--*/

KSTATUS
IoSetHandleAsynchronous (
    PIO_HANDLE IoHandle,
    HANDLE Descriptor,
    BOOL Asynchronous
    );

/*++

Routine Description:

    This routine enables or disables asynchronous mode for the given I/O
    handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

    Descriptor - Supplies the descriptor to associate with the asynchronous
        receiver state. This descriptor is passed to the signal information
        when an I/O signal occurs. Note that this descriptor may become stale
        if the handle is duped and the original closed, so the kernel should
        never access it.

    Asynchronous - Supplies a boolean indicating whether to set asynchronous
        mode (TRUE) or clear it (FALSE).

Return Value:

    Status code.

--*/

KSTATUS
IoGetSetSystemInformation (
    BOOL FromKernelMode,
    IO_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

