/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    missing.h

Abstract:

    This header contains definitions that should be in the standard headers
    but are for some reason missing in MinGW. If these ever get added, this
    should be removed.

Author:

    Evan Green 6-Sep-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define CFM_BACKCOLOR 0x04000000

#define LVGF_NONE           0x00000000
#define LVGF_HEADER         0x00000001
#define LVGF_FOOTER         0x00000002
#define LVGF_STATE          0x00000004
#define LVGF_ALIGN          0x00000008
#define LVGF_GROUPID        0x00000010

#if _WIN32_WINNT >= 0x0600

#define LVGF_SUBTITLE           0x00000100
#define LVGF_TASK               0x00000200
#define LVGF_DESCRIPTIONTOP     0x00000400
#define LVGF_DESCRIPTIONBOTTOM  0x00000800
#define LVGF_TITLEIMAGE         0x00001000
#define LVGF_EXTENDEDIMAGE      0x00002000
#define LVGF_ITEMS              0x00004000
#define LVGF_SUBSET             0x00008000
#define LVGF_SUBSETITEMS        0x00010000

#endif

#define LVGS_NORMAL             0x00000000
#define LVGS_COLLAPSED          0x00000001
#define LVGS_HIDDEN             0x00000002
#define LVGS_NOHEADER           0x00000004
#define LVGS_COLLAPSIBLE        0x00000008
#define LVGS_FOCUSED            0x00000010
#define LVGS_SELECTED           0x00000020
#define LVGS_SUBSETED           0x00000040
#define LVGS_SUBSETLINKFOCUSED  0x00000080

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct tagLVGROUP {
    UINT cbSize;
    UINT mask;
    LPWSTR pszHeader;
    int cchHeader;
    LPWSTR pszFooter;
    int cchFooter;
    int iGroupId;
    UINT stateMask;
    UINT state;
    UINT uAlign;

#if _WIN32_WINNT >= 0x0600

    LPWSTR pszSubtitle;
    UINT cchSubtitle;
    LPWSTR pszTask;
    UINT cchTask;
    LPWSTR pszDescriptionTop;
    UINT cchDescriptionTop;
    LPWSTR pszDescriptionBottom;
    UINT cchDescriptionBottom;
    int iTitleImage;
    int iExtendedImage;
    int iFirstItem;
    UINT cItems;
    LPWSTR pszSubsetTitle;
    UINT cchSubsetTitle;

#endif

} LVGROUP, *PLVGROUP;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
