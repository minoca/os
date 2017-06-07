/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtl.h

Abstract:

    This header contains definitions for the common kernel runtime library.

Author:

    Evan Green 24-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdarg.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads a 64 bit value on a 32 bit processor whose value is updated
// asynchronously. The value must be updated with the equivalent write macro.
// The first argument is a pointer to the INT64_SYNC structure, and the second
// is the address of the result to write it in to.
//

#define READ_INT64_SYNC(_Pointer, _Result)                                    \
    do {                                                                      \
        *(volatile ULONGLONG *)(_Result) =                                    \
                                        (ULONGLONG)((_Pointer)->High1) << 32; \
                                                                              \
        *(volatile ULONGLONG *)(_Result) |= (ULONGLONG)((_Pointer)->Low);     \
                                                                              \
    } while ((_Pointer)->High2 != (*(_Result) >> 32));

//
// This macro writes a 64 bit value on a 32 bit processor where readers are
// simultaneously reading it. It updates the first high value, then the low
// value, then the second high value to ensure that readers cannot observe a
// torn read. The first parameter is a pointer to the INT64_SYNC structure,
// the second parameter is the value to write.
//

#define WRITE_INT64_SYNC(_Pointer, _Value)                                    \
    (_Pointer)->High1 = (ULONGLONG)(_Value) >> 32;                            \
    (_Pointer)->Low = (ULONG)(_Value);                                        \
    (_Pointer)->High2 = (ULONGLONG)(_Value) >> 32;                            \

//
// This macro determines if the given year is a leap year. There's a leap year
// every 4 years, except there's not a leap year every 100 years, except
// there is a leap year every 400 years. Fun stuff.
//

#define IS_LEAP_YEAR(_Year) \
    ((((_Year) % 4) == 0) && ((((_Year) % 100) != 0) || (((_Year) % 400) == 0)))

//
// Character classification definitions
//

//
// This macro returns non-zero if the given character is an upper case
// character.
//

#define RtlIsCharacterUpperCase(_Character) \
    (((_Character) >= 'A') && ((_Character) <= 'Z'))

//
// This macro returns non-zero if the given character is a lower case character.
//

#define RtlIsCharacterLowerCase(_Character) \
    (((_Character) >= 'a') && ((_Character) <= 'z'))

//
// This macro returns non-zero if the given character is a digit.
//

#define RtlIsCharacterDigit(_Character) \
    (((_Character) >= '0') && ((_Character) <= '9'))

//
// This macro returns non-zero if the given character is in the alphabet.
//

#define RtlIsCharacterAlphabetic(_Character) \
    ((RtlIsCharacterUpperCase(_Character) || \
      RtlIsCharacterLowerCase(_Character)))

//
// This macro returns non-zero if the given character is alpha-numeric.
//

#define RtlIsCharacterAlphanumeric(_Character) \
    ((RtlIsCharacterAlphabetic(_Character) || RtlIsCharacterDigit(_Character)))

//
// This macro returns non-zero if the given character is in the ASCII character
// set.
//

#define RtlIsCharacterAscii(_Character) (((_Character) & (~0x7F)) == 0)

//
// This macro returns non-zero if the given character is a blank character.
//

#define RtlIsCharacterBlank(_Character) \
    (((_Character) == ' ') || ((_Character) == '\t'))

//
// This macro returns non-zero if the given character is a control character.
//

#define RtlIsCharacterControl(_Character) \
    (((_Character) < ' ') || ((_Character) == 0x7F))

//
// This macro returns non-zero if the given character is whitespace.
//

#define RtlIsCharacterSpace(_Character)                      \
    (((_Character) == ' ') || ((_Character) == '\t') ||      \
     ((_Character) == '\n') || ((_Character) == '\r') ||     \
     ((_Character) == '\f') || ((_Character) == '\v'))

//
// This macro returns non-zero if the given character is a hexadecimal digit.
//

#define RtlIsCharacterHexDigit(_Character)                   \
    ((((_Character) >= '0') && ((_Character) <= '9')) ||     \
     (((_Character) >= 'A') && ((_Character) <= 'F')) ||     \
     (((_Character) >= 'a') && ((_Character) <= 'f')))

//
// This macro returns non-zero if the given character is punctuation.
//

#define RtlIsCharacterPunctuation(_Character)                \
    ((RtlIsCharacterPrintable(_Character)) &&                \
     (!RtlIsCharacterAlphanumeric(_Character)) &&            \
     ((_Character) != ' '))

//
// This macro returns non-zero if the given character is a graphical character.
//

#define RtlIsCharacterGraphical(_Character)      \
    ((RtlIsCharacterAlphanumeric(_Character)) || \
     (RtlIsCharacterPunctuation(_Character)))

//
// This macro returns non-zero if the given character is a printable character.
//

#define RtlIsCharacterPrintable(_Character)         \
    ((RtlIsCharacterAlphanumeric(_Character)) ||    \
     (RtlIsCharacterPunctuation(_Character)) ||     \
     ((_Character) == ' '))

//
// This macro converts the given character into an ASCII character.
//

#define RtlConvertCharacterToAscii(_Character) ((_Character) & 0x7F)

//
// This macro converts the given character into a lower case character.
//

#define RtlConvertCharacterToLowerCase(_Character) \
    (RtlIsCharacterUpperCase(_Character) ? ((_Character) | 0x20) : (_Character))

//
// This macro converts the given character into an upper case character.
//

#define RtlConvertCharacterToUpperCase(_Character)  \
    (RtlIsCharacterLowerCase(_Character) ?          \
     ((_Character) & (~0x20)) : (_Character))

//
// Wide character classification definitions
//

//
// This macro returns non-zero if the given character is an upper case
// character.
//

#define RtlIsCharacterUpperCaseWide(_Character) \
    (((_Character) >= L'A') && ((_Character) <= L'Z'))

//
// This macro returns non-zero if the given character is a lower case character.
//

#define RtlIsCharacterLowerCaseWide(_Character) \
    (((_Character) >= L'a') && ((_Character) <= L'z'))

//
// This macro returns non-zero if the given character is a digit.
//

#define RtlIsCharacterDigitWide(_Character) \
    (((_Character) >= L'0') && ((_Character) <= L'9'))

//
// This macro returns non-zero if the given character is in the alphabet.
//

#define RtlIsCharacterAlphabeticWide(_Character) \
    ((RtlIsCharacterUpperCaseWide(_Character) || \
      RtlIsCharacterLowerCaseWide(_Character)))

//
// This macro returns non-zero if the given character is alpha-numeric.
//

#define RtlIsCharacterAlphanumericWide(_Character)  \
    ((RtlIsCharacterAlphabeticWide(_Character) ||   \
     RtlIsCharacterDigitWide(_Character)))

//
// This macro returns non-zero if the given character is in the ASCII character
// set.
//

#define RtlIsCharacterAsciiWide(_Character) (((_Character) & (~0x7F)) == 0)

//
// This macro returns non-zero if the given character is a blank character.
//

#define RtlIsCharacterBlankWide(_Character) \
    (((_Character) == L' ') || ((_Character) == L'\t'))

//
// This macro returns non-zero if the given character is a control character.
//

#define RtlIsCharacterControlWide(_Character) \
    (((_Character) < L' ') || ((_Character) == 0x7F))

//
// This macro returns non-zero if the given character is whitespace.
//

#define RtlIsCharacterSpaceWide(_Character)                         \
    (((_Character) == L' ') || ((_Character) == L'\t') ||           \
     ((_Character) == L'\n') || ((_Character) == L'\r') ||          \
     ((_Character) == L'\f') || ((_Character) == L'\v'))

//
// This macro returns non-zero if the given character is a hexadecimal digit.
//

#define RtlIsCharacterHexDigitWide(_Character)                      \
    ((((_Character) >= L'0') && ((_Character) <= L'9')) ||          \
     (((_Character) >= L'A') && ((_Character) <= L'F')) ||          \
     (((_Character) >= L'a') && ((_Character) <= L'f')))

//
// This macro returns non-zero if the given character is punctuation.
//

#define RtlIsCharacterPunctuationWide(_Character)                \
    ((RtlIsCharacterPrintableWide(_Character)) &&                \
     (!RtlIsCharacterAlphanumericWide(_Character)) &&            \
     ((_Character) != L' '))

//
// This macro returns non-zero if the given character is a graphical character.
//

#define RtlIsCharacterGraphicalWide(_Character)      \
    ((RtlIsCharacterAlphanumericWide(_Character)) || \
     (RtlIsCharacterPunctuationWide(_Character)))

//
// This macro returns non-zero if the given character is a printable character.
//

#define RtlIsCharacterPrintableWide(_Character)         \
    ((RtlIsCharacterAlphanumericWide(_Character)) ||    \
     (RtlIsCharacterPunctuationWide(_Character)) ||     \
     ((_Character) == L' '))

//
// This macro converts the given character into an ASCII character.
//

#define RtlConvertCharacterToAsciiWide(_Character) ((_Character) & 0x7F)

//
// This macro converts the given character into a lower case character.
//

#define RtlConvertCharacterToLowerCaseWide(_Character)  \
    (RtlIsCharacterUpperCaseWide(_Character) ?          \
     ((_Character) | 0x20) : (_Character))

//
// This macro converts the given character into an upper case character.
//

#define RtlConvertCharacterToUpperCaseWide(_Character)  \
    (RtlIsCharacterLowerCaseWide(_Character) ?          \
     ((_Character) & (~0x20)) : (_Character))

//
// ---------------------------------------------------------------- Definitions
//

#ifndef RTL_API

#define RTL_API __DLLIMPORT

#endif

#define STRING_TERMINATOR '\0'
#define WIDE_STRING_TERMINATOR L'\0'

//
// Define the maximum number of bytes in a multibyte character.
//

#define MULTIBYTE_MAX 16

//
// Define the number of characters in the scan unput buffer. This must be as
// large as both a DOUBLE_SCAN_STRING_SIZE and MULTIBYTE_MAX.
//

#define SCANNER_UNPUT_SIZE 16

//
// Defines the length of a string printing a UUID, not including the NULL
// terminator.
//

#define UUID_STRING_LENGTH 37

//
// Define time unit constants.
//

#define NANOSECONDS_PER_SECOND 1000000000ULL
#define MICROSECONDS_PER_SECOND 1000000ULL
#define MILLISECONDS_PER_SECOND 1000ULL
#define MICROSECONDS_PER_MILLISECOND 1000ULL
#define NANOSECONDS_PER_MICROSECOND 1000ULL
#define NANOSECONDS_PER_MILLISECOND 1000000ULL

//
// Define some constants used for manipulating float types.
//

#define FLOAT_SIGN_BIT 0x80000000
#define FLOAT_SIGN_BIT_SHIFT 31
#define FLOAT_NAN 0x7F800000
#define FLOAT_NAN_EXPONENT 0xFF
#define FLOAT_VALUE_MASK 0x007FFFFFUL
#define FLOAT_EXPONENT_MASK 0x7F800000UL
#define FLOAT_EXPONENT_SHIFT 23
#define FLOAT_EXPONENT_BIAS 0x7F
#define FLOAT_ONE_WORD 0x3F800000
#define FLOAT_TRUNCATE_VALUE_MASK 0xFFFFF000

//
// Define some constants used for manipulating double floating point types.
//

#define DOUBLE_SIGN_BIT 0x8000000000000000ULL
#define DOUBLE_SIGN_BIT_SHIFT 63
#define DOUBLE_EXPONENT_MASK 0x7FF0000000000000ULL
#define DOUBLE_EXPONENT_SHIFT 52
#define DOUBLE_EXPONENT_BIAS 0x3FF
#define DOUBLE_NAN_EXPONENT 0x7FF
#define DOUBLE_VALUE_MASK 0x000FFFFFFFFFFFFFULL
#define DOUBLE_HIGH_WORD_SHIFT (sizeof(ULONG) * BITS_PER_BYTE)
#define DOUBLE_SIGNIFICAND_HEX_DIGITS 13
#define DOUBLE_ONE_HIGH_WORD 0x3FF00000
#define DOUBLE_HIGH_VALUE_MASK 0x000FFFFF
#define NAN_HIGH_WORD 0x7FF00000

//
// Define macros to functions that will work on the native integer size of the
// processors.
//

#if defined(__amd64)

#define RtlAtomicExchange(_Pointer, _Value) \
    RtlAtomicExchange64((PULONGLONG)(_Pointer), (_Value))

#define RtlAtomicCompareExchange(_Pointer, _Exchange, _Compare) \
    RtlAtomicCompareExchange64((PULONGLONG)(_Pointer), \
                               (_Exchange), \
                               (_Compare))

#define RtlAtomicAdd(_Pointer, _Value) \
    RtlAtomicAdd64((PULONGLONG)(_Pointer), (_Value))

#define RtlAtomicOr(_Pointer, _Value) \
    RtlAtomicAdd64((PULONGLONG)(_Pointer), (_Value))

#define RtlCountLeadingZeros RtlCountLeadingZeros64
#define RtlCountTrailingZeros RtlCountTrailingZeros64

#else

#define RtlAtomicExchange(_Pointer, _Value) \
    RtlAtomicExchange32((PULONG)(_Pointer), (_Value))

#define RtlAtomicCompareExchange(_Pointer, _Exchange, _Compare) \
    RtlAtomicCompareExchange32((PULONG)(_Pointer), \
                               (_Exchange), \
                               (_Compare))

#define RtlAtomicAdd(_Pointer, _Value) \
    RtlAtomicAdd32((PULONG)(_Pointer), (_Value))

#define RtlAtomicOr(_Pointer, _Value) \
    RtlAtomicAdd32((PULONG)(_Pointer), (_Value))

#define RtlCountLeadingZeros RtlCountLeadingZeros32
#define RtlCountTrailingZeros RtlCountTrailingZeros32

#endif

#define DOUBLE_SCAN_STRING_SIZE 8

//
// Define some time constants.
//

#define SECONDS_PER_DAY (SECONDS_PER_HOUR * HOURS_PER_DAY)
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE * MINUTES_PER_HOUR)
#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define DAYS_PER_WEEK 7
#define DAYS_PER_YEAR 365
#define DAYS_PER_LEAP_YEAR 366
#define MONTHS_PER_YEAR 12
#define YEARS_PER_CENTURY 100

#define TIME_ZONE_ABBREVIATION_SIZE 5

//
// Define the difference between the system time epoch, January 1, 2001, and
// the standard C library epoch, January 1, 1970.
//

#define SYSTEM_TIME_TO_EPOCH_DELTA (978307200LL)

//
// Define memory heap flags.
//

//
// Set this flag to enable allocation tag statistics collection.
//

#define MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS 0x00000001

//
// Set this flag to validate the heap periodically.
//

#define MEMORY_HEAP_FLAG_PERIODIC_VALIDATION 0x00000002

//
// Set this flag to disallow partial frees of underlying regions the heap has
// allocated. Normally the heap assumes an interface where the heap can
// allocate a bunch of memory and then free it in piecemeal if it likes.
// Setting this flag forces the heap to only free complete areas it has
// allocated.
//

#define MEMORY_HEAP_FLAG_NO_PARTIAL_FREES 0x00000004

//
// Define the number of small bins to use in the memory heap. Each bin holds
// chunks with equal sizes, spaced 8 apart, less than HEAP_MIN_LARGE_SIZE
// bytes.
//

#define HEAP_SMALL_BIN_COUNT 32U

//
// Define the number of heap bins to use in the heap. Each bin contains the
// root of a tree holding a range of sizes. There are 2 equally spaced tree
// bins for each power of two from HEAP_TREE_SHIFT to HEAP_TREE_SHIFT + 16. The
// last bin holds anything larger.
//

#define HEAP_TREE_BIN_COUNT 32U

//
// Define Red-Black tree flags.
//

#define RED_BLACK_TREE_FLAG_PERIODIC_VALIDATION 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _COMPARISON_RESULT {
    ComparisonResultInvalid,
    ComparisonResultSame,
    ComparisonResultAscending,
    ComparisonResultDescending,
} COMPARISON_RESULT, *PCOMPARISON_RESULT;

typedef enum _CHARACTER_ENCODING {
    CharacterEncodingDefault,
    CharacterEncodingAscii,
    CharacterEncodingMax
} CHARACTER_ENCODING, *PCHARACTER_ENCODING;

typedef enum _HEAP_CORRUPTION_CODE {
    HeapCorruptionInvalid,
    HeapCorruptionBufferOverrun,
    HeapCorruptionDoubleFree,
    HeapCorruptionCorruptStructures,
    HeapCorruptionDoubleDestroy
} HEAP_CORRUPTION_CODE, *PHEAP_CORRUPTION_CODE;

//
// TODO: Make this just an integer on x64.
//

typedef struct _INT64_SYNC {
    volatile ULONG High1;
    volatile ULONG Low;
    volatile ULONG High2;
} INT64_SYNC, *PINT64_SYNC;

typedef struct _RED_BLACK_TREE_NODE RED_BLACK_TREE_NODE, *PRED_BLACK_TREE_NODE;
typedef struct _RED_BLACK_TREE RED_BLACK_TREE, *PRED_BLACK_TREE;

typedef
COMPARISON_RESULT
(*PCOMPARE_RED_BLACK_TREE_NODES) (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

/*++

Routine Description:

    This routine compares two Red-Black tree nodes.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

typedef
VOID
(*PRED_BLACK_TREE_ITERATION_ROUTINE) (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called once for each node in the tree (via an in order
    traversal). It assumes that the tree will not be modified during the
    traversal.

Arguments:

    Tree - Supplies a pointer to the tree being enumerated.

    Node - Supplies a pointer to the node.

    Level - Supplies the depth into the tree that this node exists at. 0 is
        the root.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a node of a Red-Black tree node.

Members:

    Red - Stores a boolean indicating if the node is colored red (TRUE) or
        black (FALSE).

    LeftChild - Stores a pointer to the left child of this node.

    RightChild - Stores a pointer to the right child of this node.

    Parent - Stores a pointer to the parent of this node.

--*/

struct _RED_BLACK_TREE_NODE {
    BOOL Red;
    PRED_BLACK_TREE_NODE LeftChild;
    PRED_BLACK_TREE_NODE RightChild;
    PRED_BLACK_TREE_NODE Parent;
};

/*++

Structure Description:

    This structure defines a Red-Black Tree.

Members:

    Flags - Stores a bitfield of flags about the tree. See
        RED_BLACK_TREE_FLAG_* definitions.

    CompareFunction - Stores a pointer to a function used to compare two nodes.

    Root - Stores the root node of the tree, which is a sentinal node. The real
        root is the left child of this guy.

    NullNode - Stores a NULL node, a sentinal, used to avoid tons of NULL checks
        throughout the algorithm. All leaf nodes point to this NULL node.

    CallCount - Stores the total number of calls to insert or delete.

--*/

struct _RED_BLACK_TREE {
    ULONG Flags;
    PCOMPARE_RED_BLACK_TREE_NODES CompareFunction;
    RED_BLACK_TREE_NODE Root;
    RED_BLACK_TREE_NODE NullNode;
    ULONG CallCount;
};

/*++

Structure Description:

    This structure stores character encoding state used to convert multibyte
    characters to wide characters. Callers should treat this structure as
    opaque and not access members directly, as they are likely to change or
    be removed without notice.

Members:

    Encoding - Stores the character encoding type.

--*/

typedef struct _MULTIBYTE_STATE {
    CHARACTER_ENCODING Encoding;
} MULTIBYTE_STATE, *PMULTIBYTE_STATE;

typedef struct _PRINT_FORMAT_CONTEXT
    PRINT_FORMAT_CONTEXT, *PPRINT_FORMAT_CONTEXT;

typedef
BOOL
(*PPRINT_FORMAT_WRITE_CHARACTER) (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes a character to the output during a printf-style
    formatting operation.

Arguments:

    Character - Supplies the character to be written.

    Context - Supplies a pointer to the printf-context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

/*++

Structure Description:

    This structure defines the context handed to a printf format function.

Members:

    WriteCharacter - Stores a pointer to a function used to write a
        character to the destination of the formatted string operation. Usually
        this is a file or string.

    Context - Stores a pointer's worth of additional context. This pointer is
        not touched by the format string function, it's generally used inside
        the write character routine.

    Limit - Stores the maximum number of characters that should be sent to
        the write character routine. If this value is zero, an unlimited number
        of characters will be sent to the write character routine.

    CharactersWritten - Stores the number of characters that have been
        written so far. The print format routine will increment this after each
        successful byte is written.

    State - Stores the multibyte character state.

--*/

struct _PRINT_FORMAT_CONTEXT {
    PPRINT_FORMAT_WRITE_CHARACTER WriteCharacter;
    PVOID Context;
    ULONG Limit;
    ULONG CharactersWritten;
    MULTIBYTE_STATE State;
};

typedef struct _SCAN_INPUT SCAN_INPUT, *PSCAN_INPUT;

typedef
BOOL
(*PSCANNER_GET_INPUT) (
    PSCAN_INPUT Input,
    PCHAR Character
    );

/*++

Routine Description:

    This routine retrieves another byte of input from the input scanner.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE if a character was read.

    FALSE if the end of the file or string was encountered.

--*/

typedef
BOOL
(*PSCANNER_GET_INPUT_WIDE) (
    PSCAN_INPUT Input,
    PWCHAR Character
    );

/*++

Routine Description:

    This routine retrieves another wide character of input from the input
    scanner.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE if a character was read.

    FALSE if the end of the file or string was encountered.

--*/

/*++

Structure Description:

    This structure defines a scanner input structure which is used to get input
    from either a string or file pointer.

Members:

    GetInput - Stores a pointer to the function called to get another character.

    String - Stores a pointer to the string for string driven inputs.

    WideString - Stores a pointer to the wide string for string driven inputs.

    Context - Stores a context pointer associated with this scan operation
        (such as a stream pointer).

    StringSize - Stores the size of the string for string driven inputs.

    CharactersRead - Stores the number of characters read.

    UnputCharacters - Stores a buffer of characters that were put back into the
        input buffer.

    ValidUnputCharacters - Stores the number of valid unput characters in the
        buffer.

    State - Stores the multibyte state.

--*/

struct _SCAN_INPUT {
    union {
        PSCANNER_GET_INPUT GetInput;
        PSCANNER_GET_INPUT_WIDE GetInputWide;
    } ReadU;

    union {
        PCSTR String;
        PCWSTR WideString;
        PVOID Context;
    } DataU;

    ULONG StringSize;
    ULONG CharactersRead;
    WCHAR UnputCharacters[SCANNER_UNPUT_SIZE];
    ULONG ValidUnputCharacters;
    MULTIBYTE_STATE State;
};

/*++

Structure Description:

    This union allows the user to pick at the bits inside of an unsigned long
    long.

Members:

    Ulong - Stores the parts in ULONGs.

    Ulonglong - Stores the bits of the 64 bit double.

--*/

typedef union _ULONGLONG_PARTS {
    struct {
        ULONG Low;
        ULONG High;
    } Ulong;

    ULONGLONG Ulonglong;
} ULONGLONG_PARTS, *PULONGLONG_PARTS;

/*++

Structure Description:

    This union allows the user to pick at the bits inside of a double.

Members:

    Double - Stores the value in its natural floating point form.

    Ulong - Stores the parts of the double in ULONGs.

    Ulonglong - Stores the bits of the 64 bit double.

--*/

typedef union _DOUBLE_PARTS {
    double Double;
    struct {
        ULONG Low;
        ULONG High;
    } Ulong;

    ULONGLONG Ulonglong;
} DOUBLE_PARTS, *PDOUBLE_PARTS;

/*++

Structure Description:

    This union allows the user to pick at the bits inside of a float.

Members:

    Float - Stores the value in its natural floating point form.

    Ulong - Stores the bits of the float.

--*/

typedef union _FLOAT_PARTS {
    float Float;
    ULONG Ulong;
} FLOAT_PARTS, *PFLOAT_PARTS;

typedef
VOID
(*PTIME_ZONE_LOCK_FUNCTION) (
    );

/*++

Routine Description:

    This routine implements an acquire or release of the lock responsible for
    guarding the global time zone data.

Arguments:

    None.

Return Value:

    None.

--*/

typedef
PVOID
(*PTIME_ZONE_REALLOCATE_FUNCTION) (
    PVOID Memory,
    UINTN NewSize
    );

/*++

Routine Description:

    This routine allocates, reallocates, or frees memory for the time zone
    library.

Arguments:

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

/*++

Structure Description:

    This structure describes the system's concept of calendar time, which is
    represented as the number of seconds since midnight January 1, 2001 GMT.

Members:

    Seconds - Stores the number of seconds since midnight January 1, 2001 GMT.

    Nanoseconds - Stores the nanoseconds portion of the time. This is usually
        a positive number, even for negative seconds values.

--*/

typedef struct _SYSTEM_TIME {
    LONGLONG Seconds;
    LONG Nanoseconds;
} SYSTEM_TIME, *PSYSTEM_TIME;

/*++

Structure Description:

    This structure contains the broken down fields of a calendar date.

Members:

    Year - Stores the year. Valid values are between 1 and 9999.

    Month - Stores the month. Valid values are between 0 and 11.

    Day - Stores the day of the month. Valid values are between 1 and 31.

    Hour - Stores the hour. Valid values are between 0 and 23.

    Minute - Stores the minute. Valid values are between 0 and 59.

    Second - Stores the second. Valid values are between 0 and 59. Arguably
        with leap seconds 60 is a valid value too, but the time functions will
        all roll that over into the next minute.

    Nanosecond - Stores the nanosecond. Valid values are between 0 and
        999,999,999.

    Weekday - Stores the day of the week. Valid values are between 0 and 6,
        with 0 being Sunday and 6 being Saturday.

    YearDay - Stores the day of the year. Valid values are between 0 and 365.

    IsDaylightSaving - Stores a value indicating if the given time is
        represented in daylight saving time. Usually 0 indicates standard
        time, 1 indicates daylight saving time, and -1 indicates "unknown".

    GmtOffset - Stores the offset from Greenwich Mean Time in seconds that
        this time is interpreted in.

    TimeZone - Stores a pointer to a string containing the time zone name.

--*/

typedef struct _CALENDAR_TIME {
    LONG Year;
    LONG Month;
    LONG Day;
    LONG Hour;
    LONG Minute;
    LONG Second;
    LONG Nanosecond;
    LONG Weekday;
    LONG YearDay;
    LONG IsDaylightSaving;
    LONG GmtOffset;
    PCSTR TimeZone;
} CALENDAR_TIME, *PCALENDAR_TIME;

typedef struct _MEMORY_HEAP MEMORY_HEAP, *PMEMORY_HEAP;

typedef
PVOID
(*PHEAP_ALLOCATE) (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
BOOL
(*PHEAP_FREE) (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the heap wants to release space it had
    previously been allocated.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the size of the allocation to free.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed.

--*/

typedef
VOID
(*PHEAP_CORRUPTION_ROUTINE) (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    );

/*++

Routine Description:

    This routine is called when the heap detects internal corruption.

Arguments:

    Heap - Supplies a pointer to the heap containing the corruption.

    Code - Supplies the code detailing the problem.

    Parameter - Supplies an optional parameter pointing at a problem area.

Return Value:

    None. This routine probably shouldn't return.

--*/

/*++

Structure Description:

    This structure defines statistics for one allocation tag.

Members:

    Node - Stores the Red-Black tree node information for this allocation
        statistic.

    Tag - Stores the allocation tag associated with this statistic.

    LargestAllocation - Stores the largest single allocation ever made under
        this tag, in bytes.

    ActiveSize - Stores the total number of bytes currently allocated under
        this tag.

    LargestActiveSize - Stores the largest number of bytes the active size has
        ever been.

    LifetimeAllocationSize - Stores the total number of bytes that have been
        allocated under this tag (not necessarily all at once).

    ActiveAllocationCount - Stores the current number of allocations under this
        allocation tag.

    LargestActiveAllocationCount - Stores the largest number the active
        allocation count has ever been for this tag.

--*/

typedef struct _MEMORY_HEAP_TAG_STATISTIC {
    RED_BLACK_TREE_NODE Node;
    ULONG Tag;
    ULONG LargestAllocation;
    ULONGLONG ActiveSize;
    ULONGLONG LargestActiveSize;
    ULONGLONG LifetimeAllocationSize;
    ULONG ActiveAllocationCount;
    ULONG LargestActiveAllocationCount;
} MEMORY_HEAP_TAG_STATISTIC, *PMEMORY_HEAP_TAG_STATISTIC;

/*++

Structure Description:

    This structure defines heap-wide memory heap statistics.

Members:

    TotalHeapSize - Stores the total size of the memory heap, in bytes.

    MaxHeapSize - Stores the maximum size the heap has ever been.

    FreeListSize - Stores the amount of free memory in the heap, in bytes.

    DirectAllocationSize - Stores the number of bytes of user allocations that
        were transparently allocated from the underlying allocator, and not
        within a heap segment.

    Allocations - Stores the number of outstanding allocations.

    TotalAllocationCalls - Stores the number of calls to allocate memory since
        the heap's initialization.

    FailedAllocations - Stores the number of calls to allocate memory that
        have been failed.

    TotalFreeCalls - Stores the number of calls to free memory since the heap's
        initialization.

--*/

typedef struct _MEMORY_HEAP_STATISTICS {
    UINTN TotalHeapSize;
    UINTN MaxHeapSize;
    UINTN FreeListSize;
    UINTN DirectAllocationSize;
    UINTN Allocations;
    UINTN TotalAllocationCalls;
    UINTN FailedAllocations;
    UINTN TotalFreeCalls;
} MEMORY_HEAP_STATISTICS, *PMEMORY_HEAP_STATISTICS;

/*++

Structure Description:

    This structure defines optionally collected memory heap tag statistics.

Members:

    Tree - Stores a pointer to the Red-Black tree storing the tags and their
        frequencies.

    StatisticEntry - Stores the built in entry for the statistic tag itself, to
        avoid infinite recursion.

    TagCount - Stores the number of unique tags that have been used for
        allocations.

--*/

typedef struct _MEMORY_HEAP_TAG_STATISTICS {
    RED_BLACK_TREE Tree;
    MEMORY_HEAP_TAG_STATISTIC StatisticEntry;
    UINTN TagCount;
} MEMORY_HEAP_TAG_STATISTICS, *PMEMORY_HEAP_TAG_STATISTICS;

typedef UINTN HEAP_BINMAP, *PHEAP_BINMAP;
typedef UINTN HEAP_BINDEX, *PHEAP_BINDEX;
typedef struct _HEAP_CHUNK HEAP_CHUNK, *PHEAP_CHUNK;
typedef struct _HEAP_TREE_CHUNK HEAP_TREE_CHUNK, *PHEAP_TREE_CHUNK;
typedef struct _HEAP_SEGMENT HEAP_SEGMENT, *PHEAP_SEGMENT;

/*++

Structure Description:

    This structure defines a segment of memory that the heap owns for carving
    up into allocations.

Members:

    Base - Stores the base address of the segment.

    Size - Stores the size of the segment.

    Next - Stores a pointer to the next segment.

    Flags - Stores a bitfield of flags describing the segment.

--*/

struct _HEAP_SEGMENT {
    CHAR *Base;
    UINTN Size;
    PHEAP_SEGMENT Next;
    ULONG Flags;
};

/*++

Structure Description:

    This structure defines a memory heap.

Members:

    Magic - Stores a magic number, HEAP_MAGIC.

    Flags - Stores a bitfield of flags governing the behavior of the heap. See
        MEMORY_HEAP_FLAG_* definitions.

    AllocateFunction - Stores a pointer to a function called when the heap
        wants to expand and needs to allocate more memory.

    FreeFunction - Stores a pointer to a function called when the heap wants
        to a free a previously allocated region of memory.

    CorruptionFunction - Stores a pointer to a function to call if heap
        corruption is detected.

    AllocationTag - Stores the magic number to put into the magic field of
        each allocation. This is also the tag that gets passed to the
        allocation routine when expanding the heap.

    MinimumExpansionSize - Stores the minimum size, in bytes, for a heap
        expansion.

    ExpansionGranularity - Stores the granularity of the expansion size. This
        must be a power of two.

    ExpansionSize - Stores the size of the previous expansion.

    PreviousExpansionSize - Stores the size of the last expansion, for
        expansion doubling.

    Statistics - Stores heap-wide statistics.

    TagStatistics - Stores the tag statistics structure.

    SmallMap - Stores the bitmap of small bins that have valid entries in them.

    TreeMap - Stores the bitmap of tree bins that have valid entries in them.

    DirectAllocationThreshold - Stores the threshold value in bytes above which
        the heap just directly turns around and calls the allocator underneath.

    DesignatedVictimSize - Stores the size of the designated victim entry.

    TopSize - Stores the size of the heap wilderness.

    LeastAddress - Stores the lowest address the heap manages, or NULL if
        the heap has no memory at all.

    DesignatedVictim - Stores a pointer to the designated victim to carve
        memory out of.

    Top - Stores a pointer to the wilderness, the free region bordering the
        highest address the heap has known.

    TrimCheck - Stores the threshold value for the top. If the top wilderness
        exceeds this size, then the heap will be trimmed down to a more
        reasonable size.

    ReleaseChecks - Stores a countdown counter that is decremented during large
        free operations. When it reaches zero, any segments that are completely
        free are released back to the system, and the counter is reloaded.

    SmallBins - Stores the array of small bins where little free fragments are
        stored.

    TreeBins - Stores the array of trees where larger free fragments are
        stored.

    FootprintLimit - Stores a value that if non-zero limits the size that the
        heap can grow to.

    Segment - Stores the built-in first memory segment of the heap.

--*/

struct _MEMORY_HEAP {
    UINTN Magic;
    ULONG Flags;
    PHEAP_ALLOCATE AllocateFunction;
    PHEAP_FREE FreeFunction;
    PHEAP_CORRUPTION_ROUTINE CorruptionFunction;
    UINTN AllocationTag;
    UINTN MinimumExpansionSize;
    UINTN ExpansionGranularity;
    UINTN PreviousExpansionSize;
    MEMORY_HEAP_STATISTICS Statistics;
    MEMORY_HEAP_TAG_STATISTICS TagStatistics;
    HEAP_BINMAP SmallMap;
    HEAP_BINMAP TreeMap;
    UINTN DirectAllocationThreshold;
    UINTN DesignatedVictimSize;
    UINTN TopSize;
    PCHAR LeastAddress;
    PHEAP_CHUNK DesignatedVictim;
    PHEAP_CHUNK Top;
    UINTN TrimCheck;
    UINTN ReleaseChecks;
    PHEAP_CHUNK SmallBins[HEAP_SMALL_BIN_COUNT * 2];
    PHEAP_TREE_CHUNK TreeBins[HEAP_TREE_BIN_COUNT];
    UINTN FootprintLimit;
    HEAP_SEGMENT Segment;
};

typedef enum _SYSTEM_RELEASE_LEVEL {
    SystemReleaseInvalid,
    SystemReleaseDevelopment,
    SystemReleasePreAlpha,
    SystemReleaseAlpha,
    SystemReleaseBeta,
    SystemReleaseCandidate,
    SystemReleaseFinal,
    SystemReleaseLevelCount
} SYSTEM_RELEASE_LEVEL, *PSYSTEM_RELEASE_LEVEL;

typedef enum _SYSTEM_BUILD_DEBUG_LEVEL {
    SystemBuildInvalid,
    SystemBuildDebug,
    SystemBuildRelease,
    SystemBuildDebugLevelCount
} SYSTEM_BUILD_DEBUG_LEVEL, *PSYSTEM_BUILD_DEBUG_LEVEL;

typedef enum _SYSTEM_VERSION_STRING_VERBOSITY {
    SystemVersionStringMajorMinorOnly,
    SystemVersionStringBasic,
    SystemVersionStringComplete,
} SYSTEM_VERSION_STRING_VERBOSITY, *PSYSTEM_VERSION_STRING_VERBOSITY;

/*++

Structure Description:

    This structure describes the system version information.

Members:

    MajorVersion - Stores the major version number.

    MinorVersion - Stores the minor version number.

    Revision - Stores the sub-minor version number.

    SerialVersion - Stores the globally increasing system version number. This
        value will always be greater than any previous builds.

    ReleaseLevel - Stores the release level of the build.

    DebugLevel - Stores the debug compilation level of the build.

    BuildTime - Stores the system build time.

    ProductName - Stores a pointer to a string containing the name of the
        product.

    BuildString - Stores an optional pointer to a string containing an
        identifier string for this build.

--*/

typedef struct _SYSTEM_VERSION_INFORMATION {
    USHORT MajorVersion;
    USHORT MinorVersion;
    USHORT Revision;
    ULONGLONG SerialVersion;
    SYSTEM_RELEASE_LEVEL ReleaseLevel;
    SYSTEM_BUILD_DEBUG_LEVEL DebugLevel;
    SYSTEM_TIME BuildTime;
    PSTR ProductName;
    PSTR BuildString;
} SYSTEM_VERSION_INFORMATION, *PSYSTEM_VERSION_INFORMATION;

//
// --------------------------------------------------------------------- Macros
//

//
// The ASSERT macro evaluates the given expression. If it is false, then it
// raises an exception.
//

#if DEBUG

#define ASSERT(_Condition)                                      \
    if ((_Condition) == FALSE) {                                \
        RtlRaiseAssertion(#_Condition, __FILE__, __LINE__);     \
    }

#else

//
// No asserts in non-debug builds.
//

#define ASSERT(_Condition)

#endif

//
// This macro determines the offset of the field in the given structure.
//

#define FIELD_OFFSET(_Structure, _Field) \
    (UINTN)(&(((_Structure *)0L)->_Field))

//
// This macro retrieves the data structure associated with a particular Red
// Black Tree node entry. _Node is a PRED_BLACK_TREE_NODE that points to the
// node. _StructureName is the type of the containing record. _MemberName is
// the name of the RED_BLACK_TREE_NODE in the containing record.

#define RED_BLACK_TREE_VALUE(_Node, _StructureName, _MemberName) \
    PARENT_STRUCTURE(_Node, _StructureName, _MemberName)

//
// This macros determines whether or not the given Red Black tree is empty.
//

#define RED_BLACK_TREE_EMPTY(_Tree) \
    ((_Tree)->Root.LeftChild == &((_Tree)->NullNode))

//
//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

RTL_API
ULONG
RtlComputeCrc32 (
    ULONG InitialCrc,
    PCVOID Buffer,
    ULONG Size
    );

/*++

Routine Description:

    This routine computes the CRC-32 Cyclic Redundancy Check on the given
    buffer of data.

Arguments:

    InitialCrc - Supplies an initial CRC value to start with. Supply 0
        initially.

    Buffer - Supplies a pointer to the buffer to compute the CRC32 of.

    Size - Supplies the size of the buffer, in bytes.

Return Value:

    Returns the CRC32 hash of the buffer.

--*/

RTL_API
VOID
RtlRaiseAssertion (
    PCSTR Expression,
    PCSTR SourceFile,
    ULONG SourceLine
    );

/*++

Routine Description:

    This routine raises an assertion failure. If a debugger is connected, it
    will attempt to connect to the debugger.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

RTL_API
VOID
RtlDebugPrint (
    PCSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints a printf-style string to the debugger.

Arguments:

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    None.

--*/

RTL_API
VOID
RtlInitializeMultibyteState (
    PMULTIBYTE_STATE State,
    CHARACTER_ENCODING Encoding
    );

/*++

Routine Description:

    This routine initializes a multibyte state structure.

Arguments:

    State - Supplies the a pointer to the state to reset.

    Encoding - Supplies the encoding to use for multibyte sequences. If the
        default value is supplied here, then the current default system
        encoding will be used.

Return Value:

    None.

--*/

RTL_API
CHARACTER_ENCODING
RtlGetDefaultCharacterEncoding (
    VOID
    );

/*++

Routine Description:

    This routine returns the system default character encoding.

Arguments:

    None.

Return Value:

    Returns the current system default character encoding.

--*/

RTL_API
KSTATUS
RtlSetDefaultCharacterEncoding (
    CHARACTER_ENCODING NewEncoding,
    PCHARACTER_ENCODING OriginalEncoding
    );

/*++

Routine Description:

    This routine sets the system default character encoding.

Arguments:

    NewEncoding - Supplies the new encoding to use.

    OriginalEncoding - Supplies an optional pointer where the previous
        character encoding will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the given character encoding is not supported on
    this system.

--*/

RTL_API
BOOL
RtlIsCharacterEncodingSupported (
    CHARACTER_ENCODING Encoding
    );

/*++

Routine Description:

    This routine determines if the system supports a given character encoding.

Arguments:

    Encoding - Supplies the encoding to query.

Return Value:

    TRUE if the parameter is a valid encoding.

    FALSE if the system does not recognize the given encoding.

--*/

RTL_API
BOOL
RtlIsCharacterEncodingStateDependent (
    CHARACTER_ENCODING Encoding,
    BOOL ToMultibyte
    );

/*++

Routine Description:

    This routine determines if the given character encoding is state-dependent
    when converting between multibyte sequences and wide characters.

Arguments:

    Encoding - Supplies the encoding to query.

    ToMultibyte - Supplies a boolean indicating the direction of the character
        encoding. State-dependence can vary between converting to multibyte and
        converting to wide character.

Return Value:

    TRUE if the given encoding is valid and state-dependent.

    FALSE if the given encoding is invalid or not state-dependent.

--*/

RTL_API
VOID
RtlResetMultibyteState (
    PMULTIBYTE_STATE State
    );

/*++

Routine Description:

    This routine resets the given multibyte state back to its initial state,
    without clearing the character encoding.

Arguments:

    State - Supplies a pointer to the state to reset.

Return Value:

    None.

--*/

RTL_API
BOOL
RtlIsMultibyteStateReset (
    PMULTIBYTE_STATE State
    );

/*++

Routine Description:

    This routine determines if the given multibyte state is in its initial
    reset state.

Arguments:

    State - Supplies a pointer to the state to query.

Return Value:

    TRUE if the state is in the initial shift state.

    FALSE if the state is not in the initial shift state.

--*/

RTL_API
KSTATUS
RtlConvertMultibyteCharacterToWide (
    PCHAR *MultibyteCharacter,
    PULONG MultibyteBufferSize,
    PWCHAR WideCharacter,
    PMULTIBYTE_STATE State
    );

/*++

Routine Description:

    This routine converts a multibyte sequence into a wide character.

Arguments:

    MultibyteCharacter - Supplies a pointer that on input contains a pointer
        to the multibyte character sequence. On successful output, this pointer
        will be advanced beyond the character.

    MultibyteBufferSize - Supplies a pointer that on input contains the size of
        the multibyte buffer in bytes. This value will be updated if the
        returned multibyte character buffer is advanced.

    WideCharacter - Supplies an optional pointer where the wide character will
        be returned on success.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_INVALID_PARAMETER if the multibyte state is invalid.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the number of bytes in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

RTL_API
KSTATUS
RtlConvertWideCharacterToMultibyte (
    WCHAR WideCharacter,
    PCHAR MultibyteCharacter,
    PULONG Size,
    PMULTIBYTE_STATE State
    );

/*++

Routine Description:

    This routine converts a wide character into a multibyte sequence.

Arguments:

    WideCharacter - Supplies the wide character to convert to a multibyte
        sequence.

    MultibyteCharacter - Supplies a pointer to the multibyte sequence.

    Size - Supplies a pointer that on input contains the size of the buffer.
        On output, it will return the number of bytes in the multibyte
        character, even if the buffer provided was too small.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_INVALID_PARAMETER if the multibyte state is invalid.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the remaining space in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

RTL_API
ULONG
RtlPrintToString (
    PSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string out to a buffer.

Arguments:

    Destination - Supplies a pointer to the buffer where the formatted string
        will be placed.

    DestinationSize - Supplies the size of the destination buffer, in bytes.

    Encoding - Supplies the character encoding to use for any wide characters
        or strings.

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    Returns the length of the final string after all formatting has been
    completed. The length will be returned even if NULL is passed as the
    destination.

--*/

RTL_API
ULONG
RtlFormatString (
    PSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCSTR Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine converts a printf-style format string given the parameters.

Arguments:

    Destination - Supplies a pointer to the buffer where the final string will
        be printed. It is assumed that this string is allocated and is big
        enough to hold the converted string. Pass NULL here to determine how big
        a buffer is necessary to hold the string. If the buffer is not big
        enough, it will be truncated but still NULL terminated.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Encoding - Supplies the character encoding to use when converting any
        wide strings or characters.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    Returns the length of the final string after all formatting has been
    completed, including the null terminator. The length will be returned even
    if NULL is passed as the destination.

--*/

RTL_API
BOOL
RtlFormat (
    PPRINT_FORMAT_CONTEXT Context,
    PCSTR Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine converts a printf-style format string given the parameters.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the destination or limit cut the conversion short.

--*/

RTL_API
ULONG
RtlPrintToStringWide (
    PWSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCWSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted wide string out to a buffer.

Arguments:

    Destination - Supplies a pointer to the buffer where the formatted string
        will be placed.

    DestinationSize - Supplies the size of the destination buffer, in bytes.

    Encoding - Supplies the character encoding to use for any non-wide
        characters or strings.

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    Returns the length of the final string (in characters) after all formatting
    has been completed. The length will be returned even if NULL is passed as
    the destination.

--*/

RTL_API
ULONG
RtlFormatStringWide (
    PWSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCWSTR Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine converts a printf-style wide format string given the
    parameters.

Arguments:

    Destination - Supplies a pointer to the buffer where the final string will
        be printed. It is assumed that this string is allocated and is big
        enough to hold the converted string. Pass NULL here to determine how big
        a buffer is necessary to hold the string. If the buffer is not big
        enough, it will be truncated but still NULL terminated.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Encoding - Supplies the character encoding to use for any non-wide
        characters or strings.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    Returns the length of the final string after all formatting has been
    completed, including the null terminator. The length will be returned even
    if NULL is passed as the destination.

--*/

RTL_API
BOOL
RtlFormatWide (
    PPRINT_FORMAT_CONTEXT Context,
    PCWSTR Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine converts a printf-style wide format string given the
    parameters.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the destination or limit cut the conversion short.

--*/

RTL_API
ULONG
RtlStringCopy (
    PSTR Destination,
    PCSTR Source,
    UINTN BufferSize
    );

/*++

Routine Description:

    This routine copies a string from one buffer to another, including the NULL
    terminator.

Arguments:

    Destination - Supplies a pointer to the buffer where the string will be
        copied to.

    Source - Supplies a pointer to the string to copy.

    BufferSize - Supplies the size of the destination buffer.

Return Value:

    Returns the number of bytes copied, including the NULL terminator. If the
    source string is longer than the destination buffer, the string will be
    truncated but still NULL terminated.

--*/

RTL_API
VOID
RtlStringReverse (
    PSTR String,
    PSTR StringEnd
    );

/*++

Routine Description:

    This routine reverses the contents of a string. For example, the string
    "abcd" would get reversed to "dcba".

Arguments:

    String - Supplies a pointer to the beginning of the string to reverse.

    StringEnd - Supplies a pointer to one beyond the end of the string. That is,
        this pointer points to the first byte *not* in the string.

Return Value:

    None.

--*/

RTL_API
ULONG
RtlStringLength (
    PCSTR String
    );

/*++

Routine Description:

    This routine determines the length of the given string, not including its
    NULL terminator.

Arguments:

    String - Supplies a pointer to the beginning of the string.

Return Value:

    Returns the length of the string, not including the NULL terminator.

--*/

RTL_API
BOOL
RtlAreStringsEqual (
    PCSTR String1,
    PCSTR String2,
    ULONG MaxLength
    );

/*++

Routine Description:

    This routine determines if the contents of two strings are equal, up to a
    maximum number of characters.

Arguments:

    String1 - Supplies a pointer to the first string to compare.

    String2 - Supplies a pointer to the second string to compare.

    MaxLength - Supplies the minimum of either string's buffer size.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

RTL_API
BOOL
RtlAreStringsEqualIgnoringCase (
    PCSTR String1,
    PCSTR String2,
    ULONG MaxLength
    );

/*++

Routine Description:

    This routine determines if the contents of two strings are equal, up to a
    maximum number of characters. This routine is case insensitive.

Arguments:

    String1 - Supplies a pointer to the first string to compare.

    String2 - Supplies a pointer to the second string to compare.

    MaxLength - Supplies the minimum of either string's buffer size.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

RTL_API
PSTR
RtlStringFindCharacter (
    PCSTR String,
    CHAR Character,
    ULONG StringLength
    );

/*++

Routine Description:

    This routine searches a string for the first instance of the given
    character, scanning from the left.

Arguments:

    String - Supplies a pointer to the string to search.

    Character - Supplies a pointer to the character to search for within the
        string.

    StringLength - Supplies the length of the string, in bytes, including the
        NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
PSTR
RtlStringFindCharacterRight (
    PCSTR String,
    CHAR Character,
    ULONG StringLength
    );

/*++

Routine Description:

    This routine searches a string for the first instance of the given
    character, scanning from the right backwards. The function will search
    starting at the NULL terminator or string length, whichever comes first.

Arguments:

    String - Supplies a pointer to the string to search.

    Character - Supplies a pointer to the character to search for within the
        string.

    StringLength - Supplies the length of the string, in bytes, including the
        NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
PSTR
RtlStringSearch (
    PSTR InputString,
    UINTN InputStringLength,
    PSTR QueryString,
    UINTN QueryStringLength
    );

/*++

Routine Description:

    This routine searches a string for the first instance of the given string
    within it.

Arguments:

    InputString - Supplies a pointer to the string to search.

    InputStringLength - Supplies the length of the string, in bytes, including
        the NULL terminator.

    QueryString - Supplies a pointer to the null terminated string to search
        for.

    QueryStringLength - Supplies the length of the query string in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the first instance of the string on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
PSTR
RtlStringSearchIgnoringCase (
    PSTR InputString,
    UINTN InputStringLength,
    PSTR QueryString,
    UINTN QueryStringLength
    );

/*++

Routine Description:

    This routine searches a string for the first instance of the given string
    within it. This routine is case insensitive.

Arguments:

    InputString - Supplies a pointer to the string to search.

    InputStringLength - Supplies the length of the string, in bytes, including
        the NULL terminator.

    QueryString - Supplies a pointer to the null terminated string to search
        for.

    QueryStringLength - Supplies the length of the query string in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the first instance of the string on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
ULONG
RtlStringCopyWide (
    PWSTR Destination,
    PWSTR Source,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine copies a wide string from one buffer to another, including the
    NULL terminator.

Arguments:

    Destination - Supplies a pointer to the buffer where the wide string will be
        copied to.

    Source - Supplies a pointer to the wide string to copy.

    BufferSize - Supplies the size of the destination buffer.

Return Value:

    Returns the number of characters copied, including the NULL terminator. If
    the source string is longer than the destination buffer, the string will be
    truncated but still NULL terminated.

--*/

RTL_API
VOID
RtlStringReverseWide (
    PWSTR String,
    PWSTR StringEnd
    );

/*++

Routine Description:

    This routine reverses the contents of a wide string. For example, the string
    L"abcd" would get reversed to L"dcba".

Arguments:

    String - Supplies a pointer to the beginning of the wide string to reverse.

    StringEnd - Supplies a pointer to one beyond the end of the string. That is,
        this pointer points to the first byte *not* in the string.

Return Value:

    None.

--*/

RTL_API
ULONG
RtlStringLengthWide (
    PWSTR String
    );

/*++

Routine Description:

    This routine determines the length of the given wide string, not including
    its NULL terminator.

Arguments:

    String - Supplies a pointer to the beginning of the string.

Return Value:

    Returns the length of the string, not including the NULL terminator.

--*/

RTL_API
BOOL
RtlAreStringsEqualWide (
    PWSTR String1,
    PWSTR String2,
    ULONG MaxLength
    );

/*++

Routine Description:

    This routine determines if the contents of two wide strings are equal, up
    to a maximum number of characters.

Arguments:

    String1 - Supplies a pointer to the first wide string to compare.

    String2 - Supplies a pointer to the second wide string to compare.

    MaxLength - Supplies the minimum of either string's buffer size.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

RTL_API
BOOL
RtlAreStringsEqualIgnoringCaseWide (
    PWSTR String1,
    PWSTR String2,
    ULONG MaxLength
    );

/*++

Routine Description:

    This routine determines if the contents of two wide strings are equal, up
    to a maximum number of characters. This routine is case insensitive.

Arguments:

    String1 - Supplies a pointer to the first wide string to compare.

    String2 - Supplies a pointer to the second wide string to compare.

    MaxLength - Supplies the minimum of either string's buffer size, in
        characters.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

RTL_API
PWSTR
RtlStringFindCharacterWide (
    PWSTR String,
    WCHAR Character,
    ULONG StringLength
    );

/*++

Routine Description:

    This routine searches a wide string for the first instance of the given
    character, scanning from the left.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Character - Supplies a pointer to the wide character to search for within
        the string.

    StringLength - Supplies the length of the string, in characters, including
        the NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
PWSTR
RtlStringFindCharacterRightWide (
    PWSTR String,
    WCHAR Character,
    ULONG StringLength
    );

/*++

Routine Description:

    This routine searches a wide string for the first instance of the given
    character, scanning from the right backwards. The function will search
    starting at the NULL terminator or string length, whichever comes first.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Character - Supplies a pointer to the character to search for within the
        wide string.

    StringLength - Supplies the length of the string, in characters, including
        the NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

RTL_API
KSTATUS
RtlStringScan (
    PCSTR Input,
    ULONG InputSize,
    PCSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    ...
    );

/*++

Routine Description:

    This routine scans in a string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the input string to scan.

    InputSize - Supplies the size of the string in bytes including the null
        terminator.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    FormatSize - Supplies the size of the format string in bytes, including
        the null terminator.

    Encoding - Supplies the character encoding to use when scanning into
        wide strings or characters.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    ... - Supplies the remaining pointer arguments where the scanned data will
        be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
KSTATUS
RtlStringScanVaList (
    PCSTR Input,
    ULONG InputSize,
    PCSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    va_list Arguments
    );

/*++

Routine Description:

    This routine scans in a string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the input string to scan.

    InputSize - Supplies the size of the string in bytes including the null
        terminator.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    FormatSize - Supplies the size of the format string in bytes, including
        the null terminator.

    Encoding - Supplies the character encoding to use when scanning into
        wide strings or characters.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    Arguments - Supplies the initialized arguments list where various pieces
        of the formatted string will be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
KSTATUS
RtlStringScanInteger (
    PCSTR *String,
    PULONG StringSize,
    ULONG Base,
    BOOL Signed,
    PLONGLONG Integer
    );

/*++

Routine Description:

    This routine converts a string into an integer. It scans past leading
    whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the string
        to scan. On output, the string advanced past the scanned value (if any)
        will be returned. If the entire string is whitespace or starts with an
        invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in bytes, including the null terminator. On output, this will
        contain the size of the string minus the number of bytes scanned by
        this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Base - Supplies the base of the integer to scan. Valid values are zero and
        two through thirty six. If zero is supplied, this routine will attempt
        to automatically detect what the base is out of bases 8, 10, and 16.

    Signed - Supplies a boolean indicating whether the integer to scan is
        signed or not.

    Integer - Supplies a pointer where the resulting integer is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_INTEGER_OVERFLOW if the result overflowed. In this case the integer
    returned will be MAX_LONGLONG, MIN_LONGLONG, or MAX_ULONGLONG depending on
    the signedness and value.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

RTL_API
KSTATUS
RtlStringScanDouble (
    PCSTR *String,
    PULONG StringSize,
    double *Double
    );

/*++

Routine Description:

    This routine converts a string into a floating point double. It scans past
    leading whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the string
        to scan. On output, the string advanced past the scanned value (if any)
        will be returned. If the entire string is whitespace or starts with an
        invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in bytes, including the null terminator. On output, this will
        contain the size of the string minus the number of bytes scanned by
        this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Double - Supplies a pointer where the resulting double is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid double could not be scanned.

    STATUS_OUT_OF_BOUNDS if the exponent was out of range.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

RTL_API
KSTATUS
RtlScan (
    PSCAN_INPUT Input,
    PCSTR Format,
    ULONG FormatLength,
    PULONG ItemsScanned,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans from an input and converts the input to various
    parameters according to a specified format.

Arguments:

    Input - Supplies a pointer to the filled out scan input structure which
        will be used to retrieve more input.

    Format - Supplies the format string which specifies how to convert the
        input to the argument list.

    FormatLength - Supplies the size of the format length string in bytes,
        including the null terminator.

    ItemsScanned - Supplies a pointer where the number of parameters filled in
        (not counting %n) will be returned.

    ArgumentList - Supplies the list of arguments that will get filled out
        based on the input and format string.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
    format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
KSTATUS
RtlStringScanWide (
    PCWSTR Input,
    ULONG InputSize,
    PCWSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    ...
    );

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the input wide string to scan.

    InputSize - Supplies the size of the string in characters including the
        null terminator.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    FormatSize - Supplies the size of the format string in characters,
        including the null terminator.

    Encoding - Supplies the character encoding to use when scanning non-wide
        items.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    ... - Supplies the remaining pointer arguments where the scanned data will
        be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
KSTATUS
RtlStringScanVaListWide (
    PCWSTR Input,
    ULONG InputSize,
    PCWSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    va_list Arguments
    );

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the wide input string to scan.

    InputSize - Supplies the size of the string in characters including the
        null terminator.

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    FormatSize - Supplies the size of the format string in characters,
        including the null terminator.

    Encoding - Supplies the character encoding to use when scanning non-wide
        items.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    Arguments - Supplies the initialized arguments list where various pieces
        of the formatted string will be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
KSTATUS
RtlStringScanIntegerWide (
    PCWSTR *String,
    PULONG StringSize,
    ULONG Base,
    BOOL Signed,
    PLONGLONG Integer
    );

/*++

Routine Description:

    This routine converts a wide string into an integer. It scans past leading
    whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the string
        to scan. On output, the string advanced past the scanned value (if any)
        will be returned. If the entire string is whitespace or starts with an
        invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in characters , including the null terminator. On output, this
        will contain the size of the string minus the number of bytes scanned by
        this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Base - Supplies the base of the integer to scan. Valid values are zero and
        two through thirty six. If zero is supplied, this routine will attempt
        to automatically detect what the base is out of bases 8, 10, and 16.

    Signed - Supplies a boolean indicating whether the integer to scan is
        signed or not.

    STATUS_INTEGER_OVERFLOW if the result overflowed. In this case the integer
    returned will be MAX_LONGLONG, MIN_LONGLONG, or MAX_ULONGLONG depending on
    the signedness and value.

    Integer - Supplies a pointer where the resulting integer is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

RTL_API
KSTATUS
RtlStringScanDoubleWide (
    PCWSTR *String,
    PULONG StringSize,
    double *Double
    );

/*++

Routine Description:

    This routine converts a wide string into a floating point double. It scans
    past leading whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the wide
        string to scan. On output, the string advanced past the scanned value
        (if any) will be returned. If the entire string is whitespace or starts
        with an invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in characters, including the null terminator. On output, this
        will contain the size of the string minus the number of bytes scanned
        by this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Double - Supplies a pointer where the resulting double is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

RTL_API
KSTATUS
RtlScanWide (
    PSCAN_INPUT Input,
    PCWSTR Format,
    ULONG FormatLength,
    PULONG ItemsScanned,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans from an input and converts the input to various
    parameters according to a specified format.

Arguments:

    Input - Supplies a pointer to the filled out scan input structure which
        will be used to retrieve more input.

    Format - Supplies the wide format string which specifies how to convert the
        input to the argument list.

    FormatLength - Supplies the size of the format length string in characters,
        including the null terminator.

    ItemsScanned - Supplies a pointer where the number of parameters filled in
        (not counting %n) will be returned.

    ArgumentList - Supplies the list of arguments that will get filled out
        based on the input and format string.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
    format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

RTL_API
VOID
RtlZeroMemory (
    PVOID Buffer,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine zeroes out a section of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    None.

--*/

RTL_API
VOID
RtlSetMemory (
    PVOID Buffer,
    INT Byte,
    UINTN Count
    );

/*++

Routine Description:

    This routine writes the given byte value repeatedly into a region of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to set.

    Byte - Supplies the byte to set.

    Count - Supplies the number of bytes to set.

Return Value:

    None.

--*/

RTL_API
PVOID
RtlCopyMemory (
    PVOID Destination,
    PCVOID Source,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine copies a section of memory.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Returns the destination pointer.

--*/

RTL_API
BOOL
RtlCompareMemory (
    PCVOID FirstBuffer,
    PCVOID SecondBuffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine compares two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are equal.

    FALSE if the buffers are not equal.

--*/

RTL_API
BOOL
RtlAreUuidsEqual (
    PUUID Uuid1,
    PUUID Uuid2
    );

/*++

Routine Description:

    This routine compares two UUIDs.

Arguments:

    Uuid1 - Supplies the first UUID.

    Uuid2 - Supplies the second UUID.

Return Value:

    TRUE if the UUIDs are equal.

    FALSE if the UUIDs are not equal.

--*/

__USED
RTL_API
ULONGLONG
RtlDivideUnsigned64 (
    ULONGLONG Dividend,
    ULONGLONG Divisor,
    PULONGLONG Remainder
    );

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns the quotient.

--*/

__USED
RTL_API
LONGLONG
RtlDivide64 (
    LONGLONG Dividend,
    LONGLONG Divisor
    );

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

Return Value:

    Returns the quotient.

--*/

__USED
RTL_API
LONGLONG
RtlDivideModulo64 (
    LONGLONG Dividend,
    LONGLONG Divisor,
    PLONGLONG Remainder
    );

/*++

Routine Description:

    This routine performs a 64-bit divide and modulo of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer where the remainder will be returned.

Return Value:

    Returns the quotient.

--*/

__USED
RTL_API
ULONG
RtlDivideUnsigned32 (
    ULONG Dividend,
    ULONG Divisor,
    PULONG Remainder
    );

/*++

Routine Description:

    This routine performs a 32-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies an optional pointer where the remainder will be
        returned.

Return Value:

    Returns the quotient.

--*/

__USED
RTL_API
LONG
RtlDivide32 (
    LONG Dividend,
    LONG Divisor
    );

/*++

Routine Description:

    This routine performs a 32-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

Return Value:

    Returns the quotient.

--*/

__USED
RTL_API
LONG
RtlDivideModulo32 (
    LONG Dividend,
    LONG Divisor,
    PLONG Remainder
    );

/*++

Routine Description:

    This routine performs a 32-bit divide and modulo of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer where the remainder will be returned.

Return Value:

    Returns the quotient.

--*/

RTL_API
ULONGLONG
RtlByteSwapUlonglong (
    ULONGLONG Input
    );

/*++

Routine Description:

    This routine performs a byte-swap of a 64-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

RTL_API
ULONG
RtlByteSwapUlong (
    ULONG Input
    );

/*++

Routine Description:

    This routine performs a byte-swap of a 32-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

RTL_API
USHORT
RtlByteSwapUshort (
    USHORT Input
    );

/*++

Routine Description:

    This routine performs a byte-swap of a 16-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

RTL_API
INT
RtlCountTrailingZeros64 (
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine determines the number of trailing zero bits in the given
    64-bit value.

Arguments:

    Value - Supplies the value to get the number of trailing zeros for. This
        must not be zero.

Return Value:

    Returns the number of trailing zero bits in the given value.

--*/

RTL_API
INT
RtlCountTrailingZeros32 (
    ULONG Value
    );

/*++

Routine Description:

    This routine determines the number of trailing zero bits in the given
    32-bit value.

Arguments:

    Value - Supplies the value to get the number of trailing zeros for. This
        must not be zero.

Return Value:

    Returns the number of trailing zero bits in the given value.

--*/

RTL_API
INT
RtlCountLeadingZeros64 (
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine determines the number of leading zero bits in the given 64-bit
    value.

Arguments:

    Value - Supplies the value to get the number of leading zeros for. This
        must not be zero.

Return Value:

    Returns the number of leading zero bits in the given value.

--*/

RTL_API
INT
RtlCountLeadingZeros32 (
    ULONG Value
    );

/*++

Routine Description:

    This routine determines the number of leading zero bits in the given 32-bit
    value.

Arguments:

    Value - Supplies the value to get the number of leading zeros for. This
        must not be zero.

Return Value:

    Returns the number of leading zero bits in the given value.

--*/

RTL_API
INT
RtlCountSetBits64 (
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine determines the number of bits set to one in the given 64-bit
    value.

Arguments:

    Value - Supplies the value to count set bits for.

Return Value:

    Returns the number of bits set to one.

--*/

RTL_API
INT
RtlCountSetBits32 (
    ULONG Value
    );

/*++

Routine Description:

    This routine determines the number of bits set to one in the given 32-bit
    value.

Arguments:

    Value - Supplies the value to count set bits for.

Return Value:

    Returns the number of bits set to one.

--*/

RTL_API
BOOL
RtlFloatIsNan (
    float Value
    );

/*++

Routine Description:

    This routine determines if the given value is Not a Number.

Arguments:

    Value - Supplies the floating point value to query.

Return Value:

    TRUE if the given value is Not a Number.

    FALSE otherwise.

--*/

RTL_API
double
RtlFloatConvertToDouble (
    float Float
    );

/*++

Routine Description:

    This routine converts the given float into a double.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the double equivalent.

--*/

RTL_API
float
RtlFloatAdd (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine adds two floats together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the sum of the two values.

--*/

RTL_API
float
RtlFloatSubtract (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine subtracts two floats from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value, the value to subtract.

Return Value:

    Returns the difference of the two values.

--*/

RTL_API
float
RtlFloatMultiply (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine multiplies two floats together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the product of the two values.

--*/

RTL_API
float
RtlFloatDivide (
    float Dividend,
    float Divisor
    );

/*++

Routine Description:

    This routine divides one float into another.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the quotient of the two values.

--*/

RTL_API
float
RtlFloatModulo (
    float Dividend,
    float Divisor
    );

/*++

Routine Description:

    This routine divides one float into another, and returns the remainder.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the modulo of the two values.

--*/

RTL_API
float
RtlFloatSquareRoot (
    float Value
    );

/*++

Routine Description:

    This routine returns the square root of the given float.

Arguments:

    Value - Supplies the value to take the square root of.

Return Value:

    Returns the square root of the given value.

--*/

RTL_API
BOOL
RtlFloatIsEqual (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given floats are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal. Note that NaN is not equal to anything,
    including itself.

--*/

RTL_API
BOOL
RtlFloatIsLessThanOrEqual (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

RTL_API
BOOL
RtlFloatIsLessThan (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
BOOL
RtlFloatSignalingIsEqual (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given values are equal, generating an
    invalid floating point exception if either is NaN.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
BOOL
RtlFloatIsLessThanOrEqualQuiet (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value. Quiet NaNs do not generate floating point exceptions.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

RTL_API
BOOL
RtlFloatIsLessThanQuiet (
    float Value1,
    float Value2
    );

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value. Quiet NaNs do not cause float point exceptions to be raised.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
float
RtlFloatConvertFromInteger32 (
    LONG Integer
    );

/*++

Routine Description:

    This routine converts the given signed 32-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

RTL_API
float
RtlFloatConvertFromUnsignedInteger32 (
    ULONG Integer
    );

/*++

Routine Description:

    This routine converts the given unsigned 32-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

RTL_API
float
RtlFloatConvertFromInteger64 (
    LONGLONG Integer
    );

/*++

Routine Description:

    This routine converts the given signed 64-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

RTL_API
float
RtlFloatConvertFromUnsignedInteger64 (
    ULONGLONG Integer
    );

/*++

Routine Description:

    This routine converts the given unsigned 64-bit integer into a float.

Arguments:

    Integer - Supplies the unsigned integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

RTL_API
LONG
RtlFloatConvertToInteger32 (
    float Float
    );

/*++

Routine Description:

    This routine converts the given float into a signed 32 bit integer.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

RTL_API
LONG
RtlFloatConvertToInteger32RoundToZero (
    float Float
    );

/*++

Routine Description:

    This routine converts the given float into a signed 32 bit integer. It
    always rounds towards zero.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

RTL_API
LONGLONG
RtlFloatConvertToInteger64 (
    float Float
    );

/*++

Routine Description:

    This routine converts the given float into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

RTL_API
LONGLONG
RtlFloatConvertToInteger64RoundToZero (
    float Float
    );

/*++

Routine Description:

    This routine converts the given float into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned. This routine
    always rounds towards zero.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

RTL_API
BOOL
RtlDoubleIsNan (
    double Value
    );

/*++

Routine Description:

    This routine determines if the given value is Not a Number.

Arguments:

    Value - Supplies the floating point value to query.

Return Value:

    TRUE if the given value is Not a Number.

    FALSE otherwise.

--*/

RTL_API
double
RtlDoubleConvertFromInteger32 (
    LONG Integer
    );

/*++

Routine Description:

    This routine converts the given signed 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

RTL_API
double
RtlDoubleConvertFromUnsignedInteger32 (
    ULONG Integer
    );

/*++

Routine Description:

    This routine converts the given unsigned 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

RTL_API
double
RtlDoubleConvertFromInteger64 (
    LONGLONG Integer
    );

/*++

Routine Description:

    This routine converts the given signed 64-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

RTL_API
double
RtlDoubleConvertFromUnsignedInteger64 (
    ULONGLONG Integer
    );

/*++

Routine Description:

    This routine converts the given unsigned 64-bit integer into a double.

Arguments:

    Integer - Supplies the unsigned integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

RTL_API
LONG
RtlDoubleConvertToInteger32 (
    double Double
    );

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

RTL_API
LONG
RtlDoubleConvertToInteger32RoundToZero (
    double Double
    );

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer. It
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

RTL_API
LONGLONG
RtlDoubleConvertToInteger64 (
    double Double
    );

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

RTL_API
LONGLONG
RtlDoubleConvertToInteger64RoundToZero (
    double Double
    );

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned. This routine
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

RTL_API
float
RtlDoubleConvertToFloat (
    double Double
    );

/*++

Routine Description:

    This routine converts the given double into a float (32 bit floating
    point number).

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the float equivalent.

--*/

RTL_API
double
RtlDoubleAdd (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine adds two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the sum of the two values.

--*/

RTL_API
double
RtlDoubleSubtract (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine subtracts two doubles from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value, the value to subtract.

Return Value:

    Returns the difference of the two values.

--*/

RTL_API
double
RtlDoubleMultiply (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine multiplies two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the product of the two values.

--*/

RTL_API
double
RtlDoubleDivide (
    double Dividend,
    double Divisor
    );

/*++

Routine Description:

    This routine divides one double into another.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the quotient of the two values.

--*/

RTL_API
double
RtlDoubleModulo (
    double Dividend,
    double Divisor
    );

/*++

Routine Description:

    This routine divides one double into another, and returns the remainder.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the modulo of the two values.

--*/

RTL_API
double
RtlDoubleSquareRoot (
    double Value
    );

/*++

Routine Description:

    This routine returns the square root of the given double.

Arguments:

    Value - Supplies the value to take the square root of.

Return Value:

    Returns the square root of the given value.

--*/

RTL_API
BOOL
RtlDoubleIsEqual (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given doubles are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal. Note that NaN is not equal to anything,
    including itself.

--*/

RTL_API
BOOL
RtlDoubleIsLessThanOrEqual (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

RTL_API
BOOL
RtlDoubleIsLessThan (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
BOOL
RtlDoubleSignalingIsEqual (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given values are equal, generating an
    invalid floating point exception if either is NaN.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
BOOL
RtlDoubleIsLessThanOrEqualQuiet (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value. Quiet NaNs do not generate floating point exceptions.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

RTL_API
BOOL
RtlDoubleIsLessThanQuiet (
    double Value1,
    double Value2
    );

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value. Quiet NaNs do not cause float point exceptions to be raised.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

RTL_API
VOID
RtlDebugBreak (
    VOID
    );

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
RtlDebugService (
    UINTN ServiceRequest,
    PVOID Parameter
    );

/*++

Routine Description:

    This routine enters the debugger for a service request.

Arguments:

    ServiceRequest - Supplies the reason for entering the debugger.

    Parameter - Supplies the parameter to pass to the debug service routine.

Return Value:

    None.

--*/

RTL_API
ULONG
RtlAtomicExchange32 (
    volatile ULONG *Address,
    ULONG ExchangeValue
    );

/*++

Routine Description:

    This routine atomically exchanges the value at the given memory address
    with the given value.

Arguments:

    Address - Supplies the address of the value to exchange with.

    ExchangeValue - Supplies the value to write to the address.

Return Value:

    Returns the original value at the given address.

--*/

RTL_API
ULONGLONG
RtlAtomicExchange64 (
    volatile ULONGLONG *Address,
    ULONGLONG ExchangeValue
    );

/*++

Routine Description:

    This routine atomically exchanges the value at the given memory address
    with the given value.

Arguments:

    Address - Supplies the address of the value to exchange with.

    ExchangeValue - Supplies the value to write to the address.

Return Value:

    Returns the original value at the given address.

--*/

RTL_API
ULONGLONG
RtlAtomicCompareExchange64 (
    volatile ULONGLONG *Address,
    ULONGLONG ExchangeValue,
    ULONGLONG CompareValue
    );

/*++

Routine Description:

    This routine atomically compares a 64-bit value at the given address with a
    value and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

RTL_API
ULONG
RtlAtomicCompareExchange32 (
    volatile ULONG *Address,
    ULONG ExchangeValue,
    ULONG CompareValue
    );

/*++

Routine Description:

    This routine atomically compares memory at the given address with a value
    and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

RTL_API
ULONG
RtlAtomicAdd32 (
    volatile ULONG *Address,
    ULONG Increment
    );

/*++

Routine Description:

    This routine atomically adds the given amount to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

RTL_API
ULONGLONG
RtlAtomicAdd64 (
    volatile ULONGLONG *Address,
    ULONGLONG Increment
    );

/*++

Routine Description:

    This routine atomically adds the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

RTL_API
ULONG
RtlAtomicOr32 (
    volatile ULONG *Address,
    ULONG Mask
    );

/*++

Routine Description:

    This routine atomically ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

RTL_API
ULONGLONG
RtlAtomicOr64 (
    volatile ULONGLONG *Address,
    ULONGLONG Mask
    );

/*++

Routine Description:

    This routine atomically ORs the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

RTL_API
ULONG
RtlAtomicAnd32 (
    volatile ULONG *Address,
    ULONG Mask
    );

/*++

Routine Description:

    This routine atomically ANDs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically AND with.

    Mask - Supplies the bitmask to logically AND in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

RTL_API
ULONG
RtlAtomicXor32 (
    volatile ULONG *Address,
    ULONG Mask
    );

/*++

Routine Description:

    This routine atomically exclusive ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically XOR with.

    Mask - Supplies the bitmask to logically XOR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

RTL_API
VOID
RtlMemoryBarrier (
    VOID
    );

/*++

Routine Description:

    This routine provides a full memory barrier, ensuring that all memory
    accesses occurring before this function complete before any memory accesses
    after this function start.

Arguments:

    None.

Return Value:

    None.

--*/

RTL_API
VOID
RtlRedBlackTreeInitialize (
    PRED_BLACK_TREE Tree,
    ULONG Flags,
    PCOMPARE_RED_BLACK_TREE_NODES CompareFunction
    );

/*++

Routine Description:

    This routine initializes a Red-Black tree structure.

Arguments:

    Tree - Supplies a pointer to a tree to initialize. Tree structures should
        not be initialized more than once.

    Flags - Supplies a bitmask of flags governing the behavior of the tree. See
        RED_BLACK_TREE_FLAG_* definitions.

    CompareFunction - Supplies a pointer to a function called to compare nodes
        to each other. This routine is used on insertion, deletion, and search.

Return Value:

    None.

--*/

RTL_API
VOID
RtlRedBlackTreeInsert (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE NewNode
    );

/*++

Routine Description:

    This routine inserts a node into the given Red-Black tree.

Arguments:

    Tree - Supplies a pointer to a tree to insert the node into.

    NewNode - Supplies a pointer to the new node to insert.

Return Value:

    None.

--*/

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeSearch (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Value
    );

/*++

Routine Description:

    This routine searches for a node in the tree with the given value. If there
    are multiple nodes with the same value, then the first one found will be
    returned.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to search.

    Value - Supplies a pointer to a dummy node that will be passed to the
        compare function. This node only has to be filled in to the extent that
        the compare function can be called to compare its value. Usually this
        is a stack allocated variable of the parent structure with that value
        filled in.

Return Value:

    Returns a pointer to a node in the tree matching the desired value on
    success.

    NULL if a node matching the given value could not be found.

--*/

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeSearchClosest (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Value,
    BOOL GreaterThan
    );

/*++

Routine Description:

    This routine searches for a node in the tree with the given value. If there
    are multiple nodes with the same value, then the first one found will be
    returned. If no node matches the given value, then the closest node
    greater than or less than the given value (depending on the parameter) will
    be returned instead.

Arguments:

    Tree - Supplies a pointer to a tree that owns the node to search.

    Value - Supplies a pointer to a dummy node that will be passed to the
        compare function. This node only has to be filled in to the extent that
        the compare function can be called to compare its value. Usually this
        is a stack allocated variable of the parent structure with that value
        filled in.

    GreaterThan - Supplies a boolean indicating whether the closest value
        greater than the given value should be returned (TRUE) or the closest
        value less than the given value shall be returned (FALSE).

Return Value:

    Returns a pointer to a node in the tree matching the desired value on
    success.

    Returns a pointer to the closest node greater than the given value if the
    greater than parameter is set and there is a node greater than the given
    value.

    Returns a pointer to the closest node less than the given node if the
    greater than parameter is not set, and such a node exists.

    NULL if the node cannot be found and there is no node greater than (or less
    than, depending on the parameter) the given value.

--*/

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetLowestNode (
    PRED_BLACK_TREE Tree
    );

/*++

Routine Description:

    This routine returns the node in the given Red-Black tree with the lowest
    value.

Arguments:

    Tree - Supplies a pointer to a tree.

Return Value:

    Returns a pointer to the node with the lowest value.

    NULL if the tree is empty.

--*/

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetHighestNode (
    PRED_BLACK_TREE Tree
    );

/*++

Routine Description:

    This routine returns the node in the given Red-Black tree with the highest
    value.

Arguments:

    Tree - Supplies a pointer to a tree.

Return Value:

    Returns a pointer to the node with the lowest value.

    NULL if the tree is empty.

--*/

RTL_API
VOID
RtlRedBlackTreeRemove (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node
    );

/*++

Routine Description:

    This routine removes the given node from the Red-Black tree.

Arguments:

    Tree - Supplies a pointer to a tree that the node is currently inserted
        into.

    Node - Supplies a pointer to the node to remove from the tree.

Return Value:

    None.

--*/

RTL_API
VOID
RtlRedBlackTreeIterate (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_ITERATION_ROUTINE Routine,
    PVOID Context
    );

/*++

Routine Description:

    This routine iterates through all nodes in a Red-Black tree (via in in
    order traversal) and calls the given routine for each node in the tree.
    The routine passed must not modify the tree.

Arguments:

    Tree - Supplies a pointer to a tree that the node is currently inserted
        into.

    Routine - Supplies a pointer to the routine that will be called for each
        node encountered.

    Context - Supplies an optional caller-provided context that will be passed
        to the interation routine for each node.

Return Value:

    None.

--*/

RTL_API
PRED_BLACK_TREE_NODE
RtlRedBlackTreeGetNextNode (
    PRED_BLACK_TREE Tree,
    BOOL Descending,
    PRED_BLACK_TREE_NODE PreviousNode
    );

/*++

Routine Description:

    This routine gets the node in the Red-Black tree with the next highest
    or lower value depending on the supplied boolean.

Arguments:

    Tree - Supplies a pointer to a Red-Black tree.

    Descending - Supplies a boolean indicating if the next node should be a
        descending value or not.

    PreviousNode - Supplies a pointer to the previous node on which to base the
        search.

Return Value:

    Returns a pointer to the node in the tree with the next highest value, or
    NULL if the given previous node is the node with the highest value.

--*/

RTL_API
BOOL
RtlValidateRedBlackTree (
    PRED_BLACK_TREE Tree
    );

/*++

Routine Description:

    This routine determines if the given Red-Black tree is valid.

    Note: This function is recursive, and should not be used outside of debug
          builds and test environments.

Arguments:

    Tree - Supplies a pointer to the tree to validate.

Return Value:

    TRUE if the tree is valid.

    FALSE if the tree is corrupt or is breaking required rules of Red-Black
    trees.

--*/

RTL_API
KSTATUS
RtlSystemTimeToGmtCalendarTime (
    PSYSTEM_TIME SystemTime,
    PCALENDAR_TIME CalendarTime
    );

/*++

Routine Description:

    This routine converts the given system time into calendar time in the
    GMT time zone.

Arguments:

    SystemTime - Supplies a pointer to the system time to convert.

    CalendarTime - Supplies a pointer to the calendar time to initialize based
        on the given system time.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given system time is too funky.

--*/

RTL_API
KSTATUS
RtlCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts the given calendar time into its corresponding system
    time.

Arguments:

    CalendarTime - Supplies a pointer to the calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given calendar time is too funky.

--*/

RTL_API
KSTATUS
RtlGmtCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts the given calendar time, assumed to be a GMT data and
    time, into its corresponding system time. On success, this routine will
    update the supplied calendar time to fill out all fields.

Arguments:

    CalendarTime - Supplies a pointer to the GMT calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given GMT calendar time is too funky.

--*/

RTL_API
UINTN
RtlFormatDate (
    PSTR StringBuffer,
    ULONG StringBufferSize,
    PSTR Format,
    PCALENDAR_TIME CalendarTime
    );

/*++

Routine Description:

    This routine converts the given calendar time into a string governed by
    the given format string.

Arguments:

    StringBuffer - Supplies a pointer where the converted string will be
        returned, including the terminating null.

    StringBufferSize - Supplies the size of the string buffer in bytes.

    Format - Supplies the format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. Conversions start with a '%' character,
        followed by an optional E or O character, followed by a conversion
        specifier. The conversion specifier can take the following values:

        %a - Replaced by the abbreviated weekday.
        %A - Replaced by the full weekday.
        %b - Replaced by the abbreviated month name.
        %B - Replaced by the full month name.
        %c - Replaced by the locale's appropriate date and time representation.
        %C - Replaced by the year divided by 100 (century) [00,99].
        %d - Replaced by the day of the month [01,31].
        %D - Equivalent to "%m/%d/%y".
        %e - Replaced by the day of the month [ 1,31]. A single digit is
             preceded by a space.
        %F - Equivalent to "%Y-%m-%d" (the ISO 8601:2001 date format).
        %G - The ISO 8601 week-based year [0001,9999]. The week-based year and
             the Gregorian year can differ in the first week of January.
        %h - Equivalent to %b (abbreviated month).
        %H - Replaced by the 24 hour clock hour [00,23].
        %I - Replaced by the 12 hour clock hour [01,12].
        %J - Replaced by the nanosecond [0,999999999].
        %j - Replaced by the day of the year [001,366].
        %m - Replaced by the month number [01,12].
        %M - Replaced by the minute [00,59].
        %N - Replaced by the microsecond [0,999999]
        %n - Replaced by a newline.
        %p - Replaced by "AM" or "PM".
        %P - Replaced by "am" or "pm".
        %q - Replaced by the millisecond [0,999].
        %r - Replaced by the time in AM/PM notation: "%I:%M:%S %p".
        %R - Replaced by the time in 24 hour notation: "%H:%M".
        %S - Replaced by the second [00,60].
        %t - Replaced by a tab.
        %T - Replaced by the time: "%H:%M:%S".
        %u - Replaced by the weekday number, with 1 representing Monday [1,7].
        %U - Replaced by the week number of the year [00,53]. The first Sunday
             of January is the first day of week 1. Days before this are week 0.
        %V - Replaced by the week number of the year with Monday as the first
             day in the week [01,53]. If the week containing January 1st has 4
             or more days in the new year, it is considered week 1. Otherwise,
             it is the last week of the previous year, and the next week is 1.
        %w - Replaced by the weekday number [0,6], with 0 representing Sunday.
        %W - Replaced by the week number [00,53]. The first Monday of January
             is the first day of week 1. Days before this are in week 0.
        %x - Replaced by the locale's appropriate date representation.
        %X - Replaced by the locale's appropriate time representation.
        %y - Replaced by the last two digits of the year [00,99].
        %Y - Replaced by the full four digit year [0001,9999].
        %z - Replaced by the offset from UTC in the standard ISO 8601:2000
             standard format (+hhmm or -hhmm), or by no characters if no
             timezone is terminable. If the "is daylight saving" member of the
             calendar structure is greater than zero, then the daylight saving
             offset is used. If the dayslight saving member of the calendar
             structure is negative, no characters are returned.
        %Z - Replaced by the timezone name or abbreviation, or by no bytes if
             no timezone information exists.
        %% - Replaced by a literal '%'.

    CalendarTime - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, including
    the null terminator.

--*/

RTL_API
UINTN
RtlFormatDateWide (
    PWSTR StringBuffer,
    ULONG StringBufferSize,
    PWSTR Format,
    PCALENDAR_TIME CalendarTime
    );

/*++

Routine Description:

    This routine converts the given calendar time into a wide string governed
    by the given wide format string.

Arguments:

    StringBuffer - Supplies a pointer where the converted wide string will be
        returned, including the terminating null.

    StringBufferSize - Supplies the size of the string buffer in characters.

    Format - Supplies the wide format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. The conversions are equivalent to the
        non-wide format date function.

    CalendarTime - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, not
    including the null terminator.

--*/

RTL_API
PSTR
RtlScanDate (
    PCSTR StringBuffer,
    PCSTR Format,
    PCALENDAR_TIME CalendarTime
    );

/*++

Routine Description:

    This routine scans the given input string into values in the calendar time,
    using the specified format.

Arguments:

    StringBuffer - Supplies a pointer to the null terminated string to scan.

    Format - Supplies the format string to govern the conversion. Ordinary
        characters in the format string will be scanned verbatim from the input.
        Whitespace characters in the format will cause all whitespace at the
        current position in the input to be scanned. Conversions will be
        scanned for their corresponding value in the provided calendar time.
        Conversions start with a '%' character, followed by an optional E or O
        character, followed by a conversion specifier. The conversion specifier
        can take the following values:

        %a - The day of the weekday name, either the full or abbreviated name.
        %A - Equivalent to %a.
        %b - The month name, either the full or abbreviated name.
        %B - Equivalent to %b.
        %c - Replaced by the locale's appropriate date and time representation.
        %C - The year divided by 100 (century) [00,99].
        %d - The day of the month [01,31].
        %D - Equivalent to "%m/%d/%y".
        %e - Equivalent to %d.
        %h - Equivalent to %b (month name).
        %H - The 24 hour clock hour [00,23].
        %I - The 12 hour clock hour [01,12].
        %J - Replaced by the nanosecond [0,999999999].
        %j - The day of the year [001,366].
        %m - The month number [01,12].
        %M - The minute [00,59].
        %N - The microsecond [0,999999]
        %n - Any whitespace.
        %p - The equivalent of "AM" or "PM".
        %q - The millisecond [0,999].
        %r - Replaced by the time in AM/PM notation: "%I:%M:%S %p".
        %R - Replaced by the time in 24 hour notation: "%H:%M".
        %S - The second [00,60].
        %t - Any white space.
        %T - Replaced by the time: "%H:%M:%S".
        %u - Replaced by the weekday number, with 1 representing Monday [1,7].
        %U - The week number of the year [00,53]. The first Sunday of January is
             the first day of week 1. Days before this are week 0.
        %w - The weekday number [0,6], with 0 representing Sunday.
        %W - The week number [00,53]. The first Monday of January is the first
             day of week 1. Days before this are in week 0.
        %x - Replaced by the locale's appropriate date representation.
        %X - Replaced by the locale's appropriate time representation.
        %y - The last two digits of the year [00,99].
        %Y - The full four digit year [0001,9999].
        %% - Replaced by a literal '%'.

    CalendarTime - Supplies a pointer to the calendar time value to place the
        values in. Only the values that are scanned in are modified.

Return Value:

    Returns the a pointer to the input string after the last character scanned.

    NULL if the result coult not be scanned.

--*/

RTL_API
VOID
RtlInitializeTimeZoneSupport (
    PTIME_ZONE_LOCK_FUNCTION AcquireTimeZoneLockFunction,
    PTIME_ZONE_LOCK_FUNCTION ReleaseTimeZoneLockFunction,
    PTIME_ZONE_REALLOCATE_FUNCTION ReallocateFunction
    );

/*++

Routine Description:

    This routine initializes library support functions needed by the time zone
    code.

Arguments:

    AcquireTimeZoneLockFunction - Supplies a pointer to the function called
        to acquire access to the time zone data.

    ReleaseTimeZoneLockFunction - Supplies a pointer to the function called to
        relinquish access to the time zone data.

    ReallocateFunction - Supplies a pointer to a function used to dynamically
        allocate and free memory.

Return Value:

    None.

--*/

RTL_API
KSTATUS
RtlFilterTimeZoneData (
    PVOID TimeZoneData,
    ULONG TimeZoneDataSize,
    PCSTR TimeZoneName,
    PVOID FilteredData,
    PULONG FilteredDataSize
    );

/*++

Routine Description:

    This routine filters the given time zone data for one specific time zone.

Arguments:

    TimeZoneData - Supplies a pointer to the buffer containing the unfiltered
        time zone data.

    TimeZoneDataSize - Supplies the size in bytes of the unfiltered time zone
        data.

    TimeZoneName - Supplies a pointer to the null terminated string containing
        the name of the time zone to retrieve.

    FilteredData - Supplies an optional pointer to the buffer where the
        filtered data will be returned. If this pointer is NULL, then only the
        size of the required data will be returned.

    FilteredDataSize - Supplies a pointer that on input contains the size of
        the filtered data buffer. On output, will return the required size of
        the output buffer to contain the filtered data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was valid but too small.

    STATUS_FILE_CORRUPT if the data is invalid.

--*/

RTL_API
KSTATUS
RtlGetTimeZoneData (
    PVOID Data,
    PULONG DataSize
    );

/*++

Routine Description:

    This routine copies the current time zone data into the given buffer.

Arguments:

    Data - Supplies a pointer where the current time zone data will be copied
        to.

    DataSize - Supplies a pointer that on input contains the size of the
        supplied data buffer. On output, will contain the size of the
        current data (whether or not a buffer was supplied).

Return Value:

    STATUS_SUCCESS if the time zone data was accepted.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was valid but too small.

    STATUS_NO_DATA_AVAILABLE if there is no data or the data is empty.

--*/

RTL_API
KSTATUS
RtlSetTimeZoneData (
    PVOID Data,
    ULONG DataSize,
    PCSTR ZoneName,
    PVOID *OldData,
    PULONG OldDataSize,
    PSTR OriginalZoneBuffer,
    PULONG OriginalZoneBufferSize
    );

/*++

Routine Description:

    This routine sets the current time zone data.

Arguments:

    Data - Supplies a pointer to the time zone data to set. No copy will be
        made, the caller must ensure the data is not modified or freed until
        another call to set time zone data completes.

    DataSize - Supplies the size of the data in bytes.

    ZoneName - Supplies an optional pointer to the name of a time zone to
        select within the data. If this pointer is NULL, the first time zone
        in the data will be used.

    OldData - Supplies a pointer where the original (now decommissioned) time
        zone data will be returned.

    OldDataSize - Supplies a pointer where the size of the original
        decommissioned data will be returned.

    OriginalZoneBuffer - Supplies an optional pointer where the original (or
        current if no new time zone was provided) time zone will be returned.

    OriginalZoneBufferSize - Supplies a pointer that on input contains the
        size of the original zone buffer in bytes. On output, this value will
        contain the size of the original zone buffer needed to contain the
        name of the current time zone (even if no buffer was provided).

Return Value:

    STATUS_SUCCESS if the time zone data was accepted.

    STATUS_FILE_CORRUPT if the data is invalid.

    STATUS_NOT_FOUND if the selected time zone could not be found in the new
        data. If this is the case, the new data will not be activated.

--*/

RTL_API
KSTATUS
RtlListTimeZones (
    PVOID Data,
    ULONG DataSize,
    PSTR ListBuffer,
    PULONG ListBufferSize
    );

/*++

Routine Description:

    This routine creates a list of all time zones available in the given (or
    currently in use) data.

Arguments:

    Data - Supplies a pointer to the time zone data to debug print. If this
        is NULL, the current data will be used.

    DataSize - Supplies the size of the data in bytes.

    ListBuffer - Supplies an optional pointer to a buffer where the null
        terminated strings representing the names of the time zones will be
        returned on success. The buffer will be terminated by an empty string.

    ListBufferSize - Supplies a pointer that on input contains the size of the
        list buffer in bytes. On output this will contain the size needed to
        hold all the strings, regardless of whether a buffer was passed in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was too small.

    STATUS_FILE_CORRUPT if the time zone data was not valid.

    STATUS_NO_DATA_AVAILABLE if there is no data or the data is empty.

--*/

RTL_API
VOID
RtlGetTimeZoneNames (
    PCSTR *StandardName,
    PCSTR *DaylightName,
    PLONG StandardGmtOffset,
    PLONG DaylightGmtOffset
    );

/*++

Routine Description:

    This routine returns the names of the currently selected time zone.

Arguments:

    StandardName - Supplies an optional pointer where a pointer to the standard
        time zone name will be returned on success. The caller must not modify
        this memory, and it may change if the time zone is changed.

    DaylightName - Supplies an optional pointer where a pointer to the Daylight
        Saving time zone name will be returned on success. The caller must not
        modify this memory, and it may change if the time zone is changed.

    StandardGmtOffset - Supplies an optional pointer where the offset from GMT
        in seconds will be returned for the time zone.

    DaylightGmtOffset - Supplies an optional pointer where the offset from GMT
        in seconds during Daylight Saving will be returned.

Return Value:

    None.

--*/

RTL_API
KSTATUS
RtlSelectTimeZone (
    PSTR ZoneName,
    PSTR OriginalZoneBuffer,
    PULONG OriginalZoneBufferSize
    );

/*++

Routine Description:

    This routine selects a time zone from the current set of data.

Arguments:

    ZoneName - Supplies an optional pointer to a null terminated string
        containing the name of the time zone. If this parameter is NULL then
        the current time zone will simply be returned.

    OriginalZoneBuffer - Supplies an optional pointer where the original (or
        current if no new time zone was provided) time zone will be returned.

    OriginalZoneBufferSize - Supplies a pointer that on input contains the
        size of the original zone buffer in bytes. On output, this value will
        contain the size of the original zone buffer needed to contain the
        name of the current time zone (even if no buffer was provided).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if a time zone with the given name could not be found.

    STATUS_NO_DATA_AVAILABLE if no time zone data has been set yet.

    STATUS_BUFFER_TOO_SMALL if the buffer provided to get the original time
        zone name was too small. If this is the case, the new time zone will
        not have been set.

--*/

RTL_API
KSTATUS
RtlSystemTimeToLocalCalendarTime (
    PSYSTEM_TIME SystemTime,
    PCALENDAR_TIME CalendarTime
    );

/*++

Routine Description:

    This routine converts the given system time into calendar time in the
    current local time zone.

Arguments:

    SystemTime - Supplies a pointer to the system time to convert.

    CalendarTime - Supplies a pointer to the calendar time to initialize based
        on the given system time.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given system time is too funky.

--*/

RTL_API
KSTATUS
RtlLocalCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts the given calendar time, assumed to be a local date
    and time, into its corresponding system time. On success, this routine will
    update the supplied calendar time to fill out all fields. The GMT offset
    of the supplied calendar time will be ignored in favor or the local time
    zone's GMT offset.

Arguments:

    CalendarTime - Supplies a pointer to the local calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given local calendar time is too funky.

--*/

RTL_API
VOID
RtlDebugPrintTimeZoneData (
    PVOID Data,
    ULONG DataSize
    );

/*++

Routine Description:

    This routine debug prints the given time zone data.

Arguments:

    Data - Supplies a pointer to the time zone data to debug print. If this
        is NULL, the current data will be used.

    DataSize - Supplies the size of the data in bytes.

Return Value:

    None.

--*/

RTL_API
VOID
RtlHeapInitialize (
    PMEMORY_HEAP Heap,
    PHEAP_ALLOCATE AllocateFunction,
    PHEAP_FREE FreeFunction,
    PHEAP_CORRUPTION_ROUTINE CorruptionFunction,
    UINTN MinimumExpansionSize,
    UINTN ExpansionGranularity,
    UINTN AllocationTag,
    ULONG Flags
    );

/*++

Routine Description:

    This routine initializes a memory heap. It does not initialize emergency
    resources.

Arguments:

    Heap - Supplies the heap to initialize.

    AllocateFunction - Supplies a pointer to a function the heap calls when it
        wants to expand and needs more memory.

    FreeFunction - Supplies a pointer to a function the heap calls when it
        wants to free a previously allocated segment.

    CorruptionFunction - Supplies a pointer to a function to call if heap
        corruption is detected.

    MinimumExpansionSize - Supplies the minimum number of bytes to request
        when expanding the heap.

    ExpansionGranularity - Supplies the granularity of expansions, in bytes.
        This must be a power of two.

    AllocationTag - Supplies the magic number to put in each allocation. This
        is also the tag supplied when the allocation function above is called.

    Flags - Supplies a bitfield of flags governing the heap's behavior. See
        MEMORY_HEAP_FLAG_* definitions.

Return Value:

    None.

--*/

RTL_API
VOID
RtlHeapDestroy (
    PMEMORY_HEAP Heap
    );

/*++

Routine Description:

    This routine destroys a memory heap, releasing all resources it was
    managing. The structure itself is owned by the caller, so that isn't freed.

Arguments:

    Heap - Supplies the heap to destroy.

Return Value:

    None. The heap and all its allocations are no longer usable.

--*/

RTL_API
PVOID
RtlHeapAllocate (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine allocates memory from a given heap.

Arguments:

    Heap - Supplies the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

RTL_API
PVOID
RtlHeapReallocate (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN NewSize,
    UINTN AllocationTag
    );

/*++

Routine Description:

    This routine resizes the given allocation, potentially creating a new
    buffer and copying the old contents in.

Arguments:

    Heap - Supplies a pointer to the heap to work with.

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

    AllocationTag - Supplies an identifier for this allocation.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

RTL_API
KSTATUS
RtlHeapAlignedAllocate (
    PMEMORY_HEAP Heap,
    PVOID *Memory,
    UINTN Alignment,
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine allocates aligned memory from a given heap.

Arguments:

    Heap - Supplies the heap to allocate from.

    Memory - Supplies a pointer that receives the pointer to the aligned
        allocation on success.

    Alignment - Supplies the requested alignment for the allocation, in bytes.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Status code.

--*/

RTL_API
VOID
RtlHeapFree (
    PMEMORY_HEAP Heap,
    PVOID Memory
    );

/*++

Routine Description:

    This routine frees memory, making it available for other users in the heap.
    This routine may potentially contract the heap periodically.

Arguments:

    Heap - Supplies the heap to free the memory back to.

    Memory - Supplies the allocation created by the heap allocation routine.

Return Value:

    None.

--*/

RTL_API
VOID
RtlHeapProfilerGetStatistics (
    PMEMORY_HEAP Heap,
    PVOID Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine fills the given buffer with the current heap statistics.

Arguments:

    Heap - Supplies a pointer to the heap.

    Buffer - Supplies the buffer to fill with heap statistics.

    BufferSize - Supplies the size of the buffer.

Return Value:

    None.

--*/

RTL_API
VOID
RtlHeapDebugPrintStatistics (
    PMEMORY_HEAP Heap
    );

/*++

Routine Description:

    This routine prints current heap statistics to the debugger.

Arguments:

    Heap - Supplies a pointer to the heap to print.

Return Value:

    None.

--*/

RTL_API
VOID
RtlValidateHeap (
    PMEMORY_HEAP Heap,
    PHEAP_CORRUPTION_ROUTINE CorruptionRoutine
    );

/*++

Routine Description:

    This routine validates a memory heap for consistency, ensuring that no
    corruption or other errors are present in the heap.

Arguments:

    Heap - Supplies a pointer to the heap to validate.

    CorruptionRoutine - Supplies an optional pointer to a routine to call if
        corruption is detected. If not supplied, the internal one supplied when
        the heap was initialized will be used.

Return Value:

    None.

--*/

RTL_API
ULONG
RtlGetSystemVersionString (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    SYSTEM_VERSION_STRING_VERBOSITY Level,
    PCHAR Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine gets the system string.

Arguments:

    VersionInformation - Supplies a pointer to the initialized version
        information to convert to a string.

    Level - Supplies the level of detail to print.

    Buffer - Supplies a pointer to the buffer that receives the version
        information.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    Returns the size of the string as written to the buffer, including the
    null terminator.

--*/

RTL_API
PSTR
RtlGetReleaseLevelString (
    SYSTEM_RELEASE_LEVEL Level
    );

/*++

Routine Description:

    This routine returns a string corresponding with the given release level.

Arguments:

    Level - Supplies the release level.

Return Value:

    Returns a pointer to a static the string describing the given release
    level. The caller should not attempt to modify or free this memory.

--*/

RTL_API
PSTR
RtlGetBuildDebugLevelString (
    SYSTEM_BUILD_DEBUG_LEVEL Level
    );

/*++

Routine Description:

    This routine returns a string corresponding with the given build debug
    level.

Arguments:

    Level - Supplies the build debug level.

Return Value:

    Returns a pointer to a static the string describing the given build debug
    level. The caller should not attempt to modify or free this memory.

--*/

