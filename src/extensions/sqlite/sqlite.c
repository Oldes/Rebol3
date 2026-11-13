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
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Sqlite_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so
// that `Options: [delay]` can postpone it until the module is imported.
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


// Registers the SQLiteDB and SQLiteSTMT handle types and starts SQLite up.
// Runs when the module body evaluates, so it happens at the same point in
// both build modes - and only on first import under `delay`.
//
// The get_path/set_path accessors are what make `db/filename` and `stmt/sql`
// work; the fields they accept come from the `handles:` block of the
// specification.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Sqlite_Init(void) {
	REBHSP spec;

	CLEARS(&spec);
	spec.size     = sizeof(SQLITE_CONTEXT);
	spec.free     = SQLiteDBHandle_free;
	spec.get_path = SQLiteDB_get_path;
	spec.set_path = SQLiteDB_set_path;
	Handle_SQLiteDB = RL_REGISTER_HANDLE_SPEC((REBYTE*)"sqlite-db", &spec);

	CLEARS(&spec);
	spec.size     = sizeof(SQLITE_STMT);
	spec.free     = SQLiteSTMTHandle_free;
	spec.get_path = SQLiteSTMT_get_path;
	// no settable fields on a statement
	Handle_SQLiteSTMT = RL_REGISTER_HANDLE_SPEC((REBYTE*)"sqlite-stmt", &spec);
	sqlite3_initialize();
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

#endif