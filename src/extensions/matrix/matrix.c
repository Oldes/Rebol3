//
// Project: Rebol/Matrix extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Entry points for the Rebol/Matrix extension.
//
// This single source builds either way:
//
//   REB_EXT defined ... a standalone extension library (matrix-x64.rebx),
//                       loaded at run time by `load-extension`
//   REB_EXT absent .... compiled straight into the host binary and
//                       registered at startup
//
// Only the entry points differ; the commands themselves (matrix-commands.c)
// and the dispatch table (matrix-commands-table.c, generated) are shared.
//

#include "gen-matrix.h"
#include "stdio.h"

#ifdef REB_EXT
// A standalone extension is its own binary and must provide the storage for
// the reb-lib pointer itself. When embedded, the storage already exists in
// host-lib.c - reb-lib.h declares it `extern` for both modes, so defining it
// here as well would be a duplicate symbol at link time.
RL_LIB *RL;
#endif

// Module header, command specs and mezzanine code, as generated from
// matrix.reb. Not const: RL_Extend() takes a REBYTE*.
static char *init_block = MATRIX_EXT_INIT_CODE;

int Matrix_Init(void) {
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

	puts("init");

	// Feature-level requirement: the commands below need a host at least
	// this new. Distinct from the ABI check in RX_Abi() - a host can be
	// new enough to be ABI-compatible while still lacking what we need.
	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;

	// Both sides must agree on struct packing.
	if (!CHECK_STRUCT_ALIGN) return 0;

	return init_block;
}

RXIEXT int RX_Quit(int opts) {
	return 0;
}

// Reports which RL_API ABI this extension was built against, so that
// `load-extension` can refuse to load it into a host whose supported
// range does not include it. An absent symbol is treated as ABI 0.
RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// `load-extension` resolves this by name, so it must keep exactly this
// spelling. The bounds-checked dispatcher itself is generated into
// matrix-commands-table.c under a prefixed name.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Matrix_RX_Call(cmd, frm, ctx);
}

#else

/***********************************************************************
**  Embedded into the host
***********************************************************************/

// Called from host-main.c under #ifdef INCLUDE_EXT_MATRIX.
//
// No version or struct-alignment check here: the extension and the host
// are the same binary, so both are true by construction.
//
// Nothing here uses the exported RX_* names - RL_Extend() takes the
// dispatcher as a pointer, so the prefixed name is passed directly and
// cannot collide with any other embedded extension.
/***********************************************************************
**
*/	RL_API void OS_Init_Ext_Matrix(void)
/*
**	Initialize embedded extension test module
**
***********************************************************************/
{
	RL = RL_Extend(b_cast(init_block), (RXICAL)&Matrix_RX_Call);
}

#endif