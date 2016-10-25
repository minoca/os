/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatfs.h

Abstract:

    This header contains internal definitions for the FAT file system driver
    in the UEFI core.

Author:

    Evan Green 21-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the FAT volume data given a pointer to the
// Simple File System protocol instance.
//

#define EFI_FAT_VOLUME_FROM_THIS(_SimpleFileSystem) \
    PARENT_STRUCTURE(_SimpleFileSystem, EFI_FAT_VOLUME, SimpleFileSystem)

//
// This macro returns a pointer to the FAT file data given a pointer to the
// File protocol instance.
//

#define EFI_FAT_FILE_FROM_THIS(_File) \
    PARENT_STRUCTURE(_File, EFI_FAT_FILE, FileProtocol)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FAT_VOLUME_MAGIC 0x56746146 // 'VtaF'
#define EFI_FAT_FILE_MAGIC 0x46746146 // 'FtaF'

#define EFI_FAT_DIRECTORY_ENTRY_SIZE 300

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores internal data regarding a FAT volume.

Members:

    Magic - Stores the constant value EFI_FAT_VOLUME_MAGIC.

    FatVolume - Stores a pointer to the FAT library volume handle.

    Handle - Stores the handle the simple file system protocol is installed on.

    DiskIo - Stores a pointer to the underlying disk I/O protocol used.

    BlockIo - Stores a pointer to the underlying block I/O protocol.

    BlockSize - Stores the block size of the underlying block I/O device.

    MediaId - Stores the identifier of the media when this file system was
        mounted.

    RootDirectoryId - Stores the ID of the root directory. This is almost
        always 2 for FAT file systems.

    ReadOnly - Stores a boolean indicating if the volume is mounted read only.

    SimpleFileSystem - Stores the simple file system protocol data.

    OpenFiles - Stores the count of open files on this volume.

--*/

typedef struct _EFI_FAT_VOLUME {
    UINT32 Magic;
    VOID *FatVolume;
    EFI_HANDLE Handle;
    EFI_DISK_IO_PROTOCOL *DiskIo;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    UINT32 BlockSize;
    UINT32 MediaId;
    UINT64 RootDirectoryId;
    BOOLEAN ReadOnly;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystem;
    UINTN OpenFiles;
} EFI_FAT_VOLUME, *PEFI_FAT_VOLUME;

/*++

Structure Description:

    This structure stores internal data regarding a FAT volume.

Members:

    Magic - Stores the constant value EFI_FAT_VOLUME_MAGIC.

    MediaId - Stores the media ID of the volume when the file was opened.

    FileProtocol - Stores the file protocol for this file.

    Volume - Stores a pointer back to the volume.

    IsRoot - Stores a boolean indicating if this is the root directory.

    IsOpenForRead - Stores a boolean indicating whether the file is open for
        read access.

    IsDirty - Stores a boolean indicating if the file properties need to be
        written out to disk.

    DirectoryFileId - Stores the file ID of the directory this file resides in.

    FileName - Stores a pointer to the name of the file.

    Properties - Stores the file properties.

    FatFile - Stores a pointer to the FAT library file information.

    SeekInformation - Stores the file seek information.

    CurrentOffset - Stores the current file offset.

--*/

typedef struct _EFI_FAT_FILE {
    UINT32 Magic;
    UINT32 MediaId;
    EFI_FILE_PROTOCOL FileProtocol;
    PEFI_FAT_VOLUME Volume;
    BOOLEAN IsRoot;
    BOOLEAN IsOpenForRead;
    BOOLEAN IsDirty;
    UINT64 DirectoryFileId;
    CHAR8 *FileName;
    FILE_PROPERTIES Properties;
    VOID *FatFile;
    FAT_SEEK_INFORMATION SeekInformation;
    UINT64 CurrentOffset;
} EFI_FAT_FILE, *PEFI_FAT_FILE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
