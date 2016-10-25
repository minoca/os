/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    random.c

Abstract:

    This module implements the random interface, which supplies pseudo-random
    numbers via a non-linear additive feedback random number generator.

Author:

    Evan Green 9-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define RANDOM_DEFAULT_STATE_SIZE 128

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RANDOM_TYPE {
    RandomType0,
    RandomType1,
    RandomType2,
    RandomType3,
    RandomType4,
    RandomTypeCount
} RANDOM_TYPE, *PRANDOM_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

int ClRandomBreaks[RandomTypeCount] = {8, 32, 64, 128, 256};
int ClRandomDegrees[RandomTypeCount] = {0, 7, 15, 31, 63};
int ClRandomSeparations[RandomTypeCount] = {0, 3, 1, 3, 1};

//
// Store the global random state, making the functions that use this state
// not thread-safe and not reentrant.
//

struct random_data ClRandomData;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
initstate (
    unsigned int Seed,
    char *State,
    size_t Size
    )

/*++

Routine Description:

    This routine initializes the state of the random number generator using
    the given state data. This routine is neither thread-safe nor reentrant.

Arguments:

    Seed - Supplies the seed value to use.

    State - Supplies a pointer the random state data to use.

    Size - Supplies the size of the random state data. Valid values are 8, 32,
        64, 128, and 256. If the value is not one of these values, it will be
        truncated down to one of these values. For data sizes less than 32, a
        simple linear congruential random number generator is used. The minimum
        valid size is 8.

Return Value:

    Returns a pointer to the previous state.

--*/

{

    void *OriginalState;

    OriginalState = ClRandomData.state;
    initstate_r(Seed, State, Size, &ClRandomData);
    return OriginalState;
}

LIBC_API
char *
setstate (
    const char *State
    )

/*++

Routine Description:

    This routine resets the state of the random number generator to the given
    state, previously acquired from initstate. This routine is neither
    thread-safe nor reentrant.

Arguments:

    State - Supplies a pointer to the state to set.

Return Value:

    Returns a pointer to the previous state.

--*/

{

    void *OriginalState;

    OriginalState = ClRandomData.state;
    setstate_r(State, &ClRandomData);
    return OriginalState;
}

LIBC_API
void
srandom (
    unsigned int Seed
    )

/*++

Routine Description:

    This routine seeds the non-linear additive feedback random number
    generator. This routine is neither thread-safe nor reentrant.

Arguments:

    Seed - Supplies the seed value to use.

Return Value:

    None.

--*/

{

    char *State;

    if (ClRandomData.state == NULL) {
        State = malloc(RANDOM_DEFAULT_STATE_SIZE);
        if (State == NULL) {
            return;
        }

        initstate_r(1, State, RANDOM_DEFAULT_STATE_SIZE, &ClRandomData);
    }

    srandom_r(Seed, &ClRandomData);
    return;
}

LIBC_API
long
random (
    void
    )

/*++

Routine Description:

    This routine returns a random number in the range of 0 to 0x7FFFFFFF,
    inclusive. This routine is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pseudo-random number in the range 0 to 2^32 - 1, inclusive.

--*/

{

    int32_t Result;
    char *State;

    if (ClRandomData.state == NULL) {
        State = malloc(RANDOM_DEFAULT_STATE_SIZE);
        if (State == NULL) {
            return -1;
        }

        initstate_r(1, State, RANDOM_DEFAULT_STATE_SIZE, &ClRandomData);
    }

    if (random_r(&ClRandomData, &Result) != 0) {
        return -1;
    }

    return Result;
}

LIBC_API
int
initstate_r (
    unsigned int Seed,
    char *State,
    size_t Size,
    struct random_data *RandomData
    )

/*++

Routine Description:

    This routine initializes the state of the random number generator using
    the given state data.

Arguments:

    Seed - Supplies the seed value to use.

    State - Supplies a pointer the random state data to use.

    Size - Supplies the size of the random state data. Valid values are 8, 32,
        64, 128, and 256. If the value is not one of these values, it will be
        truncated down to one of these values. For data sizes less than 32, a
        simple linear congruential random number generator is used. The minimum
        valid size is 8.

    RandomData - Supplies a pointer to the random state context.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    int Degree;
    int32_t *OriginalState;
    int OriginalType;
    int Separation;
    int Type;

    if (RandomData == NULL) {
        errno = EINVAL;
        return -1;
    }

    OriginalState = RandomData->state;
    if (OriginalState != NULL) {
        OriginalType = RandomData->rand_type;
        if (OriginalType == RandomType0) {
            *(OriginalState - 1) = RandomType0;

        } else {
            *(OriginalState - 1) = (RandomTypeCount *
                                    (RandomData->rptr - OriginalState)) +
                                   OriginalType;
        }
    }

    if (Size < ClRandomBreaks[RandomType0]) {
        errno = EINVAL;
        return -1;
    }

    for (Type = RandomType0; Type < RandomType4 - 1; Type += 1) {
        if (Size < ClRandomBreaks[Type + 1]) {
            break;
        }
    }

    Degree = ClRandomDegrees[Type];
    Separation = ClRandomSeparations[Type];
    RandomData->rand_type = Type;
    RandomData->rand_deg = Degree;
    RandomData->rand_sep = Separation;
    RandomData->state = ((int32_t *)State) + 1;
    RandomData->end_ptr = &(RandomData->state[Degree]);
    srandom_r(Seed, RandomData);
    *(RandomData->state - 1) = RandomType0;
    if (Type != RandomType0) {
        *(RandomData->state - 1) = ((RandomData->rptr - RandomData->state) *
                                    RandomTypeCount) + Type;
    }

    return 0;
}

LIBC_API
int
setstate_r (
    const char *State,
    struct random_data *RandomData
    )

/*++

Routine Description:

    This routine resets the state of the random number generator to the given
    state.

Arguments:

    State - Supplies a pointer to the state to set.

    RandomData - Supplies a pointer to the random state to use.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    int Degree;
    int32_t *NewState;
    int32_t *OriginalState;
    int OriginalType;
    UINTN RearIndex;
    int Separation;
    int Type;

    if ((State == NULL) || (RandomData == NULL)) {
        errno = EINVAL;
        return -1;
    }

    NewState = ((int32_t *)State) + 1;
    OriginalType = RandomData->rand_type;
    OriginalState = RandomData->state;
    if (OriginalType == RandomType0) {
        *(OriginalState - 1) = RandomType0;

    } else {
        *(OriginalState - 1) = ((RandomData->rptr - OriginalState) *
                                RandomTypeCount) + OriginalType;
    }

    Type = *(NewState - 1) % RandomTypeCount;
    if ((Type < RandomType0) || (Type >= RandomTypeCount)) {
        errno = EINVAL;
        return -1;
    }

    Degree = ClRandomDegrees[Type];
    Separation = ClRandomSeparations[Type];
    RandomData->rand_deg = Degree;
    RandomData->rand_sep = Separation;
    RandomData->rand_type = Type;
    if (Type != RandomType0) {
        RearIndex = *(NewState - 1) / RandomTypeCount;
        RandomData->rptr = &(NewState[RearIndex]);
        RandomData->fptr = &(NewState[(RearIndex + Separation) % Degree]);
    }

    RandomData->state = NewState;
    RandomData->end_ptr = &(NewState[Degree]);
    return 0;
}

LIBC_API
int
srandom_r (
    unsigned int Seed,
    struct random_data *RandomData
    )

/*++

Routine Description:

    This routine seeds the non-linear additive feedback random number generator.

Arguments:

    Seed - Supplies the seed value to use.

    RandomData - Supplies a pointer to the random state to use.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    INTN Degree;
    int32_t *Destination;
    LONG High;
    UINTN Index;
    LONG Low;
    int32_t *State;
    int Type;
    int32_t UnusedValue;
    int32_t Word;

    if ((RandomData == NULL) || (RandomData->rand_type >= RandomTypeCount)) {
        errno = EINVAL;
        return -1;
    }

    State = RandomData->state;
    Type = RandomData->rand_type;
    if (Seed == 0) {
        Seed = 1;
    }

    State[0] = Seed;
    if (Type == RandomType0) {
        return 0;
    }

    Destination = State;
    Word = Seed;
    Degree = RandomData->rand_deg;
    for (Index = 1; Index < Degree; Index += 1) {

        //
        // Set State[i] equal to (State[i - 1] * 16807) % 0x7FFFFFFF.
        //

        High = Word / 127773;
        Low = Word % 127773;
        Word = (16807 * Low) - (2836 * High);
        if (Word < 0) {
            Word += 0x7FFFFFFF;
        }

        Destination += 1;
        *Destination = Word;
    }

    RandomData->fptr = &(State[RandomData->rand_sep]);
    RandomData->rptr = &(State[0]);
    Degree *= 10;
    while (Degree > 0) {
        Degree -= 1;
        random_r(RandomData, &UnusedValue);
    }

    return 0;
}

LIBC_API
int
random_r (
    struct random_data *RandomData,
    int32_t *Result
    )

/*++

Routine Description:

    This routine returns a random number in the range of 0 to 0x7FFFFFFF,
    inclusive.

Arguments:

    RandomData - Supplies a pointer to the random state to use.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    int32_t *End;
    int32_t *Front;
    int32_t *Rear;
    int32_t *State;
    int32_t Value;

    if ((RandomData == NULL) || (Result == NULL)) {
        errno = EINVAL;
        return -1;
    }

    State = RandomData->state;
    if (RandomData->rand_type == RandomType0) {
        Value = ((RandomData->state[0] * 1103515245) + 12345) & 0x7FFFFFFF;
        RandomData->state[0] = Value;
        *Result = Value;

    } else {
        Front = RandomData->fptr;
        Rear = RandomData->rptr;
        End = RandomData->end_ptr;
        *Front += *Rear;
        Value = *Front;

        //
        // Throw out the least significant bit.
        //

        *Result = (Value >> 1) & 0x7FFFFFFF;
        Front += 1;
        if (Front >= End) {
            Front = State;
            Rear += 1;

        } else {
            Rear += 1;
            if (Rear >= End) {
                Rear = State;
            }
        }

        RandomData->fptr = Front;
        RandomData->rptr = Rear;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

