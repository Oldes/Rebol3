//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/SQLite extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Use on your own risc!
//
// Shared between the entry points (sqlite.c) and the command sources.
//
// The SQLITE_CONTEXT/SQLITE_STMT structs and the Handle_SQLiteDB /
// Handle_SQLiteSTMT externs come from the generated header, via the
// specification's `c-header:` field.
//

#ifndef SQLITE_EXT_H
#define SQLITE_EXT_H

// sqlite3.h and sqlite-vec.h arrive with the generated header, which must
// therefore be included before this one.
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// Handle release callbacks - registered by Register_SQLite_Handles() in
// sqlite.c, implemented in sqlite-commands.c.
int SQLiteDBHandle_free(void *ctx);
int SQLiteSTMTHandle_free(void *ctx);

// Handle path accessors. The words they switch on are generated from the
// `handles:` block of the specification.
int SQLiteDB_get_path  (REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int SQLiteDB_set_path  (REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int SQLiteSTMT_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);

REBSER* utf8_string(RXIARG arg);

#ifndef REB_EXT
// Embedded build only - called from host-main.c under INCLUDE_EXT_SQLITE.
void OS_Init_Ext_SQLite(void);
#endif

//==============================================================//
// Some useful defines                                          //
//==============================================================//

#define RESOLVE_SQLITE_CTX(n, i)                    \
			hob = RXA_HANDLE_CONTEXT(frm, i);       \
			n = (SQLITE_CONTEXT*)hob->data;         \
			if(!n || hob->sym != Handle_SQLiteDB )  \
				RETURN_ERROR("[SQLITE] Invalid SQLite DB handle!");

#define RESOLVE_SQLITE_STMT(n, i)                   \
			hobStmt = RXA_HANDLE_CONTEXT(frm, i);   \
			n = (SQLITE_STMT*)hobStmt->data;        \
			if(!n || hobStmt->sym != Handle_SQLiteSTMT || !(n)->stmt) \
				RETURN_ERROR("[SQLITE] Invalid SQLite STMT handle!");

#endif // SQLITE_EXT_H
