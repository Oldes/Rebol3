Rebol [
	Title:   "Rebol3 fuzz helper test script"
	Author:  "Oldes"
	File: 	 %fuzz-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3 %../fuzz-module.r3]
	type:    private
]

~~~start-file~~~ "FUZZ helper"

===start-group=== "fuzz"
	--test-- "number of inputs"
		n: 0
		r: fuzz/count/quiet func [b][n: n + 1] [#{0102030405}] 100
		--assert 100 = n
		--assert 100 = r/inputs
		--assert 100 = r/ok
		--assert empty? r/errors
	--test-- "inputs are mutated"
		inputs: copy []
		fuzz/count/quiet func [b][append/only inputs b] [#{0102030405}] 100
		--assert 100 = length? inputs
		--assert 50 < length? unique inputs
		changed: 0
		foreach b inputs [if b <> #{0102030405} [changed: changed + 1]]
		--assert changed > 90
	--test-- "same seed gives the same inputs"
		a: copy [] b: copy [] c: copy []
		fuzz/count/quiet func [x][append/only a x] [#{0102030405} #{AABBCC}] 50
		fuzz/count/quiet func [x][append/only b x] [#{0102030405} #{AABBCC}] 50
		fuzz/count/seed/quiet func [x][append/only c x] [#{0102030405} #{AABBCC}] 50 2
		--assert a == b
		--assert a <> c
	--test-- "head is never mutated"
		r: fuzz/count/keep/quiet func [b][
			unless #{0102} == copy/part b 2 [do make error! "head was mutated"]
		] [#{0102030405} #{0102}] 2000 2
		--assert 2000 = r/ok
	--test-- "errors are counted by their id"
		r: fuzz/count/quiet func [b][
			either odd? length? b [cause-error 'script 'invalid-arg b][b]
		] [#{0102030405} #{01020304}] 1000
		--assert 1000 = (r/ok + r/errors/invalid-arg)
		--assert r/ok > 0
		--assert r/errors/invalid-arg > 0
		--assert [invalid-arg] = keys-of r/errors
	--test-- "log"
		file: %fuzz-test-log.bin
		last-input: none
		fuzz/count/log/quiet func [b][last-input: copy b] [#{01020304}] 3 file
		--assert last-input == read file
		delete file
	--test-- "invalid arguments"
		--assert error? try [fuzz/quiet func [b][b] [1]]
		--assert error? try [fuzz/quiet func [b][b] ["abc"]]
		--assert error? try [fuzz/keep/quiet func [b][b] [#{01}] 2]
===end-group===

~~~end-file~~~