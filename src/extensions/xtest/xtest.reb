REBOL [
	Title:   "Test of Embedded Extension"
	Name:    xtest
	Version: 1.0.0
	Needs:   3.22.5
	Author:  @Oldes
	License: Apache-2.0
	Options: [delay]
	Exports: [xtest]
	Purpose: {
		Exercises the extension interface - argument passing of every
		datatype, callbacks, handles with path accessors, command
		context, and struct access.

		Builds either embedded into the host or as a standalone
		xtest-x64.rebx library.
	}
]

;; ---------------------------------------------------------------------------
;; C-side configuration
c-prefix:  XTEST
;c-include: []

;; Extra declarations the generated header must carry.
c-header: {
extern REBCNT Handle_XTest;

typedef struct XTest_Context {
	REBCNT id;
	REBCNT flags;
} XTEST;
}

;; ---------------------------------------------------------------------------
;; Words resolved at init time through RL_MAP_WORDS.
;;
;; The path-accessor words are collected from `handles:` below, so `arg` is
;; left empty here - list a word only if something needs it which no handle
;; field declares. The `init-words` command, the W_XTEST_ARG_* enum (with its
;; _0 sentinel) and the handler filling `Xtest_arg_words` are all generated.
words: [
	arg: []
]

;; ---------------------------------------------------------------------------
;; Handle types and their path accessors.
;;
;; Each row is: NAME, the type read from the field, the type accepted when
;; writing it (`none` for read-only), and a description. The names become the
;; `arg` word list above and the W_XTEST_ARG_* enum used by XTest_get_path
;; and XTest_set_path.
handles: [
	xtest: [
		"XTest context handle"
		;NAME   GET                SET       DESCRIPTION
		id      integer!           integer!  "User defined identifier"
		data   [binary! block!]    binary!   "The payload; a block when the handle was made /with another one"
		length  integer!           none      "Number of bytes in the payload"
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
commands: [
	xarg0:  ["return zero"]
	xarg1:  ["return first arg" arg]
	xarg2:  ["return second arg" arg1 arg2]
	xword0: ["return system word from internal string"]
	xword1: ["return word from string" str [string!]]
	xobj1:  ["return obj field value" obj [object!] field [word! lit-word!]]
	xobj2:  ["print object's field names and types" obj [object!]]
	calls:  ["test sync callback" context [object!] word [word!]]
	calla:  ["test async callback" context [object!] word [word!]]
	img0:   ["return 2x3 image"]
	cec0:   ["test command context struct" blk [block!]]
	cec1:   ["returns cec.index value or -1 if no cec"]
	hndl1:  ["creates a handle"]
	hndl2:  ["return handle's internal value as integer" hnd [handle!]]
	vec0:   ["return vector size in bytes" v [vector!]]
	vec1:   ["return vector size in bytes (from object)" o [object!]]
	blk1:   ["print type ids of all values in a block" b [block!]]
	hob1:   ["creates XTEST handle" bin [binary!] /with hnd [handle!]]
	hob2:   ["prints XTEST handle's data" hndl [handle!]]
	str0:   ["return a constructed string"]
	echo:   ["return the input value" value]
	path:   ["converts Rebol file to an OS file string" f [file!] /full "full path"]
	stru:   ["test struct passing" val [struct!]]
]

;; ---------------------------------------------------------------------------
;; Module body. Previously a C string literal with escaped newlines - as a
;; block it is ordinary Rebol code that an editor can indent and check.
mezzanine: [
	a: b: c: h: x: y: none
	i: make image! 2x2
	s: make struct! [a [uint8!]]

	xtest: does [
		foreach blk [
			[x: hob1 #{0102}]
			[print [{x is} mold x {and has data:} mold x/data {with length:} x/length {and id:} x/id]]
			[x/id: 2 print [{now the id is:} x/id]]
			[print [{It is not possible to change its length:} error? try [x/length: 3]]]
			[hob2 x]

			;; Sometimes a handle may depend on another handle - this simulates it.
			[y: hob1/with #{00} x  x: none  print [{The new handle keeps reference to the second handle:} mold y/data y/data/2/id]]
			;; Manually releasing a handle...
			[print [{Relasing:} y]  release y  print [{Result:} y {should have no data:} y/data]]

			[h: hndl1]
			[hndl2 h]
			[xarg0]
			[xarg1 111]
			[xarg1 1.1]
			[xarg1 {test}]
			[xarg1 [1 2 3]]
			[xarg1 10-Sep-2010]
			[xarg2 111 222]
			[xword0]
			[xword1 {system}]
			[xobj1 system 'version]
			[xobj2 system]

			;; Just an example context. Normally this would be your own
			;; object holding your own functions.
			[calls lib 'negate]
			[calls lib 'sine]
			[calla lib 'print]
			[img0]
			[c: do-commands [a: xarg0 b: xarg1 333 xobj1 system 'version] reduce [a b c]]
			[cec0 [a: cec1 b: cec1 c: cec1] reduce [a b c]]

			;; Shaped and plain vectors - vec0 reports the byte size,
			;; vec1 the element count taken from the packed info.
			[vec0 make vector! [integer! 16 [1 2 3]]]
			[vec1 object [v: make vector! [integer! 16 [1 2 3]]]]
			[vec0 #(u8! [1 2 3 4])]

			[blk1 [read %img /at 1]]
			[str0]

			;; https://github.com/Oldes/Rebol-issues/issues/1809
			[echo i]
			[probe i probe echo i]
			[loop 1 [probe echo i]]

			;; https://github.com/Oldes/Rebol-issues/issues/2536
			[same? s probe echo s]

			[{foo} == path %foo]

			[probe s stru s]
		][
			print [{^/^[[7mtest:^[[0m^[[1;32m} mold blk {^[[0m}]
			print join {^[[1;33m} [do blk {^[[m}]
		]
		prin {^/^[[7mAsync call result (should be printed 1234):^[[0m }
		wait 0.1 ;; let async events happen
		exit
	]
]
