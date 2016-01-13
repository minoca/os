/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    createimage.h

Abstract:

    This header contains definitions for the createimage program.

Author:

    Evan Green 20-Jun-2012

--*/

//
// --------------------------------------------------------------------- Macros
//

#define VHD_VERSION(_Major, _Minor) (((_Major) << 16) | (_Minor))
#define VHD_DISK_GEOMETRY(_Cylinders, _Heads, _Sectors)                        \
    ((RtlByteSwapUshort((_Cylinders) & 0xFFFF)) | (((_Heads) & 0xFF) << 16) |  \
     (((_Sectors) & 0xFF) << 24))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of a sector for createimage images.
//

#define CREATEIMAGE_SECTOR_SIZE 512

//
// VHD image format definitions
//

#define VHD_COOKIE 0x78697463656E6F63ULL
#define VHD_FEATURES_DEFAULT 0x00000002
#define VHD_FILE_FORMAT_VERSION 0x00010000
#define VHD_FIXED_DISK_DATA_OFFSET 0xFFFFFFFFFFFFFFFFULL
#define VHD_TIME_TO_EPOCH_DELTA 946684800
#define VHD_CREATOR_ID 0x636F6E4D // 'Mnoc'
#define VHD_HOST_OS 0x5769326B // 'Wi2k'
#define VHD_DISK_TYPE_FIXED 2
#define VHD_DISK_TYPE_DYNAMIC 3
#define VHD_DISK_TYPE_DIFFERENCING 4

//
// Define createimage options.
//

#define CREATEIMAGE_OPTION_VERBOSE                     0x00000001
#define CREATEIMAGE_OPTION_IGNORE_MISSING              0x00000002
#define CREATEIMAGE_OPTION_CREATE_ALWAYS               0x00000004
#define CREATEIMAGE_OPTION_ALIGN_PARTITIONS            0x00000008
#define CREATEIMAGE_OPTION_GPT                         0x00000010
#define CREATEIMAGE_OPTION_EFI                         0x00000020
#define CREATEIMAGE_OPTION_TARGET_DEBUG                0x00000040
#define CREATEIMAGE_OPTION_BOOT_ALLOW_SHORT_FILE_NAMES 0x00000080

#define CREATEIMAGE_DEFAULT_PERMISSIONS                             \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE |       \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_WRITE |     \
     FILE_PERMISSION_OTHER_READ)

//
// Define the VMDK text file format. The arguments to the format are:
// Block count (64-bit), output image name, long content ID (x2),
// UUID bytes (x8), cylinders (64-bit)
//

#define VMDK_FORMAT_STRING                                                     \
    "# Disk DescriptorFile\n"                                                  \
    "version=1\n"                                                              \
    "encoding=\"windows-1252\"\n"                                              \
    "CID=fffffffe\n"                                                           \
    "parentCID=ffffffff\n"                                                     \
    "isNativeSnapshot=\"no\"\n"                                                \
    "createType=\"monolithicFlat\"\n"                                          \
    "\n"                                                                       \
    "# Extent description\n"                                                   \
    "RW %I64d FLAT \"%s\" 0\n"                                                 \
    "\n"                                                                       \
    "# The Disk Data Base \n"                                                  \
    "#DDB\n"                                                                   \
    "\n"                                                                       \
    "ddb.virtualHWVersion = \"6\"\n"                                           \
    "ddb.longContentID = \"8273f1a4%08x%08xfffffffe\"\n"                       \
    "ddb.uuid = \"60 00 C2 9c 27 37 c6 51-%02x %02x %02x %02x %02x %02x %02x " \
    "%02x\"\n"                                                                 \
    "ddb.geometry.cylinders = \"%I64d\"\n"                                     \
    "ddb.geometry.heads = \"16\"\n"                                            \
    "ddb.geometry.sectors = \"63\"\n"                                          \
    "ddb.adapterType = \"ide\"\n"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CREATEIMAGE_FORMAT {
    CreateimageFormatInvalid,
    CreateimageFormatFlat,
    CreateimageFormatVmdk,
    CreateimageFormatVhd,
} CREATEIMAGE_FORMAT, *PCREATEIMAGE_FORMAT;

/*++

Structure Description:

    This structure defines a raw file that is to be written to the beginning of
    a given partition.

Members:

    FileName - Stores the path to the raw file to write to the beginning of the
        partition.

    PartitionNumber - Stores the index number of the partition where the file
        should be written.

    Partition - Stores a pointer to the partition that is bound to the given
        partition number.

--*/

typedef struct _CREATEIMAGE_RAW_FILE {
    PSTR FileName;
    ULONG PartitionNumber;
    PPARTITION_INFORMATION Partition;
} CREATEIMAGE_RAW_FILE, *PCREATEIMAGE_RAW_FILE;

/*++

Structure Description:

    This structure stores the options that this instance of the program was
    invoked with.

Members:

    Options - Stores the bitfield of options.

    Format - Stores the disk structure.

    Output - Stores the name of the output image.

    MbrFile - Stores the name of the file containing the boot code for the
        Master Boot Record, whose contents will be expanded out and merged with
        the first sector of the disk.

    VbrFile - Stores the name of the file containing the boot code for the
        Volume Boot Record, whose contents will be expanded out and merged with
        the first sector of the install partition.

    DiskSize - Stores the requested disk image size in sectors. If zero,
        createimage will attempt to detect a reasonable size.

    BootPartitionNumber - Stores the partition number of the desired boot
        partition. Partitions are numbered from one. Logical partitions start
        at 5.

    InstallPartitionNumber - Stores the partition number to install to.
        Partitions are numbered from one. Logical partitions start at 5.

    InstallPartition - Stores a pointer to the partition to install to.

    ImageMinimumSizeMegabytes - Stores the requested minimum image size in
        megabytes. If zero, createimage will attempt to detect a reasonable
        size or use the precise suggested size.

    FileCount - Stores the total number of files in the destination image.

    FilesWritten - Stores the number of files written so far.

    PartitionContext - Stores the partition library context.

    CreatePartitions - Stores a pointer to an array of partition information
        for the partition layout to create.

    CreatePartitionCount - Stores the number of elements in the create
        partition array.

    OutputFile - Stores a pointer to the output file.

    BootFiles - Stores a pointer to an arry of strings containing the files to
        put on the boot partition. This may be the same as the install
        partition.

    BootFileCount - Stores the count of boot files.

    DebugDeviceIndex - Stores the index of the debug device to use.

    KernelCommandLine - Stores an optional pointer to the kernel command line
        to use.

    RawFiles - Stores an array of raw files to write out to the beginning of
        partitions.

    RawFileCount - Stores the count of raw files.

--*/

typedef struct _CREATEIMAGE_CONTEXT {
    ULONG Options;
    CREATEIMAGE_FORMAT Format;
    PSTR Output;
    PSTR MbrFile;
    PSTR VbrFile;
    ULONGLONG DiskSize;
    ULONG BootPartitionNumber;
    ULONG InstallPartitionNumber;
    PPARTITION_INFORMATION BootPartition;
    PPARTITION_INFORMATION InstallPartition;
    ULONG ImageMinimumSizeMegabytes;
    ULONGLONG FileCount;
    ULONGLONG FilesWritten;
    PARTITION_CONTEXT PartitionContext;
    PPARTITION_INFORMATION CreatePartitions;
    ULONG CreatePartitionCount;
    FILE *OutputFile;
    PSTR *BootFiles;
    ULONG BootFileCount;
    ULONG DebugDeviceIndex;
    PSTR KernelCommandLine;
    PCREATEIMAGE_RAW_FILE RawFiles;
    ULONG RawFileCount;
} CREATEIMAGE_CONTEXT, *PCREATEIMAGE_CONTEXT;

/*++

Structure Description:

    This structure describes the VHD image format footer structure.

Members:

    Cookie - Stores the original creator of the hard disk image. Everyone
        puts "conectix" in there. Set this to VHD_COOKIE to get that effect.

    Features - Stores any special features of the disk. Set this to
        VHD_FEATURES_DEFAULT.

    FileFormatVersion - Stores the version of the VHD format. Set this to
        VHD_FILE_FORMAT_VERSION.

    DataOffset - Stores the absolute byte offset from the beginning of the file
        to the next structure. This field is used for dynamic disks and
        differencing disks, but not fixed disks. For fixed disks, set this to
        VHD_FIXED_DISK_DATA_OFFSET.

    Timestamp - Stores the creation time of the disk image, in seconds since
        midnight on January 1, 2000 UTC. Use the define VHD_TIME_TO_EPOCH_DELTA
        to convert from unix time_t to this format.

    CreatorApplication - Stores the unique ID of the application that
        created the disk.

    CreatorVersion - Stores the major/minor version of the application that
        created the disk image.

    CreatorHostOs - Stores the type of host operating system this disk image is
        created on.

    OriginalSize - Stores the size of the hard disk in bytes, from the
        perspective of the virtual machine, at creation time. This field is
        for informational purposes.

    CurrentSize - Stores the current size of the hard disk in bytes, from the
        perspective of the virtual machine. This value is the same as the
        original size when created. This value can change if the disk is
        expanded.

    DiskGeometry - Stores the disk geometry of the disk.

    DiskType - Stores the disk type (fixed, dynamic, or differencing). See
        VHD_DISK_TYPE_* definitions.

    Checksum - Stores the checksum, which is the ones complement of all the
        bytes in the footer except the checksum field.

    UniqueId - Stores a 128-bit UUID associated with the disk.

    SavedState - Stores a value that if set to 1 indicates the VM is in a
        saved state. Operations such as compaction and expansion cannot be
        performed on a disk in a saved state.

    Reserved - Stores a bunch of zeros to pad the structure out to 512 bytes.

--*/

typedef struct _VHD_FOOTER {
    ULONGLONG Cookie;
    ULONG Features;
    ULONG FileFormatVersion;
    ULONGLONG DataOffset;
    ULONG Timestamp;
    ULONG CreatorApplication;
    ULONG CreatorVersion;
    ULONG CreatorHostOs;
    ULONGLONG OriginalSize;
    ULONGLONG CurrentSize;
    ULONG DiskGeometry;
    ULONG DiskType;
    ULONG Checksum;
    UCHAR UniqueId[16];
    UCHAR SavedState;
    UCHAR Reserved[427];
} PACKED VHD_FOOTER, *PVHD_FOOTER;

/*++

Structure Description:

    This structure stores information about a file handle in the target image.

Members:

    Partition - Supplies an optional pointer to the partition this volume is in.

    Context - Supplies a pointer to the application context this volume is
        opened in.

    FileSystemHandle - Supplies the handle returned by the file system.

--*/

typedef struct _CI_VOLUME {
    PPARTITION_INFORMATION Partition;
    PCREATEIMAGE_CONTEXT Context;
    PVOID FileSystemHandle;
} CI_VOLUME, *PCI_VOLUME;

/*++

Structure Description:

    This structure stores information about a file handle in the target image.

Members:

    Volume - Stores a pointer to the mounted volume token.

    FileSystemHandle - Stores the file handle returned by the file system
        code.

    Position - Stores the current file offset in bytes.

    Properties - Stores the file properties to write out when the file is
        closed.

--*/

typedef struct _CI_HANDLE {
    PVOID Volume;
    PVOID FileSystemHandle;
    ULONGLONG Position;
    FILE_PROPERTIES Properties;
} CI_HANDLE, *PCI_HANDLE;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list head of all loaded images.
//

extern LIST_ENTRY LoadedImagesHead;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Functions implemented be creatimage, callable by others.
//

PVOID
CiMalloc (
    size_t AllocationSize
    );

/*++

Routine Description:

    This routine allocates from the heap.

Arguments:

    AllocationSize - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

VOID
CiFree (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees an allocation from the heap.

Arguments:

    Allocation - Supplies a pointer to the allocation.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

PSTR
CiCopyString (
    PSTR String
    );

/*++

Routine Description:

    This routine allocates a copy of the string.

Arguments:

    Allocation - Supplies a pointer to the allocation.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

BOOL
CiOpen (
    PCI_VOLUME Volume,
    PSTR Path,
    BOOL Create,
    PCI_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a file on the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies a pointer to the complete path within the volume to open.

    Create - Supplies a boolean indicating if the file should be created if it
        does not exist. If this is TRUE and the file does exist, the call will
        fail.

    Handle - Supplies a pointer where the handle will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
CiCreateDirectory (
    PCI_VOLUME Volume,
    PSTR Path
    );

/*++

Routine Description:

    This routine creates a directory on the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies a pointer to the complete path within the volume to open.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
CiClose (
    PCI_HANDLE Handle
    );

/*++

Routine Description:

    This routine closes an open handle on the target image.

Arguments:

    Handle - Supplies a pointer to the open handle.

Return Value:

    None.

--*/

BOOL
CiRead (
    PCI_HANDLE Handle,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine reads from a file on the target image.

Arguments:

    Handle - Supplies the open handle to the file.

    Buffer - Supplies the buffer where the read data will be returned.

    Size - Supplies the number of bytes to read.

    BytesCompleted - Supplies a pointer where the number of bytes read will be
        returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
CiWrite (
    PCI_HANDLE Handle,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine writes to a file on the target image.

Arguments:

    Handle - Supplies the open handle to the file.

    Buffer - Supplies the buffer containing the data to write.

    Size - Supplies the number of bytes to write.

    BytesCompleted - Supplies a pointer where the number of bytes written will
        be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
CiSetFileProperties (
    PCI_HANDLE Handle,
    IO_OBJECT_TYPE Type,
    FILE_PERMISSIONS Permissions,
    time_t ModificationTime,
    time_t AccessTime
    );

/*++

Routine Description:

    This routine sets the properties on the open file handle.

Arguments:

    Handle - Supplies the open handle to the file.

    Type - Supplies the file type.

    Permissions - Supplies the file permissions.

    ModificationTime - Supplies the modification time to set in the file.

    AccessTime - Supplies the last access time to set in the file.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

//
// Functions createimage calls.
//

//
// Image support functions.
//

KSTATUS
CiInitializeImageSupport (
    );

/*++

Routine Description:

    This routine initializes the image library for use in the image creation
    tool.

Arguments:

    None.

Return Value:

    Status code.

--*/

PVOID
CiGetLoadedImageBuffer (
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine returns the in-memory image buffer for the most recently
    loaded image.

Arguments:

    BufferSize - Supplies a pointer where the image buffer size will be
        returned.

Return Value:

    Returns a pointer to the in-memory image list on success.

    NULL on failure.

--*/

VOID
CiUnloadAllImages (
    );

/*++

Routine Description:

    This routine unloads all loaded images.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Partition support functions.
//

KSTATUS
CiInitializePartitionSupport (
    PCREATEIMAGE_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the partition context.

Arguments:

    Context - Supplies a pointer to the createimage context.

Return Value:

    Status code.

--*/

VOID
CiDestroyPartitionSupport (
    PCREATEIMAGE_CONTEXT Context
    );

/*++

Routine Description:

    This routine rears down the partition support in createimage.

Arguments:

    Context - Supplies a pointer to the createimage context.

Return Value:

    None.

--*/

KSTATUS
CiParsePartitionLayout (
    PCREATEIMAGE_CONTEXT Context,
    PSTR Argument
    );

/*++

Routine Description:

    This routine initializes parses the partition layout specified on the
    command line.

Arguments:

    Context - Supplies a pointer to the createimage context.

    Argument - Supplies the partition layout argument.

Return Value:

    Status code.

--*/

KSTATUS
CiWritePartitionLayout (
    PCREATEIMAGE_CONTEXT Context,
    ULONGLONG MainPartitionSize
    );

/*++

Routine Description:

    This routine writes the partition layout to the output image. This erases
    everything on the disk.

Arguments:

    Context - Supplies a pointer to the createimage context.

    MainPartitionSize - Supplies the size of the main partition, which is
        used for any unsized partitions.

Return Value:

    Status code.

--*/

KSTATUS
CiBindToPartitions (
    PCREATEIMAGE_CONTEXT Context,
    ULONGLONG DiskSize
    );

/*++

Routine Description:

    This routine binds to the partitions to install to.

Arguments:

    Context - Supplies a pointer to the createimage context.

    DiskSize - Supplies the size of the disk in blocks.

Return Value:

    Status code.

--*/

//
// Boot configuration functions
//

KSTATUS
CiCreateBootConfigurationFile (
    PCI_VOLUME BootVolume,
    PCREATEIMAGE_CONTEXT Context
    );

/*++

Routine Description:

    This routine creates the boot configuration file.

Arguments:

    BootVolume - Supplies a pointer to the boot volume, the volume that
        contains the EFI system partition (or the active partition on a legacy
        system).

    Context - Supplies a pointer to the createimage context.

Return Value:

    Status code.

--*/

