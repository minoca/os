/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzmap.h

Abstract:

    This header contains definitions common to both the LZMA encoder and
    decoder.

Author:

    Evan Green 27-Oct-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_REP_COUNT 4

#define LZMA_BIT_MODEL_BIT_COUNT 11
#define LZMA_BIT_MODEL_TOTAL (1 << LZMA_BIT_MODEL_BIT_COUNT)
#define LZMA_MOVE_BIT_COUNT 5

#define LZMA_LENGTH_TO_POSITION_STATES 4

#define LZMA_START_POSITION_MODEL_INDEX 4
#define LZMA_END_POSITION_MODEL_INDEX 14
#define LZMA_POSITION_MODEL_COUNT \
    (LZMA_END_POSITION_MODEL_INDEX - LZMA_START_POSITION_MODEL_INDEX)

#define LZMA_FULL_DISTANCES (1 << (LZMA_END_POSITION_MODEL_INDEX >> 1))

#define LZMA_POSITION_SLOT_BITS 6
#define LZMA_POSITION_SLOTS (1 << LZMA_POSITION_SLOT_BITS)

#define LZMA_STATE_COUNT 12
#define LZMA_LITERAL_STATE_COUNT 7

#define LZMA_LENGTH_LOW_BITS 3
#define LZMA_LENGTH_LOW_SYMBOLS (1 << LZMA_LENGTH_LOW_BITS)

#define LZMA_LENGTH_MID_BITS 3
#define LZMA_LENGTH_MID_SYMBOLS (1 << LZMA_LENGTH_MID_BITS)

#define LZMA_LENGTH_HIGH_BITS 8
#define LZMA_LENGTH_HIGH_SYMBOLS (1 << LZMA_LENGTH_HIGH_BITS)

#define LZMA_LENGTH_TOTAL_SYMBOL_COUNT \
    (LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS + \
     LZMA_LENGTH_HIGH_SYMBOLS)

#define LZMA_ALIGN_TABLE_BITS 4
#define LZMA_ALIGN_TABLE_SIZE (1 << LZMA_ALIGN_TABLE_BITS)

#define LZMA_MIN_MATCH_LENGTH 2

#define LZMA_PROPERTIES_SIZE 5

#define LZMA_RANGE_TOP_VALUE (1 << 24)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef USHORT LZ_PROB, *PLZ_PROB;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
