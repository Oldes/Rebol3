//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// Entry points of the Blend2D binding.
//
//   REB_EXT defined ... standalone blend2d-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Blend2d_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so that
// `Options: [delay]` can postpone it until the module is first imported.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = BLEND2D_EXT_INIT_CODE;

//==== Globals ===============================================================//
// Symbols of the registered handle types. Declared in the generated header
// (from the spec's `c-header:`), defined here.
REBCNT Handle_BLPath     = 0;
REBCNT Handle_BLFontFace = 0;
REBCNT Handle_BLImage    = 0;

// 0 means synchronous rendering, 1 async on the main thread only, more than
// that uses Blend2D's thread pool. Modified by the `set-threads` command.
uint32_t Blend2D_thread_count = 1;
//============================================================================//


// Shared by all three handle types: none of their fields may be written.
int BL_set_path_readonly(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	return PE_BAD_SET;
}


// Registers the three handle types the extension hands out. Runs when the
// module body evaluates, so it happens at the same point in both build modes
// - and only on the first import under `delay`.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Blend2d_Init(void) {
	REBHSP spec;

	// The free callbacks take the handle's data - the Blend2D core object
	// itself - so HANDLE_REQUIRES_HOB_ON_FREE is deliberately not set.
	CLEARS(&spec);
	spec.size     = sizeof(BLImageCore);
	spec.free     = BLImage_free;
	spec.get_path = BLImage_get_path;
	spec.set_path = BL_set_path_readonly;
	spec.mold     = BLImage_mold;
	Handle_BLImage = RL_REGISTER_HANDLE_SPEC(cb_cast("BLImage"), &spec);

	CLEARS(&spec);
	spec.size     = sizeof(BLPathCore);
	spec.free     = BLPath_free;
	spec.get_path = BLPath_get_path;
	spec.set_path = BL_set_path_readonly;
	spec.mold     = BLPath_mold;
	Handle_BLPath = RL_REGISTER_HANDLE_SPEC(cb_cast("BLPath"), &spec);

	CLEARS(&spec);
	spec.size     = sizeof(BLFontFaceCore);
	spec.free     = BLFontFace_free;
	spec.get_path = BLFontFace_get_path;
	spec.set_path = BL_set_path_readonly;
	spec.mold     = BLFontFace_mold;
	Handle_BLFontFace = RL_REGISTER_HANDLE_SPEC(cb_cast("BLFontFace"), &spec);

	return TRUE;
}


// The four entry points below are the only symbols this library needs to
// export - it is built with -fvisibility=hidden (see the nest file), which
// matters here because Blend2D and asmjit are linked in statically and their
// names must not leak into the process' flat namespace. `RXIEXT` carries
// API_EXPORT, so the entry points opt back in on their own.

#ifdef REB_EXT

/***********************************************************************
**  Standalone extension library
***********************************************************************/

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);
	debug_print("RX_Init blend2d; Rebol v%i.%i.%i\n", ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print("Needs at least Rebol v%i.%i.%i!\n", MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD);
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
// dispatcher is generated into gen-blend2d.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Blend2d_RX_Call(cmd, frm, ctx);
}

#endif