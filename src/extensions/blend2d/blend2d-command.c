//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// Helpers shared by the dialect evaluators.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"


// TRUE when the value at `index` is a word of the given list; `cmd` then holds
// its 1-based position (the W_BLEND2D_* value).
REBOOL fetch_word(REBSER *cmds, REBCNT index, u32 *words, REBCNT *cmd) {
	RXIARG arg;
	REBCNT type = RL_GET_VALUE(cmds, index, &arg);
	return (RXT_WORD == type && (cmd[0] = RL_FIND_WORD(words, arg.int32a)) != 0);
}


// Converts an argument word - or a plain integer - into a Blend2D enumeration
// value. `start` is the W_BLEND2D_ARG_* value the enumeration begins at.
//
// NOTE: unlike the previous implementation, a value which is neither a word of
// the `arg` list nor an integer is now rejected instead of being reported as
// the enumeration's maximum. That old result made callers consume a value
// which was not theirs (a pair following `fill`, say).
REBOOL fetch_mode(REBSER *cmds, REBCNT index, REBCNT *result, REBCNT start, REBCNT max) {
	RXIARG arg;
	REBINT wrd;
	REBCNT type = RL_GET_VALUE(cmds, index, &arg);

	if (RXT_WORD == type || RXT_LIT_WORD == type) {
		REBCNT found = RL_FIND_WORD(Blend2d_arg_words, arg.int32a);
		if (found == 0) return FALSE; // not one of our argument words
		wrd = (REBINT)found - (REBINT)start;
	}
	else if (RXT_INTEGER == type) {
		wrd = (REBINT)arg.int64;
	}
	else return FALSE;

	if (wrd < 0 || wrd > (REBINT)max) return FALSE;
	result[0] = (REBCNT)wrd;
	return TRUE;
}


// A file! is converted to an OS local path, a string! is taken as it is (all
// Rebol strings are UTF-8 encoded now); anything else gives NULL. The result
// is a fresh, null terminated series which nothing references yet, so it must
// be used before any other RL_ call which may allocate.
REBSER* b2d_file_arg(RXIARG *arg, REBCNT type) {
	REBSER *src;
	switch (type) {
	case RXT_FILE:
		return RL_TO_LOCAL_PATH(arg, FALSE, TRUE);
	case RXT_STRING:
		src = (REBSER*)arg->series;
		return RL_ENCODE_UTF8_STRING(SERIES_DATA(src), SERIES_TAIL(src), SERIES_WIDE(src) > 1, FALSE);
	}
	return NULL;
}