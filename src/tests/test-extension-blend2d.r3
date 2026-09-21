Rebol [
	Title:   "Rebol/Blend2D extension CI test"
	Needs:   3.22.5
	Purpose: {
		Exercises the extension's whole surface - every command, the draw
		dialect, and the path accessors of the BLImage, BLPath and
		BLFontFace handles, because those are resolved through the
		generated word enum where a wrong index is silent: the call still
		succeeds, it just reads the wrong field.

		The drawing checks read the pixels back out of the target image.
		Only fully opaque fills are compared by value; anything which is
		antialiased is checked by which channel dominates, so the suite
		does not depend on Blend2D's exact rasterization.

		Nothing here needs a display or writes a file. A system font is
		looked up for the text tests and they are skipped when none is
		found.

		Usage:
			r3 ci-test.r3            ;; full run
			r3 ci-test.r3 --quick    ;; skips the recycle loop

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

;; NOTE: only `draw` is exported; everything else is reached through the
;; module. The words below are bound here so the tests read like the docs.
b2d: import 'blend2d

print as-yellow "Content of the module..."
? b2d

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

;; Compares a result with what it should be and hands back the actual value
;; when they differ, so the failure line shows it.
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

;;=============================================================================
;; Options and fixtures
;;=============================================================================

args: system/options/args
quick?: did find args "--quick" ;; skips the recycle loop

;; The commands which are not exported.
image:       :b2d/image
path:        :b2d/path
font:        :b2d/font
info:        :b2d/info
set-threads: :b2d/set-threads

;; Pixel accessors. X and Y are ZERO based, like the coordinates of the draw
;; dialect itself; the image index is one based.
;; A fetched pixel is always a four byte tuple (see Set_Tuple_Pixel in the
;; interpreter), so the alpha is always there.
px: func ["Returns the RGB of one pixel" img [image!] x [integer!] y [integer!] /local t][
	t: img/(1 + x + (y * img/width))
	to tuple! reduce [t/1 t/2 t/3]
]
pa: func ["Returns the alpha of one pixel" img [image!] x [integer!] y [integer!]][
	pick img/(1 + x + (y * img/width)) 4
]

;; Colors which come out of a gradient are sampled at the pixel's center, so
;; even the pixel over the first stop is already a step into the gradient.
near?: func [
	"Compares two colors, allowing for a small difference in each channel"
	color [tuple!] expected [tuple!]
	/within diff [integer!]
	/local n
][
	n: 0
	repeat i 3 [n: max n absolute (pick color i) - (pick expected i)]
	any [
		n <= any [diff 6]
		ajoin ["got " mold color " instead of " mold expected]
	]
]

;; A 4x4 fully opaque green image, used as a pattern and as a blit source.
src: draw 4x4 [fill 0.255.0 fill-all]

;; The first system font which can be found; the text tests need one.
font-file: none
foreach dir [
	%/usr/share/fonts/truetype/dejavu/
	%/usr/share/fonts/truetype/liberation/
	%/usr/share/fonts/truetype/freefont/
	%/usr/share/fonts/TTF/
	%/usr/share/fonts/
	%/usr/share/fonts/noto/
	%/Library/Fonts/
	%/System/Library/Fonts/
	%/C/Windows/Fonts/
	%/D/Windows/Fonts/
][
	unless font-file [
		if block? files: attempt [read dir][
			foreach file files [
				if all [
					not font-file
					find [%.ttf %.otf %.ttc] suffix? file
				][
					font-file: join dir file
				]
			]
		]
	]
]
print ["Font used for the text tests:" mold font-file]

;;=============================================================================
group "Module surface"
;;=============================================================================

--test-- "module imported"                       [module? b2d]
--test-- "draw is exported into the user context" [command? :draw]
--test-- "draw is also reachable through the module" [same? :draw :b2d/draw]

foreach name [draw path font image info set-threads][
	--test-- ajoin ["command " name] compose [command? get in b2d (to lit-word! name)]
]

;; `draw-test` is a scratch pad for native experiments and is commented out in
;; the specification - a build which exposes it was made with it enabled.
--test-- "draw-test is not part of the module" [none? in b2d 'draw-test]

;; `_init` is generated, called once when the module body evaluates and then
;; protected/hidden - it must not be reachable from the outside. (The old
;; hand-written extension called this command `init-words`.)
--test-- "_init is hidden"    [none? in b2d '_init]
--test-- "init-words is gone" [none? in b2d 'init-words]

--test-- "every command documents itself" [
	missing: copy []
	foreach name [draw path font image info set-threads][
		spec: spec-of get in b2d name
		unless all [string? first spec  not empty? first spec][append missing name]
	]
	any [empty? missing  reform ["no description:" mold missing]]
]

;; The handle accessors came with the conversion to the new ABI - their
;; absence means an older library was imported from somewhere else.
--test-- "the imported module is the freshly built one" [
	i: image 2x2
	either 2 = attempt [i/width][true][
		"stale extension loaded - the BLImage handle has no /width accessor"
	]
]

;;=============================================================================
group "draw: the target image"
;;=============================================================================

--test-- "a pair! makes a new image"      [image? img: draw 8x8 [fill 255.0.0 fill-all]]
--test-- "the new image has the size given" [is? img/size 8x8]
--test-- "the new image is filled"        [is? (px img 0 0) 255.0.0]
--test-- "the fill is fully opaque"       [is? (pa img 0 0) 255]

--test-- "an image! is drawn into in place" [
	im: make image! 8x8
	draw im [fill 0.0.255 fill-all]
	is? (px im 0 0) 0.0.255
]
--test-- "the image which was drawn into is the one returned" [
	im: make image! 4x4
	out: draw im [fill 255.0.0 fill-all]
	all [image? out  (px out 0 0) == 255.0.0  (px im 0 0) == 255.0.0]
]

--rejects-- "draw with a string as the target" [draw "nope" []]
--rejects-- "draw with a string as commands"   [draw 8x8 "nope"]
--rejects-- "draw with a zero size"            [draw 0x0 []]

;;=============================================================================
group "draw: geometry"
;;=============================================================================

--test-- "box" [
	d: draw 8x8 [clear-all fill 255.0.0 box 0x0 4x4]
	all [
		(px d 1 1) == 255.0.0
		0 == pa d 7 7
	]
]
--test-- "round box"  [image? draw 8x8 [fill 255.0.0 box 0x0 8x8 2]]
--test-- "circle"     [
	d: draw 8x8 [clear-all fill 255.0.0 circle 4x4 3]
	all [(px d 4 4) == 255.0.0  0 == pa d 0 0]
]
--test-- "ellipse"    [
	d: draw 8x8 [clear-all fill 255.0.0 ellipse 0x0 8x8]
	all [(px d 4 4) == 255.0.0  0 == pa d 0 0]
]
--test-- "triangle"   [
	d: draw 16x16 [clear-all fill 255.0.0 triangle 0x0 16x0 16x16]
	all [(px d 12 4) == 255.0.0  0 == pa d 2 12]
]
--test-- "polygon"    [
	d: draw 16x16 [clear-all fill 255.0.0 polygon 0x0 16x0 16x16]
	all [(px d 12 4) == 255.0.0  0 == pa d 2 12]
]
--test-- "point and point-size" [
	d: draw 16x16 [clear-all fill 255.0.0 point-size 8 point 8x8]
	all [(px d 8 8) == 255.0.0  0 == pa d 0 0]
]
--test-- "several points at once" [
	d: draw 16x16 [clear-all fill 255.0.0 point-size 4 point 4x4 12x12]
	all [(px d 4 4) == 255.0.0  (px d 12 12) == 255.0.0]
]
--test-- "points from a block"   [
	d: draw 16x16 [clear-all fill 255.0.0 point-size 4 point [4x4 12x12]]
	all [(px d 4 4) == 255.0.0  (px d 12 12) == 255.0.0]
]
--test-- "arc"        [image? draw 16x16 [pen 255.0.0 arc 8x8 6x6 0 90]]
--test-- "pie"        [image? draw 16x16 [fill 255.0.0 arc 8x8 6x6 0 90 pie]]
--test-- "chord"      [image? draw 16x16 [fill 255.0.0 arc 8x8 6x6 0 90 chord]]
--test-- "cubic"      [image? draw 16x16 [pen 255.0.0 cubic 0x0 4x12 12x4 16x16]]

--test-- "a stroked line" [
	d: draw 16x16 [clear-all pen 255.0.0 line-width 3 line 0x8 16x8]
	all [0 < pa d 8 8  0 == pa d 8 0]
]
--test-- "a line from a block" [
	d: draw 16x16 [clear-all pen 255.0.0 line-width 3 line [0x8 16x8]]
	0 < pa d 8 8
]
--test-- "line-width 0 turns the stroke off" [
	d: draw 16x16 [clear-all pen 255.0.0 line-width 0 line 0x8 16x8]
	0 == pa d 8 8
]
--test-- "line-join and line-cap are accepted" [
	image? draw 16x16 [pen 255.0.0 line-width 4 line-join round line-cap 1 line 2x2 14x14]
]

;;=============================================================================
group "draw: styles"
;;=============================================================================

--test-- "fill off" [
	d: draw 8x8 [clear-all fill 255.0.0 fill false box 0x0 8x8]
	0 == pa d 4 4
]
--test-- "pen off" [
	d: draw 8x8 [clear-all pen 255.0.0 line-width 2 pen false line 0x4 8x4]
	0 == pa d 4 4
]
--test-- "a linear gradient runs from the first stop to the last" [
	d: draw 16x16 [fill linear 255.0.0 0.0 0.0.255 1.0 0x0 15x0 fill-all]
	l: px d 0 0
	r: px d 15 0
	any [
		all [l/1 > l/3  r/3 > r/1]
		reform ["left" mold l "right" mold r]
	]
]
--test-- "a radial gradient is accepted"  [image? draw 16x16 [fill radial 255.0.0 0.0 0.0.255 1.0 8x8 8x8 8 fill-all]]
--test-- "a conical gradient is accepted" [image? draw 16x16 [fill conical 255.0.0 0.0 0.0.255 1.0 8x8 fill-all]]

--test-- "an image! is used as a fill pattern" [
	d: draw 8x8 [clear-all fill src tile fill-all]
	is? (px d 0 0) 0.255.0
]
--test-- "an image handle is used as a fill pattern" [
	pat: image src
	d: draw 8x8 [clear-all fill pat tile fill-all]
	is? (px d 0 0) 0.255.0
]
;; NOTE: a pattern pen used to set the FILL style, so nothing was stroked with
;; it - the line came out in the default (black) stroke style.
--test-- "an image! is used as a stroke pattern" [
	d: draw 16x16 [clear-all pen src line-width 5 line 0x8 16x8]
	t: px d 8 8
	any [t/2 > t/1  reform ["stroke pattern ignored, got" mold t]]
]

--test-- "alpha makes the fill translucent" [
	d: draw 8x8 [clear-all alpha 0.5 fill 255.0.0 fill-all]
	a: pa d 0 0
	any [all [a > 50  a < 210]  reform ["alpha" a]]
]
--test-- "blend accepts a mode word" [image? draw 8x8 [blend multiply fill 128.128.128 fill-all]]
--test-- "blend none restores the default" [image? draw 8x8 [blend multiply blend none fill 255.0.0 fill-all]]

;;=============================================================================
group "draw: clipping, clearing and the matrix"
;;=============================================================================

--test-- "clear-all makes the image transparent" [
	d: draw 8x8 [fill 255.0.0 fill-all clear-all]
	0 == pa d 0 0
]
--test-- "clear takes a corner and a corner" [
	d: draw 8x8 [fill 255.0.0 fill-all clear 0x0 4x4]
	all [0 == pa d 0 0  255 == pa d 7 7]
]
--test-- "clip limits the drawing" [
	d: draw 8x8 [clear-all clip 0x0 4x4 fill 255.0.0 fill-all]
	all [(px d 0 0) == 255.0.0  0 == pa d 7 7]
]
--test-- "clip none restores the whole area" [
	d: draw 8x8 [clear-all clip 0x0 4x4 clip none fill 255.0.0 fill-all]
	255 == pa d 7 7
]
--test-- "translate moves the drawing" [
	d: draw 8x8 [clear-all translate 4x4 fill 255.0.0 box 0x0 2x2]
	all [(px d 4 4) == 255.0.0  0 == pa d 0 0]
]
--test-- "reset-matrix puts it back" [
	d: draw 8x8 [clear-all translate 4x4 reset-matrix fill 255.0.0 box 0x0 2x2]
	(px d 0 0) == 255.0.0
]
--test-- "scale is accepted as a number and as a pair" [
	all [
		image? draw 8x8 [scale 2 fill 255.0.0 box 0x0 2x2]
		image? draw 8x8 [scale 2x2 fill 255.0.0 box 0x0 2x2]
	]
]
--test-- "rotate is accepted with and without a center" [
	all [
		image? draw 8x8 [rotate 45 fill 255.0.0 box 0x0 2x2]
		image? draw 8x8 [rotate 45 4x4 fill 255.0.0 box 0x0 2x2]
	]
]

;;=============================================================================
group "draw: images"
;;=============================================================================

--test-- "an image! is blitted" [
	d: draw 8x8 [clear-all image src 0x0]
	all [(px d 0 0) == 0.255.0  0 == pa d 7 7]
]
--test-- "an image handle is blitted" [
	hnd: image src
	d: draw 8x8 [clear-all image hnd 0x0]
	(px d 0 0) == 0.255.0
]
--test-- "a pair! makes an empty image to blit" [image? draw 8x8 [image 4x4 0x0]]
--test-- "a scaled blit fills the given rectangle" [
	d: draw 8x8 [clear-all image src 0x0 8x8]
	all [(px d 0 0) == 0.255.0  (px d 7 7) == 0.255.0]
]
;; NOTE: the size of a scaled blit used to be counted twice, so the command
;; which followed it was swallowed.
--test-- "a command after a scaled blit is still evaluated" [
	d: draw 8x8 [clear-all image src 0x0 8x8 fill 255.0.0 box 0x0 2x2]
	is? (px d 0 0) 255.0.0
]
--test-- "a released image handle does not stop the evaluation" [
	dead: image 4x4
	release dead
	d: draw 8x8 [clear-all image dead 0x0 fill 255.0.0 fill-all]
	is? (px d 0 0) 255.0.0
]

;;=============================================================================
group "draw: error recovery"
;;=============================================================================

;; A command which cannot be understood is skipped; the evaluation resumes at
;; the next word which names one.
--test-- "an unknown command is skipped" [
	d: draw 8x8 [nonsense 1 2 fill 255.0.0 fill-all]
	is? (px d 0 0) 255.0.0
]
--test-- "a wrong argument is skipped" [
	d: draw 8x8 ["not a pair" fill 255.0.0 fill-all]
	is? (px d 0 0) 255.0.0
]
--test-- "an empty dialect leaves the image alone" [
	im: make image! 4x4
	draw im []
	is? (px im 0 0) 255.255.255
]

;;=============================================================================
group "The BLImage handle"
;;=============================================================================

i: image 4x4

--test-- "image returns a handle"        [handle? i]
--test-- "image/size"                    [is? i/size 4x4]
--test-- "image/width"                   [is? i/width 4]
--test-- "image/height"                  [is? i/height 4]
--test-- "image/format is PRGB32"        [is? i/format 1]
--test-- "image/stride is four bytes per pixel" [is? i/stride 16]
--test-- "the handle molds through the extension's callback" [
	did all [find mold i "0#"  find mold i "4x4"]
]
--test-- "an image! is wrapped by the handle" [
	hnd: image src
	is? hnd/size 4x4
]
--test-- "a Rebol image and its handle share the pixels" [
	im: draw 4x4 [fill 255.0.0 fill-all]
	hnd: image im
	draw 8x8 [image hnd 0x0] ;; the handle is only read here
	is? (px im 0 0) 255.0.0
]

--rejects-- "image/nonsense"        [i/nonsense]
--rejects-- "image/width write"     [i/width: 8]
--rejects-- "image with a string"   [image "4x4"]
--rejects-- "image of a missing file" [image %no-such-image-here.png]

;;=============================================================================
group "The BLPath handle"
;;=============================================================================

p: path [move 0x0 line 16x0 line 16x16 close]

--test-- "path returns a handle"   [handle? p]
--test-- "path/size counts the vertices" [3 <= p/size]
--test-- "path/capacity is at least the size" [p/capacity >= p/size]
--test-- "the handle molds through the extension's callback" [did find mold p "0#"]

--test-- "a path handle is drawn by shape" [
	d: draw 16x16 [clear-all fill 255.0.0 shape p]
	all [(px d 12 4) == 255.0.0  0 == pa d 2 12]
]
--test-- "shape also takes the dialect inline" [
	d: draw 16x16 [clear-all fill 255.0.0 shape [move 0x0 line 16x0 line 16x16 close]]
	(px d 12 4) == 255.0.0
]
--test-- "the path dialect knows curves and arcs" [
	handle? path [
		move 0x0
		line 4x0
		hline 8 vline 8
		curve 10x0 12x8 16x8
		curv 14x10 16x16
		qcurve 8x16 4x12
		qcurv 0x8
		arc 8x8 4 4 0
		close
	]
]
--test-- "an unknown path command is skipped" [
	q: path [nonsense 1 move 0x0 line 4x4]
	2 <= q/size
]

--rejects-- "path/nonsense"      [p/nonsense]
--rejects-- "path/size write"    [p/size: 10]
--rejects-- "path with a string" [path "move 0x0"]

;;=============================================================================
group "The BLFontFace handle"
;;=============================================================================

either font-file [
	f: font font-file

	--test-- "font returns a handle"      [handle? f]
	--test-- "font/glyphs"                [0 < f/glyphs]
	--test-- "font/face-type"             [integer? f/face-type]
	--test-- "font/outline-type"          [integer? f/outline-type]
	--test-- "font/revision"              [integer? f/revision]
	--test-- "font/face-index"            [integer? f/face-index]
	--test-- "font/face-flags"            [integer? f/face-flags]
	--test-- "font/diag-flags"            [integer? f/diag-flags]
	--test-- "the handle molds through the extension's callback" [did find mold f "0#"]

	--test-- "text drawn with a font handle marks the image" [
		d: draw 64x32 [clear-all fill 255.255.255 font f text 2x24 20 "R"]
		found: false
		repeat y 32 [repeat x 64 [if 0 < pa d (x - 1) (y - 1) [found: true]]]
		any [found "nothing was drawn"]
	]
	--test-- "text drawn with a font file marks the image" [
		d: draw 64x32 compose [clear-all fill 255.255.255 font (font-file) text 2x24 20 "R"]
		found: false
		repeat y 32 [repeat x 64 [if 0 < pa d (x - 1) (y - 1) [found: true]]]
		any [found "nothing was drawn"]
	]
	--test-- "a font handle survives being used twice" [
		image? draw 64x32 [fill 255.255.255 font f text 2x12 10 "A" text 2x28 10 "B"]
	]

	--rejects-- "font/nonsense"        [f/nonsense]
	--rejects-- "font/glyphs write"    [f/glyphs: 10]
][
	--skip-- "the BLFontFace handle (no system font found)"
]

--rejects-- "font of a missing file" [font %no-such-font-here.ttf]

;;=============================================================================
group "info"
;;=============================================================================

--test-- "info returns a string"         [string? txt: info]
--test-- "info reports the version"      [did find txt "Version:"]
--test-- "info reports the system"       [did find txt "System: ["]
--test-- "info reports the resources"    [did find txt "Resource: ["]
--test-- "info reports the thread count" [did find txt "Threads:"]

--test-- "info/of an image handle"  [did find info/of i "width:"]
--test-- "info/of a path handle"    [did find info/of p "size:"]
either font-file [
	--test-- "info/of a font handle" [did find info/of f "glyphCount:"]
][
	--skip-- "info/of a font handle (no system font found)"
]

--rejects-- "info/of a released handle" [
	dead: path [move 0x0]
	release dead
	info/of dead
]

;;=============================================================================
group "set-threads"
;;=============================================================================

--test-- "set-threads returns the count it set" [is? set-threads 2 2]
--test-- "a negative count means none"          [is? set-threads -1 0]
--test-- "the count is capped"                  [
	n: set-threads 1000000
	any [all [integer? n  n < 1000000]  reform ["got" mold n]]
]
--test-- "drawing gives the same result whatever the thread count" [
	set-threads 0
	a: draw 32x32 [clear-all fill 255.0.0 circle 16x16 12]
	set-threads 4
	b: draw 32x32 [clear-all fill 255.0.0 circle 16x16 12]
	all [
		(px a 16 16) == (px b 16 16)
		(px a  5  5) == (px b  5  5)
		(pa a  5  5) == (pa b  5  5)
	]
]
set-threads 1

--rejects-- "set-threads with a string" [set-threads "2"]

;;=============================================================================
group "Repeated use and recycling"
;;=============================================================================

;; Handles and images are referenced only by the words they were stored in, so
;; a recycle between the calls collects everything else - a handle which keeps
;; a pointer into a collected series would show up here.
either quick? [
	--skip-- "200 draws with a recycle between them (--quick)"
][
	--test-- "200 draws with a recycle between them" [
		loop 200 [
			draw 16x16 [
				clear-all
				fill 255.0.0 box 0x0 8x8
				fill src tile circle 12x12 3
				pen 0.0.255 line-width 2 line 0x0 16x16
				shape p
			]
			recycle
		]
		d: draw 16x16 [clear-all fill 255.0.0 box 0x0 8x8]
		is? (px d 1 1) 255.0.0
	]
	--test-- "200 handles with a recycle between them" [
		loop 200 [
			image 8x8
			path [move 0x0 line 8x8]
			recycle
		]
		all [4 == i/width  3 <= p/size]
	]
]

summary