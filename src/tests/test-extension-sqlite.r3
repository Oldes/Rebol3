Rebol [
	Title:   "Rebol/SQLite extension test"
	Needs:   3.13.2
	Purpose: {
		Exercises the extension's command surface and - importantly - every
		path accessor of every registered handle type, because those are
		resolved through generated word enums where a wrong index is silent.

		Usage:
			r3 test.r3

		Exits with 1 if any assertion failed, so it can gate CI.
	}
]

print ["Running test on Rebol build:" mold to-block system/build]
system/options/quiet: false
system/options/log/rebol: 4

unless find system/options/args "--internal" [
	either CI?: any [
		"true" = get-env "CI"
		"true" = get-env "GITHUB_ACTIONS"
		"true" = get-env "TRAVIS"
		"true" = get-env "CIRCLECI"
		"true" = get-env "GITLAB_CI"
	][
		system/options/modules: dirize to-rebol-file any [
			get-env 'REBOL_MODULES_DIR
			what-dir
		]
		;; CI Test still prioritize existing module
	][
		;; Make sure that we load a fresh extension - the module directory may hold a
		;; previously installed copy, which would import cleanly and quietly make
		;; every test below meaningless.
		try [system/modules/sqlite: none]
	]

	if all [not CI?  modules-dir: get-env 'REBOL_MODULES_DIR][
		system/options/modules: dirize to-rebol-file modules-dir
	]
]

;;=============================================================================
;; Minimal self-contained harness
;;=============================================================================

test-count: 0
fail-count: 0
skip-count: 0
failed: copy []

group: func ["Starts a named group of tests" name [string!]][
	print ajoin [lf as-yellow "== " as-yellow name]
]

;; NOTE: plain `func` with explicit locals - `function` would collect the
;; counter set-words as locals and the totals would never move.
--test--: func [
	"Passes when the code block evaluates to true"
	name [string!]
	code [block!]
	/local result
][
	test-count: test-count + 1
	result: try code
	case [
		error? :result [
			fail-count: fail-count + 1
			append failed name
			print [as-red "[FAIL]" name]
			print [as-red "      " mold/flat :result]
		]
		:result = true [
			print [as-green "[ ok ]" name]
		]
		true [
			fail-count: fail-count + 1
			append failed name
			print [as-red "[FAIL]" name "=>" mold/flat/part :result 60]
		]
	]
	:result
]

--skip--: func ["Counts a test which cannot run here" name [string!]][
	skip-count: skip-count + 1
	print [as-purple "[skip]" name]
]

;; Reads `handle/field` for a dynamically supplied field name.
;; Paths evaluate parens before dispatching to the handle's get_path, so this
;; goes down exactly the same code path as a literal `db/filename` would.
get-field: func [hndl [handle!] name [word!]][ hndl/(name) ]

--reads--: func [
	"Checks that every listed field is readable and has an expected type"
	label  [string!]
	hndl   [any-type!]
	fields [block!] "name [types] pairs"
][
	unless handle? :hndl [
		--skip-- ajoin [label " accessors (no handle)"]
		exit
	]
	foreach [name types] fields [
		--test-- ajoin [label "/" name] [
			did find types type?/word get-field hndl name
		]
	]
]

--rejects--: func [
	"Checks that reading or writing an unsupported field is refused"
	label [string!]
	code  [block!]
][
	--test-- ajoin [label " is refused"] compose/only [error? try (code)]
]

about?: func ["Compares two numbers with a tolerance" a b tol][tol >= abs a - b]

summary: does [
	print ajoin [lf as-yellow "-----------------------------------------------------------------------------"]
	print [
		"tests:" test-count
		as-green ajoin ["passed: " test-count - fail-count - skip-count]
		either fail-count > 0 [as-red ajoin ["failed: " fail-count]][ajoin ["failed: " fail-count]]
		as-purple ajoin ["skipped: " skip-count]
	]
	if fail-count > 0 [
		print as-red ajoin ["^/Failed tests:^/  " mold/flat failed]
	]
	print ""
	quit/return either fail-count > 0 [1][0]
]

;;=============================================================================
;; Test data
;;=============================================================================


;; Databases created by this suite - removed before and after the run.
test-files: [%test.db %test-gc.db %test-scheme.db]
remove-test-files: does [foreach file test-files [attempt [delete file]]]
remove-test-files

;;=============================================================================
group "Module surface"
;;=============================================================================

unless value? 'sqlite [sqlite: import 'sqlite]

;; every handle the suite may create, so a failed creation cannot abort it
db: stmt: port: none

--test-- "module imported" [module? sqlite]
--test-- "info returns a string" [string? sqlite/info]

foreach name [
	info open close exec eval prepare step reset finalize columns
	last-insert-id trace initialize shutdown
][
	--test-- ajoin ["command " name] compose [command? get in sqlite (to lit-word! name)]
]

;; `init-words` is generated, called once and then protected/hidden -
;; it must not be reachable from the outside.
--test-- "init-words is hidden" [none? in sqlite 'init-words]

;; The scheme is merged into the module through `reb-include:`. A build
;; without it is still valid, so this only decides whether the port scheme
;; group runs at all - it is not an assertion.
has-scheme?: did find system/schemes 'sqlite
print ["Port scheme registered:" has-scheme?]

;;=============================================================================
group "Opening a database"
;;=============================================================================

--test-- "open returns a handle" [handle? db: sqlite/open %test.db]

unless handle? :db [
	print as-red "^/Could not open the database - stopping here."
	summary
]

--test-- "info/of describes the handle" [string? sqlite/info/of db]
--test-- "opening a file in a missing directory fails" [
	error? try [sqlite/open %no-such-dir/nope.db]
]

;;=============================================================================
group "EXEC"
;;=============================================================================

--test-- "exec runs several statements at once" [
	sqlite/exec db {
BEGIN TRANSACTION;
DROP TABLE IF EXISTS Cars;
DROP TABLE IF EXISTS Genres;
DROP TABLE IF EXISTS Vals;
CREATE TABLE Cars(Id INTEGER PRIMARY KEY, Name TEXT, Price INTEGER);
INSERT INTO Cars VALUES(1,'Audi',52642);
INSERT INTO Cars VALUES(2,'Mercedes',57127);
INSERT INTO Cars VALUES(3,'Skoda',9000);
INSERT INTO Cars VALUES(4,'Volvo',29000);
INSERT INTO Cars VALUES(5,'Bentley',350000);
INSERT INTO Cars VALUES(6,'Citroen',21000);
INSERT INTO Cars VALUES(7,'Hummer',41400);
CREATE TABLE Genres(Id INTEGER PRIMARY KEY, Name TEXT NOT NULL);
CREATE TABLE Vals(id INTEGER PRIMARY KEY, val);
COMMIT;
}
	true
]
--test-- "exec refuses an invalid query" [error? try [sqlite/exec db "INVALID"]]

;;=============================================================================
group "EVAL"
;;=============================================================================

--test-- "eval returns a flat block of values" [
	[1 "Audi" 52642] = sqlite/eval db {SELECT * FROM Cars WHERE Id = 1}
]
--test-- "eval returns rows in order" [
	["Audi" "Mercedes"] = sqlite/eval db {SELECT Name FROM Cars WHERE Id < 3 ORDER BY Id}
]
--test-- "eval reports the number of changes" [
	3 = sqlite/eval db [{INSERT INTO Genres (Name) VALUES (?)} "Fantasy" "Crime" "Comedy"]
]
--test-- "eval accepts a query which is not at its head" [
	block? sqlite/eval db next {XSELECT * FROM Genres}
]
--test-- "eval refuses an unknown table" [
	error? try [sqlite/eval db "SELECT * FROM NoSuchTable"]
]

--test-- "last-insert-id is an integer" [integer? sqlite/last-insert-id db]

;;=============================================================================
group "Parameter binding"
;;=============================================================================

--test-- "values of every supported type can be bound" [
	sqlite/eval db [
		{INSERT INTO Vals (id, val) VALUES (?,?)}
		1 42
		2 3.14
		3 "text"
		4 none
		5 #{DECAFBAD}
		6 true
		7 false
	]
	7 = first sqlite/eval db "SELECT count(*) FROM Vals"
]
--test-- "integer! round trip"  [42          = first sqlite/eval db "SELECT val FROM Vals WHERE id = 1"]
--test-- "decimal! round trip"  [3.14        = first sqlite/eval db "SELECT val FROM Vals WHERE id = 2"]
--test-- "string! round trip"   ["text"      = first sqlite/eval db "SELECT val FROM Vals WHERE id = 3"]
--test-- "none! becomes NULL"   [none?         first sqlite/eval db "SELECT val FROM Vals WHERE id = 4"]
--test-- "binary! round trip"   [#{DECAFBAD} = first sqlite/eval db "SELECT val FROM Vals WHERE id = 5"]
;; logic! is bound as an integer, so it reads back as 1 / 0
--test-- "true is stored as 1"  [1 = first sqlite/eval db "SELECT val FROM Vals WHERE id = 6"]
--test-- "false is stored as 0" [0 = first sqlite/eval db "SELECT val FROM Vals WHERE id = 7"]

--test-- "any-string types are bound as their text content" [
	sqlite/eval db [
		{INSERT INTO Vals (id, val) VALUES (?,?)}
		10 %some/file.txt
		11 http://example.com
		12 user@example.com
		13 <a-tag>
	]
	all [
		"some/file.txt"      = first sqlite/eval db "SELECT val FROM Vals WHERE id = 10"
		"http://example.com" = first sqlite/eval db "SELECT val FROM Vals WHERE id = 11"
		"user@example.com"   = first sqlite/eval db "SELECT val FROM Vals WHERE id = 12"
		"a-tag"              = first sqlite/eval db "SELECT val FROM Vals WHERE id = 13"
	]
]

unicode-text: "Příliš žluťoučký kůň úpěl ďábelské ódy"
--test-- "wide strings survive the UTF-8 conversion" [
	sqlite/eval db [{INSERT INTO Vals (id, val) VALUES (?,?)} 20 unicode-text]
	unicode-text = first sqlite/eval db "SELECT val FROM Vals WHERE id = 20"
]
--test-- "the length is counted in characters, not in bytes" [
	(length? unicode-text) = first sqlite/eval db "SELECT length(val) FROM Vals WHERE id = 20"
]
--test-- "a parameter which is not at its head position" [
	;; the block is not reduced - words in it are resolved, but an expression
	;; has to be evaluated up front, here using compose
	sqlite/eval db compose [{INSERT INTO Vals (id, val) VALUES (?,?)} 21 (skip "XXhello" 2)]
	"hello" = first sqlite/eval db "SELECT val FROM Vals WHERE id = 21"
]
--test-- "parameters grouped in blocks run the statement repeatedly" [
	sqlite/eval db [
		{INSERT INTO Vals (id, val) VALUES (?,?)}
		[30 "a"]
		[31 "b"]
		[32     ] ;; the missing value is NULL
	]
	all [
		"a" = first sqlite/eval db "SELECT val FROM Vals WHERE id = 30"
		none?  first sqlite/eval db "SELECT val FROM Vals WHERE id = 32"
	]
]
--test-- "a NOT NULL constraint is reported" [
	error? try [sqlite/eval db [{INSERT INTO Genres (Name) VALUES (?)} none]]
]

;;=============================================================================
group "Prepared statements"
;;=============================================================================

--test-- "prepare returns a handle" [
	handle? stmt: sqlite/prepare db "SELECT Id, Name FROM Cars WHERE Price > ? ORDER BY Name"
]
--test-- "columns returns the column names" [["Id" "Name"] = sqlite/columns stmt]
--test-- "step/with binds the parameters" [
	block? sqlite/step/with/rows stmt [20000] 100
]
--test-- "reset rewinds the statement" [
	sqlite/reset stmt
	block? sqlite/step/with/rows stmt [40000.0] 100
]
--test-- "finalize releases the statement" [
	sqlite/finalize stmt
	true
]
--test-- "a finalized statement cannot be used" [
	error? try [sqlite/step stmt]
]

--test-- "columns is none when there is no result set" [
	stmt: sqlite/prepare db {INSERT INTO Vals (val) VALUES (?)}
	none? sqlite/columns stmt
]
sqlite/finalize stmt

total-cars: first sqlite/eval db "SELECT count(*) FROM Cars"
--test-- "step returns a single row by default" [
	stmt: sqlite/prepare db "SELECT Name FROM Cars ORDER BY Name"
	1 = length? sqlite/step stmt
]
--test-- "step/rows returns at most the requested number of rows" [
	3 = length? sqlite/step/rows stmt 3
]
--test-- "a count below 1 returns all the remaining rows" [
	(total-cars - 4) = length? sqlite/step/rows stmt 0
]
--test-- "an exhausted statement returns none" [none? sqlite/step stmt]
sqlite/finalize stmt

--test-- "eval accepts a prepared statement" [
	stmt: sqlite/prepare db {SELECT Name FROM Genres WHERE Name LIKE :pattern}
	all [
		block? sqlite/eval db [stmt "F%"]
		block? sqlite/eval db [stmt "C%"]
	]
]
sqlite/finalize stmt
--test-- "eval refuses a finalized statement" [
	error? try [sqlite/eval db [stmt "F%"]]
]

;;=============================================================================
group "Handle accessors"
;;=============================================================================

--reads-- "sqlite-db" db [
	filename       [file! none!]
	readonly       [logic!]
	autocommit     [logic!]
	last-insert-id [integer!]
	changes        [integer!]
	total-changes  [integer!]
	error-code     [integer!]
	error          [string!]
	trace          [integer!]
]

--test-- "db/last-insert-id matches the command" [
	db/last-insert-id = sqlite/last-insert-id db
]
--test-- "the connection is writable and in autocommit mode" [
	all [db/readonly = false  db/autocommit = true]
]
--test-- "db/trace round trip" [
	db/trace: 1
	also db/trace = 1 (db/trace: 0)
]
--test-- "db/busy-timeout can be set" [
	db/busy-timeout: 250
	true
]
--rejects-- "sqlite-db/busy-timeout read" [db/busy-timeout]
--rejects-- "sqlite-db/nonsense"          [db/nonsense]
--rejects-- "sqlite-db/filename write"    [db/filename: %other.db]

stmt: sqlite/prepare db "SELECT Id, Name FROM Cars WHERE Price > ?"

--reads-- "sqlite-stmt" stmt [
	sql          [string!]
	expanded-sql [string! none!]
	columns      [block!]
	column-count [integer!]
	data-count   [integer!]
	parameters   [integer!]
	readonly     [logic!]
	busy         [logic!]
	result-code  [integer!]
]

--test-- "stmt/sql is the text it was prepared from" [
	stmt/sql = "SELECT Id, Name FROM Cars WHERE Price > ?"
]
--test-- "stmt/columns matches the columns command" [
	stmt/columns = sqlite/columns stmt
]
--test-- "stmt reports its shape" [
	all [
		stmt/column-count = 2
		stmt/parameters   = 1
		stmt/readonly     = true
	]
]
--rejects-- "sqlite-stmt/nonsense" [stmt/nonsense]
--rejects-- "sqlite-stmt/sql write" [stmt/sql: "SELECT 1"]
sqlite/finalize stmt

;;=============================================================================
group "Vector binding (sqlite-vec)"
;;=============================================================================

;; The only place where the vector branch of the parameter binding is used -
;; an f32 vector of 8 items must arrive as exactly the 32 bytes vec0 expects.
either find sqlite/eval db {PRAGMA module_list;} "vec0" [
	embedding: attempt [make vector! [decimal! 32 [0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8]]]
	either vector? :embedding [
		--test-- "a vector! is bound as a compact blob" [
			sqlite/exec db {
				DROP TABLE IF EXISTS vec_test;
				CREATE VIRTUAL TABLE vec_test USING vec0(embedding float[8]);
			}
			sqlite/eval db [{INSERT INTO vec_test(rowid, embedding) VALUES (?,?)} 1 embedding]
			32 = length? first sqlite/eval db {SELECT embedding FROM vec_test WHERE rowid = 1}
		]
		--test-- "a vector of a wrong size is refused" [
			error? try [
				sqlite/eval db compose [
					{INSERT INTO vec_test(rowid, embedding) VALUES (?,?)}
					2 (make vector! [decimal! 32 [1.0 2.0]])
				]
			]
		]
	][
		--skip-- "vector! binding (could not construct an f32 vector)"
	]
][
	--skip-- "vector! binding (sqlite-vec is not available)"
]

;;=============================================================================
group "Port scheme"
;;=============================================================================

;; The scheme is merged into the extension module through `reb-include:`, but
;; the module may be built without it - then there is nothing to test here.
either has-scheme? [

	--test-- "a database port can be opened" [
		port? port: open/new sqlite:test-scheme.db
	]
	if port? :port [
		--test-- "write executes a query" [
			write port {CREATE TABLE Cars(Id INTEGER PRIMARY KEY, Name TEXT, Price INTEGER);}
			write port {INSERT INTO Cars VALUES
				(1,'Audi',52642),(2,'Skoda',9000),(3,'Volvo',29000),
				(4,'Bentley',350000),(5,'Citroen',21000),(6,'Hummer',41400);}
			true
		]
		--test-- "insert prepares a statement" [port? insert port "SELECT * FROM Cars"]
		--test-- "take returns a single row" [3 = length? take port]
		--test-- "read/part returns the requested rows" [6 = length? read/part port 2]
		--test-- "read returns the rest" [block? read port]
		--test-- "pick prepares and reads in one step" [
			block? pick port "SELECT Name FROM Cars ORDER BY Id"
		]
		--test-- "the statement cache reuses a prepared query" [
			a: pick port "SELECT Name FROM Cars ORDER BY Id"
			b: pick port "SELECT Name FROM Cars ORDER BY Id"
			a = b
		]
		--test-- "modify sets the trace level" [
			modify port 'trace-level 0
			true
		]
		--test-- "modify refuses an unknown field" [
			error? try [modify port 'no-such-field 1]
		]
		--test-- "the port can be closed" [
			close port
			true
		]
		--test-- "a closed port is not open" [not open? port]
		--test-- "reading a closed port is refused" [error? try [read port]]
	]
	--test-- "opening a missing file without /new is refused" [
		error? try [open sqlite:no-such-file.db]
	]
][
	--skip-- "port scheme (not registered)"
]

;;=============================================================================
group "Releasing handles"
;;=============================================================================

;; Neither the connection nor the statement below is closed explicitly, so the
;; release callbacks are the only thing which can free them.
--test-- "handles are released by the recycler" [
	recycle/torture
	gc-db:   sqlite/open %test-gc.db
	sqlite/exec gc-db {CREATE TABLE IF NOT EXISTS gc(a);}
	gc-stmt: sqlite/prepare gc-db {SELECT 1}
	gc-ok: [1] = sqlite/step gc-stmt
	;; drop every reference and let the recycler do the rest
	gc-db: gc-stmt: none
	recycle
	gc-ok
]
;; On Windows an open database cannot be deleted, so a leaked connection shows
;; up here. On POSIX an open file can be unlinked, so there the value of this
;; section is the crash-free recycle above.
--test-- "the released database file can be removed" [
	attempt [delete %test-gc.db]
	not exists? %test-gc.db
]

--test-- "close releases the connection" [
	sqlite/close db
	true
]

;;=============================================================================
group "Library lifecycle"
;;=============================================================================

--test-- "shutdown succeeds"   [true = sqlite/shutdown]
--test-- "initialize succeeds" [true = sqlite/initialize]

remove-test-files
summary
