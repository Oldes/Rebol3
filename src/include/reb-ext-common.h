/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012-2026 Rebol Open Source Contributors
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
************************************************************************
**
**  Summary: Convenience macros shared by extension command sources
**  Module:  reb-ext-common.h
**  Author:  Oldes
**  Notes:
**      Deliberately includes nothing. The extension decides which
**      header provides the API - `reb-host.h` for an extension built
**      inside this repository (either embedded or as a library), or
**      the amalgamated `rebol-extension.h` for one built out of tree.
**      List both in the specification's `c-include:`, API header first.
**
***********************************************************************/

#ifndef REB_EXT_COMMON_H
#define REB_EXT_COMMON_H

// Return type of every command function. The dispatch table requires
// them all to share this signature; see the generated commands table.
#define COMMAND int


/***********************************************************************
**  Argument access
**
**  `frm` is the command frame; argument numbering is 1-based and counts
**  refinements as slots of their own, with a refinement's own arguments
**  following it. For `foo: command [a /part len]`, `a` is 1, `/part`
**  is 2 and `len` is 3.
***********************************************************************/

#define ARG_Double(n)         RXA_DEC64(frm,n)
#define ARG_Float(n)          (float)RXA_DEC64(frm,n)
#define ARG_Int32(n)          RXA_INT32(frm,n)
#define ARG_Int64(n)          RXA_INT64(frm,n)
#define ARG_Handle_Series(n)  RXA_HANDLE_CONTEXT(frm, n)->series

// Series argument which may legitimately be none.
#define OPT_SERIES(n)         (RXA_TYPE(frm,n) == RXT_NONE ? NULL : RXA_SERIES(frm, n))

// Tests that argument `n` is a handle of the registered type `t`.
#define FRM_IS_HANDLE(n, t)   (RXA_TYPE(frm,n) == RXT_HANDLE && RXA_HANDLE_TYPE(frm, n) == t)


/***********************************************************************
**  Returning
***********************************************************************/

// Returns an error described by a null terminated C string.
//
// NOTE: on this path args[1].series is NOT a series - the interpreter
// reads it back as a `const REBYTE*`, measures it with LEN_BYTES and
// copies it into a fresh Rebol string, so a static string is correct
// and its lifetime is not an issue. The cast only silences the type.
#define RETURN_ERROR(err) \
	do { RXA_SERIES(frm, 1) = (REBSER*)(err); return RXR_ERROR; } while(0)

#define RETURN_HANDLE(hob)                   \
	RXA_HANDLE(frm, 1)       = hob;          \
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;     \
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;   \
	RXA_TYPE(frm, 1) = RXT_HANDLE;           \
	return RXR_VALUE


/***********************************************************************
**  String building
**
**  Appends printf-formatted text to a Rebol string series, expanding it
**  when needed. Requires an `int len` (or REBLEN/size_t) in scope.
***********************************************************************/

#define APPEND_STRING(str, ...) \
	len = snprintf(NULL,0,__VA_ARGS__);\
	if (len > (int)(SERIES_REST(str)-SERIES_LEN(str))) {\
		RL_EXPAND_SERIES(str, SERIES_TAIL(str), len);\
		SERIES_TAIL(str) -= len;\
	}\
	len = snprintf( \
		SERIES_TEXT(str)+SERIES_TAIL(str),\
		SERIES_REST(str)-SERIES_TAIL(str),\
		__VA_ARGS__\
	);\
	SERIES_TAIL(str) += len;


/***********************************************************************
**  Tracing (compiled out unless USE_TRACES is defined)
***********************************************************************/

#ifdef  USE_TRACES
#include <stdio.h>
#define debug_print(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)
#define trace(str) puts(str)
#else
#define debug_print(fmt, ...)
#define trace(str)
#endif

#endif // REB_EXT_COMMON_H