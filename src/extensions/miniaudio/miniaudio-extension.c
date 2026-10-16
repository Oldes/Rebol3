//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/MiniAudio extension
// SPDX-License-Identifier: MIT
// =============================================================================
// Entry points of the Rebol/MiniAudio extension.
//
//   REB_EXT defined ... standalone miniaudio-<os>-<arch>.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Miniaudio_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so that
// `Options: [delay]` can postpone it until the module is imported. That also
// keeps MiniAudio_Startup() - which opens the device context - from running
// in every host that merely embeds the extension.
// =============================================================================

#include "gen-miniaudio.h"
#include "miniaudio-extension.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = MINIAUDIO_EXT_INIT_CODE;

// Symbols of the registered handle types. Declared in the generated header
// (from the spec's `c-header:`), defined here.
REBCNT Handle_MAEngine   = 0;
REBCNT Handle_MASound    = 0;
REBCNT Handle_MANoise    = 0;
REBCNT Handle_MAWaveform = 0;
REBCNT Handle_MADelay    = 0;
REBCNT Handle_MAGroup    = 0;
REBCNT Handle_MAListener = 0;


// Registers all handle types and brings the audio context up. Runs when the
// module body evaluates, so it happens at the same point in both build modes
// - and only on first import under `delay`.
//
// The path accessors of the handles are the extension's main interface; the
// fields they accept come from the `handles:` block of the specification.
//
// A MiniAudio_Startup() failure is deliberately not fatal - the commands
// which need the context report a proper Rebol error instead - so this
// returns TRUE regardless.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Miniaudio_Init(void) {
	REBHSP spec;

	// Every handle type molds the same way; the rest of the spec is filled
	// in per type below.
	CLEARS(&spec);
	spec.mold = Common_mold;

	spec.size      = sizeof(MAContext);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = MAEngine_free;
	spec.get_path  = MAEngine_get_path;
	spec.set_path  = MAEngine_set_path;
	Handle_MAEngine = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-engine"), &spec);

	spec.size      = sizeof(ma_sound);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = MASound_free;
	spec.get_path  = MASound_get_path;
	spec.set_path  = MASound_set_path;
	Handle_MASound = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-sound"), &spec);

	spec.size      = sizeof(MAListener);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = MAListener_free;
	spec.get_path  = MAListener_get_path;
	spec.set_path  = MAListener_set_path;
	Handle_MAListener = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-listener"), &spec);

	spec.size      = sizeof(ma_noise);
	spec.flags     = 0;
	spec.free      = MANoise_free;
	spec.get_path  = MANoise_get_path;
	spec.set_path  = MANoise_set_path;
	Handle_MANoise = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-noise"), &spec);

	spec.size      = sizeof(ma_waveform);
	spec.flags     = 0;
	spec.free      = MAWaveform_free;
	spec.get_path  = MAWaveform_get_path;
	spec.set_path  = MAWaveform_set_path;
	Handle_MAWaveform = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-waveform"), &spec);

	spec.size      = sizeof(ma_delay_node);
	spec.flags     = 0;
	spec.free      = MADelay_free;
	spec.get_path  = MADelay_get_path;
	spec.set_path  = MADelay_set_path;
	Handle_MADelay = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-delay"), &spec);

	spec.size      = sizeof(ma_sound_group);
	spec.flags     = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free      = MAGroup_free;
	spec.get_path  = MAGroup_get_path;
	spec.set_path  = MAGroup_set_path;
	Handle_MAGroup = RL_REGISTER_HANDLE_SPEC(cb_cast("ma-group"), &spec);

	MiniAudio_Startup();
	return TRUE;
}


#ifdef REB_EXT

/***********************************************************************
**  Standalone extension library
***********************************************************************/

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);
	debug_print(
		"RXinit miniaudio-extension; Rebol v%i.%i.%i\n",
		ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print(
			"Needs at least Rebol v%i.%i.%i!\n",
			MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD);
		return 0;
	}
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}

	return init_block;
}

RXIEXT int RX_Quit(int opts) {
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-miniaudio.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Miniaudio_RX_Call(cmd, frm, ctx);
}

#endif