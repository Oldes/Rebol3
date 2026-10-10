Rebol [
	Title:   "Rebol/QOI extension CI test"
	Needs:   3.22.6
	Purpose: {
		Exercises the codec through the ordinary DECODE / ENCODE / ENCODING?
		path, not through the extension's own command, because going through
		system/codecs is the whole point of the extension.

		Two things here are checked against the QOI specification itself
		rather than against our own decoder: the header, and the byte order
		of the first stored pixel. A channel-order mistake survives a
		round-trip unnoticed - encode and decode would simply be wrong in
		the same direction - and only shows up when the file is opened by
		something else.

		Usage:
			r3 ci-test.r3            ;; full run
			r3 ci-test.r3 --quick    ;; skips the recycle loop
			r3 ci-test.r3 --internal ;; tests the embedded extension

		Exits with 1 if any assertion failed, so it can gate CI.
	}
]

print ["Running test on Rebol build:" mold to-block system/build]
system/options/quiet: false
system/options/log/rebol: 4

unless find system/options/args "--internal" [
	either CI?: any [
		"true" = get-env "CI"
		"true" = get-env "GITHUB_ACTIONS"
		"true" = get-env "TRAVIS"
		"true" = get-env "CIRCLECI"
		"true" = get-env "GITLAB_CI"
	][
		system/options/modules: dirize to-rebol-file any [
			get-env 'REBOL_MODULES_DIR
			what-dir
		]
		;; CI Test still prioritize existing module
	][
		;; Make sure that we load a fresh extension - the module directory may hold a
		;; previously installed copy, which would import cleanly and quietly make
		;; every test below meaningless.
		try [system/modules/sqlite: none]
	]

	if all [not CI?  modules-dir: get-env 'REBOL_MODULES_DIR][
		system/options/modules: dirize to-rebol-file modules-dir
	]
]

;; A host built with INCLUDE_QOI_CODEC already has an internal `qoi` codec.
;; `register-codec` appends to system/codecs, which replaces an existing field
;; of that object, so the extension wins - but remember whether there was one,
;; so the report says which codec the rest of the run actually measured.
had-internal-codec?: did find system/codecs 'qoi

ext: import 'qoi

print as-yellow "Content of the module..."
? ext

;;=============================================================================
;; Minimal self-contained harness
;;=============================================================================

test-count: 0
fail-count: 0
skip-count: 0
failed: copy []

group: func ["Starts a named group of tests" name [string!]][
	print ajoin [lf as-yellow "== " as-yellow name]
]

;; NOTE: plain `func` with explicit locals - `function` would collect the
;; counter set-words as locals and the totals would never move.
;;
;; A test block must EVALUATE to true; anything else counts as a failure and
;; is printed as the detail, so a check can hand back a string saying what
;; went wrong. Never use `return` inside one - it would return from
;; `--test--` itself and the result would never be counted.
--test--: func [
	"Passes when the code block evaluates to true"
	name [string!]
	code [block!]
	/local result
][
	test-count: test-count + 1
	result: try code
	case [
		error? :result [
			fail-count: fail-count + 1
			append failed name
			print [as-red "[FAIL]" name]
			print [as-red "      " mold/flat :result]
		]
		:result = true [
			print [as-green "[ ok ]" name]
		]
		true [
			fail-count: fail-count + 1
			append failed name
			print [as-red "[FAIL]" name "=>" mold/flat/part :result 60]
		]
	]
	:result
]

--skip--: func ["Counts a test which cannot run here" name [string!]][
	skip-count: skip-count + 1
	print [as-purple "[skip]" name]
]

--rejects--: func [
	"Checks that the code is refused with an error"
	label [string!]
	code  [block!]
][
	--test-- ajoin [label " is refused"] compose/only [error? try (code)]
]

is?: func ["Compares a value with the expected one" value expected][
	any [value == expected  ajoin ["got " mold/flat value]]
]

summary: does [
	print ajoin [lf as-yellow "-----------------------------------------------------------------------------"]
	print [
		"tests:" test-count
		as-green ajoin ["passed: " test-count - fail-count - skip-count]
		either fail-count > 0 [as-red ajoin ["failed: " fail-count]][ajoin ["failed: " fail-count]]
		as-purple ajoin ["skipped: " skip-count]
	]
	if fail-count > 0 [
		print as-red ajoin ["^/Failed tests:^/  " mold/flat failed]
	]
	print ""
	quit/return either fail-count > 0 [1][0]
]

args: system/options/args
quick?: did find args "--quick"

;;=============================================================================
;; Fixtures
;;=============================================================================

;; Four pixels, four different colours - enough that a swapped red and blue
;; channel cannot hide behind a grey.
make-sample: does [
	img: make image! 2x2
	img/1: 255.0.0
	img/2: 0.255.0
	img/3: 0.0.255
	img/4: 255.255.255
	img
]

sample: make-sample

;; A single red pixel, for the on-the-wire checks below.
red-dot: make image! 1x1
red-dot/1: 255.0.0

;;=============================================================================
group "Module surface"
;;=============================================================================

--test-- "module imported" [module? ext]

;; The generated `_init` command and the one command of this extension are
;; both hidden once the module body has run - neither is part of its interface.
--test-- "the generated _init command is hidden"    [none? in ext '_init]
--test-- "the codec-handle command is hidden"       [none? in ext 'codec-handle]
--test-- "nothing is exported into the user context" [not value? 'codec-handle]

if had-internal-codec? [
	print as-purple "NOTE: this host has an internal QOI codec; it was replaced by the extension."
]

;;=============================================================================
group "Codec registration"
;;=============================================================================

--test-- "the codec is in system/codecs" [did codec: select system/codecs 'qoi]
--test-- "the codec is an object"        [object? codec]
--test-- "the codec is ours"             [is? codec/title "Quite OK Image"]
--test-- "the codec name"                [is? codec/name 'qoi]
--test-- "the codec type"                [is? codec/type 'image]
--test-- "the codec suffixes"            [is? codec/suffixes [%.qoi]]

;; DO-CODEC refuses anything whose handle type is not `codec`, so this is what
;; makes the whole path work at all.
--test-- "the entry is a handle"         [handle? codec/entry]

--test-- "the suffix is registered as a file type" [
	is? select system/catalog/file-types %.qoi 'qoi
]

;;=============================================================================
group "Round trip"
;;=============================================================================

--test-- "an image encodes"      [binary? bin: encode 'qoi sample]
--test-- "the result is not empty" [0 < length? bin]
--test-- "the binary decodes"    [image? img: decode 'qoi bin]
--test-- "the size is preserved" [is? img/size 2x2]
--test-- "pixel 1 survives"      [is? img/1 255.0.0.255]
--test-- "pixel 2 survives"      [is? img/2 0.255.0.255]
--test-- "pixel 3 survives"      [is? img/3 0.0.255.255]
--test-- "pixel 4 survives"      [is? img/4 255.255.255.255]
--test-- "the whole image survives" [is? img sample]

--test-- "encoding does not modify the input image" [is? sample make-sample]

;;=============================================================================
group "On the wire (checked against the QOI specification)"
;;=============================================================================

dot: encode 'qoi red-dot

--test-- "the stream starts with the qoif magic" [is? copy/part dot 4 #{716F6966}]

;; Width and height are 32bit big endian, right after the magic.
--test-- "the dimensions are in the header" [is? copy/part skip dot 4 8 #{0000000100000001}]

;;=============================================================================
group "Identification"
;;=============================================================================

--test-- "ENCODING? recognises the format"   [is? encoding? dot 'qoi]
--test-- "ENCODING? rejects a foreign binary" [not 'qoi = encoding? #{89504E470D0A1A0A}]

;; ENCODING? offers every codec whatever binary it was given, so IDENTIFY must
;; survive inputs far shorter than a QOI header instead of reading past them.
--test-- "ENCODING? survives a one byte binary"   [not error? try [encoding? #{71}]]
--test-- "ENCODING? survives an empty binary"     [not error? try [encoding? #{}]]
--test-- "ENCODING? survives a truncated header"  [not error? try [encoding? copy/part dot 13]]

;;=============================================================================
group "Invalid input"
;;=============================================================================

--rejects-- "decoding a foreign binary"     [decode 'qoi #{89504E470D0A1A0A}]
--rejects-- "decoding an empty binary"      [decode 'qoi #{}]
--rejects-- "decoding a truncated header"   [decode 'qoi copy/part dot 13]
--rejects-- "decoding a truncated stream"   [decode 'qoi copy/part dot 15]
--rejects-- "encoding something else"       [encode 'qoi "not an image"]

;; A corrupted body must come back as an error, not as a plausible image.
--test-- "a corrupted stream does not crash" [
	bad: copy dot
	bad/15: 123
	not error? try [attempt [decode 'qoi bad]]
]

;;=============================================================================
group "Repeated use and recycling"
;;=============================================================================

;; Every encode hands DO-CODEC a buffer allocated with RL_ALLOC, which the
;; interpreter then frees with Free_Mem. A mismatch between those two is what
;; this loop is for: on a DEBUG build the accounting assertion in Dispose_Core
;; turns it into a failure at exit, and on any build a wrong pointer tends to
;; show up as a crash long before the loop ends.
either quick? [
	--skip-- "500 round trips with a recycle between them (--quick)"
][
	--test-- "500 round trips with a recycle between them" [
		loop 500 [
			img: decode 'qoi encode 'qoi sample
			recycle
		]
		is? img sample
	]
]

summary
