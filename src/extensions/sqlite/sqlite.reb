REBOL [
	Title:   "Rebol SQLite Extension"
	Name:    sqlite
	Version: 3.53.4.1
	Needs:   3.13.2
	Author:  @Oldes
	License: MIT
	Url:     https://github.com/Siskin-framework/Rebol-SQLite
	Exports: []
	Purpose: {
		Bindings to the SQLite C library (with the sqlite-vec extension
		auto-loaded on every opened connection).

		Builds either embedded into the host or as a standalone
		sqlite-x64.rebx library.
	}
]

needs: 3.22.5  ;; generated module's Needs: + MIN_REBOL_VER/REV/UPD

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; NOTE: the SQLite headers are carried by `c-header:` rather than by
;; `c-include:`. The template emits `c-include:` inside the `#ifdef REB_EXT`
;; branch only, but the structs below need the sqlite types in BOTH build
;; modes, and `$c-header` is emitted after that #ifdef/#else block.
;c-include: []

;; Extra declarations the generated header must carry.
c-header: {
#include "sqlite3.h"
#include "sqlite-vec.h"

extern REBCNT Handle_SQLiteDB;
extern REBCNT Handle_SQLiteSTMT;

extern char error_buffer[255];

typedef struct reb_sqlite_context {
	sqlite3* db;
	REBSER*  buf;
	int      id;
	int      last_insert_count;
} SQLITE_CONTEXT;

typedef struct reb_sqlite_stmt {
	sqlite3_stmt* stmt;
	int last_result_code;
} SQLITE_STMT;
}

;; ---------------------------------------------------------------------------
;; Handle types registered by this extension (used for the README).
handles: [
	SQLiteDB: [
		db: "internal sqlite3* connection pointer"
	]
	SQLiteSTMT: [
		stmt: "internal sqlite3_stmt* prepared statement pointer"
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
commands: [
	info: [
		{Returns info about SQLite extension library}
		/of handle [handle!] {SQLite Extension handle}
	]
	open: [
		{Opens a new database connection}
		file [file!]
	]
	exec: [
		{Runs zero or more semicolon-separate SQL statements}
		db   [handle!] "sqlite-db"
		sql  [string!] "statements"
	]
	eval: [
		{Evaluates SQL statement with optional paramaters}
		db    [handle!] "sqlite-db"
		query [string! block! handle!] "single statement, a single statement with parameters or a prepared statement"
	]
	last-insert-id: [
		"Returns the rowid of the most recent successful INSERT into a rowid table or virtual table on database connection"
		db    [handle!] "sqlite-db"
	]
	finalize: [
		"Deletes prepared statement"
		stmt [handle!] "sqlite-stmt"
	]
	trace: [
		{Traces debug output}
		db   [handle!] "sqlite-db"
		mask [integer!]
	]
	prepare: [
		"Prepares SQL statement"
		db   [handle!] "sqlite-db"
		sql  [string!] "statement"
	]
	reset: [
		"Resets prepared statement"
		stmt [handle!] "sqlite-stmt"
	]
	step: [
		"Executes prepared statement"
		stmt [handle!] "sqlite-stmt"
		/rows "Multiple times if there is enough rows in the result"
		 count [integer!]
		/with
		 parameters [block!]
	]
	close: [
		{Closes a database connection}
		db   [handle!] "sqlite-db"
	]
	columns: [
		{Returns column names associated with the statement}
		stmt [handle!] "sqlite-stmt"
	]

	initialize: [
		{Initializes the SQLite library}
	]
	shutdown: [
		{Deallocate any resources that were allocated}
	]
]
