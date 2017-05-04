/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pty.c

Abstract:

    This module implements support for working with pseudo-terminals in the
    C library.

Author:

    Evan Green 2-Feb-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <grp.h>
#include <pty.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the TTY group name.
//

#define TTY_GROUP_NAME "tty"

//
// Define the set of permissions that are set on grantpt.
//

#define TTY_SLAVE_PERMISSIONS (S_IRUSR | S_IWUSR | S_IWGRP)

//
// Define the conventional directory and format for pseudo-terminals.
//

#define PTY_PREFERRED_DIRECTORY "/dev"
#define PTY_PREFERRED_DIRECTORY2 "."
#define PTY_FALLBACK_DIRECTORY "/tmp"
#define PTY_MASTER_PATH_FORMAT "%s/pty%dm"
#define PTY_SLAVE_PATH_FORMAT "%s/pty%d"

//
// Define the maximum size of a conventional pseudo-terminal path.
//

#define PTY_PATH_MAX 50

//
// Define the maximum number of pseudo-terminals to try, conventionally.
//

#define PTY_MAX 1024

//
// Define the initial permission of a pseudo-terminal.
//

#define PTY_INITIAL_PERMISSIONS                                 \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE |   \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_WRITE | \
     FILE_PERMISSION_OTHER_READ)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the static pseudo-terminal slave name.
//

PSTR ClTerminalSlaveName = NULL;

//
// Store the TTY group.
//

gid_t ClTtyGroup = -1;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
openpty (
    int *Master,
    int *Slave,
    char *Name,
    const struct termios *Settings,
    const struct winsize *WindowSize
    )

/*++

Routine Description:

    This routine creates a new pseudo-terminal device.

Arguments:

    Master - Supplies a pointer where a file descriptor to the master will be
        returned on success.

    Slave - Supplies a pointer where a file descriptor to the slave will be
        returned on success.

    Name - Supplies an optional pointer where the name of the slave terminal
        will be returned on success. This buffer must be PATH_MAX size in bytes
        if supplied.

    Settings - Supplies an optional pointer to the settings to apply to the
        new terminal.

    WindowSize - Supplies an optional pointer to the window size to set in the
        new terminal.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int MasterFile;
    CHAR Path[PATH_MAX];
    int Result;
    int SlaveFile;

    SlaveFile = -1;
    MasterFile = getpt();
    if (MasterFile < 0) {
        return -1;
    }

    Result = grantpt(MasterFile);
    if (Result != 0) {
        goto openptyEnd;
    }

    Result = unlockpt(MasterFile);
    if (Result != 0) {
        goto openptyEnd;
    }

    Result = ptsname_r(MasterFile, Path, sizeof(Path));
    if (Result != 0) {
        goto openptyEnd;
    }

    SlaveFile = open(Path, O_RDWR | O_NOCTTY);
    if (SlaveFile < 0) {
        goto openptyEnd;
    }

    if (Settings != NULL) {
        tcsetattr(SlaveFile, TCSAFLUSH, Settings);
    }

    if (WindowSize != NULL) {
        ioctl(SlaveFile, TIOCSWINSZ, WindowSize);
    }

    if (Name != NULL) {
        strcpy(Name, Path);
    }

    Result = 0;

openptyEnd:
    if (Result != 0) {
        if (MasterFile >= 0) {
            close(MasterFile);
            MasterFile = -1;
        }

        if (SlaveFile >= 0) {
            close(SlaveFile);
            SlaveFile = -1;
        }
    }

    *Master = MasterFile;
    *Slave = SlaveFile;
    return Result;
}

LIBC_API
int
login_tty (
    int TerminalDescriptor
    )

/*++

Routine Description:

    This routine prepares for a login on the given terminal. It creates a new
    session, makes the given terminal descriptor the controlling terminal for
    the session, sets the terminal as standard input, output, and error, and
    closes the given descriptor.

Arguments:

    TerminalDescriptor - Supplies the file descriptor of the terminal to start
        a login on.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    if (setsid() < 0) {
        return -1;
    }

    if (ioctl(TerminalDescriptor, TIOCSCTTY, 0) < 0) {
        return -1;
    }

    dup2(TerminalDescriptor, STDIN_FILENO);
    dup2(TerminalDescriptor, STDOUT_FILENO);
    dup2(TerminalDescriptor, STDERR_FILENO);
    if (TerminalDescriptor > STDERR_FILENO) {
        close(TerminalDescriptor);
    }

    return 0;
}

LIBC_API
pid_t
forkpty (
    int *Master,
    char *Name,
    const struct termios *Settings,
    const struct winsize *WindowSize
    )

/*++

Routine Description:

    This routine combines openpty, fork, and login_tty to create a new process
    wired up to a pseudo-terminal.

Arguments:

    Master - Supplies a pointer where a file descriptor to the master will be
        returned on success. This is only returned in the parent.

    Name - Supplies an optional pointer where the name of the slave terminal
        will be returned on success. This buffer must be PATH_MAX size in bytes
        if supplied.

    Settings - Supplies an optional pointer to the settings to apply to the
        new terminal.

    WindowSize - Supplies an optional pointer to the window size to set in the
        new terminal.

Return Value:

    Returns the pid of the forked child on success in the parent.

    0 on success in the child.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    pid_t Child;
    int MasterDescriptor;
    int Slave;

    if (openpty(&MasterDescriptor, &Slave, Name, Settings, WindowSize) == -1) {
        return -1;
    }

    Child = fork();
    if (Child < 0) {
        return -1;
    }

    //
    // If this is the child, make the new slave portion the controlling
    // terminal.
    //

    if (Child == 0) {
        close(MasterDescriptor);
        MasterDescriptor = -1;

        //
        // If login_tty fails to set the controlling terminal, then do the
        // rest of it as if it succeeded.
        //

        if (login_tty(Slave) < 0) {
            syslog(LOG_ERR, "forkpty: login_tty failed.\n");
            dup2(Slave, STDIN_FILENO);
            dup2(Slave, STDOUT_FILENO);
            dup2(Slave, STDERR_FILENO);
            if (Slave > STDERR_FILENO) {
                close(Slave);
            }
        }

    //
    // In the parent, close the slave.
    //

    } else {
        *Master = MasterDescriptor;
        close(Slave);
    }

    return Child;
}

LIBC_API
int
getpt (
    void
    )

/*++

Routine Description:

    This routine creates and opens a new pseudo-terminal master.

Arguments:

    None.

Return Value:

    Returns a file descriptor to the new terminal master device on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return posix_openpt(O_RDWR | O_NOCTTY);
}

LIBC_API
int
posix_openpt (
    int Flags
    )

/*++

Routine Description:

    This routine creates and opens a new pseudo-terminal master.

Arguments:

    Flags - Supplies a bitfield of open flags governing the open. Only O_RDWR
        and O_NOCTTY are observed.

Return Value:

    Returns a file descriptor to the new terminal master device on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PSTR Directory;
    int Error;
    HANDLE Handle;
    UINTN Index;
    CHAR MasterPath[PTY_PATH_MAX];
    UINTN MasterPathSize;
    ULONG OpenFlags;
    CHAR SlavePath[PTY_PATH_MAX];
    UINTN SlavePathSize;
    KSTATUS Status;

    Handle = INVALID_HANDLE;
    OpenFlags = 0;
    switch (Flags & O_ACCMODE) {
    case O_RDONLY:
        OpenFlags |= SYS_OPEN_FLAG_READ;
        break;

    case O_WRONLY:
        OpenFlags |= SYS_OPEN_FLAG_WRITE;
        break;

    case O_RDWR:
        OpenFlags |= SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE;
        break;

    default:
        break;
    }

    if ((Flags & O_NOCTTY) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL;
    }

    //
    // Figure out where to create the terminal at. Prefer /dev, then the
    // current working directory, then /tmp.
    //

    Directory = PTY_PREFERRED_DIRECTORY;
    if (access(Directory, W_OK) != 0) {
        Directory = PTY_PREFERRED_DIRECTORY2;
        if (access(Directory, W_OK) != 0) {
            Directory = PTY_FALLBACK_DIRECTORY;
            if (access(Directory, W_OK) != 0) {
                errno = EACCES;
                return -1;
            }
        }
    }

    //
    // Loop trying to create a terminal.
    //

    Error = EAGAIN;
    for (Index = 0; Index < PTY_MAX; Index += 1) {
        MasterPathSize = RtlPrintToString(MasterPath,
                                          sizeof(MasterPath),
                                          CharacterEncodingDefault,
                                          PTY_MASTER_PATH_FORMAT,
                                          Directory,
                                          Index);

        SlavePathSize = RtlPrintToString(SlavePath,
                                         sizeof(SlavePath),
                                         CharacterEncodingDefault,
                                         PTY_SLAVE_PATH_FORMAT,
                                         Directory,
                                         Index);

        Status = OsCreateTerminal(INVALID_HANDLE,
                                  INVALID_HANDLE,
                                  MasterPath,
                                  MasterPathSize + 1,
                                  SlavePath,
                                  SlavePathSize + 1,
                                  OpenFlags,
                                  PTY_INITIAL_PERMISSIONS,
                                  PTY_INITIAL_PERMISSIONS,
                                  &Handle);

        if (KSUCCESS(Status)) {
            Error = 0;
            break;

        } else if ((Status != STATUS_FILE_EXISTS) &&
                   (Status != STATUS_ACCESS_DENIED)) {

            Error = ClConvertKstatusToErrorNumber(Status);
            break;
        }
    }

    if (Error != 0) {
        errno = Error;
        return -1;
    }

    return (int)(UINTN)Handle;
}

LIBC_API
int
grantpt (
    int Descriptor
    )

/*++

Routine Description:

    This routine changes the ownership and access permission of the slave
    pseudo-terminal associated with the given master pseudo-terminal file
    descriptor so that folks can open it.

Arguments:

    Descriptor - Supplies the file descriptor of the master pseudo-terminal.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct group Group;
    char *GroupBuffer;
    size_t GroupBufferSize;
    struct group *GroupPointer;
    int OriginalError;
    uid_t RealUserId;
    int Result;
    CHAR SlaveName[PATH_MAX];
    struct stat Stat;
    gid_t TtyGroup;

    Result = ptsname_r(Descriptor, SlaveName, sizeof(SlaveName));
    if (Result == 0) {
        Result = stat(SlaveName, &Stat);
    }

    if (Result != 0) {
        OriginalError = errno;

        //
        // If the file descriptor is not a terminal, return EINVAL.
        //

        if ((fcntl(Descriptor, F_GETFD) != 0) && (errno == EBADF)) {
            return -1;
        }

        if (OriginalError == ENOTTY) {
            errno = EINVAL;

        } else {
            errno = OriginalError;
        }

        return -1;
    }

    //
    // Own the device.
    //

    RealUserId = getuid();
    if (Stat.st_uid != RealUserId) {
        Result = chown(SlaveName, RealUserId, Stat.st_gid);
        if (Result != 0) {
            return -1;
        }
    }

    //
    // Go look up the TTY group if not found already. If it could not be found,
    // set it to the current real group ID.
    //

    if (ClTtyGroup == (gid_t)-1) {
        GroupBufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
        GroupBuffer = malloc(GroupBufferSize);
        if (GroupBuffer != NULL) {
            GroupPointer = NULL;
            getgrnam_r(TTY_GROUP_NAME,
                       &Group,
                       GroupBuffer,
                       GroupBufferSize,
                       &GroupPointer);

            if (GroupPointer != NULL) {
                ClTtyGroup = GroupPointer->gr_gid;
            }

            free(GroupBuffer);
        }
    }

    TtyGroup = ClTtyGroup;
    if (TtyGroup == (gid_t)-1) {
        TtyGroup = getgid();
    }

    //
    // Change the terminal to belong to the group.
    //

    if (Stat.st_gid != TtyGroup) {
        Result = chown(SlaveName, RealUserId, TtyGroup);
        if (Result != 0) {
            return -1;
        }
    }

    //
    // Ensure the permissions are writable by the user and group.
    //

    if ((Stat.st_mode & ACCESSPERMS) != TTY_SLAVE_PERMISSIONS) {
        Result = chmod(SlaveName, TTY_SLAVE_PERMISSIONS);
        if (Result != 0) {
            return -1;
        }
    }

    return 0;
}

LIBC_API
int
unlockpt (
    int Descriptor
    )

/*++

Routine Description:

    This routine unlocks the slave side of the pseudo-terminal associated with
    the given master side file descriptor.

Arguments:

    Descriptor - Supplies the open file descriptor to the master side of the
        terminal.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    if (!isatty(Descriptor)) {
        errno = ENOTTY;
        return -1;
    }

    return 0;
}

LIBC_API
char *
ptsname (
    int Descriptor
    )

/*++

Routine Description:

    This routine returns the name of the slave pseudoterminal associated
    with the given master file descriptor. This function is neither thread-safe
    nor reentrant.

Arguments:

    Descriptor - Supplies the open file descriptor to the master side of the
        terminal.

Return Value:

    Returns a pointer to a static area containing the name of the terminal on
    success. The caller must not modify or free this buffer, and it may be
    overwritten by subsequent calls to ptsname.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    CHAR BigBuffer[PATH_MAX];
    int Result;

    Result = ptsname_r(Descriptor, BigBuffer, PATH_MAX);
    if (Result != 0) {
        return NULL;
    }

    if (ClTerminalSlaveName != NULL) {
        free(ClTerminalSlaveName);
    }

    ClTerminalSlaveName = strdup(BigBuffer);
    return ClTerminalSlaveName;
}

LIBC_API
int
ptsname_r (
    int Descriptor,
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine returns the name of the slave pseudoterminal associated
    with the given master file descriptor. This is the reentrant version of the
    ptsname function.

Arguments:

    Descriptor - Supplies the open file descriptor to the master side of the
        terminal.

    Buffer - Supplies a pointer where the name will be returned on success.

    BufferSize - Supplies the size of the given buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UINTN Size;
    KSTATUS Status;

    if (!isatty(Descriptor)) {
        errno = ENOTTY;
        return -1;
    }

    Size = BufferSize;
    Status = OsGetFilePath((HANDLE)(UINTN)Descriptor, Buffer, &Size);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // The path had better be a least a character, 'm', and a null terminator.
    //

    if (Size < 3) {
        errno = EINVAL;
        return -1;
    }

    //
    // The only difference (by C library convention) between master and slave
    // terminals is a letter m on the end of the master. Chop that off to get
    // the slave path.
    //

    if (Buffer[Size - 2] == 'm') {
        Buffer[Size - 2] = '\0';
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

