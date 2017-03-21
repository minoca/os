/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sock.c

Abstract:

    This module contains socket support for Unix-like platforms.

Author:

    Evan Green 27-Aug-2014

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sock.h"

//
// ---------------------------------------------------------------- Definitions
//

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
// ------------------------------------------------------------------ Functions
//

int
DbgrSocketInitializeLibrary (
    void
    )

/*++

Routine Description:

    This routine initializes socket support in the application.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return 0;
}

void
DbgrSocketDestroyLibrary (
    void
    )

/*++

Routine Description:

    This routine tears down socket support in the application.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

int
DbgrSocketCreateStreamSocket (
    void
    )

/*++

Routine Description:

    This routine creates an IPv4 TCP socket.

Arguments:

    None.

Return Value:

    Returns the socket on success.

    Returns a value less than zero on failure.

--*/

{

    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

int
DbgrSocketBind (
    int Socket,
    char *Host,
    int Port
    )

/*++

Routine Description:

    This routine binds the given socket to the given address and port.

Arguments:

    Socket - Supplies the socket to bind.

    Host - Supplies a pointer to the host string. Supply NULL to use any
        address.

    Port - Supplies the port to bind on.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    struct sockaddr_in Address;
    int Result;

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = INADDR_ANY;
    if ((Host != NULL) && (*Host != '\0')) {
        Result = inet_pton(AF_INET, Host, &(Address.sin_addr));
        if (Result == 0) {
            return 1;
        }
    }

    Result = bind(Socket,
                  (struct sockaddr *)&Address,
                  sizeof(struct sockaddr_in));

    return Result;
}

int
DbgrSocketConnect (
    int Socket,
    char *Host,
    int Port
    )

/*++

Routine Description:

    This routine connects to a remote server.

Arguments:

    Socket - Supplies the socket to connect.

    Host - Supplies a pointer to the host string to connect to.

    Port - Supplies the port to bind on.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    struct sockaddr_in Address;
    int Result;

    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Result = inet_pton(AF_INET, Host, &(Address.sin_addr));
    if (Result == 0) {
        return 1;
    }

    Result = connect(Socket,
                     (struct sockaddr *)&Address,
                     sizeof(struct sockaddr_in));

    return Result;
}

int
DbgrSocketListen (
    int Socket
    )

/*++

Routine Description:

    This routine starts a server socket listening for connections.

Arguments:

    Socket - Supplies the socket to listen on.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return listen(Socket, 5);
}

int
DbgrSocketAccept (
    int Socket,
    char **Host,
    int *Port
    )

/*++

Routine Description:

    This routine accepts a new incoming connection from the given listening
    socket.

Arguments:

    Socket - Supplies the socket to accept an incoming connection from.

    Host - Supplies an optional pointer where a string describing the host will
        be returned on success. The caller is responsible for freeing this
        string.

    Port - Supplies an optional pointer where the port number will be returned
        on success.

Return Value:

    Returns the newly connected client socket on success.

    Returns a negative value on failure.

--*/

{

    struct sockaddr_in Address;
    int Result;
    socklen_t Size;
    char StringBuffer[100];

    if (Host != NULL) {
        *Host = NULL;
    }

    if (Port != NULL) {
        *Port = 0;
    }

    Size = sizeof(struct sockaddr_in);
    Result = accept(Socket, (struct sockaddr *)&Address, &Size);
    if (Result < 0) {
        return Result;
    }

    if (Host != NULL) {
        StringBuffer[0] = '\0';
        inet_ntop(Address.sin_family,
                  &(Address.sin_addr),
                  StringBuffer,
                  sizeof(StringBuffer));

        *Host = strdup(StringBuffer);
    }

    if (Port != NULL) {
        *Port = ntohs(Address.sin_port);
    }

    return Result;
}

int
DbgrSocketGetName (
    int Socket,
    char **Host,
    int *Port
    )

/*++

Routine Description:

    This routine gets the current local host and port for the given socket.

Arguments:

    Socket - Supplies the socket to query.

    Host - Supplies an optional pointer where a string describing the host will
        be returned on success. The caller is responsible for freeing this
        string.

    Port - Supplies an optional pointer where the port number will be returned
        on success.

Return Value:

    0 on success.

    Returns a non-zero value on failure.

--*/

{

    struct sockaddr_in Address;
    int Result;
    socklen_t Size;

    if (Host != NULL) {
        *Host = NULL;
    }

    if (Port != NULL) {
        *Port = 0;
    }

    Size = sizeof(Address);
    Result = getsockname(Socket, (struct sockaddr *)&Address, &Size);
    if (Result != 0) {
        return Result;
    }

    if (Host != NULL) {
        *Host = strdup(inet_ntoa(Address.sin_addr));
    }

    if (Port != NULL) {
        *Port = ntohs(Address.sin_port);
    }

    return 0;
}

int
DbgrSocketShutdown (
    int Socket
    )

/*++

Routine Description:

    This routine shuts down a socket. It shuts down both the read and write
    sides of the connection.

Arguments:

    Socket - Supplies the socket to shut down.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return shutdown(Socket, SHUT_RDWR);
}

void
DbgrSocketClose (
    int Socket
    )

/*++

Routine Description:

    This routine closes a socket.

Arguments:

    Socket - Supplies the socket to destroy.

Return Value:

    None.

--*/

{

    close(Socket);
    return;
}

int
DbgrSocketSend (
    int Socket,
    const void *Data,
    int Length
    )

/*++

Routine Description:

    This routine sends data out of a connected socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to send data out of.

    Data - Supplies the buffer of data to send.

    Length - Supplies the length of the data buffer, in bytes.

Return Value:

    Returns the number of bytes sent on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return send(Socket, Data, Length, 0);
}

int
DbgrSocketReceive (
    int Socket,
    void *Buffer,
    int Length
    )

/*++

Routine Description:

    This routine receives data from a connected socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to receive data from.

    Buffer - Supplies a pointer to a buffer where the received data will be
        returned.

    Length - Supplies the length of the data buffer, in bytes.

Return Value:

    Returns the number of bytes received on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return recv(Socket, Buffer, Length, 0);
}

int
DbgrSocketPeek (
    int Socket,
    void *Buffer,
    int Length
    )

/*++

Routine Description:

    This routine peeks at data from a received socket, but does not remove it
    from the queue.

Arguments:

    Socket - Supplies the file descriptor of the socket to receive data from.

    Buffer - Supplies a pointer to a buffer where the peeked data will be
        returned.

    Length - Supplies the length of the data buffer, in bytes.

Return Value:

    Returns the number of bytes received on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return recvfrom(Socket, Buffer, Length, MSG_PEEK, NULL, NULL);
}

//
// --------------------------------------------------------- Internal Functions
//

