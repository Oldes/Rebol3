//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Speak extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Command implementations of the Rebol/Speak extension.
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from speak.reb.
//

#include "gen-speak.h"
#include "speak.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TO_WINDOWS
#include <windows.h>
#endif

// A handle argument is only usable when it is of the registered type AND its
// context is still alive (it may have been released with `release`).
#define ARG_Is_Voice(n)    (FRM_IS_HANDLE(n, Handle_VoiceHandle) && IS_USED_HOB(RXA_HANDLE_CONTEXT(frm, n)))
#define ARG_Is_Integer(n)  (RXA_TYPE(frm, n) == RXT_INTEGER)

static const REBYTE* ERR_NO_HANDLE = (const REBYTE*)"Failed to create the voice handle!";
static const REBYTE* ERR_NO_MEMORY = (const REBYTE*)"Not enough memory for the text to be spoken!";


//== commands =================================================================

COMMAND cmd_speak_list_voices(RXIFRM *frm, void *ctx) {
	list_voices();
	return RXR_UNSET;
}

COMMAND cmd_speak_say(RXIFRM *frm, void *ctx) {
	REBSER  *ser   = RXA_SERIES(frm, 1);
	REBLEN   index = RXA_INDEX(frm, 1);
	REBLEN   tail  = SERIES_TAIL(ser);
	REBLEN   len;
	REBHOB  *hob;
	voice_t *voice;
	int no_wait = RXA_REF(frm, 4);

	if (index > tail) index = tail;
	len = tail - index;

	// `/as` carries either an existing voice handle, which is reused (and
	// returned again), or a system voice number for a fresh one.
	if (ARG_Is_Voice(3)) {
		hob = RXA_HANDLE_CONTEXT(frm, 3);
	} else {
		hob = RL_MAKE_HANDLE_CONTEXT(Handle_VoiceHandle);
	}
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);
	voice = (voice_t*)hob->data;

	if (ARG_Is_Integer(3)) voice->number = RXA_INT32(frm, 3);

	// Strings now always cross the extension interface as UTF-8 bytes, so
	// the text is simply copied out of the series into a buffer owned by
	// the handle - the series itself may be moved or collected while an
	// asynchronous (/no-wait) speech is still running.
#ifdef TO_WINDOWS
	{
		// The Windows API wants a wide string. A UTF-8 sequence never
		// expands, so `len` code units is always enough room.
		int wlen;
		REBUNI *uni = (REBUNI*)realloc(voice->text, (len + 1) * sizeof(REBUNI));
		if (uni == NULL) RETURN_ERROR(ERR_NO_MEMORY);
		voice->text = uni;
		wlen = len > 0
			? MultiByteToWideChar(CP_UTF8, 0, (LPCCH)(SERIES_DATA(ser) + index), (int)len, (LPWSTR)uni, (int)len)
			: 0;
		if (wlen < 0) wlen = 0;
		uni[wlen] = 0;
	}
#else
	{
		char *utf8 = (char*)realloc(voice->text, len + 1);
		if (utf8 == NULL) RETURN_ERROR(ERR_NO_MEMORY);
		if (len > 0) memcpy(utf8, SERIES_DATA(ser) + index, len);
		utf8[len] = 0;
		voice->text = utf8;
	}
#endif

	speak(voice, no_wait);

	RETURN_HANDLE(hob);
}


//== handle callbacks =========================================================

int VoiceHandle_free(void *hndl) {
	REBHOB  *hob;
	voice_t *voice;

	if (!hndl) return 0;
	hob = (REBHOB*)hndl;
	voice = (voice_t*)hob->data;
	if (!voice) return 0;

	debug_print("releasing voice handle: %p\n", (void*)voice);
	release_voice(voice);
	if (voice->text) free(voice->text);
	CLEARS(voice);
	UNMARK_HOB(hob);
	return 0;
}

int VoiceHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	voice_t *voice = (voice_t*)hob->data;
	word = RL_FIND_WORD(Speak_arg_words, word);
	switch (word) {
	case W_SPEAK_ARG_NUMBER:
		*type = RXT_INTEGER;
		arg->int64 = voice->number;
		break;
	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}

int VoiceHandle_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	voice_t *voice = (voice_t*)hob->data;
	word = RL_FIND_WORD(Speak_arg_words, word);
	switch (word) {
	case W_SPEAK_ARG_NUMBER:
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		voice->number = (int)arg->int64;
		break;
	default:
		return PE_BAD_SET;
	}
	return PE_OK;
}

int VoiceHandle_mold(REBHOB *hob, REBSER *str) {
	int len;

	if (!str || !hob || !hob->data) return 0;

	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx", (unsigned long)(uintptr_t)hob->data);
	return len;
}
