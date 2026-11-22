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

===start-group=== "PROTECT/words/values"
	--test-- "protect/words/values"
	;@@ https://github.com/Oldes/Rebol3/issues/2474
		pwv-a: [1 2 3]  pwv-b: "abc"
		protect/words/values [pwv-a pwv-b]
		--assert all [error? e: try [pwv-a: 1] e/id = 'locked-word]
		--assert all [error? e: try [pwv-b: 1] e/id = 'locked-word]
		--assert all [error? e: try [append pwv-a 4] e/id = 'protected]
		--assert all [error? e: try [append pwv-b "d"] e/id = 'protected]
		--assert pwv-a = [1 2 3]
		--assert pwv-b = "abc"
	--test-- "unprotect/words/values"
		unprotect/words/values [pwv-a pwv-b]
		--assert not error? try [append pwv-a 4]
		--assert not error? try [append pwv-b "d"]
		--assert not error? try [pwv-a: 1 pwv-b: 2]
	--test-- "protect/words/values with a path"
		pwv-o: object [blk: [1]]
		protect/words/values [pwv-o/blk]
		--assert all [error? e: try [pwv-o/blk: 2] e/id = 'locked-word]
		--assert all [error? e: try [append pwv-o/blk 2] e/id = 'protected]
		unprotect/words/values [pwv-o/blk]
		--assert not error? try [append pwv-o/blk 2]
===end-group===

===start-group=== "BIND/new and BIND/set with protected or hidden words"
	--test-- "bind/new and bind/set to protected object"
	;@@ https://github.com/Oldes/Rebol3/issues/1957
		po: context [x: 1 protect self]
		--assert all [error? e: try [bind/set [a:] po] e/id = 'protected]
		--assert all [error? e: try [bind/new [b] po] e/id = 'protected]
		--assert all [error? e: try [bind/new 'c po] e/id = 'protected]
		--assert all [error? e: try [bind/new/only [d [e]] po] e/id = 'protected]
		--assert [x] = words-of po
		;; binding to existing words is still allowed:
		--assert block? bind/new [x] po
		--assert block? bind/set [x:] po
		--assert word? bind/new 'x po
	--test-- "bind/new and bind/set to object with hidden word"
	;@@ https://github.com/Oldes/Rebol3/issues/1374
		po: object [a: 1 b: 2 protect/hide 'b]
		--assert 2 = length? po
		blk: bind/new [b: 0] po
		--assert not same? po context? first blk
		--assert 2 = length? po
		blk: bind/set [b: 0] po
		--assert not same? po context? first blk
		--assert 2 = length? po
		--assert all [error? e: try [bind/new 'b po] e/id = 'hidden]
		--assert 2 = length? po
		--assert [a] = words-of po
		;; other words are still added:
		bind/new [c] po
		--assert [a c] = words-of po
===end-group===

===start-group=== "Setting a path in a protected variable"
	;@@ https://github.com/Oldes/Rebol3/issues/2562
	--test-- "protected date"
		pd-d: 1-Jan-2020
		protect 'pd-d
		--assert all [error? e: try [pd-d/year: 2200] e/id = 'locked-word]
		--assert all [error? e: try [pd-d/time: 10:00] e/id = 'locked-word]
		--assert all [error? e: try [set 'pd-d/month 2] e/id = 'locked-word]
		--assert pd-d = 1-Jan-2020
		--assert pd-d/year = 2020 ;; reading is fine
		unprotect 'pd-d
		--assert not error? try [pd-d/year: 2021]
		--assert pd-d = 1-Jan-2021
	--test-- "protected time, pair and tuple"
		pd-t: 1:2:3  pd-p: 1x2  pd-c: 1.2.3
		protect/words [pd-t pd-p pd-c]
		--assert all [error? e: try [pd-t/hour: 5] e/id = 'locked-word]
		--assert all [error? e: try [pd-p/x: 5] e/id = 'locked-word]
		--assert all [error? e: try [pd-c/1: 5] e/id = 'locked-word]
		--assert all [pd-t = 1:2:3 pd-p = 1x2 pd-c = 1.2.3]
		unprotect/words [pd-t pd-p pd-c]
	--test-- "protected object field"
		pd-o: object [d: 1-Jan-2020 sub: object [d: 1-Jan-2020]]
		protect in pd-o 'd
		--assert all [error? e: try [pd-o/d/year: 2200] e/id = 'locked-word]
		--assert pd-o/d = 1-Jan-2020
		protect in pd-o 'sub
		;; the object in a protected field is not protected:
		--assert not error? try [pd-o/sub/d/year: 2021]
		--assert pd-o/sub/d = 1-Jan-2021
		protect in pd-o/sub 'd
		--assert all [error? e: try [pd-o/sub/d/year: 2200] e/id = 'locked-word]
	--test-- "date in protected block"
		pd-b: protect [1-Jan-2020]
		--assert all [error? e: try [pd-b/1/year: 2200] e/id = 'protected]
		--assert pd-b/1 = 1-Jan-2020
===end-group===

===start-group=== "Protecting function locals"
	;@@ https://github.com/Oldes/Rebol3/issues/2564
	--test-- "protect function local"
		pf: func [d][protect 'd d: 0]
		--assert all [error? e: try [pf 1] e/id = 'protect-local]
		pf: func [d][protect/words [d] d: 0]
		--assert all [error? e: try [pf 1] e/id = 'protect-local]
		pf: func [d][protect/hide 'd d: 0]
		--assert all [error? e: try [pf 1] e/id = 'protect-local]
		pf: func [d][unprotect 'd d: 0]
		--assert all [error? e: try [pf 1] e/id = 'protect-local]
	--test-- "protect value of function local"
		pf: func [d][protect/values [d] d]
		pv: pf copy [1]
		--assert all [error? e: try [append pv 2] e/id = 'protected]
	--test-- "protect closure local"
		;; closure has its own frame for each call, so it is allowed
		pc: closure [d][protect 'd try [d: 0]]
		--assert error? pc 1
		--assert error? pc 2
===end-group===

===start-group=== "PROTECT/lock of words"
	;@@ https://github.com/Oldes/Rebol3/issues/1326
	--test-- "protect/lock word in object"
		pl-o: object [a: 1 b: 2 protect/lock 'a]
		--assert all [error? e: try [pl-o/a: 2] e/id = 'locked-word]
		unprotect 'pl-o/a  ;; silently ignored
		--assert all [error? e: try [pl-o/a: 3] e/id = 'locked-word]
		unprotect in pl-o 'a
		--assert all [error? e: try [pl-o/a: 3] e/id = 'locked-word]
		--assert protected? 'pl-o/a
		--assert pl-o/a = 1
		;; other words are not affected:
		--assert not error? try [pl-o/b: 3]
	--test-- "protect/lock whole object"
		pl-o: object [a: 1]
		protect/lock pl-o
		unprotect pl-o
		--assert all [error? e: try [pl-o/a: 2] e/id = 'locked-word]
		--assert all [error? e: try [append pl-o [b: 2]] e/id = 'protected]
	--test-- "protect/lock word"
		pl-w: 1
		protect/lock 'pl-w
		unprotect 'pl-w
		--assert all [error? e: try [pl-w: 2] e/id = 'locked-word]
		--assert pl-w = 1
	--test-- "permanent protection is inherited"
		pl-o: object [a: 1 protect/lock 'a]
		pl-o2: make pl-o []
		unprotect in pl-o2 'a
		--assert all [error? e: try [pl-o2/a: 2] e/id = 'locked-word]
	--test-- "normal protection may be unprotected"
		pl-o: object [a: 1 protect 'a]
		unprotect in pl-o 'a
		--assert not error? try [pl-o/a: 2]
===end-group===

===start-group=== "PROTECTED? and PROTECT with a path"
;@@ https://github.com/Oldes/Rebol3/issues/2562
	--test-- "protected? path into a protected scalar"
		pp-o: object [t: 1 protect/words o: object [t: now/time]]
		--assert protected? 'pp-o/o/t
		--assert protected? 'pp-o/o/t/hour
		--assert error? e: try [pp-o/o/t/hour: 0]
		--assert not protected? 'pp-o/t
		pp-d: 1-Jan-2020
		protect 'pp-d
		--assert protected? 'pp-d/year
		unprotect 'pp-d
		--assert not protected? 'pp-d/year
	--test-- "protect path into a scalar"
		pp-o: object [d: 1-Jan-2020]
		protect 'pp-o/d/year  ;; protects the variable holding the date
		--assert protected? 'pp-o/d
		--assert all [error? e: try [pp-o/d/year: 2000] e/id = 'locked-word]
		unprotect 'pp-o/d/year
		--assert not protected? 'pp-o/d
	--test-- "protect path not leading to a variable"
		;; was protecting pp-o/x, because the block was skipped
		pp-o: object [b: [1] x: 1]
		protect 'pp-o/b/x
		--assert not protected? 'pp-o/x
		--assert not protected? 'pp-o/b/x
		--assert not error? try [pp-o/x: 2]
		;; not existing word was resolving to SELF
		protect 'pp-o/foo
		--assert not protected? 'pp-o/foo
		--assert not error? try [pp-o/x: 3]
===end-group===

~~~end-file~~~
