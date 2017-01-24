/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hostname.c

Abstract:

    This module implements the hostname utility.

Author:

    Evan Green 16-Jan-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define HOSTNAME_VERSION_MAJOR 1
#define HOSTNAME_VERSION_MINOR 0

#define HOSTNAME_USAGE                                                         \
    "usage: hostname [options] [-f file] [newname]\n"                          \
    "The hostname utility prints or sets the machine's network host name.\n"   \
    "Options are:\n"                                                           \
    "  -d, --domain -- Display the DNS domain name.\n"                         \
    "  -f, --fqdn, --long -- Display the fully qualified domain name (FQDN).\n"\
    "  -F, --file=file -- Set the hostname to the contents of the specified "  \
    "file.\n"                                                                  \
    "  -i, --ip-address -- Display the IP address(es) of the host. Note that\n"\
    "      this only works if the host name can be resolved.\n"                \
    "  -s, --short -- Display the short host name (truncated at the first "    \
    "dot).\n"                                                                  \
    "  -v, --verbose -- Print what's going on.\n"                              \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define HOSTNAME_OPTIONS_STRING "dfF:isvhV"

//
// Set this option to display the domain name.
//

#define HOSTNAME_OPTION_DOMAIN_NAME 0x00000001

//
// Set this option to display the fully qualified domain name.
//

#define HOSTNAME_OPTION_FQDN 0x00000002

//
// Set this option to display the IP address.
//

#define HOSTNAME_OPTION_IP_ADDRESS 0x00000004

//
// Set this option to only display the hostname truncated to the first period.
//

#define HOSTNAME_OPTION_SHORT 0x00000008

//
// Set this option to be verbose.
//

#define HOSTNAME_OPTION_VERBOSE 0x00000010

//
// Define the set of options that change the output.
//

#define HOSTNAME_OPTION_ACTIVE_MASK \
    (HOSTNAME_OPTION_DOMAIN_NAME | HOSTNAME_OPTION_FQDN | \
     HOSTNAME_OPTION_IP_ADDRESS | HOSTNAME_OPTION_SHORT)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option HostnameLongOptions[] = {
    {"domain", no_argument, 0, 'd'},
    {"fqdn", no_argument, 0, 'f'},
    {"long", no_argument, 0, 'f'},
    {"file", required_argument, 0, 'F'},
    {"ip-address", no_argument, 0, 'i'},
    {"short", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
HostnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the hostname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    struct in_addr **AddressList;
    PSTR AppName;
    ULONG ArgumentIndex;
    PSTR Comment;
    PSTR Dot;
    FILE *File;
    PSTR FilePath;
    struct hostent *HostEntry;
    CHAR HostName[_POSIX_HOST_NAME_MAX + 1];
    PSTR NewName;
    INT Option;
    ULONG Options;
    PSTR Search;
    int Status;

    FilePath = NULL;
    NewName = NULL;
    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             HOSTNAME_OPTIONS_STRING,
                             HostnameLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'd':
            Options |= HOSTNAME_OPTION_DOMAIN_NAME;
            break;

        case 'f':
            Options |= HOSTNAME_OPTION_FQDN;
            break;

        case 'F':
            FilePath = optarg;
            break;

        case 'i':
            Options |= HOSTNAME_OPTION_IP_ADDRESS;
            break;

        case 's':
            Options |= HOSTNAME_OPTION_SHORT;
            break;

        case 'v':
            Options |= HOSTNAME_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(HOSTNAME_VERSION_MAJOR, HOSTNAME_VERSION_MINOR);
            return 1;

        case 'h':
            printf(HOSTNAME_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        NewName = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            SwPrintError(0, NULL, "Too many arguments");
            Status = 1;
            goto MainEnd;
        }
    }

    Status = 0;
    if (gethostname(HostName, sizeof(HostName)) != 0) {
        HostName[0] = '\0';
        Status = errno;
    }

    //
    // If this is the "dnsdomainname" app, then it just acts like the -d option.
    //

    AppName = strrchr(Arguments[0], '/');
    if (AppName == NULL) {
        AppName = Arguments[0];
    }

    if (strstr(AppName, "domainname") != NULL) {
        Options = HOSTNAME_OPTION_DOMAIN_NAME;
    }

    //
    // If a real option was specified, do it their way.
    //

    if ((Options & HOSTNAME_OPTION_ACTIVE_MASK) != 0) {
        HostEntry = gethostbyname(HostName);
        if (HostEntry == NULL) {
            if (Options != HOSTNAME_OPTION_DOMAIN_NAME) {
                Status = errno;
                SwPrintError(Status, HostName, "Failed to look up host");
                goto MainEnd;
            }

            Dot = NULL;

        } else {
            Dot = strchr(HostEntry->h_name, '.');
        }

        if ((Options & HOSTNAME_OPTION_FQDN) != 0) {
            puts(HostEntry->h_name);

        } else if ((Options & HOSTNAME_OPTION_SHORT) != 0) {
            if (Dot != NULL) {
                *Dot = '\0';
            }

            puts(HostEntry->h_name);

        } else if ((Options & HOSTNAME_OPTION_DOMAIN_NAME) != 0) {
            if (HostEntry != NULL) {
                if (Dot != NULL) {
                    puts(Dot + 1);
                }

            } else {
                if (getdomainname(HostName, sizeof(HostName)) != 0) {
                    Status = errno;
                    SwPrintError(Status, NULL, "Failed to get domain name");
                    goto MainEnd;
                }

                if (HostName[0] != '\0') {
                    puts(HostName);
                }
            }

        } else if ((Options & HOSTNAME_OPTION_IP_ADDRESS) != 0) {
            if (HostEntry->h_length == sizeof(struct in_addr)) {
                AddressList = (struct in_addr **)(HostEntry->h_addr_list);
                while (*AddressList != NULL) {
                    printf("%s ", inet_ntoa(**AddressList));
                    AddressList += 1;
                }

                puts("");
            }
        }

    //
    // If a file was specified, use that.
    //

    } else if (FilePath != NULL) {
        File = fopen(FilePath, "r");
        if (File == NULL) {
            Status = errno;
            SwPrintError(Status, FilePath, "Cannot open");
            goto MainEnd;
        }

        //
        // Loop trying to read a line with a valid entry in it, ignoring
        // comments and blank space on either side of the line.
        //

        Status = 0;
        while (TRUE) {
            if (fgets(HostName, sizeof(HostName), File) == NULL) {
                Status = errno;
                NewName = NULL;
                break;
            }

            HostName[sizeof(HostName) - 1] = '\0';
            NewName = HostName;
            while ((*NewName != '\0') && (isspace(*NewName))) {
                NewName += 1;
            }

            Comment = strchr(NewName, '#');
            if (Comment != NULL) {
                *Comment = '\0';
            }

            Search = NewName + strlen(NewName);
            while ((Search - 1 >= NewName) && (isspace(*(Search - 1)))) {
                Search -= 1;
                *Search = '\0';
            }

            if (*NewName != '\0') {
                break;
            }
        }

        fclose(File);
        if (Status != 0) {
            goto MainEnd;
        }

    //
    // No arguments, just print the host name.
    //

    } else if (NewName == NULL) {
        if (HostName[0] != '\0') {
            puts(HostName);
        }
    }

    //
    // Set a new name if there is one.
    //

    if (NewName != NULL) {
        if ((Options & HOSTNAME_OPTION_DOMAIN_NAME) != 0) {
            Status = setdomainname(NewName, strlen(NewName));

        } else {
            Status = sethostname(NewName, strlen(NewName));
        }

        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, NewName, "Failed to set name");
            goto MainEnd;
        }
    }

    Status = 0;

MainEnd:
    if (Status != 0) {
        Status = 1;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

