Rebol [
	Title:   "Rebol3 stack test script"
	Author:  "Oldes"
	File: 	 %stack-test.r3
	Needs:   [%../quick-test-module.r3]
]

;- Helpers for stack expansion tests
expand-stack: does [
	reduce append/dup make block! 0 1 100 + stack/limit 0
	loop 200 [make binary! 300000 make string! 100000]
	recycle
]
armed: true
expand-once: does [if armed [armed: false expand-stack]]


~~~start-file~~~ "STACK"

===start-group=== "STACK native"
	--test-- "issue-1623"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1623
		--assert block? b: stack 0
		--assert 'stack = first b
		--assert 'stack = stack/word 0
===end-group===

===start-group=== "STACK expansion"
	--test-- "recycle/stack shrinks the expanded stack"
		--assert all [recycle/stack 8064 > stack/limit 0]

	;@@ Stack expansion tests:
	;@@ Code evaluated once uses `expand-stack`,
	;@@ code evaluated repeatedly (loops, comparators, parse rules) uses `expand-once`.
	;@@ Each test ends with `recycle/stack`, so the next one starts with the smallest stack.

	;-- control and evaluation natives ------------------------------------------
	--test-- "try with data stack expansion"
		--assert 1 = try [expand-stack 1]
		recycle/stack

	--test-- "attempt with data stack expansion"
		--assert none? attempt [expand-stack 1 / 0]
		recycle/stack

	--test-- "assert failure after stack expansion"
		--assert error? try [assert [1 = 1 (expand-stack 1) = 2]]
		recycle/stack

	--test-- "assert success after stack expansion"
		--assert assert [1 = 1 (expand-stack true)]
		recycle/stack

	--test-- "catch with data stack expansion"
		recycle/stack --assert 1 = catch [expand-stack throw 1]
		recycle/stack --assert 2 = catch/name [expand-stack throw/name 2 'foo] 'foo
		recycle/stack --assert 3 = catch/name [expand-stack throw/name 3 'bar] [foo bar]
		recycle/stack --assert 4 = catch/all [expand-stack throw 4]
		recycle/stack --assert 5 = catch/quit [expand-stack 5]
		

	--test-- "do with data stack expansion"
		blk: [(expand-stack 1) 2 3]
		--assert 1 = do/next blk 'pos
		--assert pos = [2 3]
		obj: object [a: 1]
		--assert 1 = do first [obj/(expand-stack 'a)]
		recycle/stack

	--test-- "control with stack expansion"
		--assert 1 = case [(expand-stack true) [1]]
		--assert 2 = switch (expand-stack 1) [1 [2]]
		--assert 3 = all [expand-stack 1 3]
		--assert 1 = any [(expand-stack 1) 3]
		recycle/stack

	--test-- "apply with stack expansion in args"
		--assert 4 = apply :add [(expand-stack 1) 3]
		--assert 4 = apply :add [1 (expand-stack 3)]
		--assert 3 = apply :do [:add 1 (expand-stack 2)]
		recycle/stack

	--test-- "operator with arg expanding stack"
		--assert 3 = (1 + (expand-stack 2))
		recycle/stack

	--test-- "function args expanding stack"
		--assert 3 = add 1 (expand-stack 2)
		recycle/stack

	--test-- "path function with refinement and arg expanding stack"
		--assert [[1]] = append/only copy [] (expand-stack [1])
		recycle/stack

	;-- paths -----------------------------------------------------------------
	--test-- "nested path with paren that expands stack"
		obj: object [sub: object [a: 1]]
		--assert 1 = do first [obj/sub/(expand-stack 'a)]
		--assert 1 = do first [obj/(expand-stack 'sub)/(expand-stack 'a)]
		recycle/stack

	--test-- "set-path with paren that expands stack"
		obj: object [a: 1]
		--assert 2 = first reduce [obj/(expand-stack 'a): 2]
		--assert 2 = obj/a
		recycle/stack

	--test-- "set-path value expanding stack"
		obj: object [a: 1]
		obj/a: (expand-stack 2)
		--assert obj/a = 2
		recycle/stack

	--test-- "set-path on block with value expanding stack"
		blk: [1 2 3]
		blk/2: (expand-stack 9)
		--assert blk = [1 9 3]
		recycle/stack

	--test-- "set-path with paren selector expanding stack"
		obj: object [a: 1]
		obj/(expand-stack 'a): 5
		--assert obj/a = 5
		recycle/stack

	--test-- "get path with paren expanding stack"
		obj: object [a: 1]
		--assert 1 = get 'obj/(expand-stack 'a)
		recycle/stack

	--test-- "get path with paren expanding stack, unset value"
		obj: object [a: 1]
		unset in obj 'a
		--assert error? try [get 'obj/(expand-stack 'a)]
		--assert unset? get/any 'obj/(expand-stack 'a)
		recycle/stack

	--test-- "in block with path expanding stack"
		o2: object [o: object [a: 1]]
		--assert 1 = get in [o2/(expand-stack 'o)] 'a
		recycle/stack

	;-- objects ---------------------------------------------------------------
	--test-- "make object! with stack expansion in spec"
		o: make object! [a: 1 b: (expand-stack 2) c: a + b]
		--assert 3 = o/c
		p: make o [d: (expand-stack 4) e: c + d]
		--assert 7 = p/e
		--assert 2 = catch/all [make object! [a: 1 expand-stack throw 2]]
		recycle/stack

	--test-- "make parent object with stack expansion in spec"
		o: make object! [a: 1 b: 2]
		p: make o [c: (expand-stack 3) d: a + b + c]
		--assert 3 = p/c
		--assert 6 = p/d
		--assert 1 = p/a   ; inherited fields survive
		--assert 1 = o/a   ; the parent is unchanged
		--assert not in o 'c
		recycle/stack

	--test-- "construct with many cascaded set-words"
		blk: append/dup make block! 0 [a:] 15000
		append blk 1
		o: construct blk
		--assert o/a = 1
		recycle/stack

	;-- reduce and compose ----------------------------------------------------
	--test-- "reduce/into with stack expansion"
		out: make block! 0
		reduce/into [1 (expand-stack 2) 3] out
		--assert [1 2 3] = out
		recycle/stack

	--test-- "compose/into with stack expansion"
		out: make block! 0
		compose/into [1 (expand-stack 2) 3] out
		--assert [1 2 3] = out
		recycle/stack

	--test-- "compose map with stack expansion"
		--assert map? compose #[a: 1 b: (expand-stack 2)]
		recycle/stack

	;-- loops (body evaluated repeatedly) -------------------------------------
	--test-- "forall string, global, with expansion"
		armed: true
		str: "abc"
		out: copy ""
		forall str [append out first str expand-once]
		--assert "abc" = out
		--assert 1 = index? str
		recycle/stack

	--test-- "forall on string argument with stack expansion"
		armed: true
		f: func [str /local out] [
			out: copy ""
			forall str [append out first str expand-once]
			reduce [out index? str]
		]
		--assert ["abc" 1] = f "abc"
		recycle/stack

	--test-- "forall on function argument with stack expansion"
		armed: true
		f: func [blk /local sum] [
			sum: 0
			forall blk [sum: sum + first blk expand-once]
			reduce [sum index? blk]
		]
		--assert [6 1] = f [1 2 3]
		recycle/stack

	--test-- "forall on function local with stack expansion"
		armed: true
		f: func [/local blk sum] [
			blk: [1 2 3]
			sum: 0
			forall blk [sum: sum + first blk expand-once]
			reduce [sum index? blk]
		]
		--assert [6 1] = f
		recycle/stack

	--test-- "forskip on function argument with stack expansion"
		armed: true
		f: func [blk /local sum] [
			sum: 0
			forskip blk 2 [sum: sum + first blk expand-once]
			reduce [sum index? blk]
		]
		--assert [4 1] = f [1 2 3 4]
		recycle/stack

	--test-- "forall throw after stack expansion"
		armed: true
		f: func [blk] [catch [forall blk [expand-once throw 5]]]
		--assert 5 = f [1 2 3]
		recycle/stack

	--test-- "forall break after stack expansion"
		armed: true
		f: func [blk] [forall blk [expand-once break] index? blk]
		--assert 1 = f [1 2 3]
		recycle/stack

	--test-- "loops with stack expansion in body"
		armed: true
		--assert [1 2 3] = map-each x [1 2 3] [expand-once x]
		recycle/stack
		armed: true
		blk: [1 2 3]
		remove-each x blk [expand-once x false]
		--assert blk = [1 2 3]
		recycle/stack
		armed: true
		--assert 3 = loop 3 [expand-once 3]
		recycle/stack
		armed: true i: 0
		--assert 2 = while [i < 2] [expand-once i: i + 1]
		recycle/stack
		armed: true i: 0
		--assert all [until [i: i + 1 expand-once i == 5] i == 5]
		recycle/stack

	--test-- "keep with stack expansion"
		armed: true
		--assert [1 2 3] = collect [foreach n [1 2 3] [keep (expand-once n)]]
		recycle/stack

	;-- parse (rules evaluated repeatedly) ------------------------------------
	--test-- "parse with paren expanding stack"
		armed: true
		--assert parse [1 2 3] [some [integer! (expand-once)]]
		recycle/stack

	--test-- "parse collect with paren expanding stack"
		armed: true
		--assert [1 2 3] = parse [1 2 3] [collect some [keep integer! (expand-once)]]
		recycle/stack

	--test-- "parse collect into with paren expanding stack"
		armed: true
		out: copy []
		--assert parse [1 2 3] [collect into out some [keep integer! (expand-once)]]
		--assert out = [1 2 3]
		recycle/stack

	--test-- "parse collect with keep paren expanding stack"
		armed: true
		--assert [1 0 2 0] = parse [1 2] [collect some [keep integer! keep (expand-once 0)]]
		recycle/stack

	;-- sort (comparator evaluated repeatedly) --------------------------------
	--test-- "sort/compare with stack expansion in comparator"
		armed: true
		blk: [3 1 2]
		sort/compare blk func [a b] [expand-once b - a]
		--assert [1 2 3] = blk
		recycle/stack

	--test-- "sort/compare/all with stack expansion in comparator"
		armed: true
		blk: [3 1 8 4]
		sort/skip/compare/all blk 2 func [a b] [expand-once a/1 - b/1]
		--assert blk = [8 4 3 1]
		recycle/stack

	;-- recycle and stack shrinking (no expansion needed) ---------------------
	--test-- "data stack shrink keeps the top of stack"
		recycle/stack f: func [/local o] [recycle]
		--assert function? :f

	--test-- "function local string survives recycle"
		f: func [/local out] [
			out: copy ""
			recycle
			append out "x"
			out
		]
		--assert "x" = f
		recycle/stack

	--test-- "function local string survives expand-stack"
		f: func [/local out] [
			out: copy ""
			expand-stack
			append out "x"
			out
		]
		--assert "x" = f
		recycle/stack

	--test-- "forall with local string and recycle only"
		f: func [str /local out] [
			out: copy ""
			forall str [append out first str recycle]
			out
		]
		--assert "abc" = f "abc"
		recycle/stack

	--test-- "native result survives recycle in argument evaluation"
		expand-stack
		recycle/stack
		--assert 3 = (add 1 do [recycle 2])
		--assert 6 = (add (do [recycle 1]) (do [recycle 5]))
		recycle/stack

	--test-- "function result survives recycle after stack shrink"
		expand-stack
		recycle/stack
		g: func [x] [recycle x + 1]
		--assert 3 = g 2
		--assert 5 = g g 3
		recycle/stack

	;-- sweeps over the stack depth (no expansion needed) ---------------------
	--test-- "sort/compare/all: comparator at every stack depth"
		sweep-sort: func [n /local blk] [
			either n > 0 [sweep-sort n - 1][
				blk: [3 1 8 4]
				sort/skip/compare/all blk 2 func [a b] [a/1 - b/1]
				blk
			]
		]
		recycle/stack
		repeat depth 1000 [
			--assert [8 4 3 1] = sweep-sort depth
		]
		recycle/stack

	--test-- "construct: cascaded set-words at every stack depth"
		sweep-construct: func [n blk] [
			either n > 0 [sweep-construct n - 1 blk][construct blk]
		]
		recycle/stack
		blk: append/dup make block! 0 [a:] 300
		append blk 1
		repeat depth 1000 [
			o: sweep-construct depth blk
			--assert o/a = 1
		]
		recycle/stack

	;-- image  
	--test-- "image path with paren expanding stack"
		img: make image! 2x2
		recycle/stack --assert 255.255.255.255 = img/(expand-stack 1)
		img/(expand-stack 2): 1.2.3
		recycle/stack --assert 1.2.3.255 = img/2
		img/(expand-stack 3): 4.5.6.7
		--assert 4.5.6.7 = img/3
		recycle/stack --assert 2x2 = img/(expand-stack 'size)
		img/(expand-stack 'size): 4x1
		--assert 4x1 = img/size
		recycle/stack

	--test-- "image pixel channel set after stack expansion"
		img: make image! 2x2
		img/1/2: (expand-stack 100)
		--assert 100 = img/1/2
		recycle/stack
===end-group===

~~~end-file~~~