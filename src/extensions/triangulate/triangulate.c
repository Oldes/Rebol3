//
// Project: Rebol/Triangulate extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Entry points of the Triangle binding.
//
//   REB_EXT defined ... standalone triangulate-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Triangulate_Init(), called from the generated
// `_init` command when the module body evaluates - not from the entry points,
// so that `Options: [delay]` can postpone it until the module is imported.
//

#include "gen-triangulate.h"
#include "triangulate.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = TRIANGULATE_EXT_INIT_CODE;


// There are no handle types to register and Triangle keeps no state of its
// own between calls, so this only checks that the C types the binding hands
// to Triangle really do match the Rebol vectors it reads them from.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Triangulate_Init(void) {
	// `points` and friends are #(f64!) vectors passed to Triangle as REAL*,
	// `markers` and the index lists are #(i32!) vectors passed as int*.
	STATIC_ASSERT(sizeof(REAL) == 8);
	STATIC_ASSERT(sizeof(int)  == 4);
	return TRUE;
}

// The four entry points below are the only symbols this library needs to
// export, so it is built with -fvisibility=hidden (see the nest file) and
// they have to say so themselves: on POSIX `RXIEXT` is a plain `extern`,
// which the hidden default would swallow along with everything else.
// API_EXPORT is what the API header already uses for exactly this; on
// Windows RXIEXT is a dllexport already, so nothing is added there.
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

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}
	return init_block;
}

EXT_ENTRY int RX_Quit(int opts) {
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
EXT_ENTRY int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-triangulate.c.
EXT_ENTRY int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Triangulate_RX_Call(cmd, frm, ctx);
}

#endif