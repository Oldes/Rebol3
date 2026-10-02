//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `font` command and the BLFontFace handle.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

static const REBYTE *ERR_NO_HANDLE = (const REBYTE*)"Blend2D failed to make a font handle!";
static const REBYTE *ERR_BAD_FILE  = (const REBYTE*)"Blend2D failed to load the font file!";


COMMAND cmd_blend2d_font(RXIFRM *frm, void *ctx) {
	BLResult r;
	BLFontFaceCore *face;
	REBHOB *hob;
	REBSER *file;

	// Converted before the handle is made: the conversion allocates a series,
	// and nothing references the fresh handle yet.
	file = b2d_file_arg(&RXA_ARG(frm, 1), RXA_TYPE(frm, 1));
	if (file == NULL) RETURN_ERROR(ERR_BAD_FILE);

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_BLFontFace);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	face = (BLFontFaceCore*)hob->data;
	bl_font_face_init(face);

	r = bl_font_face_create_from_file(face, SERIES_TEXT(file), BL_FILE_READ_MMAP_ENABLED | BL_FILE_READ_MMAP_AVOID_SMALL);
	if (r != BL_SUCCESS) {
		debug_print("Failed to load font: %s, reason: %i\n", SERIES_TEXT(file), r);
		RETURN_ERROR(ERR_BAD_FILE);
	}
	RETURN_HANDLE(hob);
}


//== handle callbacks =========================================================

int BLFontFace_free(void *data) {
	debug_print("releasing font face: %p\n", data);
	if (data) bl_font_face_destroy((BLFontFaceCore*)data);
	return 0;
}

int BLFontFace_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	BLFontFaceInfo info;
	bl_font_face_get_face_info((BLFontFaceCore*)hob->data, &info);

	*type = RXT_INTEGER;
	switch (RL_FIND_WORD(Blend2d_arg_words, word)) {
	case W_BLEND2D_ARG_GLYPHS:       arg->int64 = (i64)info.glyph_count;  break;
	case W_BLEND2D_ARG_FACE_TYPE:    arg->int64 = (i64)info.face_type;    break;
	case W_BLEND2D_ARG_OUTLINE_TYPE: arg->int64 = (i64)info.outline_type; break;
	case W_BLEND2D_ARG_REVISION:     arg->int64 = (i64)info.revision;    break;
	case W_BLEND2D_ARG_FACE_INDEX:   arg->int64 = (i64)info.face_index;   break;
	case W_BLEND2D_ARG_FACE_FLAGS:   arg->int64 = (i64)info.face_flags;   break;
	case W_BLEND2D_ARG_DIAG_FLAGS:   arg->int64 = (i64)info.diag_flags;   break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

int BLFontFace_mold(REBHOB *hob, REBSER *str) {
	BLFontFaceInfo info;
	int len = 0;

	if (!str || !hob || !hob->data) return 0;
	SERIES_TAIL(str) = 0;
	bl_font_face_get_face_info((BLFontFaceCore*)hob->data, &info);

	APPEND_STRING(str, "0#%lx glyphs: %i",
		(unsigned long)(uintptr_t)hob->data, (int)info.glyph_count);
	return len;
}