//
// Project: Rebol/QOI extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// The QOI codec dispatcher and the single command which hands it over.
//
// The dispatcher has the signature DO-CODEC expects (`codo`), so the codec
// object built in the mezzanine goes through exactly the same path in
// sys-codec.reb as the internal codecs do:
//
//     decode 'qoi bin  ->  do-codec system/codecs/qoi/entry 'decode bin
//
// Two things this file is careful about, both of which the in-tree u-qoi.c
// gets to ignore because it lives inside the interpreter:
//
//   1. DO-CODEC releases the codec's output with Free_Mem(ptr, size). That
//      pairs with RL_ALLOC only. RL_MEM_ALLOC must NOT be used here - it
//      returns a pointer two words past the real allocation and, for small
//      sizes, one that belongs to a memory pool rather than to malloc.
//
//   2. QOI stores the channels as R,G,B,A in that order, while a Rebol
//      image! keeps them in the platform's pixel order. The two agree only
//      where C_R/C_G/C_B happen to be 0/1/2 (Android), so the permutation is
//      done explicitly instead of trusting a memcpy that was tested on one
//      machine.
//

#include "gen-qoi.h"

#include <stdlib.h>
#include <string.h>

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"

// The QOI header is 14 bytes; anything shorter cannot even be identified.
#define QOI_HEADER_SIZE 14

static const REBYTE *ERR_NO_CODEC_WORD =
	(const REBYTE*)"The `codec` word could not be mapped - the host is too old for this extension.";

// Word id of `codec`, the handle type DO-CODEC accepts. Resolved once by
// Qoi_Init, because RL_MAP_WORD interns a word and there is no reason to do
// that on every call.
u32 Qoi_codec_word = 0;


//== buffers ==================================================================

// Moves a buffer which qoi.h allocated with malloc into one the interpreter
// can free, and releases the original.
//
// The extra copy is the price of leaving the vendored qoi.h untouched: its
// QOI_MALLOC and QOI_FREE are guarded by a single #ifndef, so overriding the
// allocator means overriding the deallocator too, and RL_FREE needs a size
// which qoi.h does not pass to QOI_FREE. Worth revisiting only if encoding
// large images ever shows up in a profile.
static void *To_Rebol_Buffer(void *src, size_t size)
{
	void *dst = NULL;

	if (!src) return NULL;
	if (size > 0 && (dst = RL_ALLOC(size)) != NULL)
		memcpy(dst, src, size);
	free(src);
	return dst;
}


//== channel order ============================================================

static void Pixels_To_RGBA(REBYTE *dst, const REBYTE *src, REBCNT pixels)
{
	for (; pixels > 0; pixels--, dst += 4, src += 4) {
		dst[0] = src[C_R];
		dst[1] = src[C_G];
		dst[2] = src[C_B];
		dst[3] = src[C_A];
	}
}

// In place - the transformation is a permutation of the four bytes.
static void RGBA_To_Pixels(REBYTE *buf, REBCNT pixels)
{
	REBYTE r, g, b, a;

	for (; pixels > 0; pixels--, buf += 4) {
		r = buf[0]; g = buf[1]; b = buf[2]; a = buf[3];
		buf[C_R] = r; buf[C_G] = g; buf[C_B] = b; buf[C_A] = a;
	}
}


//== codec actions ============================================================

static void Encode_QOI_Image(REBCDI *codi)
{
	qoi_desc desc;
	REBCNT   pixels = (REBCNT)codi->w * (REBCNT)codi->h;
	REBYTE  *rgba;
	void    *out;
	int      out_len = 0;

	codi->data = NULL;
	codi->len  = 0;

	if (pixels == 0) {
		codi->error = CODI_ERR_BAD_DATA;
		return;
	}

	desc.width      = (unsigned int)codi->w;
	desc.height     = (unsigned int)codi->h;
	desc.channels   = 4;
	desc.colorspace = QOI_SRGB;

	// A scratch copy in QOI's channel order; the image itself must not be
	// modified by an encode.
	rgba = (REBYTE*)malloc((size_t)pixels * 4);
	if (!rgba) {
		codi->error = CODI_ERR_BAD_DATA;
		return;
	}
	Pixels_To_RGBA(rgba, (const REBYTE*)codi->bits, pixels);

	out = qoi_encode(rgba, &desc, &out_len);
	free(rgba);

	if (!out || out_len <= 0) {
		free(out);
		codi->error = CODI_ERR_ENCODING;
		return;
	}

	codi->data = (unsigned char*)To_Rebol_Buffer(out, (size_t)out_len);
	if (!codi->data) {
		codi->error = CODI_ERR_BAD_DATA;
		return;
	}
	codi->len   = (u32)out_len;
	codi->error = 0;
}

static void Decode_QOI_Image(REBCDI *codi)
{
	qoi_desc desc;
	void    *px;
	REBCNT   pixels;

	codi->bits = NULL;

	if (codi->len < QOI_HEADER_SIZE) {
		codi->error = CODI_ERR_SIGNATURE;
		return;
	}

	px = qoi_decode(codi->data, (int)codi->len, &desc, 4);
	if (!px) {
		codi->error = CODI_ERR_BAD_DATA;
		return;
	}

	pixels  = (REBCNT)desc.width * (REBCNT)desc.height;
	codi->w = (int)desc.width;
	codi->h = (int)desc.height;

	codi->bits = (unsigned int*)To_Rebol_Buffer(px, (size_t)pixels * 4);
	if (!codi->bits) {
		codi->error = CODI_ERR_BAD_DATA;
		return;
	}
	RGBA_To_Pixels((REBYTE*)codi->bits, pixels);
	codi->error = 0;
}

static void Identify_QOI_Image(REBCDI *codi)
{
	int p = 0;

	// NOTE: qoi_read_32 reads four bytes unconditionally, so the length has
	// to be checked first. ENCODING? offers every codec whatever binary it
	// was given, including a two byte one.
	if (codi->len < QOI_HEADER_SIZE) {
		codi->error = 1;   // inverted result: not a QOI image
		return;
	}
	codi->error = (qoi_read_32(codi->data, &p) != QOI_MAGIC);
}


//== dispatcher ===============================================================

// Matches `codo` in reb-codec.h - this is the function DO-CODEC calls
// through the handle returned below.
static REBINT Codec_QOI_Image(REBCDI *codi)
{
	codi->error = 0;

	switch (codi->action) {
	case CODI_IDENTIFY:
		Identify_QOI_Image(codi);
		return CODI_CHECK;   // error code is the inverted result

	case CODI_DECODE:
		Decode_QOI_Image(codi);
		return CODI_IMAGE;

	case CODI_ENCODE:
		Encode_QOI_Image(codi);
		return CODI_BINARY;
	}

	codi->error = CODI_ERR_NA;
	return CODI_ERROR;
}


//== commands =================================================================

COMMAND cmd_qoi_codec_handle(RXIFRM *frm, void *ctx)
{
	if (!Qoi_codec_word) RETURN_ERROR(ERR_NO_CODEC_WORD);

	// A plain function handle - there is no context to allocate and nothing
	// for the GC to mark, which is the same shape SET_HANDLE gives the
	// internal codecs in Register_Codec.
	RXA_HANDLE(frm, 1)       = (void*)Codec_QOI_Image;
	RXA_HANDLE_TYPE(frm, 1)  = Qoi_codec_word;
	RXA_HANDLE_FLAGS(frm, 1) = HANDLE_FUNCTION;
	RXA_TYPE(frm, 1)         = RXT_HANDLE;
	return RXR_VALUE;
}
