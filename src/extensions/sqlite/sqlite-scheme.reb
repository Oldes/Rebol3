Rebol [
	title: "SQLite scheme (WIP)"
	file:  %sqlite-scheme.reb
	note:  {This is just an initial proof of concept}
	version: 0.1.1
	author: "Oldes"
	needs:   [
		3.13.1 ;; using system/options/modules as extension location
		sqlite
	]
]

sys/make-scheme [
	title: "SQLite database scheme"
	name:  'sqlite
	spec:   make system/standard/port-spec-file []
	sqlite: _ ;; will be set when opening a port

	actor: [
		open: func [
			port [port!]
			/new
			/local path
		][	
			if open? port [ return port ]
			path: rejoin [
				any [select port/spec 'path   %./]
				any [select port/spec 'target %.db]
			]
			port/spec/path: copy path: clean-path path
			if all [not new not exists? path][
				cause-error 'Access 'cannot-open reduce [path "file not exists!"]
			]
			;; SQLite expect full path in the local format (C:/ on Windows)
			;; but Rebol's open function does not accept string...
			;; so do this strange thing to get over it
			all [
				system/platform = 'Windows
				path: as file! to-local-file path
			]
			port/state: make object! [
				db:          sqlite/open path  ;; used to store a database handle
				statements:  make map! 0       ;; prepared statements
				query:                         ;; last used query
				stmt:        none              ;; last prepared statement
				trace-level: 0
			]
			return port
		]
		
		open?: func [port [port!]][
			;; Init sqlite extension if needed.
			unless module? sqlite [
				sqlite: import 'sqlite
			]
			;; port/state is none until the port is opened, so it must be
			;; checked first - `port/state/db` alone throws on a fresh port.
			all [
				object? port/state
				handle? port/state/db
			]
		]

		close: func [port [port!] /local state][
			unless open? port [	cause-error 'Access 'not-open port/spec/ref ]
			state: port/state
			;; Finalize the statements BEFORE closing the connection.
			;; sqlite3_close() refuses to close a connection which still has
			;; live prepared statements, and the extension discards that result
			;; code while clearing its handle - so closing first would leak the
			;; connection for good.
			foreach [query stmt] state/statements [
				sqlite/finalize stmt
			]
			clear state/statements
			sqlite/close state/db
			state/db:
			state/query:
			state/stmt: none
			port
		]

		;; WRITE now just executes a query... no result is collected, but may be printed in console
		write: func[port [port!] query [string!]][
			unless open? port [	cause-error 'Access 'not-open port/spec/ref ]
			sqlite/exec port/state/db port/state/query: query
			port
		]

		;; INSERT is used to prepare a statement, which is then used with other actions
		insert: func[port [port!] query [string!] /local ps stmt key][
			unless open? port [	cause-error 'Access 'not-open port/spec/ref ]
			ps: port/state
			ps/query: query
			either stmt: select ps/statements query [
				;; make sure that the statement starts from begining
				sqlite/reset stmt
			][
				;; prepare the new statement and store it for later use
				stmt: sqlite/prepare ps/db query
				ps/statements/:query: stmt
			]
			ps/stmt: stmt
			port
		]

		;; TAKE used to get just a single row (or multiple)
		take: func[
			port [port!]
			/part length [integer!]
		][
			read/part port any [length 1] 
		]

		;; READ used to get all rows if used without refinement (or multiple when used with /part)
		read: func[
			port [port!]
			/part length [integer!]
			/local stmt temp data
		][
			unless open? port [	cause-error 'Access 'not-open port/spec/ref ]
			stmt: port/state/stmt
			unless handle? stmt [
				cause-error 'Script 'invalid-arg "No prepared statement - use INSERT with a query first"
			]
			;; A fresh block per port.
			unless block? data: port/data [data: port/data: make block! 32]
			clear data

			temp: sqlite/step/rows stmt any [length 0]
			either block? temp [
				append data temp
				unless part [
					;; gets all rows
					while [
						block? temp: sqlite/step/rows stmt 0
					][	append data temp ]
					sqlite/reset stmt
				]
			][
				return none
			]
			data
		]

		;; PICK is a shortcut for READ INSERT "query"
		pick: func[
			port [port!]
			query [string!]
		][
			read insert port query
		]

		modify: func[
			port  [port!]
			field [word!]
			value [integer!]
		][
			switch/default field [
				trace-level [
					sqlite/trace port/state/db port/state/trace-level: value
				]
			][
				cause-error 'Script 'invalid-arg field
			]
		]
	]
]
