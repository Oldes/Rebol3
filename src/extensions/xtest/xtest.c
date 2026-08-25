//
// Project: Rebol/XTest extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Entry points for the extension interface test module.
//
//   REB_EXT defined ... standalone xtest-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Xtest_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so
// that `Options: [delay]` can postpone it until the module is imported.
//

#include "gen-xtest.h"
#include "xtest.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = XTEST_EXT_INIT_CODE;

// Symbol of the registered XTEST handle type. Declared in the generated
// header (from the spec's `c-header:`), defined here.
REBCNT Handle_XTest = 0;


// Registers the XTEST handle type, whose path accessors are what hob1/hob2
// exercise. Runs when the module body evaluates, so it happens at the same
// point in both build modes - and only on first import under `delay`.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Xtest_Init(void) {
	REBHSP spec;
	spec.size     = sizeof(XTEST);
	// XTestContext_free takes the HOB, not the raw data pointer.
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = XTestContext_free;
	spec.get_path = XTestContext_get_path;
	spec.set_path = XTestContext_set_path;
	spec.mold     = XTestContext_mold;
	Handle_XTest  = RL_REGISTER_HANDLE_SPEC(cb_cast("XTEST"), &spec);
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
// dispatcher is generated into gen-xtest.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Xtest_RX_Call(cmd, frm, ctx);
}

#else

/***********************************************************************
**  Embedded into the host
***********************************************************************/

// Called from host-main.c under #ifdef INCLUDE_EXT_XTEST.
//
// Only registers the module with the boot-exts list - no version or
// alignment check (same binary, true by construction) and no handle
// registration, which Xtest_Init() does when the module initializes.
/***********************************************************************
**
*/	RL_API void OS_Init_Ext_XTest(void)
/*
**	Initialize embedded extension test module
**
***********************************************************************/
{
	RL = RL_Extend(b_cast(init_block), (RXICAL)&Xtest_RX_Call);
}

#endif