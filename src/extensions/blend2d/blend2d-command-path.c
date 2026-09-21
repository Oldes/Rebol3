//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `path` command, the path dialect and the BLPath handle.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

static const REBYTE *ERR_NO_HANDLE = (const REBYTE*)"Blend2D failed to make a path handle!";


// Evaluates the path dialect of `cmds` (starting at `index`) into `path`.
// Returns 0 when everything was understood, 1 when at least one command was
// skipped - evaluation itself never stops on a bad command, it resumes at the
// next word which names one.
REBCNT b2d_init_path_from_block(BLPathCore *path, REBSER *cmds, REBCNT index) {
	REBCNT cmd = 0, type, cmd_pos = 0;
	REBOOL sweepFlag, largeFlag;
	BLPoint pos;
	REBDEC doubles[DOUBLE_BUFFER_SIZE];
	RXIARG arg[ARG_BUFFER_SIZE];

	while (index < SERIES_TAIL(cmds)) {
		if (!fetch_word(cmds, index++, Blend2d_cmd_words, &cmd)) {
			trace("expected word as a command!");
			goto error;
		}
	process_cmd: // reached from the error loop, which skips values until it finds a command

		cmd_pos = index; // used to report the error position
		debug_print("path cmd index: %u cmd: %u\n", index, cmd);
		switch (cmd) {

		case W_BLEND2D_CMD_MOVE:
			RESOLVE_PAIR_ARG(0, 0);
			blPathMoveTo(path, doubles[0], doubles[1]);
			break;

		case W_BLEND2D_CMD_LINE:
			RESOLVE_PAIR_ARG(0, 0);
			blPathLineTo(path, doubles[0], doubles[1]);
			while (RXT_PAIR == RL_GET_VALUE(cmds, index, &arg[0])) {
				index++;
				blPathLineTo(path, (double)arg[0].pair.x, (double)arg[0].pair.y);
			}
			break;

		case W_BLEND2D_CMD_ARC:
			RESOLVE_PAIR_ARG(0, 3);   // arc's end point
			RESOLVE_NUMBER_ARG(1, 0); // radius of the circle along x axis
			RESOLVE_NUMBER_ARG(2, 1); // radius of the circle along y axis
			RESOLVE_NUMBER_ARG(3, 2); // rotation angle of the underlying ellipse in degrees

			TO_RADIANS(doubles[2]);
			sweepFlag = FALSE;
			largeFlag = FALSE;

			OPT_WORD_FLAG(sweepFlag, W_BLEND2D_ARG_SWEEP);
			OPT_WORD_FLAG(largeFlag, W_BLEND2D_ARG_LARGE);

			blPathEllipticArcTo(path, doubles[0], doubles[1], doubles[2], largeFlag, sweepFlag, doubles[3], doubles[4]);
			break;

		case W_BLEND2D_CMD_CURVE:
			// A cubic Bezier curve is defined by a start point, an end point
			// and two control points.
			while (
				RXT_PAIR == RL_GET_VALUE(cmds, index,     &arg[0]) &&
				RXT_PAIR == RL_GET_VALUE(cmds, index + 1, &arg[1]) &&
				RXT_PAIR == RL_GET_VALUE(cmds, index + 2, &arg[2])
			) {
				index += 3;
				blPathCubicTo(path, ARG_X(0), ARG_Y(0), ARG_X(1), ARG_Y(1), ARG_X(2), ARG_Y(2));
			}
			break;

		case W_BLEND2D_CMD_CURV:
			while (
				RXT_PAIR == RL_GET_VALUE(cmds, index,     &arg[0]) &&
				RXT_PAIR == RL_GET_VALUE(cmds, index + 1, &arg[1])
			) {
				index += 2;
				blPathSmoothCubicTo(path, ARG_X(0), ARG_Y(0), ARG_X(1), ARG_Y(1));
			}
			break;

		case W_BLEND2D_CMD_QCURVE:
			// A quadratic Bezier curve is defined by a start point, an end
			// point and one control point.
			while (
				RXT_PAIR == RL_GET_VALUE(cmds, index,     &arg[0]) &&
				RXT_PAIR == RL_GET_VALUE(cmds, index + 1, &arg[1])
			) {
				index += 2;
				blPathQuadTo(path, ARG_X(0), ARG_Y(0), ARG_X(1), ARG_Y(1));
			}
			break;

		case W_BLEND2D_CMD_QCURV:
			while (RXT_PAIR == RL_GET_VALUE(cmds, index, &arg[0])) {
				index += 1;
				blPathSmoothQuadTo(path, ARG_X(0), ARG_Y(0));
			}
			break;

		case W_BLEND2D_CMD_HLINE:
			RESOLVE_NUMBER_ARG(0, 0);
			blPathGetLastVertex(path, &pos);
			blPathLineTo(path, doubles[0], pos.y);
			break;

		case W_BLEND2D_CMD_VLINE:
			RESOLVE_NUMBER_ARG(0, 0);
			blPathGetLastVertex(path, &pos);
			blPathLineTo(path, pos.x, doubles[0]);
			break;

		case W_BLEND2D_CMD_CLOSE:
			blPathClose(path);
			break;

		default:
			debug_print("unknown path command.. index: %u\n", index);
			goto error;
		} // switch end
		continue;

	error:
		// A bad command does not stop the evaluation; the remaining ones may
		// still be processed.
		debug_print("path command error at index... %u\n", cmd_pos);
		index = cmd_pos;
		while (index < SERIES_TAIL(cmds)) {
			if (fetch_word(cmds, index++, Blend2d_cmd_words, &cmd)) {
				goto process_cmd;
			}
		}
		return 1;
	} // while end
	return 0;
}


COMMAND cmd_blend2d_path(RXIFRM *frm, void *ctx) {
	BLPathCore *path;
	REBHOB *hob = RL_MAKE_HANDLE_CONTEXT(Handle_BLPath);

	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	path = (BLPathCore*)hob->data;
	debug_print("New path handle: %u data: %p\n", hob->sym, (void*)hob->data);
	blPathInit(path);

	b2d_init_path_from_block(path, RXA_SERIES(frm, 1), RXA_INDEX(frm, 1));

	RETURN_HANDLE(hob);
}


//== handle callbacks =========================================================

int BLPath_free(void *data) {
	debug_print("releasing path: %p\n", data);
	if (data) blPathDestroy((BLPathCore*)data);
	return 0;
}

int BLPath_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	BLPathCore *path = (BLPathCore*)hob->data;

	switch (RL_FIND_WORD(Blend2d_arg_words, word)) {
	case W_BLEND2D_ARG_SIZE:
		*type = RXT_INTEGER;
		arg->int64 = (i64)blPathGetSize(path);
		break;
	case W_BLEND2D_ARG_CAPACITY:
		*type = RXT_INTEGER;
		arg->int64 = (i64)blPathGetCapacity(path);
		break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

int BLPath_mold(REBHOB *hob, REBSER *str) {
	int len = 0;

	if (!str || !hob || !hob->data) return 0;
	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx size: %i",
		(unsigned long)(uintptr_t)hob->data,
		(int)blPathGetSize((BLPathCore*)hob->data));
	return len;
}