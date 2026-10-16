//
// Project: Rebol/Triangulate extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Shared between the entry points (triangulate.c) and the command sources.
//
// `REAL` comes from the generated header, via the specification's `c-header:`
// field, so it must be included before this one:
//
//     #include "gen-triangulate.h"
//     #include "triangulate.h"
//

#ifndef TRIANGULATE_EXT_H
#define TRIANGULATE_EXT_H

#ifndef REAL
#error "REAL must be defined before including triangle.h - include gen-triangulate.h first!"
#endif

// Under ANSI_DECLARATORS `triangle.h` spells the argument of trifree() with
// VOID, which Triangle only defines inside triangle.c - that does not reach
// the other translation units. `int` is what Triangle itself uses.
#ifndef VOID
#define VOID int
#endif

// Jonathan Shewchuk's mesh generator. Built with TRILIBRARY, so `triangulate()`
// is the whole interface and no file IO is compiled in.
#include "triangle.h"

// Triangulate_Init() is declared in gen-triangulate.h - the generated `_init`
// handler calls it, and every extension is required to define one.

// OS_Init_Ext_Triangulate() is declared in the generated header as well, for
// the embedded build only.

#endif // TRIANGULATE_EXT_H