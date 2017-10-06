/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    telnet.h

Abstract:

    This header contains definitions for the Telnet protocol (RFC 854 and
    others).

Author:

    Evan Green 26-Sep-2015

--*/

#ifndef _ARPA_TELNET_H
#define _ARPA_TELNET_H

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define TELCMD_OK(_Command) \
    (((unsigned int)(_Command) <= TELCMD_LAST) && \
     ((unsigned int)(_Command) >= TELCMD_FIRST))

#define TELCMD(_Command) telcmds[(_Command) - TELCMD_FIRST]

#define TELOPT_OK(_Command) ((unsigned int)(_Command) <= TELOPT_LAST)
#define TELOPT(_Command) telopts[(_Command) - TELOPT_FIRST]

#define SLC_NAME_OK(_Name) ((unsigned int)(_Name) < NSLC)
#define SLC_NAME(_Name) slc_names[(_Name)]

#define AUTHTYPE_NAME_OK(_Name) ((unsigned int)(_Name) < AUTHTYPE_CNT)
#define AUTHTYPE_NAME(_Name) authtype_names[(_Name)]

#define ENCRYPT_NAME_OK(_Name) ((unsigned int)(_Name) < ENCRYPT_CNT)
#define ENCRYPT_NAME(_Name) encrypt_names[(_Name)]

#define ENCTYPE_NAME_OK(_Name) ((unsigned int)(_Name) < ENCTYPE_CNT)
#define ENCTYPE_NAME(_Name) enctype_names[(_Name)]

//
// ---------------------------------------------------------------- Definitions
//

//
// Telnet byte definitions
//

//
// Interpret as command
//

#define IAC 255

//
// Request to the other side not to use this option
//

#define DONT 254

//
// Request to the other side to use this option
//

#define DO 253

//
// Indication that this side will not use this option
//

#define WONT 252

//
// Indication that this side will use this option
//

#define WILL 251

//
// Interpret as subnegotiation
//

#define SB 250

//
// Go ahead, reverse the line
//

#define GA 249

//
// Erase the current line
//

#define EL 248

//
// Erase the current character
//

#define EC 247

//
// Are you there
//

#define AYT 246

//
// Abort output (but let the program finish)
//

#define AO 245

//
// Interrupt process (permanently)
//

#define IP 244

//
// Break
//

#define BREAK 243

//
// Data mark
//

#define DM 242

//
// No-op
//

#define NOP 241

//
// End subnegotiation
//

#define SE 240

//
// End of record (transparent mode)
//

#define EOR 239

//
// Abort process
//

#define ABORT 238

//
// Suspend process
//

#define SUSP 237

//
// End of file
//

#define xEOF 236

#define SYNCH 242

//
// Define the range of valid telnet commands.
//

#define TELCMD_FIRST xEOF
#define TELCMD_LAST IAC

//
// Telnet options
//

//
// 8-bit data path
//

#define TELOPT_BINARY 0

//
// Echo
//

#define TELOPT_ECHO 1

//
// Prepare to reconnect
//

#define TELOPT_RCP 2

//
// Suppress go-ahead
//

#define TELOPT_SGA 3

//
// Approximate message size
//

#define TELOPT_NAMS 4

//
// Give status
//

#define TELOPT_STATUS 5

//
// Timing mark
//

#define TELOPT_TM 6

//
// Remote controlled transmission and echo
//

#define TELOPT_RCTE 7

//
// Negotiate about output line width
//

#define TELOPT_NAOL 8

//
// Negotiate about output page size
//

#define TELOPT_NAOP 9

//
// Negotiate about carriage return disposition
//

#define TELOPT_NAOCRD 10

//
// Negotiate about horizontal tab stops
//

#define TELOPT_NAOHTS 11

//
// Negotiate about horizontal tab disposition
//

#define TELOPT_NAOHTD 12

//
// Negotiate about form feed disposition
//

#define TELOPT_NAOFFD 13

//
// Negotiate about vertical tab stops
//

#define TELOPT_NAOVTS 14

//
// Negotiate about vertical tab disposition
//

#define TELOPT_NAOVTD 15

//
// Negotiate about line feed disposition
//

#define TELOPT_NAOLFD 16

//
// Extended ASCII character set
//

#define TELOPT_XASCII 17

//
// Force logout
//

#define TELOPT_LOGOUT 18

//
// Byte macro
//

#define TELOPT_BM 19

//
// Data entry terminal
//

#define TELOPT_DET 20

//
// Supdup protocol
//

#define TELOPT_SUPDUP 21

//
// Supdup output
//

#define TELOPT_SUPDUPOUTPUT 22

//
// Send location
//

#define TELOPT_SNDLOC 23

//
// Terminal type
//

#define TELOPT_TTYPE 24

//
// End of record
//

#define TELOPT_EOR 25

//
// TACACS user identification
//

#define TELOPT_TUID 26

//
// Output marking
//

#define TELOPT_OUTMRK 27

//
// Terminal location number
//

#define TELOPT_TTYLOC 28

//
// 3270 regime
//

#define TELOPT_3270REGIME 29

//
// X.3 PAD
//

#define TELOPT_X3PAD 30

//
// Window size
//

#define TELOPT_NAWS 31

//
// Terminal speed
//

#define TELOPT_TSPEED 32

//
// Remove flow control
//

#define TELOPT_LFLOW 33

//
// Line mode option
//

#define TELOPT_LINEMODE 34

//
// X Display location
//

#define TELOPT_XDISPLOC 35

//
// Old environment variables
//

#define TELOPT_OLD_ENVIRON 36

//
// Authenticate
//

#define TELOPT_AUTHENTICATION 37

//
// Encryption option
//

#define TELOPT_ENCRYPT 38

//
// New environment variables
//

#define TELOPT_NEW_ENVIRON 39

//
// extended options list
//

#define TELOPT_EXOPL 255

//
// Define the number of options and the valid range of telopt commands.
//

#define NTELOPTS (TELOPT_NEW_ENVIRON + 1)
#define TELOPT_FIRST TELOPT_BINARY
#define TELOPT_LAST TELOPT_NEW_ENVIRON

//
// Define suboption qualifiers
//

//
// Option is
//

#define TELQUAL_IS 0

//
// Send option
//

#define TELQUAL_SEND 1

//
// ENVIRON: informational version of IS
//

#define TELQUAL_INFO 2

//
// AUTHENTICATION: client version of IS
//

#define TELQUAL_REPLY 2

//
// AUTHENTICATION: client version of IS
//

#define TELQUAL_NAME 3

//
// Disable remote flow control
//

#define LFLOW_OFF 0

//
// Enable remote flow control
//

#define LFLOW_ON 1

//
// Restart output upon receipt of any character
//

#define LFLOW_RESTART_ANY 2

//
// Restart output upon receipt of XON character only
//

#define LFLOW_RESTART_XON 3

//
// Line mode suboptions
//

#define LM_MODE 1
#define LM_FORWARDMASK 2
#define LM_SLC 3

#define MODE_EDIT 0x01
#define MODE_TRAPSIG 0x02
#define MODE_ACK 0x04
#define MODE_SOFT_TAB 0x08
#define MODE_LIT_ECHO 0x10
#define MODE_MASK 0x1F

#define MODE_FLOW 0x0100
#define MODE_ECHO 0x0200
#define MODE_INBIN 0x0400
#define MODE_OUTBIN 0x0800
#define MODE_FORCE 0x1000

#define SLC_SYNCH 1
#define SLC_BRK 2
#define SLC_IP 3
#define SLC_AO 4
#define SLC_AYT 5
#define SLC_EOR 6
#define SLC_ABORT 7
#define SLC_EOF 8
#define SLC_SUSP 9
#define SLC_EC 10
#define SLC_EL 11
#define SLC_EW 12
#define SLC_RP 13
#define SLC_LNEXT 14
#define SLC_XON 15
#define SLC_XOFF 16
#define SLC_FORW1 17
#define SLC_FORW2 18

#define NSLC 18

#define SLC_NAMELIST \
    "0", \
    "SYNCH", \
    "BRK", \
    "IP", \
    "AO", \
    "AYT", \
    "EOF", \
    "ABORT", \
    "EOF", \
    "SUSP", \
    "EC", \
    "EL", \
    "EW", \
    "RP", \
    "LNEXT", \
    "XON", \
    "XOFF", \
    "FORW1", \
    "FORW2", \
    0,

#define SLC_NOSUPPORT 0
#define SLC_CANTCHANGE 1
#define SLC_VARIABLE 2
#define SLC_DEFAULT 3
#define SLC_LEVELBITS 0x03

#define SLC_FUNC 0
#define SLC_FLAGS 1
#define SLC_VALUE 2

#define SLC_ACK 0x80
#define SLC_FLUSHIN 0x40
#define SLC_FLUSHOUT 0x20

#define OLD_ENV_VAR 1
#define OLD_ENV_VALUE 0
#define NEW_ENV_VAR 0
#define NEW_ENV_VALUE 1
#define ENV_ESC 2
#define ENV_USERVAR 3

//
// Define authentication suboptions
//

//
// The client is authenticating the server
//

#define AUTH_WHO_CLIENT 0

//
// The server is authenticatin the client
//

#define AUTH_WHO_SERVER 1

#define AUTH_WHO_MASK 1

//
// Define the amount of authentication done
//

#define AUTH_HOW_ONE_WAY 0
#define AUTH_HOW_MUTUAL 2
#define AUTH_HOW_MASK 2

#define AUTHTYPE_NULL 0
#define AUTHTYPE_KERBEROS_V4 1
#define AUTHTYPE_KERBEROS_V5 2
#define AUTHTYPE_SPX 3
#define AUTHTYPE_MINK 4
#define AUTHTYPE_CNT 5

#define AUTHTYPE_TEST 99

//
// Define encryption suboptions
//

//
// Encryption type is
//

#define ENCRYPT_IS 0

//
// Supported encryption types
//

#define ENCRYPT_SUPPORT 1

//
// Initial setup response
//

#define ENCRYPT_REPLY 2

//
// Am starting to send encrypted
//

#define ENCRYPT_START 3

//
// Am ending encrypted data
//

#define ENCRYPT_END 4

//
// Request that the other side begin encryption
//

#define ENCRYPT_REQSTART 5

//
// Request that the other side end encryption
//

#define ENCRYPT_REQEND 6
#define ENCRYPT_ENC_KEYID 7
#define ENCRYPT_DEC_KEYID 8
#define ENCRYPT_CNT 9

#define ENCTYPE_ANY 0
#define ENCTYPE_DES_CFB64 1
#define ENCTYPE_DES_OFB64 2
#define ENCTYPE_CNT 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

#ifdef TELCMDS

char *telcmds[] = {
    "EOF",
    "SUSP",
    "ABORT",
    "EOR",
    "SE",
    "NOP",
    "DMARK",
    "BRK",
    "IP",
    "AO",
    "AYT",
    "EC",
    "EL",
    "GA",
    "SB",
    "WILL",
    "WONT",
    "DO",
    "DONT",
    "IAC",
    0
};

#else

extern char *telcmds[];

#endif

#ifdef TELOPTS

char *telopts[NTELOPTS + 1] = {
    "BINARY",
    "ECHO",
    "RCP",
    "SUPPRESS GO AHEAD",
    "NAME",
    "STATUS",
    "TIMING MARK",
    "RCTE",
    "NAOL",
    "NAOP",
    "NAOCRD",
    "NAOHTS",
    "NAOHTD",
    "NAOFFD",
    "NAOVTS",
    "NAOVTD",
    "NAOLFD",
    "EXTEND ASCII",
    "LOGOUT",
    "BYTE MACRO",
    "DATA ENTRY TERMINAL",
    "SUPDUP",
    "SUPDUP OUTPUT",
    "SEND LOCATION",
    "TERMINAL TYPE",
    "END OF RECORD",
    "TACACS UID",
    "OUTPUT MARKING",
    "TTYLOC",
    "3270 REGIME",
    "X.3 PAD",
    "NAWS",
    "TSPEED",
    "LFLOW",
    "LINEMODE",
    "XDISPLOC",
    "OLD-ENVIRON",
    "AUTHENTICATION",
    "ENCRYPT",
    "NEW-ENVIRON",
    0
};

#else

extern char *telopts[];

#endif

#ifdef SLC_NAMES

char *slc_names[] = {SLC_NAMELIST};

#else

extern char *slc_names[];

#define SLC_NAMES SLC_NAMELIST

#endif

#ifdef AUTH_NAMES

char *authtype_names[] = {
    "NULL",
    "KERBEROS_V4",
    "KERBEROS_V5",
    "SPX",
    "MINK",
    0,
};

#else

extern char *authtype_names[];

#endif

#ifdef ENCRYPT_NAMES

char *encrypt_names[] = {
    "IS",
    "SUPPORT",
    "REPLY",
    "START",
    "END",
    "REQUEST-START",
    "REQUEST-END",
    "ENC-KEYID",
    "DEC-KEYID",
    0
};

char *enctype_names[] = {
    "ANY",
    "DES_CFB64",
    "DES_OFB64",
    0
};

#else

extern char *encrypt_names[];
extern char *enctype_names[];

#endif

//
// -------------------------------------------------------- Function Prototypes
//

#endif

