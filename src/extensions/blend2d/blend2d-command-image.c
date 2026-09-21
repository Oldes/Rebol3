//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `image` command and the BLImage handle.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

static const REBYTE *ERR_NO_HANDLE = (const REBYTE*)"Blend2D failed to make an image handle!";
static const REBYTE *ERR_BAD_IMAGE = (const REBYTE*)"Blend2D failed to initialize the image!";


BLResult b2d_init_image_from_file(BLImageCore *image, REBSER *file_name) {
	BLArrayCore codecs;
	BLResult r;
	blImageCodecArrayInitBuiltInCodecs(&codecs);
	r = blImageReadFromFile(image, SERIES_TEXT(file_name), &codecs);
	blArrayReset(&codecs);
	return r;
}

// Initializes `image` from a Rebol value: an image! is wrapped in place (the
// pixels are shared, not copied), a file! is decoded, a pair! makes a new
// empty image of that size.
BLResult b2d_init_image_from_arg(BLImageCore *image, RXIARG *arg, REBCNT type) {
	REBSER *file;
	switch (type) {
	case RXT_IMAGE:
		blImageReset(image);
		return blImageCreateFromData(image, arg->width, arg->height, BL_FORMAT_PRGB32,
			SERIES_DATA((REBSER*)arg->image), (intptr_t)arg->width * 4, BL_DATA_ACCESS_RW, NULL, NULL);
	case RXT_FILE:
		file = b2d_file_arg(arg, type);
		if (file == NULL) return BL_ERROR_INVALID_VALUE;
		blImageReset(image);
		return b2d_init_image_from_file(image, file);
	case RXT_PAIR:
		// NOTE: the size of a pair! is in `pair.x`/`pair.y`; the `width` and
		// `height` fields of the union belong to the image! layout only.
		blImageReset(image);
		return blImageCreate(image, ROUND_TO_INT(arg->pair.x), ROUND_TO_INT(arg->pair.y), BL_FORMAT_PRGB32);
	}
	return BL_ERROR_INVALID_VALUE;
}


COMMAND cmd_blend2d_image(RXIFRM *frm, void *ctx) {
	BLResult r;
	BLImageCore *image;
	REBHOB *hob = RL_MAKE_HANDLE_CONTEXT(Handle_BLImage);

	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	image = (BLImageCore*)hob->data;
	debug_print("New image handle: %u data: %p\n", hob->sym, (void*)hob->data);
	blImageInit(image);

	r = b2d_init_image_from_arg(image, &RXA_ARG(frm, 1), RXA_TYPE(frm, 1));
	if (r != BL_SUCCESS) RETURN_ERROR(ERR_BAD_IMAGE);

	// When the pixels belong to a Rebol image!, they are shared and not
	// copied - keeping the series on the handle is what stops the garbage
	// collector from freeing the buffer Blend2D still writes into.
	if (RXA_TYPE(frm, 1) == RXT_IMAGE) hob->series = (REBSER*)RXA_ARG(frm, 1).image;

	RETURN_HANDLE(hob);
}


//== handle callbacks =========================================================

int BLImage_free(void *data) {
	debug_print("releasing image: %p\n", data);
	if (data) blImageDestroy((BLImageCore*)data);
	return 0;
}

int BLImage_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	BLImageData data;
	if (blImageGetData((BLImageCore*)hob->data, &data) != BL_SUCCESS) return PE_BAD_SELECT;

	switch (RL_FIND_WORD(Blend2d_arg_words, word)) {
	case W_BLEND2D_ARG_SIZE:
		*type = RXT_PAIR;
		arg->pair.x = (float)data.size.w;
		arg->pair.y = (float)data.size.h;
		break;
	case W_BLEND2D_ARG_WIDTH:
		*type = RXT_INTEGER;
		arg->int64 = (i64)data.size.w;
		break;
	case W_BLEND2D_ARG_HEIGHT:
		*type = RXT_INTEGER;
		arg->int64 = (i64)data.size.h;
		break;
	case W_BLEND2D_ARG_FORMAT:
		*type = RXT_INTEGER;
		arg->int64 = (i64)data.format;
		break;
	case W_BLEND2D_ARG_STRIDE:
		*type = RXT_INTEGER;
		arg->int64 = (i64)data.stride;
		break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

int BLImage_mold(REBHOB *hob, REBSER *str) {
	BLImageData data;
	int len = 0;

	if (!str || !hob || !hob->data) return 0;
	SERIES_TAIL(str) = 0;
	if (blImageGetData((BLImageCore*)hob->data, &data) != BL_SUCCESS) return 0;

	APPEND_STRING(str, "0#%lx %ix%i", (unsigned long)(uintptr_t)hob->data, (int)data.size.w, (int)data.size.h);
	return len;
}