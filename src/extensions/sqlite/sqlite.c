//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/SQLite extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Entry points for the Rebol/SQLite extension module.
//
//   REB_EXT defined ... standalone sqlite-x64.rebx
//   REB_EXT absent .... compiled into the host, registered at startup
//

#include "gen-sqlite.h"
#include "sqlite.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = SQLITE_EXT_INIT_CODE;

// Symbols of the registered SQLite handle types. Declared in the generated
// header (from the spec's `c-header:`), defined here.
REBCNT Handle_SQLiteDB   = 0;
REBCNT Handle_SQLiteSTMT = 0;

// Temporary buffer used to pass exception messages back to the Rebol side.
char error_buffer[255];


// Registers the SQLiteDB and SQLiteSTMT handle types. Must run in BOTH
// build modes.
static void Register_SQLite_Handles(void) {
	Handle_SQLiteDB   = RL_REGISTER_HANDLE((REBYTE*)"sqlite-db",   sizeof(SQLITE_CONTEXT), SQLiteDBHandle_release);
	Handle_SQLiteSTMT = RL_REGISTER_HANDLE((REBYTE*)"sqlite-stmt", sizeof(SQLITE_STMT),     SQLiteSTMTHandle_release);
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

	Register_SQLite_Handles();
	sqlite3_initialize();
	return init_block;
}

RXIEXT int RX_Quit(int opts) {
	debug_print("SQLite extension shutdown.\n", NULL);
	sqlite3_shutdown();
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-sqlite.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Sqlite_RX_Call(cmd, frm, ctx);
}

#else

/***********************************************************************
**  Embedded into the host
***********************************************************************/

// Called from host-main.c under #ifdef INCLUDE_EXT_SQLITE.
// No version or alignment check: same binary, true by construction.
/***********************************************************************
**
*/	RL_API void OS_Init_Ext_SQLite(void)
/*
**	Initialize embedded SQLite extension module
**
***********************************************************************/
{
	RL = RL_Extend(b_cast(init_block), (RXICAL)&Sqlite_RX_Call);
	Register_SQLite_Handles();
	sqlite3_initialize();
}

#endif
