//
// Project: Rebol/Easing extension
// SPDX-License-Identifier: Unlicense
// ===========================================================================
// Entry points for the easing module.
//
//   REB_EXT defined ... standalone easing-x64.rebx
//   REB_EXT absent .... compiled into the host, registered at startup
//
// The extension registers no handle types and keeps no state, so there is
// nothing to set up beyond handing the module init code to the host.
//

#include "gen-easing.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static const char *init_block = EASING_EXT_INIT_CODE;

int Easing_Init(void) {
	// No special initialisation in this extension.
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

	debug_print("RXinit easing extension; Rebol v%i.%i.%i\n",
		ver[1], ver[2], ver[3]);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) {
		debug_print("Needs at least Rebol v%i.%i.%i!\n",
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
// dispatcher is generated into gen-easing.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Easing_RX_Call(cmd, frm, ctx);
}

#else

/***********************************************************************
**  Embedded into the host
***********************************************************************/

// Called from host-main.c under #ifdef INCLUDE_EXT_EASING.
// No version or alignment check: same binary, true by construction.
/***********************************************************************
**
*/	RL_API void OS_Init_Ext_Easing(void)
/*
**	Initialize embedded easing module
**
***********************************************************************/
{
	RL = RL_Extend(b_cast(init_block), (RXICAL)&Easing_RX_Call);
}

#endif
