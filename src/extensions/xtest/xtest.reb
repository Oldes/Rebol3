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

extern REBDEV Dev_XTest;
extern int    Xtest_Dev_Id;
extern REBCNT Xtest_Dev_Polls;

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
	xobj1:  ["return obj field value" obj [object! port!] field [word! lit-word!]]
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
	stru:   ["test struct passing" val [struct!] /read "inspect only, do not modify the data"]
	stru0:  ["make a new struct of the same specification" val [struct!]]
	strua:  ["sum the elements of an integer array field" val [struct!] field [word!]]
	xdev:      ["return the id of the device the extension registered"]
	xdev-poll: ["return how many times that device has been polled"]
	xdev-err:  ["try to register that device again; returns the RDR_ code" size [integer!] "REBDEV size to claim, 0 = the real one"]
]

;; ---------------------------------------------------------------------------
;; Module body. Previously a C string literal with escaped newlines - as a
;; block it is ordinary Rebol code that an editor can indent and check.
mezzanine: [
	a: b: c: h: x: y: t: none
	i: make image! 2x2
	s: make struct! [a [uint8!]]
	;; Nested struct - `n/b` is a VIEW into n's data at a non-zero offset,
	;; so its size can only come from the specification.
	n: make struct! [a [uint8!] b [struct! [c [uint32!] d [uint32!]]]]
	;; A `rebval!` field makes t-struct flag the spec MARK|PROTECTED: the
	;; data now holds real Rebol values which the GC walks, so raw byte
	;; writes from an extension would corrupt them.
	p: make struct! [a [uint8!] v [rebval!]]
	q: none
	;; Array fields. `a` maps to a vector on the Rebol side, `w` is a block
	;; of words (no vector equivalent), `r` holds three Rebol values which
	;; the GC must mark individually.
	v: make struct! [a [int32! [4]] b [uint8!]]
	w: make struct! [n [word! [2]]]
	r: make struct! [v [rebval! [3]]]
	z: dev-id: dev-polls: none

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
			[
				prin {^/^[[7mAsync call result (should be printed 1234):^[[0m }
				wait 0.1 ;; let async events happen
				()       ;; returns unset
			]
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
			;; The returned struct must be the same value, not a copy.
			[same? s stru s]
			;; A nested view must report ITS OWN size (8), not the bytes
			;; left in the root data series, and must round-trip with its
			;; offset intact.
			[stru n/b]
			[same? n/b stru n/b]
			;; Bumping the first byte of the view must land inside `b`,
			;; i.e. touch b/c and leave a alone.
			[n/a: 10 n/b/c: 0 stru n/b reduce [n/a n/b/c]]
			
			;; Reading a marked/protected struct is allowed...
			[q: "held by the struct"  p/a: 1  p/v: q  stru/read p]
			;; ...writing into it must be refused, not silently done.
			[error? try [stru p]]
			;; ...and the refusal must leave the data untouched.
			[p/a]
			;; ...and the held value must survive a collection, which is
			;; the whole reason the flag exists.
			[recycle same? q p/v]

			;; An extension can instantiate any spec already registered in
			;; system/catalog/structs - a fresh struct, not a view into the
			;; one it was given.
			[t: stru0 s  all [struct? t  not same? t s  zero? t/a]]
			;; The data start zeroed, so a nested field is usable at once...
			[t: stru0 n  t/b/c: 42  reduce [t/a t/b/c]]
			;; ...and a `rebval!` field reads as none, because a zeroed slot
			;; is END ("never set") rather than a garbage value the GC would
			;; try to mark.
			[t: stru0 p  none? t/v]
			;; The new struct must survive a collection on its own.
			[t: stru0 p  recycle  t/v: q  recycle  same? q t/v]

			;; The C side must reach every element at the right stride, and
			;; `b` after the array must be left alone.
			[v/a: [1 2 3 4]  v/b: 200  reduce [strua v 'a  v/b]]
			;; A negative element proves the type is honoured, not just the width.
			[v/a: [1 -2 3 -4]  strua v 'a]
			;; Unknown field and non-array field are refused, not guessed at.
			[error? try [strua v 'nope]]
			[error? try [strua v 'b]]
			;; A word! array has no vector equivalent - the extension must
			;; refuse it rather than sum raw symbol ids.
			[w/n: [alpha beta]  error? try [strua w 'n]]
			;; Every element of a rebval! array must be marked, including the
			;; last one - an off-by-one in Mark_Struct_Fields would show only
			;; here.
			[q: "first"  z: "last"  r/v: reduce [q 'middle z]]
			[recycle  reduce [same? q first r/v  same? z last r/v]]
			;; ...and the same through a struct the extension created.
			[t: stru0 r  t/v: reduce [q 'middle z]  recycle  same? z last t/v]
 
			;; --- device registration -------------------------------------
			;; The extension added a device to the host device table. Any
			;; positive id means a slot was assigned; a failure would be one
			;; of the negative RDR_ codes.
			[dev-id: xdev  all [integer? dev-id  dev-id > 0]]
			;; The same device must not go in twice - it has one pending list.
			[-2 = xdev-err 0]
			;; A caller built against a different REBDEV layout is refused.
			[-3 = xdev-err 1]
			;; Neither refusal may consume a slot, so the id is unchanged.
			[dev-id = xdev]

			;; RDO_AUTO_POLL makes the host poll the device from OS_Wait even
			;; with nothing pending. This is what lets an extension pump an OS
			;; event queue during WAIT - the reason for registering at all.
			[dev-polls: xdev-poll  wait 0.1  (xdev-poll) > dev-polls]

			;; --- port! as a command argument ------------------------------
			;; A port's payload is its object frame (VAL_PORT == VAL_OBJ_FRAME),
			;; so it crosses as RXE_OBJECT and must come back as the same port,
			;; not a copy and not a plain object.
			[port? echo system/ports/event]
			[same? system/ports/event echo system/ports/event]
			;; ...and the frame the extension receives must be readable.
			[object? xobj1 system/ports/event 'spec]
		][
			print [{^/^[[7mtest:^[[0m^[[1;32m} mold blk {^[[0m}]
			print join {^[[1;33m} [do blk {^[[m}]
		]
		exit
	]
]
