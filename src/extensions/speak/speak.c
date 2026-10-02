//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Speak extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Entry points for the Rebol/Speak extension module.
//
//   REB_EXT defined ... standalone speak-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Speak_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so
// that `Options: [delay]` can postpone it until the module is imported.
//

#include "gen-speak.h"
#include "speak.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = SPEAK_EXT_INIT_CODE;

// Symbol of the registered voice handle type. Declared in the generated
// header (from the spec's `c-header:`), defined here.
REBCNT Handle_VoiceHandle = 0;


// Registers the voice handle type. Runs when the module body evaluates, so
// it happens at the same point in both build modes - and only on first
// import under `delay`.
//
// The get_path/set_path accessors are what make `voice/number` work; the
// fields they accept come from the `handles:` block of the specification.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Speak_Init(void) {
	REBHSP spec;

	CLEARS(&spec);
	spec.size     = sizeof(voice_t);
	// VoiceHandle_free takes the HOB, not the raw data pointer.
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = VoiceHandle_free;
	spec.get_path = VoiceHandle_get_path;
	spec.set_path = VoiceHandle_set_path;
	spec.mold     = VoiceHandle_mold;
	Handle_VoiceHandle = RL_REGISTER_HANDLE_SPEC(cb_cast("voice-handle"), &spec);
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
	debug_print("RX_Init speak-extension; Rebol v%i.%i.%i\n", ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;
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
// dispatcher is generated into gen-speak.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Speak_RX_Call(cmd, frm, ctx);
}

#endif
