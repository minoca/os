/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fat.h

Abstract:

    This header contains definitions for the File Allocation Table (FAT) File
    System library.

Author:

    Evan Green 23-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define FAT_ALLOCATION_TAG 0x21746146 // '!taF'

//
// Define the offset between the system time offset and FAT's epoch date of
// 1980.
//

#define FAT_EPOCH_SYSTEM_TIME_OFFSET (-662860800LL)

//
// Define the FAT mount flags.
//

#define FAT_MOUNT_FLAG_COMPATIBILITY_MODE 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef VOID FAT_IO_BUFFER, *PFAT_IO_BUFFER;

/*++

Structure Description:

    This structure defines the information returned from the FAT seek command.

Members:

    FileByteOffset - Stores the offset, in bytes, of the current file pointer,
        from the beginning of the file.

    CurrentBlock - Stores the block associated with the file pointer.

    CurrentCluster - Stores the cluster number associated with the current
        file pointer.

    ClusterByteOffset - Stores byte offset into the current cluster.

--*/

typedef struct _FAT_SEEK_INFORMATION {
    ULONGLONG FileByteOffset;
    ULONGLONG CurrentBlock;
    ULONG CurrentCluster;
    ULONG ClusterByteOffset;
} FAT_SEEK_INFORMATION, *PFAT_SEEK_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
FatFormat (
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters,
    ULONG ClusterSize,
    ULONG Alignment
    );

/*++

Routine Description:

    This routine formats a block device, making an initial FAT file system.
    This function will render the previous contents of the disk unreadable.

Arguments:

    BlockDeviceParameters - Supplies a pointer to a structure describing the
        underlying device.

    ClusterSize - Supplies the size of each cluster. Supply 0 to use a
        default cluster size chosen based on the disk size.

    Alignment - Supplies the byte alignment for volume. This is used to byte
        align the clusters and the FATs. If knowledge of the target system's
        cache behavior is known, aligning the volume to a cache size can help
        improve performance. Supply 0 to use the default alignment of 4096
        bytes.

Return Value:

    Status code.

--*/

KSTATUS
FatMount (
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters,
    ULONG Flags,
    PVOID *VolumeToken
    );

/*++

Routine Description:

    This routine attempts to load FAT as the file system for the given storage
    device.

Arguments:

    BlockDeviceParameters - Supplies a pointer to a structure describing the
        underlying device.

    Flags - Supplies a bitmask of FAT mount flags. See FAT_MOUNT_FLAG_* for
        definitions.

    VolumeToken - Supplies a pointer where the FAT file system will return
        an opaque token identifying the volume on success. This token will be
        passed to future calls to the file system.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    internal file system state.

    STATUS_UNRECOGNIZED_FILE_SYSTEM if the file system on the device is not FAT.

    Other error codes.

--*/

KSTATUS
FatUnmount (
    PVOID Volume
    );

/*++

Routine Description:

    This routine attempts to unmount a FAT volume.

Arguments:

    Volume - Supplies the token identifying the volume.

Return Value:

    Status code.

--*/

KSTATUS
FatOpenFileId (
    PVOID Volume,
    FILE_ID FileId,
    ULONG DesiredAccess,
    ULONG Flags,
    PVOID *FileToken
    );

/*++

Routine Description:

    This routine attempts to open an existing file.

Arguments:

    Volume - Supplies the token identifying the volume.

    FileId - Supplies the file ID to open.

    DesiredAccess - Supplies the desired access flags. See IO_ACCESS_*
        definitions.

    Flags - Supplies additional flags about how the file should be opened.
        See OPEN_FLAG_* definitions.

    FileToken - Supplies a pointer where an opaque token will be returned that
        uniquely identifies the opened file instance.

Return Value:

    Status code.

--*/

VOID
FatCloseFile (
    PVOID FileToken
    );

/*++

Routine Description:

    This routine closes a FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

Return Value:

    None.

--*/

KSTATUS
FatReadFile (
    PVOID FileToken,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN BytesToRead,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesRead
    );

/*++

Routine Description:

    This routine reads the specified number of bytes from an open FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    FatSeekInformation - Supplies a pointer to current seek data for the given
        file, indicating where to begin the read.

    IoBuffer - Supplies a pointer to a FAT I/O buffer where the bytes read from
        the file will be returned.

    BytesToRead - Supplies the number of bytes to read from the file.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for transfers.

    BytesRead - Supplies the number of bytes that were actually read from the
        file.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

KSTATUS
FatWriteFile (
    PVOID FileToken,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN BytesToWrite,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesWritten
    );

/*++

Routine Description:

    This routine writes the specified number of bytes from an open FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    FatSeekInformation - Supplies a pointer to current seek data for the given
        file, indicating where to begin the write.

    IoBuffer - Supplies a pointer to a FAT I/O buffer containing the data to
        write to the file.

    BytesToWrite - Supplies the number of bytes to write to the file.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for disk transfers.

    BytesWritten - Supplies the number of bytes that were actually written to
        the file.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

KSTATUS
FatLookup (
    PVOID Volume,
    BOOL Root,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameSize,
    PFILE_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine attempts to lookup an entry for a file or directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    Root - Supplies a boolean indicating if the system would like to look up
        the root entry for this device. If so, the directory file ID, file name,
        and file name size should be ignored.

    DirectoryFileId - Supplies the file ID of the directory to search in.

    FileName - Supplies a pointer to the name of the file, which may not be
        null terminated.

    FileNameSize - Supplies the size of the file name buffer including space
        for a null terminator (which may be a null terminator or may be a
        garbage character).

    Properties - Supplies the a pointer where the file properties will be
        returned if the file was found.

Return Value:

    Status code.

--*/

KSTATUS
FatCreate (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    PCSTR Name,
    ULONG NameSize,
    PULONGLONG DirectorySize,
    PFILE_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine attempts to create a file or directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    Properties - Supplies the file properties of the created file on success.
        The permissions, object type, user ID, group ID, and access times are
        all valid from the system.

Return Value:

    Status code.

--*/

KSTATUS
FatEnumerateDirectory (
    PVOID FileToken,
    ULONGLONG EntryOffset,
    PFAT_IO_BUFFER Buffer,
    UINTN BytesToRead,
    BOOL ReadSingleEntry,
    BOOL IncludeDotDirectories,
    PVOID Irp,
    PUINTN BytesRead,
    PULONG ElementsRead
    );

/*++

Routine Description:

    This routine lists the contents of a directory.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    EntryOffset - Supplies an offset into the directory, in terms of entries,
        where the enumerate should begin.

    IoBuffer - Supplies a pointer to a FAT I/O buffer where the bytes read from
        the file will be returned.

    BytesToRead - Supplies the number of bytes to read from the file.

    ReadSingleEntry - Supplies a boolean indicating if only one entry should
        be read (TRUE) or if as many entries as fit in the buffer should be
        read (FALSE).

    IncludeDotDirectories - Supplies a boolean indicating if the dot and dot
        dot directories should be returned as well.

    Irp - Supplies an optional pointer to an IRP to use for transfers.

    BytesRead - Supplies a pointer that on input contains the number of bytes
        already read from the buffer. On output, accumulates any additional
        bytes read during this function.

    ElementsRead - Supplies the number of directory entries that were
        returned from this read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

KSTATUS
FatGetFileDirectory (
    PVOID Volume,
    FILE_ID FileId,
    PFILE_ID DirectoryId
    );

/*++

Routine Description:

    This routine attempts to look up the file ID of the directory that the
    given file is in. The file must have been previously looked up.

Arguments:

    Volume - Supplies the token identifying the volume.

    FileId - Supplies the file ID whose directory is desired.

    DirectoryId - Supplies a pointer where the file ID of the directory
        containing the given file will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no file mapping exists.

--*/

VOID
FatGetDeviceInformation (
    PVOID Volume,
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters
    );

/*++

Routine Description:

    This routine returns a copy of the volume's block device information.

Arguments:

    Volume - Supplies the token identifying the volume.

    BlockDeviceParameters - Supplies a pointer that receives the information
        describing the device backing the volume.

Return Value:

    None.

--*/

KSTATUS
FatUnlink (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameSize,
    FILE_ID FileId,
    PBOOL Unlinked
    );

/*++

Routine Description:

    This routine deletes a file entry from a directory. It does not free the
    clusters associated with the file.

Arguments:

    Volume - Supplies the token identifying the volume.

    DirectoryFileId - Supplies the ID of the directory containing the file.

    FileName - Supplies the name of the file to unlink.

    FileNameSize - Supplies the length of the file name buffer in bytes,
        including the null terminator.

    FileId - Supplies the file ID of the file to unlink.

    Unlinked - Supplies a pointer that receives a boolean indicating whether or
        not the file entry was unlinked. This can get set to TRUE even if the
        routine returns a failure status.

Return Value:

    Status code.

--*/

KSTATUS
FatRename (
    PVOID Volume,
    FILE_ID SourceDirectoryId,
    FILE_ID SourceFileId,
    PBOOL SourceErased,
    FILE_ID DestinationDirectoryId,
    PBOOL DestinationCreated,
    PULONGLONG DestinationDirectorySize,
    PSTR FileName,
    ULONG FileNameSize
    );

/*++

Routine Description:

    This routine attempts to rename a file. The destination file must not
    already exist, otherwise multiple files will share the same name in a
    directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    SourceDirectoryId - Supplies the file ID of the directory containing the
        source file.

    SourceFileId - Supplies the file ID of the file to rename.

    SourceErased - Supplies a pointer that receives a boolean indicating
        whether or not the source file was erased from its parent directory.

    DestinationDirectoryId - Supplies the file ID of the directory where the
        newly renamed file will reside.

    DestinationCreated - Supplies a pointer that receives a boolean indicating
        whether or not a directory entry was created for the destination file.

    DestinationDirectorySize - Supplies a pointer that receives the updated
        size of the directory.

    FileName - Supplies the name of the newly renamed file.

    FileNameSize - Supplies the length of the file name buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

KSTATUS
FatTruncate (
    PVOID Volume,
    PVOID FileToken,
    FILE_ID FileId,
    ULONGLONG OldSize,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine truncates a file to the given file size. This can be used to
    both shrink and grow the file.

Arguments:

    Volume - Supplies a pointer to the volume.

    FileToken - Supplies the file context of the file to operate on.

    FileId - Supplies the file ID of the file to operate on.

    OldSize - Supplies the original size of the file.

    NewSize - Supplies the new size to make the file. If smaller, then
        unused clusters will be freed. If larger, then the file will be
        zeroed to make room.

Return Value:

    Status code.

--*/

KSTATUS
FatFileSeek (
    PVOID FileToken,
    PVOID Irp,
    ULONG IoFlags,
    SEEK_COMMAND SeekCommand,
    ULONGLONG Offset,
    PFAT_SEEK_INFORMATION FatSeekInformation
    );

/*++

Routine Description:

    This routine reads moves the file pointer for the given file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    Irp - Supplies an optional pointer to an IRP to use for disk operations.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    SeekCommand - Supplies the type of seek to perform.

    Offset - Supplies the offset, in bytes, to seek to, from the reference
        location specified by the seek command.

    FatSeekInformation - Supplies a pointer that receives the information
        collected by the seek operation, including the new cluster and byte
        offsets.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an unrecognized seek option is passed.

    STATUS_OUT_OF_BOUNDS if an offset was supplied that exceeds the file size.

    STATUS_FILE_CORRUPT if the file system has a problem.

    Other errors on device I/O error.

--*/

KSTATUS
FatWriteFileProperties (
    PVOID Volume,
    PFILE_PROPERTIES NewProperties,
    ULONG IoFlags
    );

/*++

Routine Description:

    This routine updates the metadata (located in the directory entry) for the
    given file.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    NewProperties - Supplies a pointer to the new file metadata.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

KSTATUS
FatDeleteFileBlocks (
    PVOID Volume,
    PVOID FileToken,
    FILE_ID FileId,
    ULONGLONG FileSize,
    BOOL Truncate
    );

/*++

Routine Description:

    This routine deletes the data contents of a file beyond the specified file
    size, freeing its corresponding clusters. It does not touch the file's
    directory entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileToken - Supplies an optional pointer to an open file token for the
        file. If this is set, then the seek table for the file will be
        maintained. If no file is supplied or there are other open files, the
        seek table for those files will be wrong and may cause volume
        corruption.

    FileId - Supplies the ID of the file whose contents should be deleted or
        truncated.

    FileSize - Supplies the file size beyond which all data is to be discarded.

    Truncate - Supplies a boolean that if set to TRUE will leave the first
        cluster (the file ID) allocated. This allows a file to be truncated to
        zero size but still maintain its file ID. If FALSE, all file blocks,
        including the first one, will be deleted.

Return Value:

    Status code.

--*/

KSTATUS
FatGetFileBlockInformation (
    PVOID Volume,
    FILE_ID FileId,
    PFILE_BLOCK_INFORMATION *BlockInformation
    );

/*++

Routine Description:

    This routine gets the block information for the given file.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileId - Supplies the ID of the file whose block information is being
        requested.

    BlockInformation - Supplies a pointer that receives a pointer to the block
        information for the file.

Return Value:

    Status code.

--*/

KSTATUS
FatAllocateFileClusters (
    PVOID Volume,
    FILE_ID FileId,
    ULONGLONG FileSize
    );

/*++

Routine Description:

    This routine expands the file capacity of the given file ID by allocating
    clusters for it. It does not zero out those clusters, so the usefulness of
    this function is limited to scenarios where the security of uninitialized
    disk contents is not a concern.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileId - Supplies the ID of the file whose size should be expanded.

    FileSize - Supplies the file size to allocate for the file.

Return Value:

    Status code.

--*/

//
// Prototypes of routines that support the FAT library.
//

PFAT_IO_BUFFER
FatAllocateIoBuffer (
    PVOID DeviceToken,
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates memory for device I/O use.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Size - Supplies the size of the required allocation, in bytes.

Return Value:

    Returns a pointer to the FAT I/O buffer, or NULL on failure.

--*/

PFAT_IO_BUFFER
FatCreateIoBuffer (
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine creates a FAT I/O buffer from the given buffer.

Arguments:

    Buffer - Supplies a pointer to the memory buffer on which to base the FAT
        I/O buffer.

    Size - Supplies the size of the memory buffer, in bytes.

Return Value:

    Returns an pointer to the FAT I/O buffer, or NULL on failure.

--*/

VOID
FatIoBufferUpdateOffset (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN OffsetUpdate,
    BOOL Decrement
    );

/*++

Routine Description:

    This routine increments the given FAT I/O buffer's current offset by the
    given amount.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

    OffsetUpdate - Supplies the number of bytes by which the offset will be
        updated.

    Decrement - Supplies a boolean indicating whether the update will be a
        decrement (TRUE) or an increment (FALSE).

Return Value:

    None.

--*/

VOID
FatIoBufferSetOffset (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN Offset
    );

/*++

Routine Description:

    This routine sets the given FAT I/O buffer's current offset.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

    Offset - Supplies the new offset to set.

Return Value:

    None.

--*/

KSTATUS
FatZeroIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN Offset,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine zeros the contents of the FAT I/O buffer starting at the
    offset for the given number of bytes.

Arguments:

    FatIoBuffer - Supplies a pointer to the FAT I/O buffer that is to be zeroed.

    Offset - Supplies the offset within the I/O buffer where the zeroing should
        begin.

    ByteCount - Supplies the number of bytes to zero.

Return Value:

    Status code.

--*/

KSTATUS
FatCopyIoBuffer (
    PFAT_IO_BUFFER Destination,
    UINTN DestinationOffset,
    PFAT_IO_BUFFER Source,
    UINTN SourceOffset,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine copies the contents of the source I/O buffer starting at the
    source offset to the destination I/O buffer starting at the destination
    offset. It assumes that the arguments are correct such that the copy can
    succeed.

Arguments:

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contents will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    ByteCount - Supplies the size of the requested copy in bytes.

Return Value:

    Status code.

--*/

KSTATUS
FatCopyIoBufferData (
    PFAT_IO_BUFFER FatIoBuffer,
    PVOID Buffer,
    UINTN Offset,
    UINTN Size,
    BOOL ToIoBuffer
    );

/*++

Routine Description:

    This routine copies from a buffer into the given I/O buffer or out of the
    given I/O buffer.

Arguments:

    FatIoBuffer - Supplies a pointer to the FAT I/O buffer to copy in or out of.

    Buffer - Supplies a pointer to the regular linear buffer to copy to or from.

    Offset - Supplies an offset in bytes from the beginning of the I/O buffer
        to copy to or from.

    Size - Supplies the number of bytes to copy.

    ToIoBuffer - Supplies a boolean indicating whether data is copied into the
        I/O buffer (TRUE) or out of the I/O buffer (FALSE).

Return Value:

    Status code.

--*/

PVOID
FatMapIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer
    );

/*++

Routine Description:

    This routine maps the given FAT I/O buffer and returns the base of the
    virtually contiguous mapping.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

VOID
FatFreeIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer
    );

/*++

Routine Description:

    This routine frees a FAT I/O buffer.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

Return Value:

    None.

--*/

PVOID
FatAllocatePagedMemory (
    PVOID DeviceToken,
    ULONG SizeInBytes
    );

/*++

Routine Description:

    This routine allocates paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    SizeInBytes - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the allocated memory, or NULL on failure.

--*/

PVOID
FatAllocateNonPagedMemory (
    PVOID DeviceToken,
    ULONG SizeInBytes
    );

/*++

Routine Description:

    This routine allocates non-paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    SizeInBytes - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the allocated memory, or NULL on failure.

--*/

VOID
FatFreePagedMemory (
    PVOID DeviceToken,
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

VOID
FatFreeNonPagedMemory (
    PVOID DeviceToken,
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

KSTATUS
FatCreateLock (
    PVOID *Lock
    );

/*++

Routine Description:

    This routine creates a lock.

Arguments:

    Lock - Supplies a pointer where an opaque pointer will be returned
        representing the lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the lock could not be allocated.

--*/

VOID
FatDestroyLock (
    PVOID Lock
    );

/*++

Routine Description:

    This routine destroys a created lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

VOID
FatAcquireLock (
    PVOID Lock
    );

/*++

Routine Description:

    This routine acquires a lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

VOID
FatReleaseLock (
    PVOID Lock
    );

/*++

Routine Description:

    This routine releases a lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

KSTATUS
FatOpenDevice (
    PBLOCK_DEVICE_PARAMETERS BlockParameters
    );

/*++

Routine Description:

    This routine opens the underlying device that the FAT file system reads
    and writes blocks to.

Arguments:

    BlockParameters - Supplies the initial block device parameters for the
        device. These parameters may be modified by this call.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if there were not enough resources to open
    the device.

--*/

VOID
FatCloseDevice (
    PVOID DeviceToken
    );

/*++

Routine Description:

    This routine closes the device backing the FAT file system.

Arguments:

    DeviceToken - Supplies a pointer to the device token returned upon opening
        the underlying device.

Return Value:

    None.

--*/

KSTATUS
FatReadDevice (
    PVOID DeviceToken,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    ULONG Flags,
    PVOID Irp,
    PFAT_IO_BUFFER FatIoBuffer
    );

/*++

Routine Description:

    This routine reads data from the underlying disk.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    BlockAddress - Supplies the block index to read (for physical disks, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to the IRP to pass to the read file
        function.

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer where the data from
        the disk will be returned.

Return Value:

    Status code.

--*/

KSTATUS
FatWriteDevice (
    PVOID DeviceToken,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    ULONG Flags,
    PVOID Irp,
    PFAT_IO_BUFFER FatIoBuffer
    );

/*++

Routine Description:

    This routine writes data to the underlying disk.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    BlockAddress - Supplies the block index to write to (for physical disks,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for the disk operation.

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer containing the data to
        write.

Return Value:

    Status code.

--*/

KSTATUS
FatGetDeviceBlockInformation (
    PVOID DeviceToken,
    PFILE_BLOCK_INFORMATION BlockInformation
    );

/*++

Routine Description:

    This routine converts a file's block information into disk level block
    information by modifying the offsets of each contiguous run.

Arguments:

    DeviceToken - Supplies an opaque token identify the underlying device.

    BlockInformation - Supplies a pointer to the block information to be
        updated.

Return Value:

    Status code.

--*/

ULONG
FatGetIoCacheEntryDataSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of data stored in each cache entry.

Arguments:

    None.

Return Value:

    Returns the size of the data stored in each cache entry, or 0 if there is
    no cache.

--*/

ULONG
FatGetPageSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of a physical memory page for the current FAT
    environment.

Arguments:

    None.

Return Value:

    Returns the size of a page in the current environment. Returns 0 if the
    size is not known.

--*/

VOID
FatGetCurrentSystemTime (
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    SystemTime - Supplies a pointer where the current system time will be
        returned.

Return Value:

    None.

--*/

