//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/MiniAudio extension
// SPDX-License-Identifier: MIT
// =============================================================================
// Shared between the entry points (miniaudio-extension.c) and the command
// sources (miniaudio-commands.c).
//
// The MAContext struct, the `Handle_MA*` externs and the SERIES_TEXT helper
// come from the generated header, via the specification's `c-header:` field.
// =============================================================================

#ifndef MINIAUDIO_EXT_H
#define MINIAUDIO_EXT_H

// Shared state, defined in miniaudio-commands.c
extern MAContext* pEngine;
extern REBHOB*    pEngineHob;
extern ma_context gContext;
extern ma_resource_manager gResourceManager;
extern int gContextReady;

// Initializes the resource manager and the device context. Called once from
// the entry point in both build modes; safe to call repeatedly.
int MiniAudio_Startup(void);

// Handle callbacks - registered by Register_MiniAudio_Handles() in
// miniaudio-extension.c, implemented in miniaudio-commands.c.
int Common_mold(REBHOB *hob, REBSER *ser);

int MAEngine_free(void* hndl);
int MAEngine_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MAEngine_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int MASound_free(void* hndl);
int MASound_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MASound_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int MANoise_free(void* hndl);
int MANoise_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MANoise_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int MAWaveform_free(void* hndl);
int MAWaveform_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MAWaveform_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int MADelay_free(void* hndl);
int MADelay_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MADelay_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

int MAGroup_free(void* hndl);
int MAGroup_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int MAGroup_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

#ifndef REB_EXT
// Embedded build only - called from host-main.c under INCLUDE_EXT_MINIAUDIO.
void OS_Init_Ext_MiniAudio(void);
#endif

#endif // MINIAUDIO_EXT_H
