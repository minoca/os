/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatlibp.h

Abstract:

    This header contains internal definitions for the FAT file system library.

Author:

    Evan Green 23-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts a cluster number into a byte offset on the disk.
//

#define FAT_CLUSTER_TO_BYTE(_Volume, _Cluster) \
    ((_Volume)->ClusterByteOffset +            \
     (((ULONGLONG)(_Cluster) - FAT_CLUSTER_BEGIN) * (_Volume)->ClusterSize))

//
// These macros read from and write to a 16-bit value that might not be
// aligned.
//

#define FAT_READ_INT16(_Pointer) \
    (((volatile UCHAR *)(_Pointer))[0] | \
     (((USHORT)((volatile UCHAR *)(_Pointer))[1]) << 8))

#define FAT_WRITE_INT16(_Pointer, _Value)               \
    ((volatile UCHAR *)(_Pointer))[0] = (UCHAR)(_Value),          \
    ((volatile UCHAR *)(_Pointer))[1] = (UCHAR)((_Value) >> 8)

//
// This macro gets the FAT window index for the given cluster. It's basically
// (ClusterIndex * SizeOfAClusterNumber) / WindowSize.
//

#define FAT_WINDOW_INDEX(_Volume, _Cluster) \
    (((_Cluster) << (_Volume)->ClusterWidthShift) >> \
     (_Volume)->FatCache.WindowShift)

//
// This macro gets the index into the total array of clusters for the given
// window index. It's basically (WindowIndex * WindowSize) /
// SizeOfAClusterNumber.
//

#define FAT_WINDOW_INDEX_TO_CLUSTER(_Volume, _WindowIndex)      \
    (((_WindowIndex) << (_Volume)->FatCache.WindowShift) >>     \
     (_Volume)->ClusterWidthShift)

//
// This macro gets the closest seek table entry for the given file offset.
//

#define FAT_SEEK_TABLE_INDEX(_Offset) ((_Offset) >> FAT_SEEK_OFFSET_SHIFT)

//
// This macro converts a seek table index back into an offset.
//

#define FAT_SEEK_TABLE_OFFSET(_Index) ((_Index) << FAT_SEEK_OFFSET_SHIFT)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define FAT directory flags.
//

#define FAT_DIRECTORY_FLAG_DIRTY 0x00000001
#define FAT_DIRECTORY_FLAG_POSITION_AT_END 0x00000002

//
// Define the number of seek table entries to keep across the entire
// theoretical 4GB file.
//

#define FAT_SEEK_TABLE_SHIFT 6
#define FAT_SEEK_TABLE_SIZE (1UL << FAT_SEEK_TABLE_SHIFT)
#define FAT_SEEK_OFFSET_SHIFT (32 - FAT_SEEK_TABLE_SHIFT)
#define FAT_SEEK_OFFSET_MASK ((1UL << FAT_SEEK_OFFSET_SHIFT) - 1)

//
// Define bits in the encoded non-standard permissions field.
//

#define FAT_ENCODED_PROPERTY_BYTE0_BIT7 (1 << 13)
#define FAT_ENCODED_PROPERTY_SYMLINK (1 << 12)
#define FAT_ENCODED_PROPERTY_PERMISSION_MASK 0x0FFF

//
// Define the size of the root directory, in bytes, when formatting FAT12/16.
//

#define FAT_MINIMUM_ROOT_DIRECTORY_SIZE 0x4000

//
// Define the FAT volume flags.
//

#define FAT_VOLUME_FLAG_COMPATIBILITY_MODE 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the dirty region for a particular FAT window.

Members:

    Min - Stores the minimum offset that's dirty.

    Max - Stores the maximum offset that's dirty.

--*/

typedef struct _FAT_WINDOW_DIRTY_REGION {
    ULONG Min;
    ULONG Max;
} FAT_WINDOW_DIRTY_REGION, *PFAT_WINDOW_DIRTY_REGION;

/*++

Structure Description:

    This structure defines the cache for the File Allocation Table.

Members:

    WindowBuffers - Stores an array of pointers to I/O buffers that store
        windows into the File Allocation Table.

    Windows - Stores an array of pointers to virtually contiguous mappings of
        the I/O buffers that store windows into the File Allocation Table.

    Dirty - Stores an array of the region that is dirty in each window.

    DirtyStart - Stores the starting index (inclusive) of the dirty FAT windows.

    DirtyEnd - Stores the ending index (exclusive) of the dirty FAT windows.

    WindowCount - Stores the number of windows.

    WindowSize - Stores the size of each window.

    WindowShift - Stores the number of bits in the window size.

--*/

typedef struct _FAT_CACHE {
    PFAT_IO_BUFFER *WindowBuffers;
    PVOID *Windows;
    PFAT_WINDOW_DIRTY_REGION Dirty;
    ULONG DirtyStart;
    ULONG DirtyEnd;
    ULONG WindowCount;
    ULONG WindowSize;
    ULONG WindowShift;
} FAT_CACHE, *PFAT_CACHE;

/*++

Structure Description:

    This structure defines global state associated with a mounted FAT volume.

Members:

    Device - Stores information about the underlying device.

    Format - Stores the FAT format: FAT12, FAT16, or FAT32.

    Flags - Stores a bitmask of FAT volume flags. See FAT_VOLUME_FLAG_* for
        definitions.

    BlockShift - Stores the number of bits to shift to convert from bytes to
        blocks. This means the block size must be a power of two.

    ClusterSize - Stores the size of a cluster, in bytes.

    SectorSize - Stores the size of one sector, according to the volume.

    MaxCluster - Stores the highest addressable cluster number in the volume,
        inclusive. If there are 2 actual data clusters on the volume (a tiny
        volume), then this value would be 4 (as the data area is offset by
        2 clusters).

    ClusterShift - Stores the number of bits to shift to convert from bytes to
        clusters. This mean the cluster size must be a power of two.

    ClusterCount - Stores the total number of clusters in the volume, including
        clusters 0 and 1.

    ClusterBad - Stores the cluster value that indicates a bad cluster. All
        values above this are considered the end of file marker.

    ClusterEnd - Stores the value to write for the end file marker.

    ClusterWidthShift - Stores the number of bits to shift to get the width of
        a cluster entry. For FAT12 this is incorrectly set to 0 (as the correct
        value would really by 1.5). For FAT16 this is 1 (because a cluster is
        2 bytes wide) and for FAT32 this is 2 (because a cluster is 4 bytes
        wide).

    ReservedSectorCount - Stores the number of sectors before the first FAT.

    RootDirectoryCluster - Stores the cluster number of the root directory.
        For FAT12/16, this is set to 0.

    RootDirectoryCount - Stores the maximum number of entries in the root
        directory. This only applies to FAT12/16, the root directory in FAT32
        is a normal data cluster.

    RootDirectoryByteOffset - Stores the offset in bytes from the beginning of
        the volume to the root directory.

    ClusterByteOffset - Stores the offset, in bytes, from the beginning of
        the volume to cluster 2 (the first valid cluster).

    ClusterSearchStart - Stores the cluster to start searching from. 0
        specifies an uninitialized value.

    InformationByteOffset - Stores the offset, in bytes, to the FS information
        block.

    FatByteStart - Stores the offset, in bytes, to the beginning of the first
        File Allocation Table.

    FatSize - Stores the size in bytes of each File Allocation Table.

    FatCount - Stores the number of File Allocation Tables.

    Lock - Stores a pointer to the lock synchronizing global access to the
        volume and file mapping tree.

    FileMappingTree - Stores the tree of mappings between file IDs and
        directory entries.

    FatCache - Stores the File Allocation Table cache. This is used for cluster
        allocation and next cluster lookup during seek, read, and write.

--*/

typedef struct _FAT_VOLUME {
    BLOCK_DEVICE_PARAMETERS Device;
    FAT_FORMAT Format;
    ULONG Flags;
    ULONG BlockShift;
    ULONG ClusterSize;
    ULONG ClusterShift;
    ULONG ClusterCount;
    ULONG ClusterBad;
    ULONG ClusterEnd;
    ULONG ClusterWidthShift;
    ULONG SectorSize;
    USHORT ReservedSectorCount;
    ULONG RootDirectoryCluster;
    ULONG RootDirectoryCount;
    ULONGLONG RootDirectoryByteOffset;
    ULONGLONG ClusterByteOffset;
    ULONG ClusterSearchStart;
    ULONGLONG InformationByteOffset;
    ULONGLONG FatByteStart;
    ULONGLONG FatSize;
    ULONG FatCount;
    PVOID Lock;
    RED_BLACK_TREE FileMappingTree;
    FAT_CACHE FatCache;
} FAT_VOLUME, *PFAT_VOLUME;

/*++

Structure Description:

    This structure defines file system state associated with an open file.

Members:

    Volume - Stores a pointer to the volume that has the file on it.

    OpenFlags - Stores the flags supplied when the file was opened. See
        OPEN_FILE_FLAG_* definitions.

    IsRootDirectory - Stores a boolean indicating if this file is actually the
        root directory outside the data area. This will only be set for root
        directories in FAT12/FAT16, as the root directory in FAT32 resides in
        the data area.

    ScratchIoBufferLock - Stores a pointer to the lock synchronizing access to
        the scratch FAT I/O buffer.

    ScratchIoBuffer - Stores a pointer to a FAT I/O buffer where file system
        data can be temporarily read and written to the device. This buffer
        will be the native block size of the underlying block device, which is
        guaranteed to be at least 512 bytes large. This should only be used
        for page file operations.

    SeekTable - Stores cluster numbers for file offsets spread evenly through
        out the maximum theoretical file size of 4GB. The first value is file
        offset 0, and is always filled in.

--*/

typedef struct _FAT_FILE {
    PFAT_VOLUME Volume;
    ULONG OpenFlags;
    BOOL IsRootDirectory;
    PVOID ScratchIoBufferLock;
    PFAT_IO_BUFFER ScratchIoBuffer;
    ULONG SeekTable[FAT_SEEK_TABLE_SIZE];
} FAT_FILE, *PFAT_FILE;

/*++

Structure Description:

    This structure defines temporary context used to seek, read, write, and
    flush a directory file.

Members:

    File - Stores a pointer to the FAT file for the directory.

    ClusterBuffer - Stores a pointer to a FAT I/O buffer that stores cluster
        data for the directory. It is always cluster-aligned.

    ClusterPosition - Stores the file seek information for the contents of the
        cluster buffer.

    BufferNextIndex - Stores the byte offset of the next place in the buffer to
        read from or write to.

    FatFlags - Stores a field of FAT directory flags. See FAT_DIRECTORY_FLAG_*
        definitions.

    IoFlags - Stores a bitmask of flags for all directory I/O operations. See
        IO_FLAG_* definitions.

--*/

typedef struct _FAT_DIRECTORY_CONTEXT {
    PFAT_FILE File;
    PFAT_IO_BUFFER ClusterBuffer;
    FAT_SEEK_INFORMATION ClusterPosition;
    ULONG BufferNextIndex;
    ULONG FatFlags;
    ULONG IoFlags;
} FAT_DIRECTORY_CONTEXT, *PFAT_DIRECTORY_CONTEXT;

/*++

Structure Description:

    This structure defines the file properties encoded into a short name. This
    format is non-standard, but allows proper permissions and ownership to be
    encoded in FAT. This should appear to other OSes as a junky but valid
    short name. Any OS that supports long names will display the proper names.

Members:

    Cluster - Stores the file cluster number, used to verify that the
        information is valid and consistent.

    Owner - Stores the file owner ID.

    Group - Stores the file group ID.

    Permissions - Stores the file permissions and other odds and ends.

--*/

typedef struct _FAT_ENCODED_PROPERTIES {
    ULONG Cluster;
    USHORT Owner;
    USHORT Group;
    USHORT Permissions;
} FAT_ENCODED_PROPERTIES, *PFAT_ENCODED_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to prevent the use of encoded non-standard file properties.
// If encoded properties are not in use, a random short file name will be
// created when needed.
//

extern BOOL FatDisableEncodedProperties;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
FatpLookupDirectoryEntry (
    PFAT_VOLUME Volume,
    PFAT_DIRECTORY_CONTEXT Directory,
    PCSTR Name,
    ULONG NameLength,
    PFAT_DIRECTORY_ENTRY Entry,
    PULONGLONG EntryOffset
    );

/*++

Routine Description:

    This routine locates the directory entry for the given file or directory.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    Directory - Supplies a pointer to the directory context for the open file.

    Name - Supplies the name of the file or directory to open.

    NameLength - Supplies the size of the path buffer in bytes, including the
        null terminator.

    Entry - Supplies a pointer where the directory entry will be returned.

    EntryOffset - Supplies an optional pointer where the offset of the returned
        entry will be returned, in bytes, from the beginning of the directory
        file.

Return Value:

    Status code.

--*/

KSTATUS
FatpCreateFile (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameLength,
    PULONGLONG DirectorySize,
    PFILE_PROPERTIES FileProperties
    );

/*++

Routine Description:

    This routine creates a file or directory at the given path. This routine
    fails if the file already exists.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    FileName - Supplies the name of the file to create.

    FileNameLength - Supplies the length of the file name buffer, in bytes,
        including the null terminator.

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    FileProperties - Supplies a pointer that on input contains the file
        properties to set on the created file. On output, the new file ID
        will be returned here, and the size and hard link count will be
        initialized.

Return Value:

    Status code.

--*/

KSTATUS
FatpCreateDirectoryEntry (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameLength,
    PULONGLONG DirectorySize,
    PFAT_DIRECTORY_ENTRY Entry
    );

/*++

Routine Description:

    This routine creates a file or directory entry at the given path. This
    routine fails if the file already exists.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    FileName - Supplies the name of the file to create.

    FileNameLength - Supplies the length of the file name buffer, in bytes,
        including the null terminator.

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    Entry - Supplies a pointer to the directory entry information to use for
        the entry (except for the file name which is provided in the previous
        parameter).

Return Value:

    Status code.

--*/

KSTATUS
FatpReadNextDirectoryEntry (
    PFAT_DIRECTORY_CONTEXT Directory,
    PIRP Irp,
    PSTR FileName,
    PULONG FileNameLength,
    PFAT_DIRECTORY_ENTRY DirectoryEntry,
    PULONG EntriesRead
    );

/*++

Routine Description:

    This routine reads the next valid directory entry out of the directory.

Arguments:

    Directory - Supplies a pointer to the directory context to use for this
        read.

    Irp - Supplies the optional IRP to use for these device reads and writes.

    FileName - Supplies a pointer where the name of the file will be returned.
        This buffer must be at least FAT_MAX_LONG_FILE_LENGTH + 1 bytes large.

    FileNameLength - Supplies a pointer that on input contains the size of the
        file name buffer. On output, returns the size of the file name in
        bytes including the null terminator.

    DirectoryEntry - Supplies a pointer where the directory entry information
        will be returned.

    EntriesRead - Supplies a pointer where the number of directory entries read
        (valid or not) to get to this valid entry will be returned. The
        official offset of the returned directory entry is this returned value
        minus one.

Return Value:

    Status code.

--*/

KSTATUS
FatpGetNextCluster (
    PFAT_VOLUME Volume,
    ULONG IoFlags,
    ULONG CurrentCluster,
    PULONG NextCluster
    );

/*++

Routine Description:

    This routine follows the singly linked list of clusters by looking up the
    current entry in the File Allocation Table to determine the index of the
    next cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    IoFlags - Supplies flags regarding any necessary I/O operations.
        See IO_FLAG_* definitions.

    CurrentCluster - Supplies the cluster to follow.

    NextCluster - Supplies a pointer where the next cluster will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid cluster was supplied.

    Other error codes on device I/O errors.

--*/

KSTATUS
FatpAllocateCluster (
    PFAT_VOLUME Volume,
    ULONG PreviousCluster,
    PULONG NewCluster,
    BOOL Flush
    );

/*++

Routine Description:

    This routine allocates a free cluster, and chains the cluster so that the
    specified previous cluster points to it.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    PreviousCluster - Supplies the cluster that should point to the newly
        allocated cluster. Specify FAT32_CLUSTER_END if no previous cluster
        should be updated.

    NewCluster - Supplies a pointer that will receive the new cluster number.

    Flush - Supplies a boolean indicating if the FAT cache should be flushed.
        Supply TRUE here unless multiple clusters are going to be allocated in
        bulk, in which case the caller needs to explicitly flush the FAT
        cache.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid cluster was supplied.

    STATUS_VOLUME_FULL if no free clusters exist.

    Other error codes on device I/O errors.

--*/

KSTATUS
FatpFreeClusterChain (
    PFAT_VOLUME Volume,
    PVOID Irp,
    ULONG FirstCluster
    );

/*++

Routine Description:

    This routine marks all clusters in the given list as free.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    Irp - Supplies an optional pointer to an IRP to use for disk operations.

    FirstCluster - Supplies the first cluster in the list, which will also be
        marked as free.

Return Value:

    Status code.

--*/

KSTATUS
FatpIsDirectoryEmpty (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PBOOL Empty
    );

/*++

Routine Description:

    This routine determines if the given directory is empty or not.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the ID of the directory to query.

    Empty - Supplies a pointer where a boolean will be returned indicating
        whether or not the directory is empty.

Return Value:

    Status code.

--*/

CHAR
FatpChecksumDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry
    );

/*++

Routine Description:

    This routine returns the checksum of the given fat short directory entry
    based on the file name.

Arguments:

    Entry - Supplies a pointer to the directory entry.

Return Value:

    Returns the checksum of the directory entry.

--*/

KSTATUS
FatpEraseDirectoryEntry (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONGLONG EntryOffset,
    PBOOL EntryErased
    );

/*++

Routine Description:

    This routine writes over the specified directory entry and any long file
    name entries before it.

Arguments:

    Directory - Supplies a pointer to a context for the directory to be
        modified.

    EntryOffset - Supplies the directory offset of the entry to delete.

    EntryErased - Supplies a pointer that receives a boolean indicating whether
        or not the entry was erased. This can be set to TRUE even if the
        routine returns a failure status.

Return Value:

    Status code.

--*/

KSTATUS
FatpFixupDotDot (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    ULONG NewCluster
    );

/*++

Routine Description:

    This routine changes the cluster number for the dot dot entry.

Arguments:

    Volume - Supplies the opaque pointer to the volume.

    DirectoryFileId - Supplies the ID of the directory that needs to be fixed
        up.

    NewCluster - Supplies the new cluster number dot dot should point at.

Return Value:

    Status code.

--*/

KSTATUS
FatpAllocateClusterForEmptyFile (
    PFAT_VOLUME Volume,
    PFAT_DIRECTORY_CONTEXT DirectoryContext,
    ULONG DirectoryFileId,
    PFAT_DIRECTORY_ENTRY Entry,
    ULONGLONG EntryOffset
    );

/*++

Routine Description:

    This routine allocates and writes out a cluster for a given empty file,
    since the starting cluster ID is what uniquely identifies a file.

Arguments:

    Volume - Supplies a pointer to the volume.

    DirectoryContext - Supplies a pointer to the context to be initialized.

    DirectoryFileId - Supplies the file ID of the directory this file resides
        in.

    Entry - Supplies a pointer to the directory entry that needs a cluster.

    EntryOffset - Supplies the offset within the directory where this entry
        resides.

Return Value:

    Status code.

--*/

KSTATUS
FatpPerformLongEntryMaintenance (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONGLONG EntryOffset,
    UCHAR Checksum,
    ULONG NewChecksum
    );

/*++

Routine Description:

    This routine operates on long entries preceding the given short directory
    entry (offset), either deleting them or updating their short name checksums.

Arguments:

    Directory - Supplies a pointer to a context for the directory to be
        modified.

    EntryOffset - Supplies the directory offset of the short entry. Long
        entries to modify will be directly behind this entry.

    Checksum - Supplies the checksum of the short directory entry. Long
        entries are expected to contain this checksum.

    NewChecksum - Supplies the new checksum to set in the long entries. If this
        is -1, then the directory entries will be marked erased.

Return Value:

    Status code.

--*/

VOID
FatpInitializeDirectoryContext (
    PFAT_DIRECTORY_CONTEXT DirectoryContext,
    PFAT_FILE DirectoryFile
    );

/*++

Routine Description:

    This routine initializes the given directory context for the provided FAT
    file.

Arguments:

    DirectoryContext - Supplies a pointer to the context to be initialized.

    DirectoryFile - Supplies a pointer to the FAT file data for the directory.

Return Value:

    None.

--*/

KSTATUS
FatpReadDirectory (
    PFAT_DIRECTORY_CONTEXT Directory,
    PFAT_DIRECTORY_ENTRY Entries,
    ULONG EntryCount,
    PULONG EntriesRead
    );

/*++

Routine Description:

    This routine reads the specified number of directory entries from the given
    directory at its current index.

Arguments:

    Directory - Supplies a pointer to the directory context.

    Entries - Supplies an array of directory entries to read.

    EntryCount - Supplies the number of entries to read into the array.

    EntriesRead - Supplies a pointer that receives the total number of entries
        read.

Return Value:

    Status code.

--*/

KSTATUS
FatpWriteDirectory (
    PFAT_DIRECTORY_CONTEXT Directory,
    PFAT_DIRECTORY_ENTRY Entries,
    ULONG EntryCount,
    PULONG EntriesWritten
    );

/*++

Routine Description:

    This routine writes the given directory entries to the directory at the
    given current offset.

Arguments:

    Directory - Supplies a pointer to the directory context.

    Entries - Supplies an array of directory entries to write.

    EntryCount - Supplies the number of entries to write to the directory.

    EntriesWritten - Supplies a pointer that receives the total number of
        entries written.

Return Value:

    Status code.

--*/

KSTATUS
FatpDirectorySeek (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONG EntryOffset
    );

/*++

Routine Description:

    This routine seeks within the directory to the given entry offset.

Arguments:

    Directory - Supplies a pointer to the directory's context.

    EntryOffset - Supplies the desired entry offset from the beginning of the
        directory.

Return Value:

    Status code.

--*/

KSTATUS
FatpDirectoryTell (
    PFAT_DIRECTORY_CONTEXT Directory,
    PULONG Offset
    );

/*++

Routine Description:

    This routine returns the current offset of the given directory context.

Arguments:

    Directory - Supplies a pointer to the directory context to query.

    Offset - Supplies a pointer where the current offset in entries will be
        returned on success.

Return Value:

    Status code.

--*/

KSTATUS
FatpFlushDirectory (
    PFAT_DIRECTORY_CONTEXT Directory
    );

/*++

Routine Description:

    This routine flushes writes accumulated in a directory context.

Arguments:

    Directory - Supplies a pointer to the directory context to flush.

Return Value:

    Status code.

--*/

VOID
FatpDestroyDirectoryContext (
    PFAT_DIRECTORY_CONTEXT DirectoryContext
    );

/*++

Routine Description:

    This routine destroys any allocations stored in the directory context.

Arguments:

    DirectoryContext - Supplies a pointer to a directory context.

Return Value:

    None.

--*/

VOID
FatpConvertSystemTimeToFatTime (
    PSYSTEM_TIME SystemTime,
    PUSHORT Date,
    PUSHORT Time,
    PUCHAR Time10ms
    );

/*++

Routine Description:

    This routine converts a system time value to a FAT date and time.

Arguments:

    SystemTime - Supplies a pointer to the system time to convert.

    Date - Supplies an optional pointer where the FAT date will be stored.

    Time - Supplies an optional pointer where the FAT time will be stored.

    Time10ms - Supplies an optional pointer where the remainder of the time in
        10 millisecond units will be returned.

Return Value:

    None.

--*/

VOID
FatpConvertFatTimeToSystemTime (
    USHORT Date,
    USHORT Time,
    CHAR Time10ms,
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts a FAT time value into a system time value.

Arguments:

    Date - Supplies the FAT date portion.

    Time - Supplies the FAT time portion.

    Time10ms - Supplies the fine-grained time portion.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

VOID
FatpReadEncodedProperties (
    PFAT_DIRECTORY_ENTRY Entry,
    PFAT_ENCODED_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine converts the file name portion of a short form directory
    entry into a FAT encoded file properties structure. This format is
    non-standard and doesn't translate to other operating systems.

Arguments:

    Entry - Supplies a pointer to the directory entry to convert.

    Properties - Supplies a pointer where the encoded properties are returned.

Return Value:

    None.

--*/

VOID
FatpWriteEncodedProperties (
    PFAT_DIRECTORY_ENTRY Entry,
    PFAT_ENCODED_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine converts encoded properties into a FAT short filename.

Arguments:

    Entry - Supplies a pointer where the encoded properties will be returned.
        Only the file name and extension are modified.

    Properties - Supplies a pointer to the properties to set.

Return Value:

    None.

--*/

ULONG
FatpGetRandomNumber (
    VOID
    );

/*++

Routine Description:

    This routine returns a random number.

Arguments:

    None.

Return Value:

    Returns a random 32-bit value.

--*/

//
// Cluster to directory entry mapping support functions.
//

VOID
FatpInitializeFileMappingTree (
    PFAT_VOLUME Volume
    );

/*++

Routine Description:

    This routine initializes the file mapping tree for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

VOID
FatpDestroyFileMappingTree (
    PFAT_VOLUME Volume
    );

/*++

Routine Description:

    This routine drains and frees all entries on the file mapping tree.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

KSTATUS
FatpSetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    ULONG DirectoryCluster,
    ULONGLONG DirectoryOffset
    );

/*++

Routine Description:

    This routine attempts to store the file mapping relationship between a
    file and its directory entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

    DirectoryCluster - Supplies the cluster number of the directory.

    DirectoryOffset - Supplies the offset into the directory where the file
        resides.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

VOID
FatpUnsetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster
    );

/*++

Routine Description:

    This routine unsets the mapping for the given cluster number.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

Return Value:

    None.

--*/

KSTATUS
FatpGetFileMapping (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    PULONG DirectoryCluster,
    PULONGLONG DirectoryOffset
    );

/*++

Routine Description:

    This routine gets the global offset of the directory entry for the file
    starting at this cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the starting cluster number of the file.

    DirectoryCluster - Supplies a pointer where the directory cluster will be
        returned on success.

    DirectoryOffset - Supplies a pointer where the offset within the directory
        where the file entry begins will be returned on success.

Return Value:

    STATUS_SUCCESS if the entry was successfully looked up and returned.

    STATUS_NOT_FOUND if no entry could be found for the mapping.

--*/

//
// File Allocation Table cache support functions.
//

KSTATUS
FatpCreateFatCache (
    PFAT_VOLUME Volume
    );

/*++

Routine Description:

    This routine creates the FAT cache for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    Status code.

--*/

VOID
FatpDestroyFatCache (
    PFAT_VOLUME Volume
    );

/*++

Routine Description:

    This routine destroys the FAT cache for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

BOOL
FatpFatCacheIsClusterEntryPresent (
    PFAT_VOLUME Volume,
    ULONG Cluster
    );

/*++

Routine Description:

    This routine determines whether or not the FAT cache entry for the given
    cluster is present in the cache.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the cluster whose FAT entry is in question.

Return Value:

    Returns TRUE if the cluster's FAT cache entry is present, or FALSE
    otherwise.

--*/

KSTATUS
FatpFatCacheReadClusterEntry (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG Cluster,
    PULONG Value
    );

/*++

Routine Description:

    This routine reads the FAT cache to get the next cluster for the given
    cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    VolumeLockHeld - Supplies a boolean indicating whether or not the volume
        lock is already held.

    Cluster - Supplies the cluster whose FAT entry is being read.

    Value - Supplies a pointer where the value of the cluster entry will be
        return.

Return Value:

    Status code.

--*/

KSTATUS
FatpFatCacheGetFatWindow (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG Cluster,
    PVOID *Window,
    PULONG WindowOffset
    );

/*++

Routine Description:

    This routine returns a portion of the FAT.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    VolumeLockHeld - Supplies a boolean indicating whether or not the volume
        lock is already held.

    Cluster - Supplies the cluster to get the containing window for.

    Window - Supplies a pointer where the window will be returned.

    WindowOffset - Supplies a pointer where the offset into the window where
        the desired cluster resides will be returned.

Return Value:

    Status code.

--*/

KSTATUS
FatpFatCacheWriteClusterEntry (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    ULONG NewValue,
    PULONG OldValue
    );

/*++

Routine Description:

    This routine writes the FAT cache to set the next cluster for the given
    cluster. This routine assumes that the volume lock is held. It will
    optionally return the previous contents of the entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the cluster whose FAT entry is being written.

    NewValue - Supplies the new value to be written to the given cluster's
        FAT entry.

    OldValue - Supplies an optional pointer that receives the old value of the
        given cluster's FAT entry.

Return Value:

    Status code.

--*/

KSTATUS
FatpFatCacheFlush (
    PFAT_VOLUME Volume,
    ULONG IoFlags
    );

/*++

Routine Description:

    This routine flushes the FATs down to the disk. This routine assumes the
    volume lock is already held.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

Return Value:

    Status code.

--*/
