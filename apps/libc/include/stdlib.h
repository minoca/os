/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stdlib.h

Abstract:

    This header contains standard C Library definitions.

Author:

    Evan Green 6-Mar-2013

--*/

#ifndef _STDLIB_H
#define _STDLIB_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <alloca.h>
#include <stddef.h>
#include <wchar.h>
#include <sys/select.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define values to give to exit on failure and success.
//

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

//
// Define the maximum value that the random functions will return.
//

#define RAND_MAX 0x7FFFFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the type returned by the div function.
//

typedef struct {
    int quot;
    int rem;
} div_t;

//
// Define the type returned by the ldiv function.
//

typedef struct {
    long quot;
    long rem;
} ldiv_t;

//
// Define the type returned by the lldiv function.
//

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

/*++

Structure Description:

    This structure stores random state information.

Members:

    fptr - Stores the front pointer.

    rptr - Stores the rear pointer.

    state - Stores the array of state values.

    rand_type - Stores the type of random number generator.

    rand_deg - Stores the degree of the generator.

    rand_sep - Stores the distance between the front and the rear.

    end_ptr - Stores a pointer to the end of the state table.

--*/

struct random_data {
    int32_t *fptr;
    int32_t *rptr;
    int32_t *state;
    int rand_type;
    int rand_deg;
    int rand_sep;
    int32_t *end_ptr;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Define the maximum number of bytes in a multibyte character for the current
// locale.
//

LIBC_API extern int MB_CUR_MAX;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
abs (
    int Value
    );

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

LIBC_API
long
labs (
    long Value
    );

/*++

Routine Description:

    This routine returns the absolute value of the given long value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

LIBC_API
long long
llabs (
    long long Value
    );

/*++

Routine Description:

    This routine returns the absolute value of the given long long value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

LIBC_API
div_t
div (
    int Numerator,
    int Denominator
    );

/*++

Routine Description:

    This routine divides two integers. If the division is inexact, the
    resulting quotient is the integer of lesser magnitude that is nearest to
    the algebraic quotient. If the result cannot be represented, the behavior
    is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

LIBC_API
ldiv_t
ldiv (
    long Numerator,
    long Denominator
    );

/*++

Routine Description:

    This routine divides two long integers. If the division is inexact, the
    resulting quotient is the integer of lesser magnitude that is nearest to
    the algebraic quotient. If the result cannot be represented, the behavior
    is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

LIBC_API
lldiv_t
lldiv (
    long long Numerator,
    long long Denominator
    );

/*++

Routine Description:

    This routine divides two long long integers. If the division is inexact,
    the resulting quotient is the integer of lesser magnitude that is nearest
    to the algebraic quotient. If the result cannot be represented, the
    behavior is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

LIBC_API
__NO_RETURN
void
abort (
    void
    );

/*++

Routine Description:

    This routine causes abnormal process termination to occur, unless the
    signal SIGABRT is being caught and the signal handler does not return. The
    abort function shall override ignoring or blocking of the SIGABRT signal.

Arguments:

    None.

Return Value:

    This routine does not return.

--*/

__HIDDEN
int
atexit (
    void (*ExitFunction)(void)
    );

/*++

Routine Description:

    This routine registers a function to be called when the process exits
    normally via a call to exit or a return from main. Calls to exec clear
    the list of registered exit functions. This routine may allocate memory.
    Functions are called in the reverse order in which they were registered.
    If this function is called from within a shared library, then the given
    function will be called when the library is unloaded.

Arguments:

    ExitFunction - Supplies a pointer to the function to call when the
        process exits normally or the shared object is unloaded.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
__cxa_atexit (
    void (*DestructorFunction)(void *),
    void *Argument,
    void *SharedObject
    );

/*++

Routine Description:

    This routine is called to register a global static destructor function.

Arguments:

    DestructorFunction - Supplies a pointer to the function to call.

    Argument - Supplies an argument to pass the function when it is called.

    SharedObject - Supplies a pointer to the shared object this destructor is
        associated with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
__NO_RETURN
void
exit (
    int Status
    );

/*++

Routine Description:

    This routine terminates the current process, calling any routines registered
    to run upon exiting.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

LIBC_API
__NO_RETURN
void
_Exit (
    int Status
    );

/*++

Routine Description:

    This routine terminates the current process. It does not call any routines
    registered to run upon exit.

Arguments:

    Status - Supplies a status code to return to the parent program.

Return Value:

    None. This routine does not return.

--*/

LIBC_API
void
free (
    void *Memory
    );

/*++

Routine Description:

    This routine frees previously allocated memory.

Arguments:

    Memory - Supplies a pointer to memory returned by the allocation function.

Return Value:

    None.

--*/

LIBC_API
void *
malloc (
    size_t AllocationSize
    );

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    AllocationSize - Supplies the required allocation size in bytes.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on failure.

--*/

LIBC_API
void *
realloc (
    void *Allocation,
    size_t AllocationSize
    );

/*++

Routine Description:

    This routine resizes the given buffer. If the new allocation size is
    greater than the original allocation, the contents of the new bytes are
    unspecified (just like the contents of a malloced buffer).

Arguments:

    Allocation - Supplies the optional original allocation. If this is not
        supplied, this routine is equivalent to malloc.

    AllocationSize - Supplies the new required allocation size in bytes.

Return Value:

    Returns a pointer to the new buffer on success.

    NULL on failure or if the supplied size was zero. If the new buffer could
    not be allocated, errno will be set to ENOMEM.

--*/

LIBC_API
void *
calloc (
    size_t ElementCount,
    size_t ElementSize
    );

/*++

Routine Description:

    This routine allocates memory from the heap large enough to store the
    given number of elements of the given size (the product of the two
    parameters). The buffer returned will have all zeros written to it.

Arguments:

    ElementCount - Supplies the number of elements to allocate.

    ElementSize - Supplies the size of each element in bytes.

Return Value:

    Returns a pointer to the new zeroed buffer on success.

    NULL on failure or if the either the element count of the element size was
    zero. If the new buffer could not be allocated, errno will be set to ENOMEM.

--*/

LIBC_API
int
posix_memalign (
    void **AllocationPointer,
    size_t AllocationAlignment,
    size_t AllocationSize
    );

/*++

Routine Description:

    This routine allocates aligned memory from the heap. The given alignment
    must be a power of 2 and a multiple of the size of a pointer.

Arguments:

    AllocationPointer - Supplies a pointer that receives a pointer to the
        allocated memory on success.

    AllocationAlignment - Supplies the required allocation alignment in bytes.

    AllocationSize - Supplies the required allocation size in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
char *
getenv (
    const char *Name
    );

/*++

Routine Description:

    This routine returns the value for the environment variable with the
    given name. This function is neither reentrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the null terminated string containing the name
        of the environment variable to get.

Return Value:

    Returns a pointer to the value of the environment variable on success. This
    memory may be destroyed by subsequent calls to getenv, setenv, or
    unsetenv. The caller does not own it and must not modify or free this
    memory.

--*/

LIBC_API
int
setenv (
    const char *Name,
    const char *Value,
    int Overwrite
    );

/*++

Routine Description:

    This routine sets the value for the given environment variable. This
    function is neither reentrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the null terminated string containing the name
        of the environment variable to set. The routine will fail if this
        string has an equal in it.

    Value - Supplies a pointer to the null terminated string containing the
        value to set for this variable.

    Overwrite - Supplies an integer that if non-zero will cause an existing
        environment variable with the same name to be overwritten. If this is
        zero and the given name exists, the function will return successfully
        but the value will not be changed.

Return Value:

    0 on success.

    -1 on failure, an errno will be set to contain the error code.

--*/

LIBC_API
int
putenv (
    char *String
    );

/*++

Routine Description:

    This routine adds the given string to the environment list.

Arguments:

    String - Supplies a pointer to the null terminated string in the form
        "name=value". This string will become part of the environment, if it
        is modified then that modification will be reflected in the
        environment. The memory supplied in this argument is used directly, so
        the argument must not be automatically allocated. If the given string
        contains no equal sign, then the functionality is equivalent to
        unsetenv with the given string.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
unsetenv (
    const char *Name
    );

/*++

Routine Description:

    This routine removes the environment variable with the given name from
    the current environment. This routine is neither re-entrant nor thread safe.

Arguments:

    Name - Supplies a pointer to the name of the variable to unset. This
        string must not have an equals '=' in it.

Return Value:

    0 on success (whether or not the environment variable previously existed).

    -1 on failure, and errno will be set to contain more information. Errno is
    commonly set to EINVAL if the argument is a null pointer, an empty string,
    or contains an equals.

--*/

LIBC_API
const char *
getexecname (
    void
    );

/*++

Routine Description:

    This routine returns the path name of the executable.

Arguments:

    None.

Return Value:

    Returns a pointer to the pathname of the executable on success. The caller
    must not alter this memory.

--*/

LIBC_API
void *
bsearch (
    const void *Key,
    const void *Base,
    size_t ElementCount,
    size_t ElementSize,
    int (*CompareFunction)(const void *, const void *)
    );

/*++

Routine Description:

    This routine searches an array of sorted objects for one matching the given
    key.

Arguments:

    Key - Supplies a pointer to the element to match against in the given
        array.

    Base - Supplies a pointer to the base of the array to search.

    ElementCount - Supplies the number of elements in the array. Searching an
        element with a count of zero shall return NULL.

    ElementSize - Supplies the size of each element in the array.

    CompareFunction - Supplies a pointer to a function that will be called to
        compare elements. The function takes in two pointers that will point
        to elements within the array. It shall return less than zero if the
        left element is considered less than the right object, zero if the left
        object is considered equal to the right object, and greater than zero
        if the left object is considered greater than the right object.

Return Value:

    Returns a pointer to the element within the array matching the given key.

    NULL if no such element exists or the element count was zero.

--*/

LIBC_API
void
qsort (
    void *ArrayBase,
    size_t ElementCount,
    size_t ElementSize,
    int (*CompareFunction)(const void *, const void *)
    );

/*++

Routine Description:

    This routine sorts an array of items in place using the QuickSort algorithm.

Arguments:

    ArrayBase - Supplies a pointer to the array of items that will get pushed
        around.

    ElementCount - Supplies the number of elements in the array.

    ElementSize - Supplies the size of one of the elements.

    CompareFunction - Supplies a pointer to a function that will be used to
        compare elements. The function takes in two pointers that will point
        within the array. It returns less than zero if the first element is
        less than the second, zero if the first element is equal to the second,
        and greater than zero if the first element is greater than the second.
        The routine must not modify the array itself or inconsistently
        report comparisons, otherwise the sorting will not come out correctly.

Return Value:

    None.

--*/

LIBC_API
int
atoi (
    const char *String
    );

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtol instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

LIBC_API
double
atof (
    const char *String
    );

/*++

Routine Description:

    This routine converts a string to a double floating point value. This
    routine is provided for compatibility with existing applications. New
    applications should use strtod instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        double.

Return Value:

    Returns the floating point representation of the string. If the value could
    not be converted, 0 is returned.

--*/

LIBC_API
long
atol (
    const char *String
    );

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtol instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

LIBC_API
long long
atoll (
    const char *String
    );

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtoll instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

LIBC_API
float
strtof (
    const char *String,
    char **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    float. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        float.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the float was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the float representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
double
strtod (
    const char *String,
    char **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        double.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the double was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
long double
strtold (
    const char *String,
    char **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    long double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        long double.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the long double
        was scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the long double representation of the string. If the value could not
    be converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
long
strtol (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
long long
strtoll (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to EINVAL to indicate the
    number could not be converted.

--*/

LIBC_API
unsigned long
strtoul (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
unsigned long long
strtoull (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to EINVAL to indicate the
    number could not be converted.

--*/

//
// Random number functions.
//

LIBC_API
int
rand (
    void
    );

/*++

Routine Description:

    This routine returns a pseudo-random number.

Arguments:

    None.

Return Value:

    Returns a pseudo-random integer between 0 and RAND_MAX, inclusive.

--*/

LIBC_API
int
rand_r (
    unsigned *Seed
    );

/*++

Routine Description:

    This routine implements the re-entrant and thread-safe version of the
    pseudo-random number generator.

Arguments:

    Seed - Supplies a pointer to the seed to use. This seed will be updated
        to contain the next seed.

Return Value:

    Returns a pseudo-random integer between 0 and RAND_MAX, inclusive.

--*/

LIBC_API
void
srand (
    unsigned Seed
    );

/*++

Routine Description:

    This routine sets the seed for the rand function.

Arguments:

    Seed - Supplies the seed to use.

Return Value:

    None.

--*/

LIBC_API
char *
initstate (
    unsigned int Seed,
    char *State,
    size_t Size
    );

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

LIBC_API
char *
setstate (
    const char *State
    );

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

LIBC_API
void
srandom (
    unsigned int Seed
    );

/*++

Routine Description:

    This routine seeds the non-linear additive feedback random number
    generator. This routine is neither thread-safe nor reentrant.

Arguments:

    Seed - Supplies the seed value to use.

Return Value:

    None.

--*/

LIBC_API
long
random (
    void
    );

/*++

Routine Description:

    This routine returns a random number in the range of 0 to 0x7FFFFFFF,
    inclusive. This routine is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pseudo-random number in the range 0 to 2^32 - 1, inclusive.

--*/

LIBC_API
int
initstate_r (
    unsigned int Seed,
    char *State,
    size_t Size,
    struct random_data *RandomData
    );

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

LIBC_API
int
setstate_r (
    const char *State,
    struct random_data *RandomData
    );

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

LIBC_API
int
srandom_r (
    unsigned int Seed,
    struct random_data *RandomData
    );

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

LIBC_API
int
random_r (
    struct random_data *RandomData,
    int32_t *Result
    );

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

LIBC_API
int
system (
    const char *Command
    );

/*++

Routine Description:

    This routine passes the given command to the command line interpreter. If
    the command is null, this routine determines if the host environment has
    a command processor. The environment of the executed command shall be as if
    the child process were created using the fork function, and then the
    child process invoked execl(<shell path>, "sh", "-c", <command>, NULL).

    This routine will ignore the SIGINT and SIGQUIT signals, and shall block
    the SIGCHLD signal while waiting for the command to terminate.

Arguments:

    Command - Supplies an optional pointer to the command to execute.

Return Value:

    Returns a non-zero value if the command is NULL and a command processor is
    available.

    0 if no command processor is available.

    127 if the command processor could not be executed.

    Otherwise, returns the termination status of the command language
    interpreter.

--*/

LIBC_API
char *
mktemp (
    char *Template
    );

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    filename.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns a pointer to the template string.

--*/

LIBC_API
char *
mkdtemp (
    char *Template
    );

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    directory name, and attempts to create that directory.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns a pointer to the template string.

--*/

LIBC_API
int
mkstemp (
    char *Template
    );

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    filename, and returns an open file descriptor to that file.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns the open file descriptor to the newly created file on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
getpt (
    void
    );

/*++

Routine Description:

    This routine creates and opens a new pseudo-terminal master.

Arguments:

    None.

Return Value:

    Returns a file descriptor to the new terminal master device on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
posix_openpt (
    int Flags
    );

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

LIBC_API
int
grantpt (
    int Descriptor
    );

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

LIBC_API
int
unlockpt (
    int Descriptor
    );

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

LIBC_API
char *
ptsname (
    int Descriptor
    );

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

LIBC_API
int
ptsname_r (
    int Descriptor,
    char *Buffer,
    size_t BufferSize
    );

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

LIBC_API
char *
realpath (
    const char *Path,
    char *ResolvedPath
    );

/*++

Routine Description:

    This routine returns the canonical path for the given file path. This
    canonical path will include no '.' or '..' components, and will not
    contain symbolic links in any components of the path. All path components
    must exist.

Arguments:

    Path - Supplies a pointer to the path to canonicalize.

    ResolvedPath - Supplies an optional pointer to the buffer to place the
        resolved path in. This must be at least PATH_MAX bytes.

Return Value:

    Returns a pointer to the resolved path on success.

    NULL on failure.

--*/

LIBC_API
int
getloadavg (
    double LoadAverage[],
    int ElementCount
    );

/*++

Routine Description:

    This routine returns the number of processes in the system run queue
    averaged over various periods of time. The elements returned (up to three)
    return the number of processes over the past one, five, and fifteen minutes.

Arguments:

    LoadAverage - Supplies a pointer where up to three load average values
        will be returned on success.

    ElementCount - Supplies the number of elements in the supplied array.

Return Value:

    Returns the number of elements returned on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

