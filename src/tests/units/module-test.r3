Rebol [
	Title:   "Rebol module! test script"
	Author:  "Peter W A Wood, Oldes"
	File: 	 %module-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]
system/options/quiet: false
system/options/log/rebol: 4

~~~start-file~~~ "module!"

modules-dir: system/options/modules
? modules-dir


===start-group=== "module keywords"
	--test-- "hidden"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1696
		--assert all [
			[a] = words-of m: module [] [a: 1]
			m/a = 1
		]
		--assert all [
			[b] = words-of m: module [] [hidden a: 1 b: does[a + 1]]
			error? try [m/a]
			m/b = 2
		]

	--test-- "export"
	;-- Note: using tests against user's context, because the unit tests are called using `wrap` which
	;--       callects and binds all set-words used in modules code bellow and so values are not imported.
	;@@ https://github.com/Oldes/Rebol-issues/issues/689
	;@@ https://github.com/Oldes/Rebol-issues/issues/1446
		unset in system/contexts/user 'a
		unset in system/contexts/user 'b
		unset in system/contexts/user 'c
		m: module [][export a: 1 b: 2 export c: does [a + b]]
		--assert all [
			object? spec: spec-of m ;@@ https://github.com/Oldes/Rebol-issues/issues/1006
			[a c] = spec/exports
			m/c = 3
			unset? :system/contexts/user/a
			unset? :system/contexts/user/b
			unset? :system/contexts/user/c
			module? import (m)
			system/contexts/user/a = 1
			unset? :system/contexts/user/b
			system/contexts/user/c = 3
		]

		unset in system/contexts/user 'a
		unset in system/contexts/user 'b
		unset in system/contexts/user 'c
		m: module [][a: 1 b: 2  c: does [a + b] export [a c]]
		--assert all [
			object? spec: spec-of m
			[a c] = spec/exports
			m/c = 3
			unset? :system/contexts/user/a
			unset? :system/contexts/user/b
			unset? :system/contexts/user/c
			module? import (m)
			system/contexts/user/a = 1
			unset? :system/contexts/user/b
			system/contexts/user/c = 3
		]

		unset in system/contexts/user 'a
		unset in system/contexts/user 'b
		unset in system/contexts/user 'c
		m: module [][set export [a c] none]
		--assert all [
			object? spec: spec-of m
			[a c] = spec/exports
			none? m/c
			unset? :system/contexts/user/a
			unset? :system/contexts/user/b
			unset? :system/contexts/user/c
			module? import (m)
			none? system/contexts/user/a
			unset? :system/contexts/user/b
			none? system/contexts/user/c
		]
	--test-- "select"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1380
		m: module [] [a: 1]
		--assert 1 = select m 'a
		--assert none? select m 'b

	--test-- "path notation access"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1007
		m: module [] [a: 1 2]
		--assert 1 = m/a
		unset in system/contexts/user 'a
		m: module [exports: [a]] [a: 1 2]
		--assert 1 = m/a
		--assert unset? :system/contexts/user/a
		import (m)
		--assert 1 = :system/contexts/user/a
		unset in system/contexts/user 'a

===end-group===

===start-group=== "make module"
	--test-- "title-of"
	;@@ https://github.com/Oldes/Rebol-issues/issues/572
		test: module [Title: "Test Module"] []
		--assert "Test Module" = try [title-of test]
		
	--test-- "inlined module"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1628
		unset 'z1 z2: 2
		--assert all [
			error? e: try [import "rebol [type: module] attempt [z1: 12345] z1"]
			e/id = 'not-defined
			e/arg1 = 'z1
		]
		--assert all [
			error? e: try [import "rebol [type: module] attempt [z2: 12345] z2"]
			e/id = 'not-defined
			e/arg1 = 'z2
		]
		--assert all [
			error? e: try [import "rebol [] module [] [attempt [z1: 12345] z1]"]
			e/id = 'not-defined
			e/arg1 = 'z1
		]
		--assert all [
			error? e: try [import "rebol [] module [] [attempt [z2: 12345] z2]"]
			e/id = 'not-defined
			e/arg1 = 'z2
		]
	--test-- "module body modified on export"
		module [] body: [export 'b]
		--assert body = ['b]

	--test-- "make module type assertion"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1444
		--assert all [
			error? err: try [module [type: :print][]]
			err/id = 'wrong-type
		]
		--assert all [
			error? err: try [module compose [type: (:print)][]]
			err/id = 'wrong-type
		]
	--test-- "make module! integer!" ; not allowed
	;@@ https://github.com/Oldes/Rebol-issues/issues/1551
	;@@ https://github.com/Oldes/Rebol-issues/issues/926
		--assert all [
			error? err: try [make module! 10]
			err/id = 'bad-make-arg
		]
	--test-- "make module! map!" ; not allowed
	;@@ https://github.com/Oldes/Rebol-issues/issues/1551
		--assert all [
			error? err: try [make module! #[a: 1]]
			err/id = 'bad-make-arg
		]
	--test-- "make module! object!" ; not allowed
	;@@ https://github.com/Oldes/Rebol-issues/issues/899
	;@@ https://github.com/Oldes/Rebol-issues/issues/1551
		--assert all [
			error? err: try [make module! context [a: 1]]
			err/id = 'bad-make-arg
		]
		--assert all [
			error? err: try [to module! context []]
			err/id = 'bad-make-arg
		]
		--assert all [
			error? err: try [make module! context []]
			err/id = 'bad-make-arg
		]
	--test-- "make module! empty block!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/898
		--assert all [
			error? err: try [to module! []]
			err/id = 'bad-make-arg
		]

===end-group===


===start-group=== "module import"
	--test-- "import"
	;@@ https://github.com/Oldes/Rebol-issues/issues/923
	write modules-dir/mymodule.reb {
Rebol [
    type: module
    name: mymodule
    exports: [myfunc]
]
print "mymodule imported"
myfunc: func [arg [string!]][reverse arg]
}
	import mymodule ;; does not have to be lit-word!
	--assert "cba" = myfunc "abc"
	import 'mymodule     ;-- this works... the file isn't reloaded... and indeed the console doesn't print another "mymodule imported"
	--assert "cba" = myfunc "abc"
	import (modules-dir/mymodule.reb) ;-- no crash (but must be in parens, because it is expression!)
	--assert "cba" = myfunc "abc"
	delete modules-dir/mymodule.reb

	--test-- "module needs itself"
	;@@ https://github.com/Oldes/Rebol3/issues/1108
		write modules-dir/needs-self.reb {
Rebol [
    type: module
    name: needs-self
    needs: [needs-self] ;<--- recursion!
]
export needs-self-value: 1
}
		--assert all [
			error? e: try [import 'needs-self]
			e/id = 'recursive-needs
			e/arg1 = 'needs-self
		]
		--assert all [ ;; the failed import must not leave any trace
			error? e: try [import 'needs-self]
			e/id = 'recursive-needs
			none? find system/modules 'needs-self
		]
		;; the same, when the module is referenced using its file
		write modules-dir/needs-self.reb ajoin [
			{Rebol [type: module name: needs-self needs: [} mold modules-dir/needs-self.reb {]]}
		]
		--assert all [
			error? e: try [import (modules-dir/needs-self.reb)]
			e/id = 'recursive-needs
		]
		;; unnamed module is recognized by its location
		write modules-dir/needs-self.reb ajoin [
			{Rebol [type: module needs: [} mold modules-dir/needs-self.reb {]]}
		]
		--assert all [
			error? e: try [import (modules-dir/needs-self.reb)]
			e/id = 'recursive-needs
		]
		delete modules-dir/needs-self.reb

	--test-- "modules needing each other"
	;@@ https://github.com/Oldes/Rebol3/issues/1108
		write modules-dir/needs-a.reb {Rebol [type: module name: needs-a needs: [needs-b]] export needs-a-value: 1}
		write modules-dir/needs-b.reb {Rebol [type: module name: needs-b needs: [needs-a]] export needs-b-value: 2}
		--assert all [
			error? e: try [import 'needs-a]
			e/id = 'recursive-needs
			e/arg1 = 'needs-a
			none? find system/modules 'needs-a
			none? find system/modules 'needs-b
		]
		;; once the cycle is removed, the modules may be imported
		write modules-dir/needs-b.reb {Rebol [type: module name: needs-b] export needs-b-value: 2}
		--assert all [
			module? try [import 'needs-a]
			module? system/modules/needs-a
			module? system/modules/needs-b
			system/modules/needs-a/needs-a-value = 1
			system/modules/needs-b/needs-b-value = 2
		]
		delete modules-dir/needs-a.reb
		delete modules-dir/needs-b.reb

	--test-- "import to user context"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1680
	;- using external script as tests are by default inside `wrap`
	;- and in such a case the words from module are not imported
	;- (I'm quite not sure yet, if it is good or bad)
	write %issue-1680.reb {
REBOL []
c: true ; should not be rewritten
m: module [][export a: 1 b: 2 export c: 3]
import (m)
;-assert-1
probe a = 1     ; exported
;-assert-2
probe unset? :b ; not exported
;-assert-3
probe logic? :c ; not rewritten

;-assert-4
unset [a b c]   ; reset all
import/no-user (m)
probe all [unset? :a unset? :b unset? :c]
}
	o: copy ""
	call/wait/shell/output reform [to-local-file system/options/boot %issue-1680.reb] o
	--assert "#(true)^/#(true)^/#(true)^/#(true)^/" = o
	delete %issue-1680.reb

	--test-- "import/no-lib"
	;- using external script again!
	write %no-lib-import.reb {
REBOL []
;-assert-1 = make sure, that a and b are not in lib
probe all [
	none? get in system/contexts/lib 'a
	none? get in system/contexts/lib 'b
]

m1: module [name: no-lib-test-a][export a: 1]
import (m1)

;-assert-2 = check existence of imported values
probe all [
	1 = a                      ; imported to user context
	1 = system/contexts/lib/a  ; imported also to lib
]

;-assert-3
unset 'a ; uset user's value
probe 1 = system/contexts/lib/a ; still available in lib

;-assert-4 = import using /no-lib
m2: module [name: no-lib-test-b][export b: 2]
import/no-lib (m2)
probe all [
	2 = b                      ; imported to user context
	none? get in system/contexts/lib 'b ; but not to lib
]}
	o: copy ""
	call/wait/shell/output reform [to-local-file system/options/boot %no-lib-import.reb] o
	--assert "#(true)^/#(true)^/#(true)^/#(true)^/" = o
	delete %no-lib-import.reb

	--test-- "import block"
	;- using external script again!
	write modules-dir/m1.reb {Rebol [name: m1 type: module] export m1a: 1}
	write modules-dir/m2.reb {Rebol [name: m2 type: module] export m2b: 2}
	write %block-import-1.reb {
Rebol []
probe all [
	block? b: import [m1 m2]
	module? b/1
	module? b/2
	m1a = 1
	m2b = 2
]}
	o: copy ""
	call/wait/shell/output reform [to-local-file system/options/boot %block-import-1.reb] o
	--assert "#(true)^/" = o
	--test-- "import/no-lib block"
	write %block-import-2.reb {
Rebol []
probe all [
	block? b: import/no-lib [m1 m2]
	module? b/1
	module? b/2
	m1a = 1
	m2b = 2
	none? get in system/contexts/lib 'm1a
	none? get in system/contexts/lib 'm1a
]}
	o: copy ""
	call/wait/shell/output reform [to-local-file system/options/boot %block-import-2.reb] o
	--assert "#(true)^/" = o
	delete %block-import-1.reb
	delete %block-import-2.reb
	delete modules-dir/m1.reb
	delete modules-dir/m2.reb

	--test-- "import/version"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1687
		m-1687: module [version: 1.0.0 name: m-1687][a: 1]
		--assert all [
			error? e: try [import/version (m-1687) 2.2.2]
			e/id = 'needs
		]
		--assert module? m: try [import/version (m-1687) 1.0.0]
		--assert all [
			;@@ https://github.com/Oldes/Rebol-wishes/issues/13
			none? in m 'lib-local ; because there was no import in this module
		]

	--test-- "import/check"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1686
		s: save/header none [f: func[arg][arg]] [type: module checksum: true]
		c: select first load/header s 'checksum
		--assert #{3556AE790FFAC48BBBBC1CF2BC5F8942782416F7} = c
		--assert block? m: sys/load-module/check s c
		--assert module? m/2
		--assert 22 = m/2/f 22
		--assert module? m: import/check (s) c
		--assert 33 = m/f 33
		; corrupt the source:
		change find s to-binary "f:" "g"
		--assert all [
			error? e: try [import/check (s) c]
			e/id = 'bad-checksum
		]


	--test-- "import string!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1721
		unset in system/contexts/user 'issue-1721-a
		unset in system/contexts/user 'issue-1721-b
		import {rebol [type: module] issue-1721-a: 1 export issue-1721-b: 2}
		--assert unset? :system/contexts/user/issue-1721-a
		--assert system/contexts/user/issue-1721-b = 2

system/options/modules: %files/
	;@@ https://github.com/Oldes/Rebol-issues/issues/1694
	--test-- "import needs word!"
		import 'test-needs-name
		--assert true? test-needs-name-result
	--test-- "import needs file!"
		;- at this moment files imported from other file
		;- does not use parent file's location!
		m: import %files/test-needs-file.reb
		--assert true? test-needs-file-result
		--assert all [
			;@@ https://github.com/Oldes/Rebol-wishes/issues/13
			none? in m 'lib-local ; the local import library is not visible from outside
		]

	--test-- "import needs url!"
		--assert not error? try [
			import https://github.com/Oldes/Rebol3/raw/master/src/tests/units/files/test-needs-url.reb
			--assert true? test-needs-url-result
		]
system/options/modules: modules-dir

	--test-- "lib-local"
	;@@ https://github.com/Oldes/Rebol-wishes/issues/13
		;; module with private imports has them available as `lib-local`...
		m: module/mixin [][
			x: 1
			get-local: does [lib-local/x]
			get-words: does [words-of lib-local]
		] object [x: 42 y: 2]
		--assert 1  = m/x
		--assert 42 = m/get-local
		--assert [x y] = m/get-words
		;; ...but only for its own code
		--assert [x get-local get-words] = words-of m
		--assert none? in m 'lib-local
		--assert error? try [m/lib-local]
		--assert [x: get-local: get-words:] = extract body-of m 2
		;; module without private imports does not have it at all
		m: module [][get-local: does [lib-local]]
		--assert [get-local] = words-of m
		--assert error? try [m/get-local]

	--test-- "do module from file"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1203
		--assert all [
			function? fn: do %files/test-1203.reb ;; result of the module's body
			object? o: fn                         ;; system/script at the time, when the module was evaluated
			o/title = "test-1203"
			system/script/title <> "test-1203"
		]

	--test-- "do module file"
	;@@ https://github.com/Oldes/Rebol3/issues/2915
		system/user/data/test-do: copy []
		write %test-do-module.reb {
Rebol [type: module name: test-do-module exports: [test-do-value]]
append system/user/data/test-do 'init
test-do-value: 1
1 + 2
}
		--assert 3 = try [do %test-do-module.reb] ;; result of the module's body
		--assert 3 = try [do %test-do-module.reb] ;; and it is evaluated every time
		--assert [init init] = system/user/data/test-do
		--assert all [ ;; the module is isolated: it is not registered and nothing is exported
			none? select system/modules 'test-do-module
			none? in lib 'test-do-value
			unset? get/any in system/contexts/user 'test-do-value
		]
		write %test-do-module.reb {
Rebol [type: module name: test-do-module exports: [test-do-value]]
test-do-value: 2
}
		--assert 2 = try [do %test-do-module.reb]
		;; IMPORT is the way to get the module and to have it registered
		--assert all [
			module? m: try [import %test-do-module.reb]
			same? m select system/modules 'test-do-module
			2 = lib/test-do-value
			2 = get/any in system/contexts/user 'test-do-value
		]
		;; DO of the already imported module still evaluates it as an isolated one
		--assert all [
			2 = try [do %test-do-module.reb]
			same? m select system/modules 'test-do-module
		]
		delete %test-do-module.reb

	--test-- "module file as a command line script"
		;; unlike with DO, the module is imported, so it may be used from the console (--halt)
		;- using the legacy REPL, because it accepts a piped input
		write %test-cli-module.reb {
Rebol [type: module name: test-cli-module exports: [test-cli-value]]
test-cli-value: 42
print "module evaluated"
}
		o: copy ""
		call/wait/shell/input/output reform [
			to-local-file system/options/boot "--legacy-repl --halt" %test-cli-module.reb
		] {print ["result:" test-cli-value module? select system/modules 'test-cli-module  same? :test-cli-value :lib/test-cli-value]^/quit^/} o
		--assert did find o "module evaluated"
		--assert did find o "result: 42 true true"
		;; unnamed module is not registered and its exports are only in the user's context
		write %test-cli-module.reb {
Rebol [type: module exports: [test-cli-value]]
test-cli-value: 42
}
		o: copy ""
		call/wait/shell/input/output reform [
			to-local-file system/options/boot "--legacy-repl --halt" %test-cli-module.reb
		] {print ["result:" test-cli-value none? in lib 'test-cli-value]^/quit^/} o
		--assert did find o "result: 42 true"
		delete %test-cli-module.reb

	--test-- "--import option"
	;@@ https://github.com/Oldes/Rebol3/issues/1394
		;; the module is imported before any other code (--do or the script)
		;; and its file is relative to the directory, where Rebol was started
		make-dir %test-1394/
		write %test-1394/pre.reb {
Rebol [type: module name: test-1394 exports: [test-1394-value]]
test-1394-value: 42
}
		write %test-1394/script.reb {
Rebol []
prin ["script:" test-1394-value module? select system/modules 'test-1394]
}
		o: copy ""
		call/wait/shell/output reform [
			to-local-file system/options/boot {--import test-1394/pre.reb --do "prin [{do:} test-1394-value]"}
		] o
		--assert did find o "do: 42"
		;; the script is evaluated from its own directory, but the import is still relative to the starting one
		o: copy ""
		call/wait/shell/output reform [
			to-local-file system/options/boot {--import test-1394/pre.reb test-1394/script.reb}
		] o
		--assert did find o "script: 42 true"
		delete %test-1394/pre.reb
		delete %test-1394/script.reb
		delete %test-1394/

	--test-- "do module file with and without the isolate option"
		;; DO does not force the isolation, it is the module's header, which decides
		repend lib ['test-do-lib-value 1]
		write %test-do-module.reb {
Rebol [type: module]
change-it: does [test-do-lib-value: 2]
change-it
}
		--assert 2 = try [do %test-do-module.reb]
		--assert 2 = lib/test-do-lib-value ;; not isolated module shares words with lib
		write %test-do-module.reb {
Rebol [type: module options: [isolate]]
change-it: does [test-do-lib-value: 3]
change-it
}
		--assert 3 = try [do %test-do-module.reb]
		--assert 2 = lib/test-do-lib-value ;; isolated module has its own copies
		delete %test-do-module.reb

===end-group===

===start-group=== "import of already imported modules"
	--test-- "import by name of a module with a different name"
	;@@ https://github.com/Oldes/Rebol3/issues/1362
	;@@ https://github.com/Oldes/Rebol3/issues/1900
		write modules-dir/test-1900.reb {Rebol [type: module name: test-1900-spoof] export test-1900-value: 1}
		--assert all [
			module? m: try [import 'test-1900]
			'test-1900 = select spec-of m 'name ;; renamed to the requested name
			same? m select system/modules 'test-1900
			none? find system/modules 'test-1900-spoof
		]
		delete modules-dir/test-1900.reb

	--test-- "import by name of an unnamed module"
	;@@ https://github.com/Oldes/Rebol3/issues/1900
		system/user/data/test-1900: copy []
		write modules-dir/test-1900-unnamed.reb {Rebol [type: module] append system/user/data/test-1900 'init}
		--assert all [
			module? m: try [import 'test-1900-unnamed]
			'test-1900-unnamed = select spec-of m 'name ;; it gets the requested name
			same? m try [import 'test-1900-unnamed]     ;; so it is not loaded again
			[init] = system/user/data/test-1900
		]
		delete modules-dir/test-1900-unnamed.reb

	--test-- "import file! of already imported named module"
	;@@ https://github.com/Oldes/Rebol3/issues/1360
		system/user/data/test-1360: copy []
		repend lib ['test-1360-own 'mine] ;; value, which is in lib before the module is imported
		write modules-dir/test-1360.reb {
Rebol [type: module name: test-1360 version: 1.0.0]
append system/user/data/test-1360 1.0.0
export test-1360-value: 1
export test-1360-own: 1
export test-1360-gone: 1
}
		--assert all [
			module? m: try [import 'test-1360]
			same? m try [import (modules-dir/test-1360.reb)] ;; the existing module is reused
			[1.0.0] = system/user/data/test-1360             ;; and its code is not evaluated again
		]

	--test-- "import of an older version of already imported module"
	;@@ https://github.com/Oldes/Rebol3/issues/1361
		write modules-dir/test-1360-old.reb {
Rebol [type: module name: test-1360 version: 0.5.0]
append system/user/data/test-1360 0.5.0
export test-1360-value: 0
export test-1360-old-value: 0
}
		--assert all [
			same? m try [import (modules-dir/test-1360-old.reb)] ;; the newer module is used instead
			same? m select system/modules 'test-1360
			[1.0.0] = system/user/data/test-1360                 ;; the older one is not even made
			1 = lib/test-1360-value
			none? in lib 'test-1360-old-value
		]

	--test-- "import of a newer version of already imported module"
		--assert all [ ;; state before the upgrade
			1 = lib/test-1360-value
			1 = lib/test-1360-gone
			'mine = lib/test-1360-own ;; lib's own value was not replaced by the export
			none? in lib 'test-1360-added
		]
		write modules-dir/test-1360-new.reb {
Rebol [type: module name: test-1360 version: 2.0.0]
append system/user/data/test-1360 2.0.0
export test-1360-value: 2
export test-1360-own: 2
export test-1360-added: 2
}
		--assert all [
			module? o: try [import (modules-dir/test-1360-new.reb)]
			not same? m o
			same? o select system/modules 'test-1360
			2.0.0 = select spec-of o 'version
			[1.0.0 2.0.0] = system/user/data/test-1360
		]
		--assert all [ ;; exports of the older version are replaced in lib
			2 = lib/test-1360-value
			2 = lib/test-1360-added   ;; new export is added
			'mine = lib/test-1360-own ;; lib's own value is still not touched
			1 = lib/test-1360-gone    ;; value, which is not exported anymore, stays
		]
		--assert all [ ;; importing the newer version again changes nothing
			same? o try [import (modules-dir/test-1360-new.reb)]
			[1.0.0 2.0.0] = system/user/data/test-1360
			2 = lib/test-1360-value
		]
		delete modules-dir/test-1360.reb
		delete modules-dir/test-1360-old.reb
		delete modules-dir/test-1360-new.reb

	--test-- "import of an older module! value"
	;@@ https://github.com/Oldes/Rebol3/issues/1358
		m: module [name: test-1358 version: 1.0.0][export test-1358-value: 1]
		o: module [name: test-1358 version: 0.5.0][export test-1358-value: 0]
		--assert same? m try [import (m)]
		--assert all [ ;; the given value cannot be silently replaced by the newer one
			error? e: try [import (o)]
			e/id = 'newer-module
			e/arg1 = 'test-1358
			e/arg2 = 1.0.0
		]
		--assert all [
			same? m select system/modules 'test-1358
			1 = lib/test-1358-value
		]
		--assert all [ ;; but it still may be imported without touching the lib
			same? o try [import/no-lib (o)]
			same? m select system/modules 'test-1358
			1 = lib/test-1358-value
		]
		;; when a module value of the same version is imported, the existing one is used
		o: module [name: test-1358 version: 1.0.0][export test-1358-value: 3]
		--assert same? m try [import (o)]
		;; an older module is recognized also when it has the same content as the newer one
		o: module [name: test-1358 version: 0.5.0][export test-1358-value: 1]
		--assert all [
			equal? m o
			error? e: try [import (o)]
			e/id = 'newer-module
		]
		--assert same? o try [import/no-lib (o)]

	--test-- "import/version of already imported module"
		--assert module? try [import/version 'test-1360 2.0.0]
		--assert all [
			error? e: try [import/version 'test-1360 2.0.1]
			e/id = 'needs
		]

	--test-- "import/check of already imported module"
	;@@ https://github.com/Oldes/Rebol3/issues/1359
		s: save/header none [export test-1359-value: 1] [type: module name: test-1359 checksum: true]
		c: select first load/header s 'checksum
		write modules-dir/test-1359.reb s
		--assert module? m: try [import/check 'test-1359 c]
		--assert same? m try [import/check 'test-1359 c] ;; the saved checksum is used
		--assert same? m try [import 'test-1359]
		--assert all [
			error? e: try [import/check 'test-1359 #{00}]
			e/id = 'invalid-check
		]
		--assert all [ ;; a module without a checksum can never pass the check
			error? e: try [import/check 'test-1360 c]
			e/id = 'invalid-check
		]
		--assert same? m try [import/check (m) c]       ;; also when used as a value
		delete modules-dir/test-1359.reb
		;; delayed module
		s: save/header none [export test-1359-delayed-value: 1] [type: module name: test-1359-delayed checksum: true]
		c: select first load/header s 'checksum
		--assert [test-1359-delayed #(none)] = sys/load-module/delay s
		--assert all [
			error? e: try [import/check 'test-1359-delayed #{00}]
			e/id = 'invalid-check
		]
		--assert module? try [import/check 'test-1359-delayed c]

	--test-- "import of an unnamed module"
	;@@ https://github.com/Oldes/Rebol3/issues/1363
		system/user/data/test-1363: copy []
		write modules-dir/test-1363.reb {Rebol [type: module] append system/user/data/test-1363 'init export test-1363-value: 1}
		--assert all [
			module? m: try [import (modules-dir/test-1363.reb)]
			none? select spec-of m 'name
			none? in lib 'test-1363-value                 ;; its exports are not in lib,
			1 = system/contexts/user/test-1363-value      ;; but in the context, which requested it
		]
		--assert all [ ;; it is registered using its location, so it is not made again
			same? m select system/modules to-real-file modules-dir/test-1363.reb
			same? m try [import (modules-dir/test-1363.reb)]
			[init] = system/user/data/test-1363
		]
		--assert all [ ;; DO is the way, how to evaluate it again
			1 = try [do modules-dir/test-1363.reb]
			[init init] = system/user/data/test-1363
			same? m select system/modules to-real-file modules-dir/test-1363.reb
		]
		delete modules-dir/test-1363.reb

	--test-- "system/modules"
		--assert map? system/modules
		;; imported module is registered using its name and also its location
		system/user/data/test-modules: copy []
		write modules-dir/test-modules.reb {Rebol [type: module name: test-modules version: 1.0.0] append system/user/data/test-modules 'init}
		--assert all [
			module? m: try [import 'test-modules]
			same? m select system/modules 'test-modules
			same? m select system/modules to-real-file modules-dir/test-modules.reb
			same? m system/modules/test-modules
			same? m try [import (modules-dir/test-modules.reb)]
			[init] = system/user/data/test-modules
		]
		;; /version and /check are still validated, when the module is found using its location
		--assert all [
			error? e: try [import/version (modules-dir/test-modules.reb) 2.0.0]
			e/id = 'needs
		]
		--assert all [
			error? e: try [import/check (modules-dir/test-modules.reb) #{00}]
			e/id = 'invalid-check
		]
		;; when the module is replaced by its newer version, the old location is not used anymore
		write modules-dir/test-modules-new.reb {Rebol [type: module name: test-modules version: 2.0.0] append system/user/data/test-modules 'new}
		--assert all [
			module? o: try [import (modules-dir/test-modules-new.reb)]
			not same? m o
			same? o select system/modules 'test-modules
			same? o try [import (modules-dir/test-modules.reb)] ;; the newer one is used instead
			[init new] = system/user/data/test-modules
		]
		;; module imported using /no-lib is not registered using its name, but it is not made again
		system/user/data/test-modules: copy []
		write modules-dir/test-modules-no-lib.reb {Rebol [type: module name: test-modules-no-lib] append system/user/data/test-modules 'init}
		--assert all [
			module? m: try [import/no-lib (modules-dir/test-modules-no-lib.reb)]
			none? select system/modules 'test-modules-no-lib
			same? m try [import/no-lib (modules-dir/test-modules-no-lib.reb)]
			same? m try [import (modules-dir/test-modules-no-lib.reb)]
			same? m select system/modules 'test-modules-no-lib ;; now it is registered also by its name
			[init] = system/user/data/test-modules
		]
		delete modules-dir/test-modules.reb
		delete modules-dir/test-modules-new.reb
		delete modules-dir/test-modules-no-lib.reb

===end-group===

===start-group=== "embedded extensions"
	;@@ https://github.com/Oldes/Rebol3/issues/1901
	;- these tests need a build, which includes the embedded extensions (like Rebol/Bulk)

	if block? select system/modules 'xtest [
		--test-- "delayed embedded extension"
			;; it is registered, but not made, until it is imported
			--assert all [
				object? first select system/modules 'xtest ;; its header
				find select first select system/modules 'xtest 'options 'delay
				none? in lib 'xtest
			]
			--assert all [
				module? m: try [import 'xtest]
				same? m select system/modules 'xtest
				same? m try [import 'xtest]
				did find query m 'options 'extension
			]
			--assert all [ ;; its exports are in lib now
				any-function? get in m 'xtest
				same? get in m 'xtest get in lib 'xtest
			]
			--assert all [ ;; internal values are protected
				protected? in m 'lib-base
				none? in m 'lib-boot
			]
	]

	if module? m: select system/modules 'easing [
		--test-- "embedded extension imported on boot"
			options: query m 'options
			--assert all [
				find options 'extension
				not find options 'delay
				same? m try [import 'easing]
			]
			--assert all [ ;; its exports are in lib (or only in the user context, when private)
				[tween] = select spec-of m 'exports
				any-function? get in m 'tween
				same? get in m 'tween get in either find options 'private [
					system/contexts/user
				][	lib ] 'tween
			]
			--assert all [ ;; internal values are protected also when loaded on boot
				protected? in m 'lib-base
				none? in m 'lib-boot
			]
	]

	;; any private extension loaded on boot must export only to the user context
	foreach [name m] system/modules [
		if all [
			module? :m
			options: query m 'options
			find options 'extension
			find options 'private
			block? exports: query m 'exports
		][
			--test-- join "private embedded extension " name
			foreach word exports [
				--assert all [
					not same? get/any in m word get/any in lib word ;; not in lib
					same? get/any in m word get/any in system/contexts/user word
				]
			]
		]
	]

===end-group===

===start-group=== "unload"
	--test-- "unload module"
		system/user/data/test-unload: copy []
		write modules-dir/test-unload.reb {
Rebol [type: module name: test-unload]
append system/user/data/test-unload 'init
export test-unload-a: 1
export test-unload-b: 2
}
		--assert all [
			module? m: try [import 'test-unload]
			1 = lib/test-unload-a
			1 = system/contexts/user/test-unload-a
			2 = system/contexts/user/test-unload-b
		]
		lib/test-unload-b: 'changed ;; lib's value is not the module's value anymore
		--assert same? m try [unload 'test-unload]
		--assert all [ ;; it is not registered anymore (using its name and its location)
			none? select system/modules 'test-unload
			none? select system/modules to-real-file modules-dir/test-unload.reb
		]
		--assert all [ ;; exported values are unset in lib and in the user's context...
			unset? get/any in lib 'test-unload-a
			unset? get/any in system/contexts/user 'test-unload-a
			unset? get/any in system/contexts/user 'test-unload-b
			'changed = lib/test-unload-b ;; ...unless there is already a different value
		]
		--assert 1 = m/test-unload-a ;; the module itself is still usable, when referenced
		unset in lib 'test-unload-b
		;; the module may be imported again
		--assert all [
			module? o: try [import 'test-unload]
			not same? m o
			[init init] = system/user/data/test-unload
			1 = lib/test-unload-a
			2 = lib/test-unload-b
		]
		;; a protected module is not unloaded
		protect o
		--assert all [
			error? e: try [unload 'test-unload]
			e/id = 'protected
			same? o select system/modules 'test-unload
			1 = lib/test-unload-a
		]
		unprotect o
		;; unload using the module value
		--assert same? o try [unload (o)]
		--assert none? select system/modules 'test-unload
		;; unload of something what is not loaded
		--assert none? try [unload 'test-unload]
		--assert none? try [unload 'test-unload-unknown]
		;; a module value is accepted even when it is not registered (anymore)
		--assert same? o try [unload (o)]
		;; so also exports of a module, which was never registered, may be removed
		o: module [][export test-unload-d: 4]
		import (o)
		--assert 4 = system/contexts/user/test-unload-d
		--assert same? o try [unload (o)]
		--assert unset? get/any in system/contexts/user 'test-unload-d
		delete modules-dir/test-unload.reb

	--test-- "unload unnamed module"
		write modules-dir/test-unload.reb {Rebol [type: module] export test-unload-c: 3}
		--assert all [
			module? m: try [import (modules-dir/test-unload.reb)]
			3 = system/contexts/user/test-unload-c
			same? m select system/modules to-real-file modules-dir/test-unload.reb
		]
		--assert all [ ;; using its location
			same? m try [unload (modules-dir/test-unload.reb)]
			none? select system/modules to-real-file modules-dir/test-unload.reb
			unset? get/any in system/contexts/user 'test-unload-c
		]
		delete modules-dir/test-unload.reb

===end-group===

===start-group=== "module issues"
	--test-- "issue-1005"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1005
		m: module [] [a: 1 2]
		--assert [a: 1] = body-of m
		--assert [a] = keys-of m
		--assert [1] = values-of m
		m: module [exports: [a]] [a: 1 2]
		--assert [a: 1] = body-of m
		--assert [a] = keys-of m
		--assert [1] = values-of m

	--test-- "mold module!"
	;@@ https://github.com/Oldes/Rebol3/issues/1169
	;@@ https://github.com/Oldes/Rebol3/issues/1183
		;; module is molded as its spec (only used fields) and its body (not hidden words)
		m: module [title: "my-module" name: test-1169 version: 1.2.3][export a: 1 b: 2 hidden c: 3]
		--assert (mold/flat m) = {make module! [[version: 1.2.3 title: "my-module" name: 'test-1169 type: 'module exports: [a]] [a: 1 b: 2]]}
		--assert (mold/all/flat m) = {#(module! [[version: 1.2.3 title: "my-module" name: test-1169 type: module exports: [a]] [a: 1 b: 2]])}
		--assert (mold m) = {make module! [
    [
        version: 1.2.3
        title: "my-module"
        name: 'test-1169
        type: 'module
        exports: [a]
    ]
    [
        a: 1
        b: 2
    ]
]}
		--assert (mold/flat module [][]) = {make module! [[version: 0.0.0 title: "Untitled" type: 'module] []]}
		--assert (mold/flat reduce [m 1]) = ajoin [#"[" mold/flat m " 1]"]
		;; module referencing itself
		m: module [][me: none]
		m/me: m
		--assert (mold/flat m) = {make module! [[version: 0.0.0 title: "Untitled" type: 'module] [me: make module! [[version: 0.0.0 title: "Untitled" type: 'module] [...]]]]}
		;; but there is no construction syntax for it
		--assert all [
			error? e: try [mold/all m]
			e/id = 'recursive-value
			e/arg1 = module!
		]

	--test-- "construct module!"
	;@@ https://github.com/Oldes/Rebol3/issues/1169
		m: module [title: "my-module" name: test-1169 version: 1.2.3 options: [private]][export a: 1 b: 2 hidden c: 3]
		--assert all [
			module? o: try [load mold/all m]
			not same? o m
			(mold/all o) = (mold/all m)
			"my-module" = title-of o
			'test-1169  = select spec-of o 'name
			[private]   = select spec-of o 'options
			[a]         = select spec-of o 'exports
			[a b] = words-of o ;; hidden words are not molded, so these are lost
			1 = o/a
		]
		;; nothing is evaluated, when the module is constructed
		--assert all [
			module? o: try [load "#(module! [[title: reverse {ab}] [a: 1 + 1]])"]
			'reverse = select spec-of o 'title
			1 = o/a
		]
		;; missing type is set
		--assert all [
			module? o: try [load "#(module! [[] []])"]
			'module = select spec-of o 'type
		]
		;; spec and body blocks are required and important fields of the spec are validated
		foreach src [
			"#(module! [])"
			"#(module! [a: 1])"
			"#(module! [[title: {x}]])"
			"#(module! [[] [] []])"
			"#(module! [[name: {x}] []])"
			"#(module! [[type: 1] []])"
			"#(module! [[version: 1] []])"
			"#(module! [[options: private] []])"
		][
			--assert all [
				error? e: try [load src]
				e/id = 'malconstruct
			]
		]
		;; module may be also made from its (not serialized) molded form
		--assert all [
			module? o: try [do mold m]
			not same? o m
			(mold o) = (mold m)
		]

	--test-- "query module"
		m: module [title: "Query test" name: test-query version: 1.2.3 options: [private]][export a: 1 title: "own"]
		--assert "Query test" = query m 'title ;; header's field, even when the module has the same word
		--assert 'test-query  = query m 'name
		--assert 1.2.3        = query m 'version
		--assert [private]    = query m 'options
		--assert [a]          = query m 'exports
		--assert none?          query m 'checksum
		--assert [name: test-query version: 1.2.3] = query m [name version]
		--assert [test-query 1.2.3] = query m [:name :version]
		--assert all [
			block? f: query m none
			did find f 'name
			did find f 'exports
		]
		--assert all [ ;; whole header as a new object
			object? o: query m object!
			not same? o spec-of m
			o/name = 'test-query
			o/exports = [a]
		]
		--assert all [
			error? e: try [query m 'foo]
			e/id = 'cannot-use
		]
		;; module without exports
		m: module [][a: 1]
		--assert none? query m 'exports
		--assert none? query m 'name
		--assert "Untitled" = query m 'title

	--test-- "issue-1764"
	;@@ https://github.com/Oldes/Rebol3/issues/1764
		;; context of a module's word provides the module's header (it is a feature)
		m: module [title: "Test 1764"][a: 1]
		o: context? in m 'a
		--assert same? spec-of o spec-of m
		--assert "Test 1764" = select spec-of o 'title
		--assert none? spec-of make object! [a: 1] ;; ordinary object has no spec

	--test-- "issue-1708"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1708
		--assert none? find system/modules module [] []

===end-group===

~~~end-file~~~

