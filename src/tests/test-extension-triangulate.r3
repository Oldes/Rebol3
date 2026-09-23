Rebol [
	Title:   "Rebol/Triangulate extension CI test"
	Needs:   3.22.5
	Purpose: {
		Exercises the extension's whole surface - the command's inputs and
		outputs, the fields it fills in, and every way it can refuse a call.

		The result vectors are pinned to values which were produced by the
		Triangle library itself, because a wrong offset or element size is
		otherwise silent: the call still succeeds, it just returns numbers
		which look plausible.

		Usage:
			r3 ci-test.r3            ;; full run
			r3 ci-test.r3 --quick    ;; skips the recycle loop and the drawing

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

;; NOTE: the module and its only command share the name `triangulate`, so the
;; module is bound to another word here - the plain `triangulate` must stay
;; the exported command, which is what every test calls.
ext: import 'triangulate

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
quick?: did find args "--quick" ;; skip the recycle loop and the drawing scripts

;; A fresh result object declaring every possible output field.
new-out: does [object [
	points:          none
	attributes:      none
	markers:         none
	segments:        none
	segment-markers: none
	edges:           none
	triangles:       none
	v-points:        none
	v-attributes:    none
	v-edges:         none
	v-norms:         none
]]

;; The unit square with a point in the middle - small enough that every result
;; below can be written out in full.
pts: #(f64! [
	 0.0   0.0
	10.0   0.0
	10.0  10.0
	 0.0  10.0
	 2.5   2.5
])
quiet-inp: object [points: pts]

;; The same input, with Triangle's own report turned on.
inp: object [points: pts  report: true]

;; The results of the fixture above, as Triangle produces them.
exp-points:    #(f64! [0.0 0.0 10.0 0.0 10.0 10.0 0.0 10.0 2.5 2.5])
exp-markers:   #(i32! [1 1 1 1 0])
exp-segments:  #(i32! [1 0 2 1 3 2 0 3])
exp-seg-marks: #(i32! [1 1 1 1])
exp-edges:     #(i32! [3 0 0 4 4 3 4 1 1 2 2 4 0 1 2 3])
exp-triangles: #(i32! [3 0 4 4 1 2 1 4 0 4 2 3])
exp-v-points:  #(f64! [-2.5 5.0 7.5 5.0 5.0 -2.5 5.0 7.5])
exp-v-edges:   #(i32! [0 -1 0 2 0 3 1 2 1 -1 1 3 2 -1 3 -1])
;; Only the four rays (the edges ending with -1) have a direction; Triangle
;; leaves the norms of the finite edges at zero.
exp-v-norms:   #(f64! [
	-10.0 0.0
	  0.0 0.0
	  0.0 0.0
	  0.0 0.0
	 10.0 0.0
	  0.0 0.0
	  0.0 -10.0
	  0.0 10.0
])

;;=============================================================================
group "Module surface"
;;=============================================================================

--test-- "module imported"                         [module? ext]
--test-- "triangulate is exported into the user context" [command? :triangulate]
--test-- "the command is also reachable through the module" [command? :ext/triangulate]

;; The generated `_init` command resolves the field names once and is hidden
;; afterwards, so nothing in the module's interface should mention it.
--test-- "the generated _init command is hidden" [none? in ext '_init]

;; A pre-conversion library returned a copy of the edge list in `triangles`,
;; so the real corner list identifies the freshly built one.
--test-- "the imported module is the freshly built one" [
	out: new-out
	triangulate quiet-inp out
	either out/triangles == exp-triangles [true][
		reform ["stale extension loaded from" mold select ext 'lib-file]
	]
]

;;=============================================================================
group "Basic triangulation"
;;=============================================================================

;; The `report` field prints the triangulation and the Voronoi diagram.
out: new-out
--test-- "a reporting call succeeds" [triangulate inp out  true]

--test-- "points"          [is? out/points          exp-points   ]
--test-- "attributes"      [is? out/attributes      none         ]
--test-- "markers"         [is? out/markers         exp-markers  ]
--test-- "segments"        [is? out/segments        exp-segments ]
--test-- "segment-markers" [is? out/segment-markers exp-seg-marks]
--test-- "edges"           [is? out/edges           exp-edges    ]
;; Four triangles of three corners each - one for every edge of the square,
;; all of them meeting in the point in the middle.
--test-- "triangles"       [is? out/triangles       exp-triangles]
--test-- "v-points"        [is? out/v-points        exp-v-points ]
--test-- "v-attributes"    [is? out/v-attributes    none         ]
--test-- "v-edges"         [is? out/v-edges         exp-v-edges  ]
--test-- "v-norms"         [is? out/v-norms         exp-v-norms  ]

;;=============================================================================
group "Degenerate inputs"
;;=============================================================================

;; Triangle keeps the vertices of a collinear set, but there is no mesh to
;; build, so every other field is left untouched.
out: new-out
--test-- "collinear points are accepted" [
	triangulate object [points: #(f64! [0.0 0.0 1.0 1.0 2.0 2.0 3.0 3.0])] out
	true
]
--test-- "collinear points are kept"     [is? out/points  #(f64! [0.0 0.0 1.0 1.0 2.0 2.0 3.0 3.0])]
--test-- "collinear markers are zero"    [is? out/markers #(i32! [0 0 0 0])]
--test-- "collinear points build no mesh" [
	all [
		none? out/segments
		none? out/edges
		none? out/triangles
		none? out/v-points
		none? out/v-edges
	]
]

;; A duplicate stays in `points` and `markers`, but takes no part in the mesh.
out: new-out
--test-- "a duplicated point is accepted" [
	triangulate object [points: #(f64! [0.0 0.0 10.0 0.0 10.0 10.0 0.0 10.0 2.5 2.5 2.5 2.5])] out
	true
]
--test-- "a duplicated point is kept in points"  [is? out/points  #(f64! [0.0 0.0 10.0 0.0 10.0 10.0 0.0 10.0 2.5 2.5 2.5 2.5])]
--test-- "a duplicated point is kept in markers" [is? out/markers #(i32! [1 1 1 1 0 0])]
--test-- "a duplicated point is left out of the mesh" [
	all [out/edges == exp-edges  out/triangles == exp-triangles]
]

;; The smallest input which triangulates.
out: new-out
--test-- "three points are enough" [
	triangulate object [points: #(f64! [0.0 0.0 10.0 0.0 5.0 8.0])] out
	true
]
--test-- "three points make one triangle" [is? out/triangles #(i32! [0 1 2])]
--test-- "three points make three edges"  [is? out/edges     #(i32! [0 1 1 2 2 0])]
--test-- "three points make one Voronoi point" [is? out/v-points #(f64! [5.0 2.4375])]

;;=============================================================================
group "Vectors which are not at their head"
;;=============================================================================

;; The input vectors are used where they are, so a skipped vector must
;; triangulate only the part which is left of it.
out: new-out
--test-- "a skipped vector is accepted" [
	triangulate object [points: skip #(f64! [99.0 99.0 0.0 0.0 10.0 0.0 5.0 8.0]) 2] out
	true
]
--test-- "a skipped vector drops the values before it" [is? out/points #(f64! [0.0 0.0 10.0 0.0 5.0 8.0])]
--test-- "a skipped vector triangulates the rest"      [is? out/triangles #(i32! [0 1 2])]

;;=============================================================================
group "Selection of the result fields"
;;=============================================================================

;; Only the fields which the result object already has are filled in.
partial: object [edges: none]
--test-- "a result object may declare a single field" [
	triangulate quiet-inp partial
	is? partial/edges exp-edges
]

--test-- "a result object without any known field is accepted" [
	triangulate quiet-inp object []
	true
]

;;=============================================================================
group "Optional input fields"
;;=============================================================================

out: new-out
--test-- "attributes, markers and regions are accepted together" [
	triangulate object [
		points:     pts
		attributes: #(f64! [100.0 2.0 3.0 4.0 5.0])
		markers:    #(i32! [2 1 1 1 1])
		regions:    #(f64! [5.0 5.0 10.0 1.0])
	] out
	true
]
--test-- "attributes are carried through"     [is? out/attributes #(f64! [100.0 2.0 3.0 4.0 5.0])]
--test-- "the input markers are kept"         [is? out/markers    #(i32! [2 1 1 1 1])]
--test-- "one attribute per Voronoi point"    [is? (length? out/v-attributes) 4]

;; Several attributes per point are allowed, as long as every point has the
;; same number of them.
out: new-out
--test-- "two attributes per point are accepted" [
	triangulate object [
		points:     pts
		attributes: #(f64! [1.0 10.0 2.0 20.0 3.0 30.0 4.0 40.0 5.0 50.0])
	] out
	true
]
--test-- "two attributes per point are carried through" [is? out/attributes #(f64! [1.0 10.0 2.0 20.0 3.0 30.0 4.0 40.0 5.0 50.0])]
--test-- "two attributes per Voronoi point"             [is? (length? out/v-attributes) 8]

;;=============================================================================
group "Invalid input"
;;=============================================================================

;; Each of these must come back as an error instead of a crash or a result
;; built out of misread memory.
--rejects-- "an input object without points"  [triangulate object [] out]
--rejects-- "points set to none"              [triangulate object [points: none] out]
--rejects-- "points as a block"               [triangulate object [points: [0.0 0.0 1.0 0.0 1.0 1.0]] out]
--rejects-- "points as 32bit integers"        [triangulate object [points: #(i32! [0 0 1 0 1 1])] out]
--rejects-- "only two points"                 [triangulate object [points: #(f64! [0.0 0.0 1.0 0.0])] out]
--rejects-- "an odd number of coordinates"    [triangulate object [points: #(f64! [0.0 0.0 1.0 0.0 1.0])] out]
--rejects-- "markers as decimals"             [triangulate object [points: pts markers: #(f64! [1.0 1.0 1.0 1.0 1.0])] out]
--rejects-- "markers as 64bit integers"       [triangulate object [points: pts markers: #(i64! [1 1 1 1 1])] out]
--rejects-- "one marker short"                [triangulate object [points: pts markers: #(i32! [1 1 1 1])] out]
--rejects-- "attributes as integers"          [triangulate object [points: pts attributes: #(i32! [1 2 3 4 5])] out]
--rejects-- "attributes not divisible by the number of points" [triangulate object [points: pts attributes: #(f64! [1.0 2.0 3.0])] out]
--rejects-- "a region of three values"        [triangulate object [points: pts regions: #(f64! [5.0 5.0 10.0])] out]

out: new-out
try [triangulate object [points: #(f64! [0.0 0.0])] out]
--test-- "a refused call leaves the result object alone" [
	all [none? out/points  none? out/edges]
]

;;=============================================================================
group "Invariants on a random point set"
;;=============================================================================

random/seed 2021
values: 100
rnd-points: make vector! compose [decimal! 64 (values)]
repeat i values [rnd-points/:i: random 1000.0]

out: new-out
--test-- "50 random points triangulate" [triangulate object [points: rnd-points] out  true]

;; A triangulation of n points, h of them on the hull, has exactly 2n-2-h
;; triangles and 3n-3-h edges - whatever the points are. The hull is what the
;; `segments` output holds, so both counts come out of the result itself.
--test-- "the number of triangles follows Euler's formula" [
	n: to integer! (length? out/points)   / 2
	h: to integer! (length? out/segments) / 2
	is? (length? out/triangles) (3 * ((2 * n) - 2 - h))
]
--test-- "the number of edges follows Euler's formula" [
	is? (length? out/edges) (2 * ((3 * n) - 3 - h))
]

--test-- "every triangle corner addresses one of the points" [
	bad: none
	repeat i length? out/triangles [
		if all [none? bad  any [out/triangles/:i < 0  out/triangles/:i >= n]][bad: i]
	]
	any [none? bad  reform ["out of range at" bad "=>" out/triangles/:bad]]
]

--test-- "every edge addresses one of the points" [
	bad: none
	repeat i length? out/edges [
		if all [none? bad  any [out/edges/:i < 0  out/edges/:i >= n]][bad: i]
	]
	any [none? bad  reform ["out of range at" bad "=>" out/edges/:bad]]
]

;; Only the rays of the Voronoi diagram (the edges ending with -1) carry a
;; direction; the norms of the finite edges must stay at zero.
;; NOTE: the flag is deliberately not called `zero?` - a set-word here would
;; rebind the native for the rest of the script.
--test-- "only the Voronoi rays carry a direction" [
	bad: none
	repeat i to integer! (length? out/v-edges) / 2 [
		ray?:     -1 == out/v-edges/(2 * i)
		no-norm?: all [zero? out/v-norms/(2 * i - 1)  zero? out/v-norms/(2 * i)]
		if all [none? bad  ray? = no-norm?][bad: i]
	]
	any [none? bad  reform ["wrong norm on Voronoi edge" bad]]
]

;;=============================================================================
group "Repeated use and recycling"
;;=============================================================================

;; A result vector is referenced only by the field it was stored in, so a
;; recycle between the calls would collect it if that reference was missing.
either quick? [
	--skip-- "200 calls with a recycle between them (--quick)"
][
	out: new-out
	--test-- "200 calls with a recycle between them" [
		loop 200 [
			triangulate quiet-inp out
			recycle
		]
		all [out/edges == exp-edges  out/triangles == exp-triangles]
	]
]


summary
