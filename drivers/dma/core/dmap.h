/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dmap.h

Abstract:

    This header contains internal definitions for the DMA core driver.

Author:

    Evan Green 1-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#define DMA_API __DLLEXPORT

#include <minoca/dma/dmahost.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the DMA allocation Tag: Dma!
//

#define DMA_ALLOCATION_TAG 0x21616D44
#define DMA_CONTROLLER_MAGIC DMA_ALLOCATION_TAG

#define DMA_CONTROLLER_INFORMATION_MAX_VERSION 0x0001000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal data of a Direct Memory Access library
    channel.

Members:

    Lock - Stores the lock serializing access to this channel.

    Transfer - Stores a pointer to the transfer currently in progress on this
        channel.

    Queue - Stores the head of the queue of transfers on this channel.

--*/

typedef struct _DMA_CHANNEL {
    KSPIN_LOCK Lock;
    PDMA_TRANSFER Transfer;
    LIST_ENTRY Queue;
} DMA_CHANNEL, *PDMA_CHANNEL;

/*++

Structure Description:

    This structure stores the internal data of a Direct Memory Access library
    controller.

Members:

    Magic - Stores the constant DMA_CONTROLLER_MAGIC.

    Host - Stores the host controller information.

    Interface - Stores the public published interface.

    Channels - Stores a pointer to the array of DMA channels in the controller.

    ChannelCount - Stores the number of elements in the channels array.

    ArbiterCreated - Stores a boolean indicating whether or not the DMA
        arbiter has been created yet.

--*/

struct _DMA_CONTROLLER {
    ULONG Magic;
    DMA_CONTROLLER_INFORMATION Host;
    DMA_INTERFACE Interface;
    PDMA_CHANNEL Channels;
    ULONG ChannelCount;
    BOOL ArbiterCreated;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
