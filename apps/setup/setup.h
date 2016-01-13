/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    setup.h

Abstract:

    This header contains definitions for the setup application.

Author:

    Evan Green 10-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API DLLEXPORT

#include <osbase.h>
#include <minoca/partlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the assumed block size of the install media.
//

#define SETUP_BLOCK_SIZE 512

//
// Set this flag to enable more printing.
//

#define SETUP_FLAG_VERBOSE 0x00000001

//
// Set this flag to disable the reboot at the end of the installation.
//

#define SETUP_FLAG_NO_REBOOT 0x00000002

//
// Set this flag to use MBR formatting.
//

#define SETUP_FLAG_MBR 0x00000004

//
// Set this flag to write the boot sector LBA and length into the MBR and VBR.
//

#define SETUP_FLAG_WRITE_BOOT_LBA 0x00000008

//
// Set this flag to align partitions to 1MB.
//

#define SETUP_FLAG_1MB_PARTITION_ALIGNMENT 0x00000010

//
// Set this flag to enable debugging in the new boot configuration.
//

#define SETUP_FLAG_INSTALL_DEBUG 0x00000020

//
// Set this flag to run in automated mode, where setup will try to install to
// any free partition that is not the current system partition. This flag
// shouldn't be used by real users, only test automation.
//

#define SETUP_FLAG_AUTO_DEPLOY 0x00000040

//
// Set this flag if the page file size was specified on the command line.
//

#define SETUP_FLAG_PAGE_FILE_SPECIFIED 0x00000080

//
// Set this flag to create two evenly divided main partitions (in addition to
// the small boot partition).
//

#define SETUP_FLAG_TWO_PARTITIONS 0x00000100

//
// Set this flag to enable boot debugging in the target.
//

#define SETUP_FLAG_INSTALL_BOOT_DEBUG 0x00000200

//
// Set this flag to allow short file names on the boot partition.
//

#define SETUP_FLAG_BOOT_ALLOW_SHORT_FILE_NAMES 0x00000400

//
// Define the name of the source install image.
//

#define SETUP_DEFAULT_IMAGE_NAME "./install.img"

//
// Define the path of the page file.
//

#define SETUP_PAGE_FILE_PATH "pagefile.sys"

//
// This flag is set if this is the system disk or partition.
//

#define SETUP_DEVICE_FLAG_SYSTEM 0x00000001

//
// Define the well known offsets of the boot sector where its LBA and size are
// stored.
//

#define SETUP_BOOT_SECTOR_BLOCK_ADDRESS_OFFSET 0x5C
#define SETUP_BOOT_SECTOR_BLOCK_LENGTH_OFFSET 0x60

//
// Define the default factor to multiply system memory by to get the page file
// size.
//

#define SETUP_DEFAULT_PAGE_FILE_NUMERATOR 2
#define SETUP_DEFAULT_PAGE_FILE_DENOMINATOR 1
#define SETUP_MAX_PAGE_FILE_DISK_DIVISOR 10
#define SETUP_PAGE_FILE_ZERO_BUFFER_SIZE 0x100000
#define SETUP_SYMLINK_MAX 512

//
// Define away some things on Windows.
//

#ifdef _WIN32

#define S_IRGRP 0
#define S_ISLNK(_Mode) (((_Mode) & S_IFMT) == S_IFLNK)
#define S_IFLNK 0x0000A000
#define O_NOFOLLOW 0

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SETUP_PLATFORM {
    SetupPlatformInvalid,
    SetupPlatformUnknown,
    SetupPlatformBiosPc,
    SetupPlatformEfiPc,
    SetupPlatformTiPandaBoard,
    SetupPlatformRaspberryPi,
    SetupPlatformRaspberryPi2,
    SetupPlatformTiBeagleBoneBlack,
} SETUP_PLATFORM, *PSETUP_PLATFORM;

typedef enum _SETUP_DESTINATION_TYPE {
    SetupDestinationInvalid,
    SetupDestinationDisk,
    SetupDestinationPartition,
    SetupDestinationDirectory,
    SetupDestinationImage,
    SetupDestinationFile,
} SETUP_DESTINATION_TYPE, *PSETUP_DESTINATION_TYPE;

/*++

Structure Description:

    This structure defines a destination for setup.

Members:

    Type - Stores the type of object this destination refers to.

    Path - Stores a pointer to a heap-allocated path to the destination. If
        this is NULL, then the destination is assumed to point to a device ID.

    IsDeviceId - Stores a boolean indicating if the destination is a device ID
        (TRUE) or a path (FALSE).

    DeviceId - Stores the device ID of the destination.

--*/

typedef struct _SETUP_DESTINATION {
    SETUP_DESTINATION_TYPE Type;
    PSTR Path;
    ULONGLONG DeviceId;
} SETUP_DESTINATION, *PSETUP_DESTINATION;

/*++

Structure Description:

    This structure describes a partition.

Members:

    Partition - Stores the partition information.

    Destination - Stores a pointer to the destination needed to open this
        partition.

    Flags - Stores a bitfield of flags about the device. See
        SETUP_DEVICE_FLAG_* definitions.

--*/

typedef struct _SETUP_PARTITION_DESCRIPTION {
    PARTITION_DEVICE_INFORMATION Partition;
    PSETUP_DESTINATION Destination;
    ULONG Flags;
} SETUP_PARTITION_DESCRIPTION, *PSETUP_PARTITION_DESCRIPTION;

/*++

Structure Description:

    This structure stores installation directions for a specific platform.

Members:

    Platform - Stores the platform identifier.

    ShortName - Stores the short name of the platform, usually used at the
        command line.

    Description - Stores the longer name of the platform, used when printing.

    SmbiosProductName - Stores the platform name in the SMBIOS product name
        field.

    Flags - Stores additional flags to enable in the setup context.

    Mbr - Stores an optional path to a file to set as the MBR.

    Vbr - Stores an optional path to a file to set as the VBR.

    Loader - Store an optional path to the loader to use, if not the standard
        EFI loader.

    BootFiles - Stores an array of additional files to place in the boot
        volume, separated by newlines.

--*/

typedef struct _SETUP_PLATFORM_RECIPE {
    SETUP_PLATFORM Platform;
    PSTR ShortName;
    PSTR Description;
    PSTR SmbiosProductName;
    ULONG Flags;
    PSTR Mbr;
    PSTR Vbr;
    PSTR Loader;
    PSTR BootFiles;
} SETUP_PLATFORM_RECIPE, *PSETUP_PLATFORM_RECIPE;

/*++

Structure Description:

    This structure describes setup's application context.

Members:

    Flags - Stores a bitfield of flags governing the behavior. See
        SETUP_FLAG_* definitions.

    DiskFormat - Stores the partition format to use on the disk.

    PartitionContext - Stores a pointer to the partition library context for
        the disk layout.

    BootPartitionOffset - Stores the offset in blocks to the boot partition.

    BootPartitionSize - Stores the size in blocks of the boot partition.

    InstallPartition - Stores the partition device information for the install
        partition.

    CurrentPartitionOffset - Stores the offset in blocks to the partition
        being actively read from and written to.

    CurrentPartitionSize - Stores the size in blocks of the partition being
        actively read from and written to.

    DiskPath - Stores an optional pointer to the disk to install to.

    PartitionPath - Stores an optional pointer to the partition to install to.

    DirectoryPath - Stores an optional pointer to the directory to install to.

    BootPartitionPath - Stores an optional pointer to the boot partition to
        update.

    Disk - Stores a pointer to the install disk.

    SourceVolume - Stores a pointer to the install source volume.

    Recipe - Stores a pointer to a specific recipe to follow.

    PageFileSize - Stores the size in megabytes of the page file.

--*/

typedef struct _SETUP_CONTEXT {
    ULONG Flags;
    PARTITION_FORMAT DiskFormat;
    PARTITION_CONTEXT PartitionContext;
    ULONGLONG BootPartitionOffset;
    ULONGLONG BootPartitionSize;
    PARTITION_DEVICE_INFORMATION InstallPartition;
    ULONGLONG CurrentPartitionOffset;
    ULONGLONG CurrentPartitionSize;
    PSETUP_DESTINATION DiskPath;
    PSETUP_DESTINATION PartitionPath;
    PSETUP_DESTINATION DirectoryPath;
    PSETUP_DESTINATION BootPartitionPath;
    PVOID Disk;
    PVOID SourceVolume;
    PSETUP_PLATFORM_RECIPE Recipe;
    ULONGLONG PageFileSize;
} SETUP_CONTEXT, *PSETUP_CONTEXT;

/*++

Structure Description:

    This structure describes a handle to a volume in the setup app.

Members:

    Context - Stores a pointer back to tha application context.

    DestinationType - Stores the destination type of the open volume.

    PathPrefix - Stores the path to prefix onto every open file operation.

    BlockHandle - Stores a pointer to the native OS handle to the disk,
        partition, or image.

    VolumeToken - Stores the mounted file system token.

    OpenFiles - Stores the number of files opened against this volume.

--*/

typedef struct _SETUP_VOLUME {
    PSETUP_CONTEXT Context;
    SETUP_DESTINATION_TYPE DestinationType;
    PSTR PathPrefix;
    PVOID BlockHandle;
    PVOID VolumeToken;
    ULONG OpenFiles;
} SETUP_VOLUME, *PSETUP_VOLUME;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// OS specific functions
//

INT
SetupOsEnumerateDevices (
    PSETUP_PARTITION_DESCRIPTION *DeviceArray,
    PULONG DeviceCount
    );

/*++

Routine Description:

    This routine enumerates all the disks and partitions on the system.

Arguments:

    DeviceArray - Supplies a pointer where an array of partition structures
        will be returned on success.

    DeviceCount - Supplies a pointer where the number of elements in the
        partition array will be returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
SetupOsGetPartitionInformation (
    PSETUP_DESTINATION Destination,
    PPARTITION_DEVICE_INFORMATION Information
    );

/*++

Routine Description:

    This routine returns the partition information for the given destination.

Arguments:

    Destination - Supplies a pointer to the partition to query.

    Information - Supplies a pointer where the information will be returned
        on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

PVOID
SetupOsOpenBootVolume (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine opens the boot volume on the current machine.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the open handle to the boot volume on success.

    NULL on failure.

--*/

//
// OS specific native file interface.
//

INT
SetupOsReadLink (
    PSTR Path,
    PSTR *LinkTarget,
    INT *LinkTargetSize
    );

/*++

Routine Description:

    This routine attempts to read a symbolic link.

Arguments:

    Path - Supplies a pointer to the path to open.

    LinkTarget - Supplies a pointer where an allocated link target will be
        returned on success. The caller is responsible for freeing this memory.

    LinkTargetSize - Supplies a pointer where the size of the link target will
        be returned on success.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

INT
SetupOsSymlink (
    PSTR Path,
    PSTR LinkTarget,
    INT LinkTargetSize
    );

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Path - Supplies a pointer to the path of the symbolic link to create.

    LinkTarget - Supplies a pointer to the target of the link.

    LinkTargetSize - Supplies a the size of the link target buffer in bytes.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

PVOID
SetupOsOpenDestination (
    PSETUP_DESTINATION Destination,
    INT Flags,
    INT CreatePermissions
    );

/*++

Routine Description:

    This routine opens a handle to a given destination.

Arguments:

    Destination - Supplies a pointer to the destination to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

VOID
SetupOsClose (
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

ssize_t
SetupOsRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

ssize_t
SetupOsWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

ULONGLONG
SetupOsSeek (
    PVOID Handle,
    ULONGLONG Offset
    );

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

ULONGLONG
SetupOsTell (
    PVOID Handle
    );

/*++

Routine Description:

    This routine returns the current offset in the given file or device.

Arguments:

    Handle - Supplies the handle.

Return Value:

    Returns the file offset on success.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

INT
SetupOsFstat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    );

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsFtruncate (
    PVOID Handle,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsEnumerateDirectory (
    PVOID Handle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    );

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    Handle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsCreateDirectory (
    PSTR Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsSetAttributes (
    PSTR Path,
    time_t ModificationDate,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine sets attributes on a given path.

Arguments:

    Path - Supplies the path string of the file to modify.

    ModificationDate - Supplies the new modification date to set.

    Permissions - Supplies the new permissions to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsReboot (
    VOID
    );

/*++

Routine Description:

    This routine reboots the machine.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsGetPlatformName (
    PSTR *Name,
    PSETUP_PLATFORM Fallback
    );

/*++

Routine Description:

    This routine gets the platform name.

Arguments:

    Name - Supplies a pointer where a pointer to an allocated string containing
        the SMBIOS system information product name will be returned if
        available. The caller is responsible for freeing this memory when done.

    Fallback - Supplies a fallback platform to use if the given platform
        string was not returned or did not match a known platform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupOsGetSystemMemorySize (
    PULONGLONG Megabytes
    );

/*++

Routine Description:

    This routine returns the number of megabytes of memory installed on the
    currently running system.

Arguments:

    Megabytes - Supplies a pointer to where the system memory capacity in
        megabytes will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Cache wrapper functions for OS layer functionality.
//

PVOID
SetupOpenDestination (
    PSETUP_DESTINATION Destination,
    INT Flags,
    INT CreatePermissions
    );

/*++

Routine Description:

    This routine opens a handle to a given destination.

Arguments:

    Destination - Supplies a pointer to the destination to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

VOID
SetupClose (
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

ssize_t
SetupRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

ssize_t
SetupWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

ULONGLONG
SetupSeek (
    PVOID Handle,
    ULONGLONG Offset
    );

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

INT
SetupFstat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    );

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupFtruncate (
    PVOID Handle,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupEnumerateDirectory (
    PVOID Handle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    );

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    Handle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// File I/O functions
//

PVOID
SetupVolumeOpen (
    PSETUP_CONTEXT Context,
    PSETUP_DESTINATION Destination,
    BOOL Format,
    BOOL AllowShortFileNames
    );

/*++

Routine Description:

    This routine opens a handle to a given volume.

Arguments:

    Context - Supplies a pointer to the application context.

    Destination - Supplies a pointer to the destination to open.

    Format - Supplies a boolean indicating if the volume should be formatted
        (TRUE) or just mounted (FALSE).

    AllowShortFileNames - Supplies a boolean indicating if the volume should be
        mounted to allow the creation of short file names.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

VOID
SetupVolumeClose (
    PSETUP_CONTEXT Context,
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a volume.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies a pointer to the open volume handle.

Return Value:

    None.

--*/

INT
SetupFileReadLink (
    PVOID Handle,
    PSTR Path,
    PSTR *LinkTarget,
    INT *LinkTargetSize
    );

/*++

Routine Description:

    This routine attempts to read a symbolic link.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path to open.

    LinkTarget - Supplies a pointer where an allocated link target will be
        returned on success. The caller is responsible for freeing this memory.

    LinkTargetSize - Supplies a pointer where the size of the link target will
        be returned on success.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

INT
SetupFileSymlink (
    PVOID Handle,
    PSTR Path,
    PSTR LinkTarget,
    INT LinkTargetSize
    );

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path of the symbolic link to create.

    LinkTarget - Supplies a pointer to the target of the link.

    LinkTargetSize - Supplies a the size of the link target buffer in bytes.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

PVOID
SetupFileOpen (
    PVOID Handle,
    PSTR Path,
    INT Flags,
    INT CreatePermissions
    );

/*++

Routine Description:

    This routine opens a handle to a file in a volume.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

VOID
SetupFileClose (
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a file.

Arguments:

    Handle - Supplies the handle to close.

Return Value:

    None.

--*/

ssize_t
SetupFileRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine reads from a file.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read on success.

--*/

ssize_t
SetupFileWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine writes data to an open file handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

ULONGLONG
SetupFileSeek (
    PVOID Handle,
    ULONGLONG Offset
    );

/*++

Routine Description:

    This routine seeks in the given file.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

INT
SetupFileFileStat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    );

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupFileFileTruncate (
    PVOID Handle,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupFileEnumerateDirectory (
    PVOID VolumeHandle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    );

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    VolumeHandle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupFileCreateDirectory (
    PVOID VolumeHandle,
    PSTR Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    VolumeHandle - Supplies a pointer to the volume handle.

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupFileSetAttributes (
    PVOID VolumeHandle,
    PSTR Path,
    time_t ModificationDate,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine sets attributes on a given path.

Arguments:

    VolumeHandle - Supplies a pointer to the volume handle.

    Path - Supplies the path string of the file to modify.

    ModificationDate - Supplies the new modification date to set.

    Permissions - Supplies the new permissions to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Partition I/O functions
//

PVOID
SetupPartitionOpen (
    PSETUP_CONTEXT Context,
    PSETUP_DESTINATION Destination,
    PPARTITION_DEVICE_INFORMATION PartitionInformation
    );

/*++

Routine Description:

    This routine opens a handle to a given partition destination.

Arguments:

    Context - Supplies a pointer to the application context.

    Destination - Supplies a pointer to the destination to open.

    PartitionInformation - Supplies an optional pointer where the partition
        information will be returned on success.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

VOID
SetupPartitionClose (
    PSETUP_CONTEXT Context,
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the open handle.

Return Value:

    None.

--*/

ssize_t
SetupPartitionRead (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine reads from a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read on success.

--*/

ssize_t
SetupPartitionWrite (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine writes to a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the data to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

--*/

ULONGLONG
SetupPartitionSeek (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    ULONGLONG Offset
    );

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Offset - Supplies the offset in blocks to seek to.

Return Value:

    Returns the resulting file offset in blocks after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

//
// Disk functions
//

INT
SetupFormatDisk (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine partitions a disk.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Platform support functions
//

BOOL
SetupParsePlatformString (
    PSETUP_CONTEXT Context,
    PSTR PlatformString
    );

/*++

Routine Description:

    This routine converts a platform string into a platform identifier, and
    sets it in the setup context.

Arguments:

    Context - Supplies a pointer to the setup context.

    PlatformString - Supplies a pointer to the string to convert to a
        platform identifier.

Return Value:

    TRUE if the platform name was successfully converted.

    FALSE if the name was invalid.

--*/

VOID
SetupPrintPlatformList (
    VOID
    );

/*++

Routine Description:

    This routine prints the supported platform list.

Arguments:

    None.

Return Value:

    None.

--*/

INT
SetupDeterminePlatform (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine finalizes the setup platform recipe to use.

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupInstallBootSector (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes the MBR and VBR to the disk. This should be done after
    the file system has set up the beginning of the install partition (so that
    it doesn't clobber the VBR being written here).

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupInstallPlatformBootFiles (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    );

/*++

Routine Description:

    This routine writes any custom platform specific boot files to the boot
    volume.

Arguments:

    Context - Supplies a pointer to the setup context.

    BootVolume - Supplies a pointer to the open boot volume.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Utility functions
//

PSETUP_DESTINATION
SetupCreateDestination (
    SETUP_DESTINATION_TYPE Type,
    PSTR Path,
    DEVICE_ID DeviceId
    );

/*++

Routine Description:

    This routine creates a setup destination structure.

Arguments:

    Type - Supplies the destination type.

    Path - Supplies an optional pointer to the path. A copy of this string will
        be made.

    DeviceId - Supplies an optional device ID.

Return Value:

    Returns a pointer to the newly created destination on success.

    NULL on allocation failure.

--*/

VOID
SetupDestroyDestination (
    PSETUP_DESTINATION Destination
    );

/*++

Routine Description:

    This routine destroys a setup destination structure.

Arguments:

    Destination - Supplies a pointer to the destination structure to free.

Return Value:

    None.

--*/

VOID
SetupDestroyDeviceDescriptions (
    PSETUP_PARTITION_DESCRIPTION Devices,
    ULONG DeviceCount
    );

/*++

Routine Description:

    This routine destroys an array of device descriptions.

Arguments:

    Devices - Supplies a pointer to the array to destroy.

    DeviceCount - Supplies the number of elements in the array.

Return Value:

    None.

--*/

VOID
SetupPrintDeviceDescription (
    PSETUP_PARTITION_DESCRIPTION Device,
    BOOL PrintHeader
    );

/*++

Routine Description:

    This routine prints a device description.

Arguments:

    Device - Supplies a pointer to the device description.

    PrintHeader - Supplies a boolean indicating if the column descriptions
        should be printed.

Return Value:

    None.

--*/

ULONG
SetupPrintSize (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine prints a formatted size a la 5.8M (M for megabytes).

Arguments:

    String - Supplies a pointer to the string buffer to print to.

    StringSize - Supplies the total size of the string buffer in bytes.

    Value - Supplies the value in bytes to print.

Return Value:

    Returns the number of bytes successfully converted.

--*/

VOID
SetupPrintDestination (
    PSETUP_DESTINATION Destination
    );

/*++

Routine Description:

    This routine prints a destination structure.

Arguments:

    Destination - Supplies a pointer to the destination.

Return Value:

    None.

--*/

PSETUP_DESTINATION
SetupParseDestination (
    SETUP_DESTINATION_TYPE DestinationType,
    PSTR Argument
    );

/*++

Routine Description:

    This routine converts a string argument into a destination. Device ID
    destinations can start with "0x", and everything else is treated as a
    path. An empty string is not valid.

Arguments:

    DestinationType - Supplies the destination type.

    Argument - Supplies the string argument.

Return Value:

    Returns a pointer to a newly created destination on success. The caller
    is responsible for destroying this structure.

    NULL if the argument is not valid.

--*/

PSTR
SetupAppendPaths (
    PSTR Path1,
    PSTR Path2
    );

/*++

Routine Description:

    This routine appends two paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

INT
SetupUpdateFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PVOID Source,
    PSTR DestinationPath,
    PSTR SourcePath
    );

/*++

Routine Description:

    This routine copies the given path from the source to the destination if
    the destination is older than the source. If the source is a directory, the
    contents of that directory are recursively copied to the destination
    (regardless of the age of the files inside the directory).

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Destination - Supplies a pointer to the open destination volume
        handle.

    Source - Supplies a pointer to the open source volume handle.

    DestinationPath - Supplies a pointer to the path of the file to create at
        the destination.

    SourcePath - Supplies the source path of the copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupCopyFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PVOID Source,
    PSTR DestinationPath,
    PSTR SourcePath
    );

/*++

Routine Description:

    This routine copies the given path from the source to the destination. If
    the source is a directory, the contents of that directory are recursively
    copied to the destination.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Destination - Supplies a pointer to the open destination volume
        handle.

    Source - Supplies a pointer to the open source volume handle.

    DestinationPath - Supplies a pointer to the path of the file to create at
        the destination.

    SourcePath - Supplies the source path of the copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

