Rebol [
	Title:   "Rebol protect test script"
	Author:  "Oldes"
	File: 	 %protect-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "Protect"

===start-group=== "Checks if protected data are really protected"
	bin: #{cafe}
	str:  "cafe"
	blk: ["cafe"]
	protect/values [bin str blk]

	is-protected-error?: function[code][
		true? all [
			error? err: try code
			err/id = 'protected
		]
	]
	is-locked-error?: function[code][
		true? all [
			error? err: try code
			err/id = 'locked-word
		]
	]
	is-invalid-path-error?: function[code][
		true? all [
			error? err: try code
			err/id = 'invalid-path
		]
	]

	--test-- "clear"   --assert is-protected-error? [clear bin]
	--test-- "append"  --assert is-protected-error? [append bin #{0bad}]
	--test-- "insert"  --assert is-protected-error? [insert bin #{0bad}]

	;@@ https://github.com/Oldes/Rebol-issues/issues/2321
	--test-- "encloak" --assert is-protected-error? [encloak bin "key"]
	--test-- "decloak" --assert is-protected-error? [decloak bin "key"]
	;@@ https://github.com/Oldes/Rebol-issues/issues/1780
	;@@ https://github.com/Oldes/Rebol-issues/issues/2272
	--test-- "remove-each" --assert is-protected-error? [remove-each a bin [a < 3]]
	;@@ https://github.com/Oldes/Rebol-issues/issues/1780
	--test-- "random block"  --assert is-protected-error? [random blk]
	--test-- "random string" --assert is-protected-error? [random str]
	--test-- "random binary" --assert is-protected-error? [random bin]

	;@@ https://github.com/Oldes/Rebol-issues/issues/695
	--test-- "swap block" 
		--assert is-protected-error? [swap blk [0]]
		--assert is-protected-error? [swap [0] blk]
	--test-- "swap string"
		--assert is-protected-error? [swap str "0bad"]
		--assert is-protected-error? [swap "0bad" str]
	--test-- "swap binary"
		--assert is-protected-error? [swap bin #{0bad}]
		--assert is-protected-error? [swap #{0bad} bin]

	;@@ https://github.com/Oldes/Rebol-issues/issues/2325
	str: protect "a^M^/b"
	--test-- "deline string"  --assert is-protected-error? [deline str]
	--test-- "enline string"  --assert is-protected-error? [enline str]
	;@@ https://github.com/Oldes/Rebol-issues/issues/694
	--test-- "uppercase string"  --assert is-protected-error? [uppercase str]
	--test-- "lowercase string"  --assert is-protected-error? [lowercase str]

	--test-- "delect"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1783
	if any-function? :delect [
		dialect: context [default: [tuple!]]
		inp: [1.2.3]
		out: make block! 4
		protect out
		--assert is-protected-error? [delect dialect inp out]
	]

	--test-- "protect bitset"
	;@@ https://github.com/Oldes/Rebol-issues/issues/977
		ws: protect charset "^- "
		--assert is-protected-error? [clear ws  ]
		--assert is-protected-error? [ws/1: true]

	--test-- "protect inside an object"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1193
		o: make object! [a: 1 f: does [a: 2] g: does [self/a: 3]]
		protect in o 'a
		--assert is-locked-error? [o/a: 4]
		--assert is-locked-error? [o/f]
		--assert is-locked-error? [o/g]
	;@@ https://github.com/Oldes/Rebol-issues/issues/1323
		a: object [b: "abc"]
		protect/deep 'a
		--assert is-protected-error? [insert a/b "x"]
		unprotect 'a
		a: object [b: "abc"]
		protect/deep/words 'a
		--assert is-protected-error? [insert a/b "x"]
		unprotect 'a

	--test-- "protect object inside an object"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1324
		o: make object! [b: context [c: "d"]]
		--assert not error? try [protect/deep 'o/b]
		--assert not error? try [protect/deep/words [o/b]]
		--assert is-locked-error? [o/b/c: 4]
		--assert is-protected-error? [clear o/b/c]

	--test-- "protect/hide inside an object"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1139
		o: make object! [f: 1 g: self h: does [f] protect/hide 'f]
		--assert is-invalid-path-error? [do in o [self/f]]
		--assert is-invalid-path-error? [do in o [self/f: 2]]
		--assert is-invalid-path-error? [do bind [self/f] o]

	--test-- "resolve"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1095
		obj: make object! [a: 1]
		protect/deep obj
		--assert is-protected-error? [resolve obj make object! [a: 99]]
		a: make object! [pass: "1234" getp: does [return pass] protect/hide 'pass]
		--assert is-invalid-path-error? [a/pass]
		--assert object? resolve a make object! [pass: 999]
		--assert "1234" = a/getp
	--test-- "SET object!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1090
		obj: make object! [a: 1 b: 2]
		protect/deep obj
		--assert all [error? e: try [obj/a: 99] e/id = 'locked-word]
		--assert all [error? e: try [set in obj 'a 99] e/id = 'locked-word]
		--assert all [error? e: try [set obj [99]] e/id = 'locked-word]

	--test-- "EXTEND object!"
		obj: object [a: 1] protect/deep 'obj
		--assert all [error? e: try [obj/a: 2]  e/id = 'locked-word] 
		--assert all [error? e: try [put obj 'a 2]  e/id = 'protected] ; cannot modify the value
		--assert all [error? e: try [append obj [b: 2]]  e/id = 'protected]
		--assert all [error? e: try [put obj 'b 2]  e/id = 'protected]
		unprotect obj
		--assert object? append obj [b: 2]
		--assert all [22 = put obj 'a 22  22 == obj/a]
		unprotect 'obj

	--test-- "protect recursion"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1764
		--assert is-locked-error? [new-block: reduce [1 2 protect/deep 'new-block 3 4]]

	--test-- "protect paths"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1747
		o: context [a: "aha"] p: 'o/a
		;; by default it protects the value it points to
		--assert all [
			'o/a = protect p
			is-locked-error? [o/a: none]
		]
		;; to protect the path value...
		o: context [a: "aha"] p: 'o/a
		--assert all [
			'o/a = protect/values p
			none? try [o/a: none]
			is-protected-error? [append p 'x]
		]
		p: quote :o/a
		--assert all [
			(quote :o/a) = protect/values p
			none? try [o/a: none]
			is-protected-error? [append p 'x]
		]

	--test-- "protect/deep of a variable not holding a series"
		;; it was crashing when the value was for example a word! or a datatype!
		o: context [a: 'word b: integer! c: :print d: 1 e: none f: #"a" g: [word] h: "str"]
		--assert not error? try [foreach w words-of o [protect/deep in o w]]
		--assert is-locked-error? [o/a: 1]
		--assert is-locked-error? [o/b: 1]
		--assert is-protected-error? [append o/g 1]
		--assert not error? try [foreach w words-of o [unprotect/deep in o w]]
		--assert all [
			1 = try [o/a: 1]
			[word 1] = try [append o/g 1]
		]
		;; using paths
		o: context [a: 'word b: integer! c: :print d: 1 e: none f: #"a" g: [word] h: "str"]
		--assert not error? try [protect/deep 'o/a protect/deep 'o/b protect/deep 'o/d protect/deep 'o/g]
		--assert is-locked-error? [o/b: 1]
		--assert is-protected-error? [append o/g 1]
		;; using lists
		o: context [a: 'word b: integer! c: :print d: 1 e: none f: #"a" g: [word] h: "str"]
		--assert not error? try [protect/deep/words   [o/a o/b o/c o/d o/e o/f o/g o/h]]
		--assert is-locked-error? [o/a: 1]
		--assert not error? try [unprotect/deep/words [o/a o/b o/c o/d o/e o/f o/g o/h]]
		--assert not error? try [protect/deep/values  [o/a o/b o/c o/d o/e o/f o/g o/h]]
		--assert is-protected-error? [append o/g 1]


===end-group===


===start-group=== "protect/hide"
	--test-- "trim object! with hidden fields"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1137
		--assert 0 = length? trim make object! [f: 1 protect/hide 'f]
		--assert 0 = length? trim make object! [f: none protect/hide 'f]
	--test-- "resolve/all hidden"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1769
		con1: context [a: 1 b: 2 c: "hidden" protect/hide 'c]
		con2: context [a: 9 b: 8 c: none]
		--assert object? resolve/all con2 con1
		--assert all [con2/a = 1 con2/b = 2]
		--assert none? con2/c ;- not holding "hidden"!
	--test-- "protect/hide on series" ;not allowed
	;@@ https://github.com/Oldes/Rebol-issues/issues/697
		a: "123"
		--assert all [error? e: try [protect/hide :a] e/id = 'bad-refines]

	--test-- "compare objects with hidden values"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1778
		--assert equal? context [a: 1 protect/hide 'a] context [a: 2 protect/hide 'a]
		--assert equal? context [a: 1 protect/hide 'a] context [a: 1 protect/hide 'a]
		--assert equal? context [a: 1 protect/hide 'a] context [b: 1 protect/hide 'b]
		--assert not equal? context [a: 1 protect/hide 'a] context [a: 1]
		--assert not equal? context [a: 1] context [a: 1 protect/hide 'a]
		--assert not equal? context [a: 1 protect/hide 'a] context []

===end-group===


===start-group=== "PUT on object with protected/hidden words"
	--test-- "put on protected word"
		o: make object! [a: 1]
		protect in o 'a
		--assert all [
			error? e: try [put o 'a 2]
			e/id = 'locked-word
			e/arg1 = 'a
		]
		--assert o/a = 1
	--test-- "put on hidden word"
		o: make object! [a: 1 h: 2]
		protect/hide in o 'h
		--assert all [
			error? e: try [put o 'h 3]
			e/id = 'hidden
		]
		--assert 1 = length? words-of o
===end-group===


===start-group=== "Derived objects (shared word list)"
	;@@ https://github.com/Oldes/Rebol3/issues/1333
	--test-- "protect in derived object (make o [])"
		po: make object! [a: 1 b: 2]
		po1: make po []
		protect in po1 'b
		--assert all [error? e: try [po1/b: 3] e/id = 'locked-word]
		--assert not protected? in po 'b
		--assert not error? try [po/b: 3]
		--assert po/b = 3
	--test-- "protect in derived object (make o [a: ...])"
		po: make object! [a: 1 b: 2]
		po1: make po [a: 10]  ;; no new words
		protect in po1 'a
		--assert all [error? e: try [po1/a: 3] e/id = 'locked-word]
		--assert not error? try [po/a: 3]
	--test-- "protect in copied object"
		po: make object! [a: 1 b: 2]
		po1: copy po
		protect in po1 'b
		--assert all [error? e: try [po1/b: 3] e/id = 'locked-word]
		--assert not error? try [po/b: 3]
	--test-- "protect in prototype"
		po: make object! [a: 1 b: 2]
		po1: make po []
		protect in po 'a
		--assert all [error? e: try [po/a: 3] e/id = 'locked-word]
		--assert not error? try [po1/a: 3]
	--test-- "protect whole derived object"
		po: make object! [a: 1 b: 2]
		po1: make po []
		protect po1
		--assert error? try [po1/a: 3]
		--assert not error? try [po/a: 3]
		unprotect po1
	--test-- "unprotect in derived object"
		po: make object! [a: 1 b: 2]
		protect in po 'a
		po1: make po []      ;; protection is inherited
		--assert error? try [po1/a: 3]
		unprotect in po1 'a
		--assert not error? try [po1/a: 3]
		--assert error? try [po/a: 3]
	--test-- "protect/hide in derived object"
		po: make object! [a: 1 b: 2]
		po1: make po []
		protect/hide in po1 'b
		--assert [a] = words-of po1
		--assert [a b] = words-of po
		--assert po/b = 2
	--test-- "bind/new to derived object"
		po: make object! [a: 1]
		po1: make po []
		bind/new [c: 3] po1
		--assert [a c] = words-of po1
		--assert [a] = words-of po
		--assert none? in po 'c
	--test-- "bind/set to derived object"
		po: make object! [a: 1]
		po1: make po []
		set bind/set [d:] po1 [4]
		--assert po1/d = 4
		--assert [a] = words-of po
		--assert none? in po 'd
	--test-- "protect in closure frame"
		;; each closure call has its own frame, but all used the function's words
		pc: closure [x /local r][r: try [x: 0] protect 'x r]
		--assert not error? pc 1
		--assert not error? pc 2
===end-group===

~~~end-file~~~
