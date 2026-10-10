//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Speak extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Shared between the entry points (speak.c), the command implementations
// (speak-commands.c) and the platform backends (speak-win.cpp, speak-mac.m).
//
// The voice_t struct and the `Handle_VoiceHandle` extern come from the
// generated header, via the specification's `c-header:` field, so include
// "gen-speak.h" before this file.
//

#ifndef SPEAK_EXT_H
#define SPEAK_EXT_H

//== platform backend =========================================================
// Implemented once per system: speak-win.cpp (SAPI) and speak-mac.m
// (NSSpeechSynthesizer). Declared with C linkage, because speak-win.cpp is
// compiled as C++.

#ifdef __cplusplus
extern "C" {
#endif

void list_voices(void);
int  speak(voice_t *voice, int no_wait);
void release_voice(voice_t *voice);

#ifdef __cplusplus
}
#endif

//== handle callbacks =========================================================
// Callbacks of the voice handle, named after the handle type - not after the
// extension, which owns Speak_Init() and the generated symbols.
// Registered by Speak_Init(); implemented in speak-commands.c.

int VoiceHandle_free(void *hndl);
int VoiceHandle_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int VoiceHandle_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int VoiceHandle_mold(REBHOB *hob, REBSER *str);

// Speak_Init() is declared in gen-speak.h - the generated `_init` handler
// calls it, and every extension is required to define one.

#ifndef REB_EXT
// Embedded build only - called from host-main.c under INCLUDE_EXT_SPEAK.
RL_API void OS_Init_Ext_Speak(void);
#endif

#endif // SPEAK_EXT_H
