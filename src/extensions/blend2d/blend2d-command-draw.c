//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `draw` command - evaluates the drawing dialect into a Rebol image.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

static const REBYTE *ERR_TARGET  = (const REBYTE*)"Blend2D failed to attach the target image!";
static const REBYTE *ERR_CONTEXT = (const REBYTE*)"Blend2D failed to initialize a rendering context!";
static const REBYTE *ERR_DRAW    = (const REBYTE*)"Blend2D draw failed!";


// The first argument is either an image! to draw into, or a pair! - the size
// of a new image. Either way the frame's first slot is left holding the image
// which the command returns.
REBSER* b2d_target_image(RXIFRM *frm, REBINT *width, REBINT *height) {
	REBSER *reb_img;
	REBINT w, h;

	if (RXA_TYPE(frm, 1) == RXT_PAIR) {
		REBXYF size = RXA_PAIR(frm, 1);
		w = ROUND_TO_INT(size.x);
		h = ROUND_TO_INT(size.y);
		if (w <= 0 || h <= 0) return NULL;
		reb_img = (REBSER*)RL_MAKE_IMAGE(w, h);
	}
	else {
		w = RXA_IMAGE_WIDTH(frm, 1);
		h = RXA_IMAGE_HEIGHT(frm, 1);
		reb_img = (REBSER*)RXA_ARG(frm, 1).image;
	}
	if (reb_img == NULL || w <= 0 || h <= 0) return NULL;

	RXA_TYPE(frm, 1) = RXT_IMAGE;
	RXA_ARG(frm, 1).image  = reb_img;
	RXA_ARG(frm, 1).width  = w;
	RXA_ARG(frm, 1).height = h;

	*width  = w;
	*height = h;
	return reb_img;
}


COMMAND cmd_blend2d_draw(RXIFRM *frm, void *reb_ctx) {
	BLResult r;
	BLImageCore img_target, img_pattern, img;
	BLImageCore *current_img = NULL;
	BLPatternCore pattern;
	BLGradientCore gradient;
	BLPathCore path;
	BLFontCore font;
	BLFontFaceCore font_face;
	BLFontFaceCore *font_face_ext = NULL;

	BLContextCore ctx;
	BLContextCreateInfo cci;
	BLRect  rect;
	BLRectI rectI;
	BLPoint pt;
	BLPoint origin = { 0, 0 };

	REBINT  w, h;
	REBSER *reb_img;
	REBSER *cmds;
	REBCNT  index, cmd_pos = 0, cmd = 0, type, mode, count, i;
	REBCNT  err = BL_SUCCESS;
	REBI64  cap;
	REBOOL  has_fill = FALSE, has_stroke = FALSE;
	REBDEC  offset, sz, width;
	REBDEC  font_size = 10.0;
	REBDEC  point[4] = { 0, 0, 1.0, 1.0 }; // default size of the point is 2px

	REBDEC doubles[DOUBLE_BUFFER_SIZE];
	RXIARG arg[ARG_BUFFER_SIZE];

	blPathInit(&path);
	blImageInit(&img_target);
	blImageInit(&img_pattern);
	blImageInit(&img);
	blFontInit(&font);
	blFontFaceInit(&font_face);

	reb_img = b2d_target_image(frm, &w, &h);
	if (reb_img == NULL) RETURN_ERROR(ERR_TARGET);

	r = blImageCreateFromData(&img_target, w, h, BL_FORMAT_PRGB32,
		SERIES_DATA(reb_img), (intptr_t)w * 4, BL_DATA_ACCESS_WRITE, NULL, NULL);
	if (r != BL_SUCCESS) RETURN_ERROR(ERR_TARGET);

	memset(&cci, 0, sizeof(cci));
	cci.threadCount = Blend2D_thread_count;

	r = blContextInitAs(&ctx, &img_target, &cci);
	if (r != BL_SUCCESS) {
		blImageReset(&img_target);
		RETURN_ERROR(ERR_CONTEXT);
	}

	cmds  = RXA_SERIES(frm, 2);
	index = RXA_INDEX(frm, 2);

	while (index < SERIES_TAIL(cmds)) {
		if (!fetch_word(cmds, index++, Blend2d_cmd_words, &cmd)) {
			trace("expected word as a command!");
			goto error;
		}
	process_cmd: // reached from the error loop, which skips values until it finds a command

		cmd_pos = index; // used to report the error position
		debug_print("cmd index: %u cmd: %u\n", index, cmd);
		switch (cmd) {

		case W_BLEND2D_CMD_FILL:
		case W_BLEND2D_CMD_FILL_PEN:

			RESOLVE_ARG(0)
			if (RXT_TUPLE == type) {
				blContextSetFillStyleRgba32(&ctx, TUPLE_TO_COLOR(arg[0]));
				has_fill = TRUE;
			}
			else if (type == RXT_LOGIC) {
				if (!arg[0].int32a) has_fill = FALSE;
			}
			else if (type == RXT_HANDLE) {
				if (!VAL_IS_HANDLE(arg[0], Handle_BLImage)) goto error;
				current_img = (BLImageCore*)arg[0].handle.hob->data;
				goto pattern_mode;
			}
			else if (type == RXT_IMAGE) {
				blImageReset(&img_pattern);
				r = blImageCreateFromData(&img_pattern, arg[0].width, arg[0].height, BL_FORMAT_PRGB32,
					SERIES_DATA((REBSER*)arg[0].image), (intptr_t)arg[0].width * 4, BL_DATA_ACCESS_READ, NULL, NULL);
				if (r != BL_SUCCESS) goto error;
				current_img = &img_pattern;
			pattern_mode:
				if (fetch_mode(cmds, index, &mode, W_BLEND2D_ARG_PAD, BL_EXTEND_MODE_MAX_VALUE)) {
					index++;
				} else {
					mode = BL_EXTEND_MODE_REPEAT;
				}
				blPatternInitAs(&pattern, current_img, NULL, mode, NULL);
				blContextSetFillStyle(&ctx, &pattern);
				has_fill = TRUE;
				blPatternReset(&pattern);
			}
			else if (RXT_UNSET == type && fetch_mode(cmds, index - 1, &mode, W_BLEND2D_ARG_LINEAR, BL_GRADIENT_TYPE_MAX_VALUE)) {
				// gradient fill
				blGradientInit(&gradient);
				blGradientCreate(&gradient, mode, doubles, 0, NULL, 0, NULL);

				RESOLVE_ARG(0)
				while (type == RXT_TUPLE) {
					RESOLVE_NUMBER_ARG(2, 1);
					offset = doubles[1];
					blGradientAddStopRgba32(&gradient, offset, TUPLE_TO_COLOR(arg[0]));
					RESOLVE_ARG(0)
				}
				index--;

				RESOLVE_PAIR_ARG(0, 0);
				if (mode == BL_GRADIENT_TYPE_RADIAL) {
					RESOLVE_PAIR_ARG(0, 2);
					RESOLVE_NUMBER_ARG(3, 4);
					blGradientSetValues(&gradient, 0, doubles, 5);
				}
				else if (mode == BL_GRADIENT_TYPE_LINEAR) {
					RESOLVE_PAIR_ARG(0, 2);
					blGradientSetValues(&gradient, 0, doubles, 4);
				}
				else { // conical
					doubles[2] = 0; RESOLVE_NUMBER_ARG_OPTIONAL(0, 2)
					doubles[3] = 1; RESOLVE_NUMBER_ARG_OPTIONAL(1, 3)
					blGradientSetExtendMode(&gradient, BL_EXTEND_MODE_REPEAT);
					blGradientSetValues(&gradient, 0, doubles, 4);
				}
				blContextSetFillStyle(&ctx, &gradient);
				has_fill = TRUE;
				blGradientReset(&gradient);
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_PEN:

			RESOLVE_ARG(0)
			if (RXT_TUPLE == type) {
				blContextSetStrokeStyleRgba32(&ctx, TUPLE_TO_COLOR(arg[0]));
				has_stroke = TRUE;
			}
			else if (type == RXT_LOGIC) {
				if (!arg[0].int32a) has_stroke = FALSE;
			}
			else if (type == RXT_HANDLE) {
				if (!VAL_IS_HANDLE(arg[0], Handle_BLImage)) goto error;
				current_img = (BLImageCore*)arg[0].handle.hob->data;
				goto stroke_pattern_mode;
			}
			else if (type == RXT_IMAGE) {
				// NOTE: reset first - the previous pattern image, if any, is
				// still held here (the old code initialized it a second time).
				blImageReset(&img_pattern);
				r = blImageCreateFromData(&img_pattern, arg[0].width, arg[0].height, BL_FORMAT_PRGB32,
					SERIES_DATA((REBSER*)arg[0].image), (intptr_t)arg[0].width * 4, BL_DATA_ACCESS_READ, NULL, NULL);
				if (r != BL_SUCCESS) {
					trace("failed to init pattern image!");
					goto error;
				}
				current_img = &img_pattern;
			stroke_pattern_mode:
				if (fetch_mode(cmds, index, &mode, W_BLEND2D_ARG_PAD, BL_EXTEND_MODE_MAX_VALUE)) {
					index++;
				} else {
					mode = BL_EXTEND_MODE_REPEAT;
				}
				blPatternInitAs(&pattern, current_img, NULL, mode, NULL);
				// NOTE: this used to set the FILL style, so a pattern pen
				// silently replaced the fill and never stroked anything.
				blContextSetStrokeStyle(&ctx, &pattern);
				has_stroke = TRUE;
				blPatternReset(&pattern);
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_LINE:
			if (has_stroke) {
				RESOLVE_ARG(0)
				if (RXT_PAIR == type) {
					blPathMoveTo(&path, (double)arg[0].pair.x, (double)arg[0].pair.y);
					type = RL_GET_VALUE(cmds, index, &arg[0]);
					while (RXT_PAIR == type) {
						blPathLineTo(&path, (double)arg[0].pair.x, (double)arg[0].pair.y);
						type = RL_GET_VALUE(cmds, ++index, &arg[0]);
					}
				}
				else if (RXT_BLOCK == type) {
					REBSER *blk = (REBSER*)arg[0].series;
					REBCNT  n   = arg[0].index;
					type = RL_GET_VALUE(blk, n, &arg[0]);
					if (RXT_PAIR != type) goto error;
					blPathMoveTo(&path, (double)arg[0].pair.x, (double)arg[0].pair.y);
					type = RL_GET_VALUE(blk, ++n, &arg[0]);
					while (RXT_PAIR == type) {
						blPathLineTo(&path, (double)arg[0].pair.x, (double)arg[0].pair.y);
						type = RL_GET_VALUE(blk, ++n, &arg[0]);
					}
				}
				else if (RXT_VECTOR == type) {
					// a vector of 64bit decimal coordinates, followed by a
					// vector of 32bit integer point indexes (edges)
					REBSER *ser_points = (REBSER*)arg[0].vector.series;
					REBCNT  info       = arg[0].vector.info;
					REBCNT  ind        = arg[0].vector.index;
					REBDEC *points;
					REBINT *edges;
					REBINT  cnt_points, cnt_edges, prev = 0;
					REBSER *ser_edges;

					if (!RXI_VECTOR_FLOAT(info) || RXI_VECTOR_BITS(info) != 64) goto error;
					points     = (REBDEC*)SERIES_DATA(ser_points) + ind;
					cnt_points = (REBINT)(SERIES_TAIL(ser_points) - ind);

					RESOLVE_ARG(0);
					if (RXT_VECTOR != type) goto error;
					ser_edges = (REBSER*)arg[0].vector.series;
					info      = arg[0].vector.info;
					ind       = arg[0].vector.index;
					if (RXI_VECTOR_FLOAT(info) || RXI_VECTOR_BITS(info) != 32 || !RXI_VECTOR_SIGNED(info)) goto error;
					edges     = (REBINT*)SERIES_DATA(ser_edges) + ind;
					cnt_edges = (REBINT)(SERIES_TAIL(ser_edges) - ind);

					for (i = 0; (REBINT)i + 1 < cnt_edges; i += 2) {
						REBINT p1 = edges[i]     * 2;
						REBINT p2 = edges[i + 1] * 2;
						if (p1 >= 0 && p1 + 1 < cnt_points && p2 >= 0 && p2 + 1 < cnt_points) {
							if (i == 0 || prev != p1) {
								blPathMoveTo(&path, points[p1], points[p1 + 1]);
								blPathLineTo(&path, points[p2], points[p2 + 1]);
							}
							else {
								blPathLineTo(&path, points[p1], points[p1 + 1]);
								blPathLineTo(&path, points[p2], points[p2 + 1]);
							}
							prev = p2;
						}
					}
				}
				else goto error;
				blContextStrokePathD(&ctx, &origin, &path);
				blPathReset(&path);
			}
			break;


		case W_BLEND2D_CMD_LINE_WIDTH:

			RESOLVE_NUMBER_ARG(0, 0)
			width = doubles[0];

			if (width == 0) {
				has_stroke = FALSE;
			}
			else {
				has_stroke = TRUE;
				blContextSetStrokeWidth(&ctx, width);
			}
			break;


		case W_BLEND2D_CMD_LINE_JOIN:
			if (fetch_mode(cmds, index, &mode, W_BLEND2D_ARG_MITER, 3)) {
				index++;
				switch (mode) {
				case  0: // miter, check for the bevel or round variant
					if (fetch_mode(cmds, index, &mode, W_BLEND2D_ARG_BEVEL, 2)) {
						index++;
						type = mode + 1; // BL_STROKE_JOIN_MITER_BEVEL or BL_STROKE_JOIN_MITER_ROUND
					}
					else {
						type = BL_STROKE_JOIN_MITER_CLIP;
					}
					break;
				case  1: type = BL_STROKE_JOIN_BEVEL; break;
				default: type = BL_STROKE_JOIN_ROUND;
				}
			}
			else goto error;
			blContextSetStrokeJoin(&ctx, type);
			break;


		case W_BLEND2D_CMD_LINE_CAP:

			//TODO: use words instead of integers?
			RESOLVE_INT_ARG(0);
			cap = arg[0].int64;
			if (cap < 0 || cap > BL_STROKE_CAP_MAX_VALUE) goto error; // invalid cap value

			RESOLVE_INT_ARG_OPTIONAL(1); // end cap
			if (RXT_INTEGER == type) {
				blContextSetStrokeCap(&ctx, BL_STROKE_CAP_POSITION_START, (uint32_t)cap);
				cap = arg[1].int64;
				if (cap < 0 || cap > BL_STROKE_CAP_MAX_VALUE) goto error; // invalid cap value
				blContextSetStrokeCap(&ctx, BL_STROKE_CAP_POSITION_END, (uint32_t)cap);
			}
			else {
				blContextSetStrokeCaps(&ctx, (uint32_t)cap);
			}
			break;


		case W_BLEND2D_CMD_CUBIC:

			type = RL_GET_VALUE(cmds, index++, &arg[0]);
			if (RXT_PAIR == type) {
				blPathMoveTo(&path, (double)arg[0].pair.x, (double)arg[0].pair.y);
			} else goto error;
			while (FETCH_3_PAIRS(cmds, index, arg[1], arg[2], arg[3])) {
				blPathCubicTo(&path,
					(double)arg[1].pair.x, (double)arg[1].pair.y,
					(double)arg[2].pair.x, (double)arg[2].pair.y,
					(double)arg[3].pair.x, (double)arg[3].pair.y
				);
				index += 3;
			}
			if (has_fill  ) blContextFillPathD  (&ctx, &origin, &path);
			if (has_stroke) blContextStrokePathD(&ctx, &origin, &path);

			blPathReset(&path);
			break;


		case W_BLEND2D_CMD_POLYGON:

			RESOLVE_PAIR_ARG(0, 0)
			blPathMoveTo(&path, doubles[0], doubles[1]);
			count = 0; i = 0;
			while (RXT_PAIR == RL_GET_VALUE(cmds, index, &arg[0])) {
				index++;
				count++;
				doubles[i++] = arg[0].pair.x;
				doubles[i++] = arg[0].pair.y;
				if (i >= DOUBLE_BUFFER_SIZE) {
					// the buffer could be extended here; processing in
					// batches instead keeps the memory use flat.
					blPathPolyTo(&path, (BLPoint*)doubles, count);
					count = 0; i = 0;
				}
			}
			if (count > 0) blPathPolyTo(&path, (BLPoint*)doubles, count);
			blPathClose(&path);

			DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_PATH, &path)
			blPathReset(&path);
			break;


		case W_BLEND2D_CMD_BOX:

			RESOLVE_PAIR_ARG(0, 0) // top-left
			RESOLVE_PAIR_ARG(1, 2) // size (Blend2D's RECTD is x, y, w, h)
			type = RL_GET_VALUE_RESOLVED(cmds, index, &arg[2]);
			if (type == RXT_DECIMAL || type == RXT_INTEGER) {
				index++;
				mode = BL_GEOMETRY_TYPE_ROUND_RECT;
				doubles[4] = (type == RXT_DECIMAL) ? arg[2].dec64 : (double)arg[2].int64;
				type = RL_GET_VALUE_RESOLVED(cmds, index, &arg[3]);
				if (type == RXT_DECIMAL || type == RXT_INTEGER) {
					index++;
					doubles[5] = (type == RXT_DECIMAL) ? arg[3].dec64 : (double)arg[3].int64;
				}
				else {
					doubles[5] = doubles[4];
				}
			} else {
				mode = BL_GEOMETRY_TYPE_RECTD;
			}
			debug_print("box type: %u size: %f %f %f %f radius: %f %f\n", mode, doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5]);
			DRAW_GEOMETRY(ctx, mode, doubles);
			break;


		case W_BLEND2D_CMD_CIRCLE:

			RESOLVE_PAIR_ARG(0, 0)   // center
			RESOLVE_NUMBER_ARG(1, 2) // radius or radius-x
			RESOLVE_NUMBER_ARG_OPTIONAL(2, 3)  // radius-y
			if (type) {
				DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_ELLIPSE, doubles);
			}
			else {
				DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_CIRCLE, doubles);
			}
			break;


		case W_BLEND2D_CMD_ELLIPSE:

			RESOLVE_PAIR_ARG(0, 0) // top-left
			RESOLVE_PAIR_ARG(1, 2) // size
			doubles[2] *= 0.5;
			doubles[3] *= 0.5;
			doubles[0] += doubles[2];
			doubles[1] += doubles[3];
			DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_ELLIPSE, doubles);
			break;


		case W_BLEND2D_CMD_POINT:

			RESOLVE_ARG(0); // center
			if (type == RXT_PAIR) {
				// one or more pairs are allowed...
				while (type == RXT_PAIR) {
					point[0] = arg[0].pair.x;
					point[1] = arg[0].pair.y;
					DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_CIRCLE, point);
					RESOLVE_ARG(0);
				}
				index--; // the last resolved value is not a pair anymore
			}
			else if (type == RXT_VECTOR) {
				REBSER *vect = (REBSER*)arg[0].vector.series;
				REBCNT  info = arg[0].vector.info;
				REBCNT  ind  = arg[0].vector.index;
				REBCNT  num;
				REBDEC *data;

				if (!RXI_VECTOR_FLOAT(info) || RXI_VECTOR_BITS(info) != 64) goto error;
				num  = (SERIES_TAIL(vect) - ind) >> 1;        // 2 values per point
				data = (REBDEC*)SERIES_DATA(vect) + ind;      // the value may not be at head

				while (num-- > 0) {
					point[0] = data[0];
					point[1] = data[1];
					data += 2;
					DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_CIRCLE, point);
				}
			}
			else if (RXT_BLOCK == type) {
				REBSER *blk = (REBSER*)arg[0].series;
				REBCNT  ind = arg[0].index;
				type = RL_GET_VALUE(blk, ind, &arg[0]);
				if (RXT_PAIR != type) goto error;
				while (RXT_PAIR == type) {
					point[0] = (double)arg[0].pair.x;
					point[1] = (double)arg[0].pair.y;
					DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_CIRCLE, point);
					type = RL_GET_VALUE(blk, ++ind, &arg[0]);
				}
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_POINT_SIZE:
			RESOLVE_NUMBER_ARG(1, 0); // diameter
			point[2] = doubles[0] * 0.5;
			point[3] = doubles[0] * 0.5;
			break;


		case W_BLEND2D_CMD_TRIANGLE:
			RESOLVE_ARG(0);
			if (RXT_PAIR == type) {
				index--;
				while (FETCH_3_PAIRS(cmds, index, arg[0], arg[1], arg[2])) {
					doubles[0] = arg[0].pair.x;
					doubles[1] = arg[0].pair.y;
					doubles[2] = arg[1].pair.x;
					doubles[3] = arg[1].pair.y;
					doubles[4] = arg[2].pair.x;
					doubles[5] = arg[2].pair.y;
					DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_TRIANGLE, doubles);
					index += 3;
				}
			}
			else if (RXT_VECTOR == type) {
				REBSER *vect = (REBSER*)arg[0].vector.series;
				REBCNT  info = arg[0].vector.info;
				REBCNT  ind  = arg[0].vector.index;
				REBCNT  num;
				REBDEC *data;

				if (!RXI_VECTOR_FLOAT(info) || RXI_VECTOR_BITS(info) != 64) goto error;
				num  = (SERIES_TAIL(vect) - ind) / 6;    // 6 values per triangle (3 points)
				data = (REBDEC*)SERIES_DATA(vect) + ind;

				while (num-- > 0) {
					DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_TRIANGLE, data);
					data += 6;
				}
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_ARC:

			RESOLVE_PAIR_ARG(0, 0)    // center
			RESOLVE_PAIR_ARG(1, 2)    // radius
			RESOLVE_NUMBER_ARG(2, 4); // begin
			RESOLVE_NUMBER_ARG(3, 5); // sweep

			TO_RADIANS(doubles[4]);
			TO_RADIANS(doubles[5]);

			// arc, pie or chord?
			if (fetch_word(cmds, index, Blend2d_arg_words, &cmd) && cmd >= W_BLEND2D_ARG_PIE && cmd <= W_BLEND2D_ARG_CHORD) {
				index++;
				type = (cmd == W_BLEND2D_ARG_CHORD) ? BL_GEOMETRY_TYPE_CHORD : BL_GEOMETRY_TYPE_PIE;
			}
			else {
				type = BL_GEOMETRY_TYPE_ARC;
			}
			DRAW_GEOMETRY(ctx, type, doubles);
			break;


		case W_BLEND2D_CMD_IMAGE: {
			// image area (could be used for a texture atlas)
			BLImageData imgData;
			REBOOL scaledImage = FALSE;

			rectI.x = 0;
			rectI.y = 0;

			type = RL_GET_VALUE_RESOLVED(cmds, index++, &arg[0]);
			if (type == RXT_HANDLE) {
				if (!VAL_IS_HANDLE(arg[0], Handle_BLImage)) goto error;
				current_img = (BLImageCore*)arg[0].handle.hob->data;
			}
			else {
				r = b2d_init_image_from_arg(&img, &arg[0], type);
				if (r != BL_SUCCESS) { err = r; goto end_ctx; }
				current_img = &img;
			}

			blImageGetData(current_img, &imgData);
			rectI.w = imgData.size.w;
			rectI.h = imgData.size.h;

			RESOLVE_PAIR_ARG(1, 0) // top-left
			pt.x = doubles[0];
			pt.y = doubles[1];

			// size of the destination rectangle (optional):
			// NOTE: the value is already consumed by the macro - the old code
			// skipped one more value here.
			RESOLVE_PAIR_ARG_OPTIONAL(2, 0)
			if (type == RXT_PAIR) {
				scaledImage = TRUE;
				rect.x = pt.x;
				rect.y = pt.y;
				rect.w = doubles[0];
				rect.h = doubles[1];
			}
			debug_print("blitImage size: %i %i at: %f %f\n", rectI.w, rectI.h, pt.x, pt.y);
			if (scaledImage) {
				blContextBlitScaledImageD(&ctx, &rect, current_img, &rectI);
			}
			else {
				blContextBlitImageD(&ctx, &pt, current_img, &rectI);
			}
			break;
		}


		case W_BLEND2D_CMD_FONT:
			blFontReset(&font);
			type = RL_GET_VALUE_RESOLVED(cmds, index++, &arg[0]);
			if (type == RXT_HANDLE) {
				if (!VAL_IS_HANDLE(arg[0], Handle_BLFontFace)) {
					trace("Invalid font handle type!");
					font_face_ext = NULL;
					goto error;
				}
				font_face_ext = (BLFontFaceCore*)arg[0].handle.hob->data;
				debug_print("Font handle: %p\n", (void*)font_face_ext);
				blFontCreateFromFace(&font, font_face_ext, font_size);
			}
			else if (type == RXT_FILE || type == RXT_STRING) {
				REBSER *file = b2d_file_arg(&arg[0], type);
				font_face_ext = NULL;
				if (file == NULL) goto error;
				blFontFaceReset(&font_face);
				r = blFontFaceCreateFromFile(&font_face, SERIES_TEXT(file), BL_FILE_READ_MMAP_ENABLED | BL_FILE_READ_MMAP_AVOID_SMALL);
				if (BL_SUCCESS != r) {
					debug_print("Failed to load font! (%s) %i\n", SERIES_TEXT(file), r);
					goto error;
				}
				blFontCreateFromFace(&font, &font_face, font_size);
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_TEXT: {
			REBSER *str;

			RESOLVE_PAIR_ARG(0, 0) // position
			pt.x = doubles[0];
			pt.y = doubles[1];

			// size (optional):
			sz = 0.0;
			RESOLVE_NUMBER_ARG_OPTIONAL(1, 3);
			if (type) {
				sz = doubles[3];
#ifdef TO_WINDOWS
				sz = (sz * 96.0) / 72.0;
#endif
			}
			if (sz != font_size && sz > 0.0) {
				font_size = sz;
				blFontReset(&font);
				blFontCreateFromFace(&font, (font_face_ext == NULL ? &font_face : font_face_ext), font_size);
				debug_print("font_size: %f\n", font_size);
			}

			RESOLVE_STRING_ARG(3) // text

			str = (REBSER*)arg[3].series;
			if (BYTE_SIZE(str)) {
				// all Rebol strings are UTF-8 encoded now
				blContextFillUtf8TextD(&ctx, &pt, &font, SERIES_TEXT(str), SERIES_TAIL(str));
			} else {
				blContextFillUtf16TextD(&ctx, &pt, &font, (uint16_t*)SERIES_DATA(str), SERIES_TAIL(str));
			}
			break;
		}


		case W_BLEND2D_CMD_SCALE:
			RESOLVE_NUMBER_OR_PAIR_ARG(0, 0);
			blContextApplyTransformOp(&ctx, BL_TRANSFORM_OP_POST_SCALE, doubles);
			break;


		case W_BLEND2D_CMD_ROTATE:
			RESOLVE_NUMBER_ARG(0, 0);
			TO_RADIANS(doubles[0]);
			RESOLVE_PAIR_ARG_OPTIONAL(1, 1);
			blContextApplyTransformOp(&ctx, type ? BL_TRANSFORM_OP_POST_ROTATE_PT : BL_TRANSFORM_OP_POST_ROTATE, doubles);
			break;


		case W_BLEND2D_CMD_TRANSLATE:
			RESOLVE_PAIR_ARG(0, 0);
			blContextApplyTransformOp(&ctx, BL_TRANSFORM_OP_POST_TRANSLATE, doubles);
			break;


		case W_BLEND2D_CMD_RESET_MATRIX:
			blContextApplyTransformOp(&ctx, BL_TRANSFORM_OP_RESET, NULL);
			break;


		case W_BLEND2D_CMD_ALPHA:
			RESOLVE_NUMBER_ARG(0, 0);
			blContextSetGlobalAlpha(&ctx, doubles[0]);
			break;


		case W_BLEND2D_CMD_BLEND:
		case W_BLEND2D_CMD_COMPOSITE:
			type = RL_GET_VALUE_RESOLVED(cmds, index++, &arg[0]);
			if (fetch_mode(cmds, index - 1, &mode, W_BLEND2D_ARG_SOURCE_OVER, BL_COMP_OP_MAX_VALUE)) {
				debug_print("mode: %i\n", mode);
				blContextSetCompOp(&ctx, mode);
			}
			else if (RXT_NONE == type || (RXT_LOGIC == type && !arg[0].int32a)) { // blend none or blend off
				blContextSetCompOp(&ctx, BL_COMP_OP_SRC_OVER);
			}
			else goto error;
			break;


		case W_BLEND2D_CMD_FILL_ALL:
			blContextFillAll(&ctx);
			break;


		case W_BLEND2D_CMD_CLEAR_ALL:
			blContextClearAll(&ctx);
			break;


		case W_BLEND2D_CMD_CLEAR:
			RESOLVE_PAIR_ARG(0, 0);
			RESOLVE_PAIR_ARG(1, 2);
			rect.x = doubles[0];
			rect.y = doubles[1];
			rect.w = doubles[2] - doubles[0];
			rect.h = doubles[3] - doubles[1];
			blContextClearRectD(&ctx, &rect);
			break;


		case W_BLEND2D_CMD_CLIP:
			type = RL_GET_VALUE_RESOLVED(cmds, index, &arg[0]);
			if (RXT_NONE == type || (RXT_LOGIC == type && !arg[0].int32a)) {
				blContextRestoreClipping(&ctx);
				index++;
			}
			else {
				RESOLVE_PAIR_ARG(0, 0);
				RESOLVE_PAIR_ARG(1, 2);
				rect.x = doubles[0];
				rect.y = doubles[1];
				rect.w = doubles[2] - doubles[0];
				rect.h = doubles[3] - doubles[1];
				blContextClipToRectD(&ctx, &rect);
			}
			break;


		case W_BLEND2D_CMD_SHAPE:
			type = RL_GET_VALUE_RESOLVED(cmds, index++, &arg[0]);
			if (type == RXT_HANDLE) {
				if (!VAL_IS_HANDLE(arg[0], Handle_BLPath)) goto error;
				DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_PATH, (BLPathCore*)arg[0].handle.hob->data);
			}
			else if (type == RXT_BLOCK) {
				b2d_init_path_from_block(&path, (REBSER*)arg[0].series, arg[0].index);
				DRAW_GEOMETRY(ctx, BL_GEOMETRY_TYPE_PATH, &path);
				blPathReset(&path);
			}
			else goto error;
			break;


		default:
			debug_print("unknown command.. index: %u\n", index);
			goto error;
		} // switch end
		continue;

	error:
		// A bad command does not stop the evaluation; the remaining commands
		// may still be processed.
		//TODO: these could be collected as warnings of the module instead of
		//      this debug print!
		debug_print("CMD error at index... %u\n", cmd_pos);
		index = cmd_pos;
		// find the next valid command name
		while (index < SERIES_TAIL(cmds)) {
			if (fetch_word(cmds, index++, Blend2d_cmd_words, &cmd)) {
				goto process_cmd;
			}
		}
	} // while end

end_ctx:
	trace("Cleaning...");
	blContextEnd(&ctx);
	blContextReset(&ctx);
	blImageReset(&img_target);
	blImageReset(&img_pattern);
	blImageReset(&img);
	blFontReset(&font);
	blFontFaceReset(&font_face);
	blPathReset(&path);

	if (err == BL_SUCCESS) return RXR_VALUE;

	debug_print("error: %u\n", err);
	RETURN_ERROR(ERR_DRAW);
}