//
// Project: Rebol/Web extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Entry points of the browser bridge (WebAssembly build only).
//
// It is always embedded into the wasm host (REB_EXT absent); the standalone
// branch is kept only so the file follows the same layout as the other
// extensions.
//

#include "gen-web.h"

#ifdef REB_EXT
RL_LIB *RL;
#endif

// Installs the JS side (value conversion, event dispatch and the DOM
// helpers - see JS_Web_Install). The commands keep no state on the C side.
// Returns plain TRUE/FALSE, not an RXR_* code.
int Web_Init(void) {
	Web_Install_JS();
	return TRUE;
}


#ifdef REB_EXT

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);
	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;
	if (!CHECK_STRUCT_ALIGN) return 0;
	return WEB_EXT_INIT_CODE;
}

RXIEXT int RX_Quit(int opts) {
	return 0;
}

RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Web_RX_Call(cmd, frm, ctx);
}

#endif
