//
// Project: Rebol/QOI extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Entry points of the QOI codec extension.
//
//   REB_EXT defined ... standalone qoi-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Qoi_Init(), called from the generated `_init`
// command when the module body evaluates - which is also when the mezzanine
// calls `register-codec`, so the word below is always resolved before the
// dispatcher can be handed out.
//

#include "gen-qoi.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = QOI_EXT_INIT_CODE;

extern u32 Qoi_codec_word;   // qoi-commands.c


// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Qoi_Init(void)
{
	// DO-CODEC refuses any handle whose type is not `codec`, so this is
	// what makes the dispatcher callable at all. The word already exists
	// in the host (SYM_CODEC), and RL_MAP_WORD returns its canonical id.
	Qoi_codec_word = AS_WORD("codec");

	// The decoder hands DO-CODEC a buffer of w * h * 4 bytes and the image
	// is copied out of it verbatim, so a pixel really has to be four bytes.
	STATIC_ASSERT(sizeof(unsigned int) == 4);

	return Qoi_codec_word != 0;
}


// The four entry points below are the only symbols this library needs to
// export, so it is built with -fvisibility=hidden (see the nest file) and
// they have to say so themselves: on POSIX `RXIEXT` is a plain `extern`,
// which the hidden default would swallow along with everything else.
#ifdef TO_WINDOWS
#define EXT_ENTRY RXIEXT
#else
#define EXT_ENTRY RXIEXT API_EXPORT
#endif

#ifdef REB_EXT

/***********************************************************************
**  Standalone extension library
***********************************************************************/

EXT_ENTRY const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);

	// NOTE: this check is load bearing here, more than in other
	// extensions. RL_ALLOC was appended to the end of the RL_API table
	// without an ABI bump, so an older host has a shorter table and
	// calling it would read past the end of that host's Ext_Lib.
	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}
	return init_block;
}

// NOTE: system/codecs/qoi keeps a handle holding a raw pointer into this
// library, so unloading it would leave DECODE and ENCODING? calling into
// freed code. Nothing unregisters the codec yet - see the README.
EXT_ENTRY int RX_Quit(int opts) {
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
EXT_ENTRY int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-qoi.c.
EXT_ENTRY int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Qoi_RX_Call(cmd, frm, ctx);
}

#endif
