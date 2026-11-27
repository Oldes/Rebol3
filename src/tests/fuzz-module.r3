Rebol [
	Title:   "Fuzz testing helper"
	Author:  "Oldes"
	File:    %fuzz-module.r3
	Version: 1.0.0
	Name:    Fuzz-test
	Type:    module
	Exports: [fuzz]
	Purpose: {
		Calls an action with many randomly damaged copies of valid inputs.
		It is meant for functions, which read untrusted binary data (decoders),
		which must refuse not valid input with an error and never crash or hang.
	}
	Usage: {
		result: fuzz/count/keep :my-decoder [#{...valid input...}] 10000 8
		--assert result/inputs = 10000
		--assert result/ok > 0                  ;; some inputs were still valid
		--assert [bad-media] = keys-of result/errors ;; only expected errors

		A crash or a hang stops the whole test run, so it cannot be reported as
		a failed assertion. The inputs are the same for the same seeds and
		/seed value, so such a case can be repeated with the /log refinement,
		which writes each input into a file before it is used. After a crash,
		the file holds the input which caused it.
	}
]

fuzz: function/with [
	"Calls an action with randomly mutated copies of the seed inputs"
	action [any-function!] "Called with one binary! argument. It should use the result (for example mold it)."
	seeds  [block!]        "Valid binary! inputs"
	/count inputs [integer!] "Number of calls (default is 10000)"
	/keep  head   [integer!] "Number of leading bytes which are never mutated (for example a header)"
	/seed  rseed  [integer!] "Random seed (default is 1)"
	/log   file   [file!]    "Write each input into the file before the call"
	/quiet "Do not print the summary"
	return: [object!] "Counts of inputs, ok results and errors (map of error ids)"
][
	inputs: any [inputs 10000]
	head:   any [head 0]
	foreach s seeds [
		unless binary? s [cause-error 'script 'expect-val [binary! type? s]]
		if head > length? s [cause-error 'script 'out-of-range head]
	]
	random/seed any [rseed 1]

	result: make object! [inputs: 0 ok: 0 errors: make map! 8]

	loop inputs [
		data: copy random/only seeds
		mutate data head
		if log [write file data]
		result/inputs: result/inputs + 1
		either error? err: try [action data][
			result/errors/(err/id): 1 + any [result/errors/(err/id) 0]
		][
			result/ok: result/ok + 1
		]
		;; the GC must be able to handle what the action left behind
		if zero? result/inputs // 1000 [recycle]
	]
	recycle
	unless quiet [
		print ["    fuzz:" result/inputs "inputs," result/ok "ok," "errors:" mold/flat body-of result/errors]
	]
	result
][
	mutate: func [
		"Applies 1 to 3 random changes to the data after its head"
		data [binary!] head [integer!]
		/local pos len
	][
		loop random 3 [
			len: (length? data) - head
			either len <= 0 [
				append data (random 256) - 1
			][
				pos: head + random len
				switch random 6 [
					1 [poke data pos (random 256) - 1]                 ;; change a byte
					2 [poke data pos (pick data pos) xor shift 1 (random 8) - 1] ;; flip a bit
					3 [remove at data pos]                             ;; remove a byte
					4 [insert at data pos (random 256) - 1]            ;; insert a byte
					5 [insert at data pos copy/part at data pos random 8] ;; repeat a part
					6 [clear at data pos]                              ;; cut the rest
				]
			]
		]
		data
	]
]
