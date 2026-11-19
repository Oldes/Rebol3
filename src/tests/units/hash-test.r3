Rebol [
	Title:   "Rebol3 hash! test script"
	Author:  "Oldes"
	File: 	 %hash-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "hash"

===start-group=== "HASH!"
	--test-- "hash! find/select"
		h: make hash! []
		repeat i 1000 [append h reduce [ajoin ["k" i] i]]
		--assert 500 = select h "k500"
		--assert 500 = select h "K500"
		--assert none? select/case h "K500"
		--assert "k1000" = first find h "k1000"
		--assert none? find h "k1001"
		k: "k500"
		--assert 500 = h/:k
		;; map-like equality
		append h [1 one 1.0 dec]
		--assert 'one = select skip h 4 1
		--assert 'dec = select h 1.0
		;; words match by canon (any word type), /case needs exact word
		append h [foo: 1 Bar 2]
		--assert 1 = select h 'foo
		--assert 2 = select h 'bar
		--assert none? select/case h 'bar
		;; block semantics kept for sequences
		--assert "k2" = first find h ["k2" 2]

	--test-- "copy hash!"
		small: make hash! [1 a 1.0 b]
		--assert 4 = length? copy small
		--assert small = copy small
		--assert hash? copy small
		--assert [a 1.0] = to block! copy/part next small 2
		--assert 2 = length? copy/part small 2
		--assert 'b = select copy small 1.0
	--test-- "to hash!"
		--assert hash? to hash! 1
		--assert 1 = length? to hash! 1
		--assert [1 2] = to block! to hash! [1 2]

	--test-- "hash! small and large behave the same"
		small: make hash! [1 a 1.0 b]
		large: copy small
		repeat i 100 [append large reduce [i + 1000 none]]
		--assert 'a = select small 1
		--assert 'a = probe select large 1
		--assert 'b = select small 1.0
		--assert 'b = select large 1.0

	--test-- "hash! duplicates and position"
		h: make hash! []
		repeat i 100 [append h 'x]
		append h 'y
		--assert 1 = index? find h 'x
		--assert 50 = index? find at h 50 'x
		--assert 101 = index? find h 'y
		--assert none? find next find h 'y 'x

	--test-- "hash! after modification"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 50 = index? find h 50
		insert h 0
		--assert 51 = index? find h 50
		--assert 1 = index? find h 0
		remove h
		--assert 50 = index? find h 50
		h/50: 'changed
		--assert none? find h 50
		--assert 50 = index? find h 'changed
		poke h 1 'first
		--assert none? find h 1
		reverse h
		--assert 1 = index? find h 100
		clear at h 51
		--assert none? find h 'first
		--assert 50 = index? find h 51
		change h 'a
		--assert 1 = index? find h 'a
		--assert none? find h 100
		append h 1000
		--assert 51 = index? find h 1000
===end-group===


===start-group=== "HASH! construction"
	--test-- "make hash! with size"
		h: make hash! 100
		--assert hash? h
		--assert empty? h
		--assert none? find h 1
		append h 1
		--assert 1 = index? find h 1

	--test-- "make hash! from block and paren"
		--assert hash? h: make hash! [a 1 b 2]
		--assert 2 = select h 'b
		--assert hash? h: make hash! to paren! [a 1 b 2]
		--assert 2 = select h 'b

	--test-- "make hash! from hash!"
		h1: make hash! [a 1 b 2]
		h2: make hash! h1
		--assert hash? h2
		--assert 2 = select h2 'b
		append h2 [c 3]
		--assert none? find h1 'c         ;; not the same series
		--assert 3 = select h2 'c

	--test-- "to block! of hash!"
		h: make hash! [a 1 b 2]
		b: to block! h
		--assert block? b
		--assert [a 1 b 2] = b
		append b [c 3]
		--assert 4 = length? h            ;; a copy

	--test-- "hash! from offset block"
		h: make hash! next [x a 1 b 2]
		--assert 4 = length? h
		--assert none? find h 'x
		--assert 1 = select h 'a
===end-group===


===start-group=== "HASH! lookup"
	--test-- "hash! find returns position"
		h: make hash! []
		repeat i 100 [append h i]
		p: find h 42
		--assert hash? p
		--assert 42 = index? p
		--assert 42 = first p
		--assert 43 = second p
		--assert 100 = length? head p

	--test-- "hash! find/tail"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 43 = index? find/tail h 42
		--assert tail? find/tail h 100

	--test-- "hash! find/last and find/reverse"
		h: make hash! []
		repeat i 100 [append h 'x]
		--assert 100 = index? find/last h 'x
		--assert 49 = index? find/reverse at h 50 'x
		--assert none? find/reverse h 'x

	--test-- "hash! find/match"
		h: make hash! [a b c]
		repeat i 100 [append h i]
		--assert did find/match h 'a
		--assert none? find/match h 'b
		--assert 2 = index? find/match/tail h 'a

	--test-- "hash! find/part"
		h: make hash! []
		repeat i 100 [append h i]
		--assert none? find/part h 50 10
		--assert 5 = index? find/part h 5 10

	--test-- "hash! find/only with block values"
		h: make hash! []
		repeat i 100 [append/only h reduce [i]]
		--assert 42 = index? find/only h [42]
		--assert none? find/only h [101]
		--assert [43] = select/only h [42]

	--test-- "hash! searching from offset"
		h: make hash! []
		repeat i 100 [append h i]
		--assert none? find at h 51 1
		--assert none? find at h 51 50
		--assert 51 = index? find at h 51 51
		--assert none? find tail h 100

	--test-- "hash! select at tail"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 2 = select h 1
		--assert none? select h 100     ;; last value, nothing after it
		--assert none? select h 101

	--test-- "hash! with none values"
		h: make hash! []
		repeat i 100 [append h i]
		append h none
		append h 'after-none
		--assert 101 = index? find h none
		--assert 'after-none = select h none

	--test-- "hash! does not mix datatypes"
		h: make hash! []
		repeat i 100 [append h i]
		append h [foo "foo" #"a"]
		--assert 101 = index? find h 'foo
		--assert 102 = index? find h "foo"
		--assert none? find h "100"
		--assert none? find h 100.0
		--assert none? find h 'bar

	--test-- "hash! word types"
		h: make hash! []
		repeat i 100 [append h i]
		append h [foo: 1]
		--assert 101 = index? find h 'foo
		--assert 101 = index? find h first [foo:]
		--assert 101 = index? find h first [:foo]
		--assert 101 = index? find h first ['foo]
		--assert 101 = index? find h first [/foo]
		;; /case needs the same word type and spelling
		--assert none? find/case h 'foo
		--assert 101 = index? find/case h first [foo:]
		--assert none? find/case h first [Foo:]

	--test-- "hash! char case sensitivity"
		h: make hash! []
		repeat i 100 [append h i]
		append h [#"a" 1 #"č" 2]
		--assert 1 = select h #"A"
		--assert 2 = select h #"Č"
		--assert none? select/case h #"A"
		--assert 1 = select/case h #"a"

	--test-- "hash! UTF-8 string case sensitivity"
		h: make hash! []
		repeat i 100 [append h i]
		append h ["Čau" 1]
		--assert 1 = select h "čau"
		--assert 1 = select h "ČAU"
		--assert none? select/case h "čau"
		--assert 1 = select/case h "Čau"

	--test-- "hash! binary is case sensitive"
		h: make hash! []
		repeat i 100 [append h i]
		append h [#{61} 1 #{41} 2]
		--assert 1 = select h #{61}
		--assert 2 = select h #{41}

	--test-- "hash! exact decimal keys"
		a: 0.1 + 0.2
		h: make hash! []
		repeat i 100 [append h i]
		append h reduce [a 1]
		--assert none? find h 0.3
		--assert 1 = select h a
		;; -0.0 and 0.0 are the same key
		append h [0.0 zero]
		--assert 'zero = select h negate 0.0

	--test-- "hash! canonical keys"
		h: make hash! []
		repeat i 100 [append h i]
		append h reduce [
			1-Jan-2020/10:00+1:00  'date
			1.2.3                  'tuple
			$1.00                  'money
			1x2                    'pair
		]
		--assert 'date    = select h 1-Jan-2020/9:00
		--assert 'tuple   = select h 1.2.3.0
		--assert 'money   = select h $1
		--assert 'pair    = select h 1x2
		--assert none?      select h 2x1

	--test-- "hash! with datatype and typeset values"
		;; without /only, a datatype or typeset finds a value of that type (block semantics)
		h: make hash! []
		repeat i 100 [append h i]
		append h reduce [integer! 'int string! 'str make typeset! [integer!] 'typeset]
		--assert 2 = select h integer!
		--assert none? select h string!
		;; with /only, the value itself is looked up
		--assert 'int = select/only h integer!
		--assert 'str = select/only h string!
		--assert none? select/only h char!
		--assert 'typeset = select/only h make typeset! [integer!]
		--assert none? select/only h make typeset! [string!]
===end-group===


===start-group=== "HASH! modification"
	--test-- "hash! append fast path"
		h: make hash! []
		repeat i 1000 [
			append h i
			--assert i = index? find h i     ;; lookup after every append
		]
		--assert 1000 = length? h

	--test-- "hash! append block"
		h: make hash! []
		repeat i 100 [append h i]
		append h [a b c]
		--assert 101 = index? find h 'a
		--assert 103 = index? find h 'c
		append/only h [x y]
		--assert 104 = index? find/only h [x y]

	--test-- "hash! append/part and append/dup"
		h: make hash! []
		repeat i 100 [append h i]
		append/part h [a b c] 2
		--assert 101 = index? find h 'a
		--assert 102 = index? find h 'b
		--assert none? find h 'c
		append/dup h 'd 3
		--assert 103 = index? find h 'd
		--assert 105 = index? find/last h 'd

	--test-- "hash! insert in the middle"
		h: make hash! []
		repeat i 100 [append h i]
		insert at h 50 [a b]
		--assert 50 = index? find h 'a
		--assert 52 = index? find h 50
		--assert 102 = index? find h 100

	--test-- "hash! insert/dup"
		h: make hash! []
		repeat i 100 [append h i]
		insert/dup h 'x 10
		--assert 11 = index? find h 1
		--assert 10 = index? find/last h 'x

	--test-- "hash! remove/part"
		h: make hash! []
		repeat i 100 [append h i]
		remove/part at h 10 10
		--assert none? find h 10
		--assert none? find h 19
		--assert 10 = index? find h 20
		--assert 90 = length? h

	--test-- "hash! take"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 1 = take h
		--assert 100 = take/last h
		--assert none? find h 1
		--assert none? find h 100
		--assert 1 = index? find h 2
		--assert #(hash! [2 3]) = take/part h 2
		--assert 1 = index? find h 4

	--test-- "hash! change/part"
		h: make hash! []
		repeat i 100 [append h i]
		change/part at h 10 [a] 5
		--assert none? find h 10
		--assert none? find h 14
		--assert 10 = index? find h 'a
		--assert 11 = index? find h 15
		--assert 96 = length? h

	--test-- "hash! sort"
		h: make hash! []
		repeat i 100 [insert h i]           ;; 100 99 ... 1
		--assert 100 = index? find h 1
		sort h
		--assert 1 = index? find h 1
		--assert 100 = index? find h 100

	--test-- "hash! swap"
		h: make hash! []
		repeat i 100 [append h i]
		swap h at h 100
		--assert 1 = index? find h 100
		--assert 100 = index? find h 1

	--test-- "hash! replace"
		h: make hash! []
		repeat i 100 [append h i]
		replace h 50 'x
		--assert none? find h 50
		--assert 50 = index? find h 'x
		replace/all h 'x 'y
		--assert none? find h 'x
		--assert 50 = index? find h 'y

	--test-- "hash! remove-each"
		h: make hash! []
		repeat i 100 [append h i]
		remove-each v h [odd? v]
		--assert 50 = length? h
		--assert none? find h 1
		--assert 1 = index? find h 2
		--assert 50 = index? find h 100

	--test-- "hash! modified through other reference"
		h: make hash! []
		repeat i 100 [append h i]
		p: at h 50
		insert p 'x
		--assert 50 = index? find h 'x
		--assert 51 = index? find h 50
		remove p
		--assert none? find h 'x
		--assert 50 = index? find h 50

	--test-- "hash! path set"
		h: make hash! []
		repeat i 100 [append h i]
		h/10: 'ten
		--assert none? find h 10
		--assert 10 = index? find h 'ten

	--test-- "hash! clear and refill"
		h: make hash! []
		repeat i 100 [append h i]
		clear h
		--assert empty? h
		--assert none? find h 1
		repeat i 100 [append h i * 2]
		--assert 1 = index? find h 2
		--assert none? find h 1

	--test-- "hash! take + append without lookup (queue)"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 1 = index? find h 1    ;; builds the table
		take h
		append h 101
		--assert 1 = index? find h 2
		--assert 100 = index? find h 101

	--test-- "hash! remove + append without lookup"
		h: make hash! []
		repeat i 100 [append h i]
		--assert 1 = index? find h 1
		remove h
		append h 101
		--assert 1 = index? find h 2

	--test-- "hash! parse change"
		h: make hash! []
		repeat i 100 [append h ajoin ["k" i]]
		--assert 50 = index? find h "k50"
		parse h [to "k50" change skip "x" to end]
		--assert none? find h "k50"
		--assert 50 = index? find h "x"

	--test-- "hash! trim/all + append without lookup"
		h: make hash! []
		repeat i 100 [append h reduce [none i]]
		--assert 2 = index? find h 1    ;; builds the table
		trim/all h ;== removes nones
		--assert 100 = length? h
		repeat i 100 [append h i + 100]
		--assert 1 = index? find h 1
		--assert 101 = index? find h 101

	--test-- "hash! put"
		;; same map-like equality as SELECT
		h: make hash! [1 a]
		put h 1.0 'b
		--assert 'a = select h 1
		--assert 'b = select h 1.0
		--assert 4 = length? h
		;; words by canon, /case needs exact word
		h: make hash! [foo 1]
		put h 'FOO 2
		--assert [foo 2] = to block! h
		put/case h 'FOO 3
		--assert [foo 2 FOO 3] = to block! h
		;; large (hashed) series
		h: make hash! []
		repeat i 100 [append h reduce [i none]]
		--assert 1 = put h 50 1
		--assert 1 = select h 50
		--assert 200 = length? h
		put h 50.0 2
		--assert 1 = select h 50
		--assert 2 = select h 50.0
		--assert 202 = length? h
		;; key as the last value
		append h 'last
		put h 'last 3
		--assert 3 = select h 'last
		--assert 204 = length? h
		;; put from offset
		h: make hash! [a 1 b 2 a 3]
		put next next h 'a 4
		--assert [a 1 b 2 a 4] = to block! h

	--test-- "hash! after recycle"
		h: make hash! []
		repeat i 1000 [append h ajoin ["k" i]]
		recycle
		--assert 500 = index? find h "k500"
===end-group===

===start-group=== "PUT with non-scalar keys"
	--test-- "put with block key"
		b: copy [[x] 1]
		put b [x] 2
		--assert [[x] 2] = b
		put b [y] 3
		--assert [[x] 2 [y] 3] = b
		h: make hash! [[x] 1]
		put h [x] 2
		--assert [[x] 2] = to block! h
		put h [y] 3
		--assert [[x] 2 [y] 3] = to block! h

	--test-- "put with datatype key"
		b: reduce [1 integer! 2]
		put b integer! 3
		--assert 3 = length? b
		--assert 3 = last b
		--assert 1 = first b
		h: make hash! reduce [1 integer! 2]
		put h integer! 3
		--assert 3 = length? h
		--assert 3 = last h

	--test-- "put with typeset key"
		t: make typeset! [integer!]
		b: reduce [1 t 2]
		put b t 3
		--assert 3 = length? b
		--assert 3 = last b
		h: make hash! reduce [1 t 2]
		put h t 3
		--assert 3 = length? h
		--assert 3 = last h
===end-group===

===start-group=== "HASH! misc"
	--test-- "protected hash!"
		h: make hash! []
		repeat i 100 [append h i]
		protect h
		--assert 50 = index? find h 50
		--assert all [
			error? e: try [append h 101]
			e/id = 'protected
		]
		--assert none? find h 101
		unprotect h
		append h 101
		--assert 101 = index? find h 101

	--test-- "copy/deep hash!"
		h: make hash! []
		repeat i 100 [append/only h reduce [i]]
		c: copy/deep h
		--assert hash? c
		--assert not same? first h first c
		--assert 42 = index? find/only c [42]
		--assert 42 = index? find/only h [42]

	--test-- "copy of hash! at offset"
		h: make hash! []
		repeat i 100 [append h i]
		c: copy at h 51
		--assert 50 = length? c
		--assert none? find c 1
		--assert 1 = index? find c 51

	--test-- "mold/all and load of hash!"
		h: make hash! [a 1 "b" 2 3.0 c]
		h2: transcode/one mold/all h
		--assert hash? h2
		--assert h = h2
		--assert 'c = select h2 3.0

	--test-- "foreach on hash!"
		h: make hash! []
		repeat i 100 [append h reduce [i i * 2]]
		n: 0
		foreach [k v] h [n: n + v - k]
		--assert n = 5050

	--test-- "hash! equality"
		--assert equal? make hash! [a 1] make hash! [a 1]
		--assert not equal? make hash! [a 1] make hash! [a 2]
		h: make hash! [a 1]
		--assert same? h h
		--assert not same? h copy h
===end-group===


~~~end-file~~~
