REBOL [
	Title:   "Rebol SQLite Extension"
	Name:    sqlite
	Version: 3.53.4.2
	Needs:   3.22.5
	Author:  @Oldes
	License: MIT
	Url:     https://github.com/Siskin-framework/Rebol-SQLite
	Exports: []
	Options: [delay]
	Purpose: {
		Bindings to the SQLite C library (with the sqlite-vec extension
		auto-loaded on every opened connection).

		Builds either embedded into the host or as a standalone
		sqlite-x64.rebx library.
	}
]

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
	unsigned trace_mask;   // SQLite has no getter for it, so it is kept here
} SQLITE_CONTEXT;

typedef struct reb_sqlite_stmt {
	sqlite3_stmt* stmt;
	int last_result_code;
} SQLITE_STMT;
}

;; ---------------------------------------------------------------------------
;; Rebol sources merged into the module's mezzanine section at generation time.
;reb-include: [%sqlite-scheme.reb]

;; ---------------------------------------------------------------------------
;; Handle types registered by this extension.
;;
;; The keys are the names the handles are registered under on the C side
;; (RL_REGISTER_HANDLE), and each field is:
;;
;;     NAME  GET-types  SET-types  "description"
;;
;; where `none` in the SET column marks a read-only field.
;;
;; The generator collects these field names into `words/arg`, so the
;; W_SQLITE_ARG_* enum and `Sqlite_arg_words` are generated from this block.
;; The accessors themselves are SQLiteDB_get_path / SQLiteDB_set_path and
;; SQLiteSTMT_get_path in %sqlite-commands.c.
handles: [
	sqlite-db: [
		"SQLite database connection"
		;NAME            GET       SET       DESCRIPTION
		filename         file!     none      "Name of the file the database was opened from"
		readonly         logic!    none      "Whether the main database is read-only"
		autocommit       logic!    none      "Whether the connection is in the autocommit mode"
		last-insert-id   integer!  none      "Rowid of the most recent successful insert"
		changes          integer!  none      "Number of rows modified by the most recent statement"
		total-changes    integer!  none      "Number of rows modified since the connection was opened"
		error-code       integer!  none      "Result code of the most recent failed API call"
		error            string!   none      "Message of the most recent failed API call"
		trace            integer!  integer!  "Trace mask (1 = statements, 2 = profiles, 3 = both)"
		busy-timeout     none      integer!  "Milliseconds to wait for a locked table (write only)"
	]
	sqlite-stmt: [
		"SQLite prepared statement"
		;NAME            GET       SET       DESCRIPTION
		sql              string!   none      "The SQL text the statement was prepared from"
		expanded-sql     string!   none      "The SQL text with the bound parameters expanded"
		columns          block!    none      "Column names of the result set"
		column-count     integer!  none      "Number of columns in the result set"
		data-count       integer!  none      "Number of columns in the current result row"
		parameters       integer!  none      "Number of bindable parameters"
		readonly         logic!    none      "Whether the statement only reads from the database"
		busy             logic!    none      "Whether the statement is in the middle of its execution"
		result-code      integer!  none      "Result code of the last step"
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
