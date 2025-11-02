//
//  file: %sys-base.h
//  summary: -[Excerpts from Ren-C's %struct-base.h and %sys-base.h]-
//  project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2025 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Ren-C is licensed under the LGPL 3.0 license, BUT an exemption is made for
// this file, so it can be used in Apache-licensed projects.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//
// In order to implement several "tricks", the first pointer-size slots of
// many datatypes is a `HeaderUnion` union.  Using byte-order-sensitive
// macros like FLAG_LEFT_BIT(), the layout of this header is chosen in such a
// way that not only can Cell pointers be distinguished from Stub pointers,
// but these can be discerned from a valid UTF-8 string just by looking at the
// first byte.  That's a safe C operation since reading a `char*` is not
// subject to "strict aliasing" requirements.
//
// On a semi-superficial level, this permits a kind of dynamic polymorphism,
// such as that used by crash():
//
//     REBVAL* cell = ...;
//     crash (cell);  // can tell this is a Cell
//
//     REBSER* stub = ...;
//     crash (stub)  // can tell this is a Stub
//
//     crash ("Ḧéllŏ");  // can tell this is UTF-8 data (not Stub or Cell)
//
// An even more compelling case is the usage through the API, so variadic
// combinations of strings and values can be intermixed, as in:
//
//     rebElide("poke", block, "1", value)
//
// Internally, the ability to discern these types helps certain structures or
// arrangements from having to find a place to store a kind of "flavor" bit
// for a stored pointer's type.  They can just check the first byte instead.
//
// The generic type covering the superclass is called "Base".
//

#if !defined(SYS_BASE_H_INCLUDED)
#define SYS_BASE_H_INCLUDED


//=//// BASE_FLAG_BASE (leftmost bit) /////////////////////////////////////=//
//
// For the sake of simplicity, the leftmost bit in all Base is always one.
// This is because every UTF-8 string starting with a bit pattern 10xxxxxxx
// in the first byte is invalid.
//
#define BASE_FLAG_BASE \
    FLAG_LEFT_BIT(0)
#define BASE_BYTEMASK_0x80_NODE  0x80


//=//// BASE_FLAG_UNREADABLE (second-leftmost bit) ////////////////////////=//
//
// The second-leftmost bit will be 0 for most Cells and Stubs in the system.
// This gives the most freedom to set the other Base bits independently, since
// the bit pattern 10xxxxxx, is always an invalid leading byte in UTF-8.
//
// But when the bit is set and the pattern is 11xxxxxx, it's still possible
// to cleverly use subsets of the remaining bit patterns for Cells and Stubs
// and avoid conflating with legal UTF-8 states.  See BASE_FLAG_CELL for
// how this is done.
//
// Additional non-UTF-8 states that have BASE_FLAG_UNREADABLE set are
// END_SIGNAL_BYTE, which uses 11000000, and FREE_POOLUNIT_BYTE, which uses
// 110000001... which are the illegal UTF-8 bytes 192 and 193.
//
#define BASE_FLAG_UNREADABLE \
    FLAG_LEFT_BIT(1)
#define BASE_BYTEMASK_0x40_UNREADABLE  0x40


//=//// BASE_FLAG_GC_ONE / BASE_FLAG_GC_TWO (3rd/4th-leftmost bits) ////////=//
//
// Ren-C uses these flags to indicate which pointer fields in Cell or Stub
// need to be marked by the GC.
//
#define BASE_FLAG_GC_ONE \
    FLAG_LEFT_BIT(2)
#define BASE_BYTEMASK_0x20_GC_ONE  0x20

#define BASE_FLAG_GC_TWO \
    FLAG_LEFT_BIT(3)
#define BASE_BYTEMASK_0x10_GC_TWO  0x10


//=//// BASE_FLAG_CELL (fifth-leftmost bit) //////////////////////////////=//
//
// If this bit is set in the header, it indicates the slot the header is for
// is `sizeof(Cell)`.
//
// The position chosen is not random.  It is picked as the 5th bit from the
// left so that unreadable Base can have the pattern:
//
//    11111xxx: Flags: NODE | UNREADABLE | GC_ONE | GC_TWO | CELL | ...
//
// This pattern is for an Not_Cell_Readable() cell, and so long as we set the
// GC_ONE and GC_TWO flags we can still have free choices of `xxx` (e.g.
// arbitrary ROOT, MANAGED, and MARKED flags), while Detect_Rebol_Pointer()
// can be certain it's a Cell and not UTF-8.
//
#define BASE_FLAG_CELL \
    FLAG_LEFT_BIT(4)
#define BASE_BYTEMASK_0x08_CELL  0x08


//=//// BASE_FLAG_MANAGED (sixth-leftmost bit) ////////////////////////////=//
//
// Ren-C uses this to indicate Cells or Stubs that are under GC management.
//
#define BASE_FLAG_MANAGED \
    FLAG_LEFT_BIT(5)
#define BASE_BYTEMASK_0x04_MANAGED  0x04


//=//// BASE_FLAG_ROOT (seventh-leftmost bit) /////////////////////////////=//
//
// Ren-C uses this to indicate Cells or Stubs that are roots for GC purposes.
//
#define BASE_FLAG_ROOT \
    FLAG_LEFT_BIT(6)
#define BASE_BYTEMASK_0x02_ROOT  0x02


//=//// BASE_FLAG_MARKED (eighth-leftmost bit) ////////////////////////////=//
//
// On Stubs, this flag is used by the mark-and-sweep of the garbage collector,
// and should not be referenced outside of %m-gc.c.
//
// 1. THE CHOICE OF BEING THE LAST BIT IS NOT RANDOM.  This means that decayed
//    Stub states can be represented as 11000000 and 11000001, where you have
//    just BASE_FLAG_BASE and BASE_FLAG_STUB plus whether the stub has been
//    marked or not, and these are illegal UTF-8.
//
#define BASE_FLAG_MARKED \
    FLAG_LEFT_BIT(7)
#define BASE_BYTEMASK_0x01_MARKED  0x01

#define DIMINISHED_NON_CANON_BYTE      0xC0  // 11000000: illegal UTF-8 [1]
#define DIMINISHED_CANON_BYTE          0xC1  // 11000001: illegal UTF-8 [1]


// All the illegal UTF-8 bit patterns are in use for some purpose in the
// Cell and Stub space except for these 3 bytes:
//
//        0xF5 (11110101), 0xF6 (11110110), 0xF7 (11110111)
//
// If these were interpreted as flags, it's a stub (no BASE_FLAG_CELL) with:
//
//    11110xxx: Flags: NODE | UNREADABLE | GC_ONE | GC_TWO
//
// 0xF7 is used for END_SIGNAL_BYTE
// 0xF6 is used for FREE_POOLUNIT_BYTE (0x00 conflates with empty UTF-8)
// 0xF5 is BASE_BYTE_WILD that can be used for arbitrary purposes.
//
// 1. At time of writing, the END_SIGNAL_BYTE must always be followed by a
//    zero byte.  It's easy to do with C strings(*see rebEND definition*).
//    Not strictly necessary--one byte suffices--but it's a good sanity check.

#define END_SIGNAL_BYTE  0xF7  // followed by a zero byte [1]
/* STATIC_ASSERT(not (END_SIGNAL_BYTE & BASE_BYTEMASK_0x08_CELL)); */

#define FREE_POOLUNIT_BYTE  0xF6

#define BASE_BYTE_WILD  0xF5  // not BASE_FLAG_CELL, use for whatever purposes


/*
 * In order to leave nullptr available to use in-band for API calls, the cue
 * for reaching the end of a va_list is made a special signal.
 *
 * The first bit being 1 means it's a "Base" (any non-UTF8 pointer for an
 * entity in the interpreter), the second that it's "Unreadable", the third
 * and fourth bits would pertain to GC behavior (if it were applicable), the
 * fifth bit being clear means it's *not* a Cell.  The seventh bit is for GC
 * marking by design (to leverage the special 0xC0 and 0xC1 as marked and
 * unmarked states of "diminished Stubs")
 *
 * rebEND's second byte is 0, coming from the '\0' terminator of the C string.
 * This isn't strictly necessary, as the 0xF7 is enough to know it's not a
 * Cell, Series Stub, or UTF-8.  But it can guard against interpreting garbage
 * input as rebEND, as the sequence {0xF7, 0} is less likely to occur at
 * random than {0xF7, ...}.  And leveraging a literal form means we don't
 * need to define a single byte somewhere to then point at it.
 */

#define rebEND "\xF7"


//=//// ACCESSORS /////////////////////////////////////////////////////////=//

#define BASE_BYTE(p) \
    FIRST_BYTE(m_cast(Base*, (p)))  // base byte always conceptually mutable 

#define FLAG_BASE_BYTE(byte)    FLAG_FIRST_BYTE(byte)


#define Is_Base(p) \
    (cast(Byte*, (p))[0] & BASE_BYTEMASK_0x80_NODE)

#define Is_Base_A_Cell(n)   (did (BASE_BYTE(n) & BASE_BYTEMASK_0x08_CELL))
#define Is_Base_A_Stub(n)   (not Is_Base_A_Cell(n))

#define Is_Base_Marked(n)   (did (BASE_BYTE(n) & BASE_BYTEMASK_0x01_MARKED))
#define Not_Base_Marked(n)  (not Is_Base_Marked(n))

#define Is_Base_Managed(n)  (did (BASE_BYTE(n) & BASE_BYTEMASK_0x04_MANAGED))
#define Not_Base_Managed(n) (not Is_Base_Managed(n))

#define Is_Base_Readable(n) \
    (did (BASE_BYTE(n) & BASE_BYTEMASK_0x40_UNREADABLE))

#define Not_Base_Readable(n) (not Is_Base_Readable(n))

// Is_Base_Root() sounds like it might be the only node.
// Is_Base_A_Root() sounds like a third category vs Is_Base_A_Cell()/Stub()
//
#define Is_Base_Root_Bit_Set(n) \
    (did (BASE_BYTE(n) & BASE_BYTEMASK_0x02_ROOT))

#define Not_Base_Root_Bit_Set(n) \
    (not (BASE_BYTE(n) & BASE_BYTEMASK_0x02_ROOT))

// Add "_Bit" suffix to reinforce lack of higher level function.  (A macro
// with the name Set_Base_Managed() might sound like it does more, like
// removing from the manuals list the way Manage_Stub() etc. do)

#define Set_Base_Root_Bit(n) \
    BASE_BYTE(n) |= BASE_BYTEMASK_0x02_ROOT

#define Clear_Base_Root_Bit(n) \
    BASE_BYTE(n) &= (~ BASE_BYTEMASK_0x02_ROOT)

#define Set_Base_Marked_Bit(n) \
    BASE_BYTE(n) |= BASE_BYTEMASK_0x01_MARKED

#define Clear_Base_Marked_Bit(n) \
    BASE_BYTE(n) &= (~ BASE_BYTEMASK_0x01_MARKED)

#define Set_Base_Managed_Bit(n) \
    BASE_BYTE(n) |= BASE_BYTEMASK_0x04_MANAGED

#define Clear_Base_Managed_Bit(n) \
    BASE_BYTE(n) &= (~ BASE_BYTEMASK_0x04_MANAGED)

#define Set_Base_Unreadable_Bit(n) \
    BASE_BYTE(n) |= BASE_BYTEMASK_0x40_UNREADABLE

#define Clear_Base_Unreadable_Bit(n) \
    BASE_BYTE(n) &= (~ BASE_BYTEMASK_0x40_UNREADABLE)


//=//// POINTER DETECTION (UTF-8, STUB, CELL, END) ////////////////////////=//

typedef enum {
    DETECTED_AS_UTF8 = 1,
    DETECTED_AS_REBVAL,
    DETECTED_AS_REBSER,
    DETECTED_AS_END,  // a rebEND signal (Note: has char* alignment!)
    DETECTED_AS_FREE,
    DETECTED_AS_WILD  // arbitrary out-of-band purposes
} PointerDetect;

#endif  // SYS_BASE_H_INCLUDED
