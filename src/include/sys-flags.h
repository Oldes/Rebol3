//
//  file: %sys-flags.h
//  summary: "Byte-Order Sensitive Bit Flags And Masking"
//  project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2016-2025 Ren-C Open Source Contributors
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
// To facilitate the tricks of the Rebol Base, these macros are purposefully
// arranging bit flags with respect to the "leftmost" and "rightmost" bytes of
// the underlying platform, when encoding them into an unsigned integer the
// size of a platform pointer:
//
//     uintptr_t flags = FLAG_LEFT_BIT(0);
//     Byte byte = *cast(Byte*, &flags);
//
// In the code above, the leftmost bit of the flags has been set to 1, giving
// `byte == 128` on all supported platforms.
//
// These can form *compile-time constants*, which can be singly assigned to
// a uintptr_t in one instruction.  Quantities smaller than a byte can be
// mixed in on with bytes:
//
//    uintptr_t flags
//        = FLAG_LEFT_BIT(0) | FLAG_LEFT_BIT(1) | FLAG_SECOND_BYTE(13);
//
// They can be masked or shifted out efficiently:
//
//    Byte second = SECOND_BYTE(flags);  // == 13
//
// Other tools that might be tried with this all have downsides:
//
// * bitfields arranged in a `union` with integers have no layout guarantee
// * `#pragma pack` is not standard C98 or C99...nor is any #pragma
// * `char[4]` or `char[8]` targets don't usually assign in one instruction
//
//=////////////////////////////////////////////////////////////////////////=//
//
// * Ren-C code used `uintptr_t` and other <stdint.h> types.  R3-Alpha did
//   not assume the availability of these types, defining things like REBUPT.
//   In 2025 it is probably safe to assume C99 <stdint.h> availability, but
//   adapting to use things like REBUPT or i16 as a first cut.
//
// * Byte is also defined by Ren-C (as opposed to REBYTE), but changed here.
//

#if !defined(SYS_FLAGS_H_INCLUDED)
#define SYS_FLAGS_H_INCLUDED

#define PLATFORM_BITS \
    (sizeof(REBUPT) * 8)

#if defined(ENDIAN_BIG)  // Byte w/most significant bit first

    // 63,62,61...or...31,30,20
    #define FLAG_LEFT_BIT(n) \
        (cast(REBUPT, 1) << (PLATFORM_BITS - (n) - 1))

    #define FLAG_FIRST_BYTE(b) \
        (cast(REBUPT, (b)) << (24 + (PLATFORM_BITS - 8)))

    #define FLAG_SECOND_BYTE(b) \
        (cast(REBUPT, (b)) << (16 + (PLATFORM_BITS - 8)))

    #define FLAG_THIRD_BYTE(b) \
        (cast(REBUPT, (b)) << (8 + (PLATFORM_BITS - 32)))

    #define FLAG_FOURTH_BYTE(b) \
        (cast(REBUPT, (b)) << (0 + (PLATFORM_BITS - 32)))

#elif defined(ENDIAN_LITTLE)  // Byte w/least significant bit first (e.g. x86)

    // 7,6,..0|15,14..8|..
    #define FLAG_LEFT_BIT(n) \
        (u_cast(REBUPT, 1) << (7 + ((n) / 8) * 8 - (n) % 8))

    #define FLAG_FIRST_BYTE(b)      cast(REBUPT, (b))
    #define FLAG_SECOND_BYTE(b)     (cast(REBUPT, (b)) << 8)
    #define FLAG_THIRD_BYTE(b)      (cast(REBUPT, (b)) << 16)
    #define FLAG_FOURTH_BYTE(b)     (cast(REBUPT, (b)) << 24)
#else
    // !!! There are macro hacks which can actually make reasonable guesses
    // at endianness, and should possibly be used in the config if nothing is
    // specified explicitly.
    //
    // http://stackoverflow.com/a/2100549/211160
    //
    #error "ENDIAN_BIG or ENDIAN_LITTLE must be defined"
    #include <stophere>  // https://stackoverflow.com/a/45661130
#endif


// Access memory at a pointer as a Byte.  Can act as a read, or a write when
// used as the left-hand side of an assignment.  Strict-aliasing safe.
//
//  https://en.cppreference.com/w/c/language/object.html
//
// 1. The macros are in all-caps to show they are "weird" and usable as
//    LValues.
//
// 2. u_cast() is used for "unhookable const-preserving casts" in the C++
//    build, so if (p) is const Byte* the Byte won't be mutable.  (The C
//    build throws away constness in u_cast(), since it can't "sense" it.)
// 
//    ** Ren-C feature only, u_cast() C++ mechanics missing in Oldes R3
//
// 3. Byte alias for `unsigned char` is used vs. `uint8_t`, due to strict
//    aliasing exemption for char types (some say uint8_t should count...).
//    This means supposedly, it doesn't matter what type the memory you are
//    reading from...you will get the correct up-to-date value of that byte.

#define FIRST_BYTE(p)       u_cast(REBYTE*, (p))[0]  // CAPS_NAME: LValue [1]
#define SECOND_BYTE(p)      u_cast(REBYTE*, (p))[1]  // const-preserving [2]
#define THIRD_BYTE(p)       u_cast(REBYTE*, (p))[2]  // Byte strict exempt [3]
#define FOURTH_BYTE(p)      u_cast(REBYTE*, (p))[3]


#endif  // SYS_FLAGS_H_INCLUDED
