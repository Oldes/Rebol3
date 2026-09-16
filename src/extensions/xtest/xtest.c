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

//-------------------------------------------------------------------------
// A minimal device, registered to prove that an extension can add one to
// the host device table and that OS_Poll_Devices reaches it. There is no
// port scheme for it: an extension has only the RL_ API and cannot call
// OS_Do_Device itself, so the command table is exercised through polling.

int    Xtest_Dev_Id   = 0;  // 0 = not registered (a real id is >= RDI_MAX)
REBCNT Xtest_Dev_Polls = 0; // bumped by Poll_XTest

static DEVICE_CMD Init_XTest(REBREQ *dr) {
	REBDEV *dev = (REBDEV*)dr;
	SET_FLAG(dev->flags, RDF_INIT);
	return DR_DONE;
}

static DEVICE_CMD Quit_XTest(REBREQ *dr) {
	return DR_DONE;
}

// Must report DR_DONE, not DR_PEND: a non-zero result counts as a status
// change, OS_Wait then returns -1 immediately and WAIT would spin instead
// of sleeping. A real device returns non-zero only when it has something.
static DEVICE_CMD Poll_XTest(REBREQ *dr) {
	Xtest_Dev_Polls++;
	return DR_DONE;
}

static DEVICE_CMD_FUNC Xtest_Dev_Cmds[RDC_MAX] = {
	Init_XTest,
	Quit_XTest,
	0,	// RDC_OPEN
	0,	// RDC_CLOSE
	0,	// RDC_READ
	0,	// RDC_WRITE
	Poll_XTest,
	0,	// RDC_CONNECT
	0,	// RDC_QUERY
};

DEFINE_DEV(Dev_XTest, "XTest device", 1, Xtest_Dev_Cmds, RDC_MAX, 0);

//-------------------------------------------------------------------------
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

	// DEFINE_DEV leaves flags zeroed, so ask for polling before registering.
	SET_FLAG(Dev_XTest.flags, RDO_AUTO_POLL);
	Xtest_Dev_Id = RL_REGISTER_DEVICE(&Dev_XTest, sizeof(REBDEV));

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

#endif