Rebol [
	Title:   "Rebol REPL context binding test script"
	Author:  "Oldes"
	File:    %repl-test.r3
	Tabs:    4
	Needs:   quick-test
]

~~~start-file~~~ "REPL"

console: line-editor!
saved-console: system/console/current

;; Value of WORD in CTX, or NONE when the word is missing or unset.
ctx-value: func[ctx [any-object!] word [word!]][
	all [word: in ctx word  value? word  get word]
]
;; Fresh per session context, like starting a new console.
reset-console-ctx: does [console/console-ctx: context []]
;; Evaluate a command line exactly the way ON-LINE does.
eval-line: func[line [string!]][do console/bind-code transcode line]


===start-group=== "console session context"
	--test-- "set-word is session local"
		reset-console-ctx
		--assert 1 = try [eval-line "repl-a: 1"]
		--assert 1 = ctx-value console/console-ctx 'repl-a
		--assert none? ctx-value system/contexts/user 'repl-a
		--assert none? ctx-value system/contexts/lib  'repl-a

	--test-- "value from the user context is visible"
		reset-console-ctx
		set bind/new 'repl-b system/contexts/user 42
		--assert 42 = try [eval-line "repl-b"]

	--test-- "self reference to a user context value"
		;; BIND/SET adds REPL-B to the console context before the block is
		;; bound, so the plain REPL-B must not end up masked by an unset slot
		reset-console-ctx
		set bind/new 'repl-b system/contexts/user 42
		--assert 43 = try [eval-line "repl-b: repl-b + 1"]
		--assert 43 = ctx-value console/console-ctx 'repl-b
		--assert 42 = ctx-value system/contexts/user 'repl-b ;; user context untouched

	--test-- "unreached set-word does not mask LIB"
		reset-console-ctx
		try [eval-line "if false [append: 1]"]
		--assert same? get in system/contexts/lib 'append ctx-value console/console-ctx 'append
		--assert [1] = try [eval-line "append copy [] 1"]

	--test-- "console context is not shared between sessions"
		reset-console-ctx
		try [eval-line "repl-c: 1"]
		reset-console-ctx
		--assert none? ctx-value console/console-ctx 'repl-c
===end-group===


===start-group=== "module exports and the console context"
	system/console/current: console

	--test-- "exports are resolved into the console context"
		reset-console-ctx
		lib-append: get in system/contexts/lib 'append
		m: module [name: repl-mod-1 exports: [append repl-exported]][
			append:        func[x][join "MOD:" x]
			repl-exported: does [1]
		]
		import (m)
		;; a colliding export must not override LIB...
		--assert same? :lib-append ctx-value system/contexts/lib 'append
		;; ...but must be usable in the console session
		--assert same? get in m 'append ctx-value console/console-ctx 'append
		--assert "MOD:x" = try [eval-line {append "x"}]
		;; a non colliding export goes to LIB as before
		--assert same? get in m 'repl-exported ctx-value system/contexts/lib 'repl-exported
		--assert same? get in m 'repl-exported ctx-value console/console-ctx 'repl-exported

	--test-- "later import refreshes the console copy"
		m: module [name: repl-mod-2 exports: [append]][
			append: func[x][join "MOD2:" x]
		]
		import (m)
		--assert "MOD2:x" = try [eval-line {append "x"}]

	--test-- "import without a console does not fail"
		system/console/current: none
		--assert module? try [import (module [name: repl-mod-3 exports: [repl-exported-3]][
			repl-exported-3: does [3]
		])]
		system/console/current: console

	--test-- "private module exports stay in the user context"
		reset-console-ctx
		m: module [options: [private] exports: [repl-private]][repl-private: does [1]]
		import (m)
		--assert same? get in m 'repl-private ctx-value system/contexts/user 'repl-private
		--assert none? ctx-value system/contexts/lib 'repl-private
		--assert 1 = try [eval-line "repl-private"]
===end-group===

system/console/current: saved-console
reset-console-ctx

~~~end-file~~~