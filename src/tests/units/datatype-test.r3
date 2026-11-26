Rebol [
	Title:   "Rebol3 datatype! test script"
	Author:  "Oldes, Peter W A Wood"
	File: 	 %datatype-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
	type:    private
]

~~~start-file~~~ "datatype"

===start-group=== "datatype!"
	--test-- "system/catalog/datatypes"
		--assert block? system/catalog/datatypes
		--assert datatype? first system/catalog/datatypes
		--assert #(end!) = first system/catalog/datatypes

	--test-- "reflect datatype!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1534
		--assert object? sp: reflect integer! 'spec
		--assert  sp/title = reflect integer! 'title
		--assert  sp/type  = reflect integer! 'type
		--assert  sp/type = 'scalar

	--test-- "to word! datatype!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/38
		--assert 'logic!   = to word! logic!
		--assert 'percent! = to word! percent!
		--assert 'money!   = to word! money!

	--test-- "find datatype!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/256
		--assert none? find reduce [string! binary!] binary!
		--assert none? find [#(string!) #(binary!)] #(binary!)
		--assert none? find [#(string!) #(binary!)] binary!	

		--assert not none? find/only reduce [string! binary!] binary!
		--assert not none? find/only [#(string!) #(binary!)] #(binary!)
		--assert not none? find/only [#(string!) #(binary!)] binary!

		--assert not none? find ["test"] string!
		--assert not none? find ["test"] series!
		--assert  none? find reduce [integer! binary!] series!
		--assert ["aha"] = find reduce [integer! "aha"] series!
		--assert not none? find any-string! ref!

		--assert 1 = index? find [#{00}  #(string!) #(binary!)] #(binary!)
		--assert 3 = index? find/only [#{00}  #(string!) #(binary!)] #(binary!)

		--assert 2 = index? find/only reduce ["" string! any-string! binary!] string!
		--assert 3 = index? find/only reduce ["" string! any-string! binary!] any-string!
		--assert 1 = index? find reduce ["" string! any-string! binary!] any-string!
		--assert 1 = index? find reduce [%a string! any-string! binary!] any-string!
		--assert      none? find reduce [%a string! any-string! binary!] string!

		;; using old construction syntax
		--assert none? find [#(datatype! string!) #(datatype! binary!)] #(datatype! binary!)
		--assert none? find [#(datatype! string!) #(datatype! binary!)] binary!
		--assert not none? find/only [#(datatype! string!) #(datatype! binary!)] #(datatype! binary!)
		--assert not none? find/only [#(datatype! string!) #(datatype! binary!)] binary!

	--test-- "number?"
		--assert number? 1
		--assert number? 1.0
		--assert number? 1%
		--assert not number? "1"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2672
		--assert not number? ()

	--test-- "datatype - comparison"
		--assert #(integer!) =  #(integer!)
		--assert #(integer!) == #(integer!)
		--assert #(integer!) <= #(integer!)
		--assert #(integer!) >= #(integer!)
		--assert not (#(integer!) > #(integer!))
		--assert not (#(integer!) < #(integer!))
		--assert not (#(integer!) != #(integer!))

		--assert not (#(op!) =  #(integer!))
		--assert not (#(op!) == #(integer!))
		--assert not (#(op!) <= #(integer!))
		--assert      #(op!) >= #(integer!)
		--assert      #(op!) > #(integer!)
		--assert not (#(op!) < #(integer!))
		--assert      #(op!) != #(integer!)

===end-group===

===start-group=== "contains-type?"
	--test-- "top-level value"
		--assert contains-type? :print native!
		--assert not contains-type? 1 native!
		--assert contains-type? 1 make typeset! [decimal! integer!]
	--test-- "nested in blocks and parens"
		--assert contains-type? reduce [1 reduce [2 to paren! reduce [3 :print]]] native!
		--assert not contains-type? [1 [2 (3 "x")]] any-function!
	--test-- "nested in maps"
		m: #[a: 1]
		put m 'f :print
		--assert contains-type? m any-function!
		remove/key m 'f
		--assert not contains-type? m any-function! ;; removed entries are ignored
	--test-- "nested in objects"
		--assert contains-type? object [a: 1 b: object [c: does [1]]] function!
		--assert not contains-type? object [a: [1 2]] function!

	--test-- "circular references are not followed again"
		b: [1]
		append/only b b
		--assert not contains-type? b native!
		--assert contains-type? b integer!
	--test-- "contains-type?/circular"
		--assert contains-type?/circular b native!
		o: object [ref: none]
		o/ref: o
		--assert contains-type?/circular o native!
		m: #[a: 1]
		put m 'self-ref m
		--assert contains-type?/circular m native!
		s: [1 2]
		--assert not contains-type?/circular reduce [s s] native!
		--assert not contains-type?/circular 1 native!
	--test-- "serializable?"
		--assert serializable? [1 "x" #[k: 2.5] 1-Jan-2026]
		--assert not serializable? reduce [1 :print]
		--assert not serializable? b
===end-group===

~~~end-file~~~