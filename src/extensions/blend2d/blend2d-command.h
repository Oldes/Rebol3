//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// Shared between the entry points (blend2d.c) and the command sources.
//
// The Blend2D API header, the `Handle_BL*` symbols, `Blend2D_thread_count`
// and the `Blend2d_cmd_words` / `Blend2d_arg_words` externs all come from the
// generated header, so it must be included before this one:
//
//     #include "gen-blend2d.h"
//     #include "blend2d-command.h"
//

#ifndef BLEND2D_COMMAND_H
#define BLEND2D_COMMAND_H

//== dialect evaluation buffers ==============================================
// Sizes of the per-call scratch buffers (`doubles` and `arg`) which the
// RESOLVE_* macros below write into. These are locals of each command
// function - not globals - so the dialect stays re-entrant.
#define DOUBLE_BUFFER_SIZE 16
#define ARG_BUFFER_SIZE     8

#ifndef PI
#define PI 3.14159265358979323846264338327950288
#endif

#define TO_RADIANS(value) ((value) *= PI / 180.0)

// Rebol tuple (r.g.b or r.g.b.a) to Blend2D's 0xAARRGGBB. A tuple shorter
// than four bytes is fully opaque.
#define TUPLE_TO_COLOR(t) ( \
	((REBCNT)((t).tuple_len < 4 ? 0xFF : (t).tuple_bytes[3]) << 24) | \
	((REBCNT)(t).tuple_bytes[0] << 16) | \
	((REBCNT)(t).tuple_bytes[1] <<  8) | \
	 (REBCNT)(t).tuple_bytes[2] )

// True when the RXIARG `v` (already known to be RXT_HANDLE) is a live handle
// of the registered type `t`. Guards against handles of a foreign type and
// against handles which were already released.
#define VAL_IS_HANDLE(v, t) ((v).handle.type == (t) && IS_USED_HOB((v).handle.hob))


//== helpers implemented in blend2d-command.c =================================

// Reads a word at `index` and maps it through `words`; TRUE when it is one.
REBOOL fetch_word(REBSER *cmds, REBCNT index, u32 *words, REBCNT *cmd);

// Reads an argument word (or a plain integer) at `index` and converts it into
// a Blend2D enumeration value: `start` is the W_BLEND2D_ARG_* value the
// enumeration begins at, `max` its highest valid value. FALSE when the value
// is not a word of that group, or is out of range - the caller then keeps its
// default and does NOT consume the value.
REBOOL fetch_mode(REBSER *cmds, REBCNT index, REBCNT *result, REBCNT start, REBCNT max);

// Converts a file! (to an OS local path) or a string! argument into a fresh,
// null terminated UTF-8 series, or NULL for any other type. The result is not
// referenced by anything yet - use it before calling other RL_ functions.
REBSER* b2d_file_arg(RXIARG *arg, REBCNT type);


//== helpers implemented by the command sources ===============================

// blend2d-command-image.c
BLResult b2d_init_image_from_file(BLImageCore *image, REBSER *file_name);
BLResult b2d_init_image_from_arg(BLImageCore *image, RXIARG *arg, REBCNT type);

// blend2d-command-path.c - fills `path` from the path dialect in `cmds`
REBCNT b2d_init_path_from_block(BLPathCore *path, REBSER *cmds, REBCNT index);

// blend2d-command-draw.c - resolves the first argument (an image! to draw
// into, or a pair! - the size of a new one) and leaves it in frame slot 1,
// which is what the command returns. NULL when the size is not usable.
REBSER* b2d_target_image(RXIFRM *frm, REBINT *width, REBINT *height);


//== handle callbacks =========================================================
// Registered by Blend2d_Init() in blend2d.c. The free callbacks receive the
// handle's data pointer (no HANDLE_REQUIRES_HOB_ON_FREE flag is used), the
// accessors receive the handle context itself.

// Every field of every handle reports on Blend2D's own state, so all of them
// are read-only - this is what all three types register as their set_path.
int BL_set_path_readonly(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int BLImage_free(void *data);
int BLImage_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int BLImage_mold(REBHOB *hob, REBSER *str);

int BLPath_free(void *data);
int BLPath_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int BLPath_mold(REBHOB *hob, REBSER *str);

int BLFontFace_free(void *data);
int BLFontFace_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int BLFontFace_mold(REBHOB *hob, REBSER *str);


//== dialect argument resolution ==============================================
// Each macro consumes one value of the command block at `index` and expects
// these locals to be in scope: `cmds`, `index`, `type`, `arg[]`, `doubles[]`
// and an `error:` label. `a` is a slot in `arg`, `d` a slot in `doubles`.

#define RESOLVE_ARG(a) (type = RL_GET_VALUE_RESOLVED(cmds, index++, &arg[a]));

#define RESOLVE_PAIR_ARG(a, d) RESOLVE_ARG(a) \
	if (type != RXT_PAIR) goto error; \
	doubles[d]   = arg[a].pair.x; \
	doubles[d+1] = arg[a].pair.y;

#define RESOLVE_PAIR_ARG_OPTIONAL(a, d) RESOLVE_ARG(a) \
	if (type == RXT_PAIR) { \
		doubles[d]   = arg[a].pair.x; \
		doubles[d+1] = arg[a].pair.y; \
	} else { type = 0; index--; }

#define RESOLVE_INT_ARG(a) RESOLVE_ARG(a) \
	if (type != RXT_INTEGER) goto error;

#define RESOLVE_INT_ARG_OPTIONAL(a) RESOLVE_ARG(a) \
	if (type != RXT_INTEGER) { type = 0; index--; }

#define RESOLVE_NUMBER_ARG(a, d) RESOLVE_ARG(a) \
	if (type == RXT_DECIMAL || type == RXT_PERCENT) doubles[d] = arg[a].dec64; \
	else if (type == RXT_INTEGER) doubles[d] = (double)arg[a].int64; \
	else goto error;

#define RESOLVE_NUMBER_ARG_OPTIONAL(a, d) RESOLVE_ARG(a) \
	if (type == RXT_DECIMAL || type == RXT_PERCENT) doubles[d] = arg[a].dec64; \
	else if (type == RXT_INTEGER) doubles[d] = (double)arg[a].int64; \
	else { type = 0; index--; }

#define RESOLVE_NUMBER_OR_PAIR_ARG(a, d) RESOLVE_ARG(a) \
	if (type == RXT_DECIMAL || type == RXT_PERCENT) doubles[d] = doubles[d+1] = arg[a].dec64; \
	else if (type == RXT_INTEGER) doubles[d] = doubles[d+1] = (double)arg[a].int64; \
	else if (type == RXT_PAIR) { \
		doubles[d]   = arg[a].pair.x; \
		doubles[d+1] = arg[a].pair.y; \
	} else goto error;

#define RESOLVE_STRING_ARG(a) RESOLVE_ARG(a) \
	if (type != RXT_STRING) goto error;

#define FETCH_3_PAIRS(c, i, a1, a2, a3) ( \
	   RXT_PAIR == RL_GET_VALUE(c, i,   &a1) \
	&& RXT_PAIR == RL_GET_VALUE(c, i+1, &a2) \
	&& RXT_PAIR == RL_GET_VALUE(c, i+2, &a3))

#define ARG_X(n) (arg[n].pair.x)
#define ARG_Y(n) (arg[n].pair.y)

#define OPT_WORD_FLAG(flag, name) \
	if (fetch_word(cmds, index, Blend2d_arg_words, &cmd) && cmd == name) { flag = TRUE; index++; }

#define DRAW_GEOMETRY(ctx, mode, data) \
	if (has_fill  ) bl_context_fill_geometry  (&ctx, mode, data); \
	if (has_stroke) bl_context_stroke_geometry(&ctx, mode, data);

#endif // BLEND2D_COMMAND_H