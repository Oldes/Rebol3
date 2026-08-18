REBOL [
	Title:   "Rebol/Matrix Extension"
	Name:    matrix
	Version: 1.0.0
	Needs:   3.22.5      ;; drives both `Needs:` and MIN_REBOL_VER/REV/UPD
	Date:    15-Aug-2026
	Author:  @Oldes
	License: MIT
	Purpose: {Matrix operations over shape-aware vector! values}
]

;; ---------------------------------------------------------------------------
;; C-side configuration
c-prefix:  MATRIX                      ;; CMD_MATRIX_* / Matrix_Command[] / Matrix_RX_Call
;c-include: ["rebol-extension.h" "reb-ext-common.h"]

;; ---------------------------------------------------------------------------
;; Command specifications
;; Shape is a pair! (cols x rows); rows <= 1 means a plain unshaped vector.
commands: [
	transpose: [
		"Transposes a matrix, returns a new vector"
		m [vector!]
	]
	identity: [
		"Turns a square matrix into an identity matrix (in place)"
		m [vector!] "Must be square; element type and order taken from the input"
	]
	trace: [
		"Returns the sum of the diagonal elements"
		m [vector!] "Must be square"
	]
	diagonal: [
		"Returns the diagonal as a plain (unshaped) vector"
		m [vector!]
	]
	swap-rows: [
		"Swaps two rows in place, returns the same value"
		m [vector!]
		a [integer!] "1-based row index"
		b [integer!] "1-based row index"
	]
	rotate: [
		"Returns a new matrix rotated by 90 degrees"
		m [vector!]
		/left  "Counter-clockwise instead of clockwise"
		/twice "180 degrees"
	]
	kronecker: [
		"Kronecker product of two matrices"
		a [vector!]
		b [vector!]
	]
	matmul: [
		"Matrix multiplication; a's columns must match b's rows"
		a [vector!]
		b [vector!]
	]
	determinant: [
		"Determinant of a square matrix"
		m [vector!] "Must be square"
	]
	invert: [
		"Inverse of a square matrix, as a new float64! matrix"
		m [vector!] "Must be square and non-singular"
	]
	solve: [
		"Solves A * x = b, returning x as a float64! column"
		a [vector!] "Square coefficient matrix"
		b [vector!] "Right-hand side, as a single-column matrix"
	]
]

;; ---------------------------------------------------------------------------
;; Mezzanine code appended to the module body
mezzanine: [
	;; ---- internal helpers ---------------------------------------------

	as-float: func [
		"Returns a float64! copy of a matrix, or a plain copy if already float"
		m [vector!]
		/local out
	][
		if find [float32! float64!] m/element-type [return copy m]
		out: make vector! reduce ['float64! m/shape]
		repeat n length? m [poke out n pick m n]
		out
	]

	square?: func [
		"Returns TRUE if the matrix has as many rows as columns"
		m [vector!]
		/local s
	][
		s: m/shape
		s/1 = s/2
	]

	symmetric?: func [
		m [vector!]
		/local n
	][
		unless square? m [return false]
		n: second m/shape
		repeat r n [
			repeat c n [
				if (pick m as-pair c r) <> (pick m as-pair r c) [return false]
			]
		]
		true
	]

	;; ---- construction --------------------------------------------------

	augment: func [
		"Returns a new matrix with B's columns appended to A's"
		a [vector!] b [vector!]
		/local sa sb rows out r c
	][
		if a/element-type <> b/element-type [cause-error 'script 'invalid-arg reduce [b]]
		sa: a/shape  sb: b/shape
		if sa/2 <> sb/2 [cause-error 'script 'invalid-arg reduce [b]]
		rows: sa/2
		out: make vector! reduce [a/element-type as-pair sa/1 + sb/1 rows]
		repeat r rows [
			repeat c sa/1 [poke out as-pair c r          pick a as-pair c r]
			repeat c sb/1 [poke out as-pair (sa/1 + c) r pick b as-pair c r]
		]
		out
	]

	row: func [
		"Returns row N as a new single-row matrix"
		m [vector!] n [integer!]
		/local s
	][
		s: m/shape
		if any [n < 1  n > s/2] [cause-error 'script 'out-of-range n]
		copy/part skip m (n - 1) * s/1  s/1
	]

	col: func [
		"Returns column N as a new single-column matrix"
		m [vector!] n [integer!]
		/local s out
	][
		s: m/shape
		if any [n < 1  n > s/1] [cause-error 'script 'out-of-range n]
		out: make vector! reduce [m/element-type as-pair 1 s/2]
		repeat r s/2 [out/:r: m/(as-pair n r)]
		out
	]

	;; ---- elimination ---------------------------------------------------

	rref: func [
		"Reduces a float matrix to reduced row echelon form (modifies)"
		m [vector!]
		/local s cols rows lead r i best pivot v c eps mag
	][
		unless find [float32! float64!] m/element-type [
			;; integer elements would truncate on every POKE
			cause-error 'script 'invalid-arg m
		]
		s: m/shape
		cols: s/1  rows: s/2

		;; Elimination leaves rounding noise where an exact zero was expected,
		;; so "is this column empty" has to be a relative test, not `zero?`.
		;; An all-zero matrix gives eps = 0 and is caught by the >= below.
		mag: 0.0
		repeat i length? m [mag: max mag abs pick m i]
		eps: mag * 1E-12

		lead: 1   r: 1
		while [all [r <= rows  lead <= cols]][
			;; partial pivoting: largest magnitude in this column
			best: r  i: r
			while [i <= rows][
				if (abs pick m as-pair lead i) > (abs pick m as-pair lead best) [best: i]
				i: i + 1
			]
			either eps >= abs pick m as-pair lead best [
				lead: lead + 1
			][
				if best <> r [swap-rows m best r]
				pivot: pick m as-pair lead r
				repeat c cols [poke m as-pair c r (pick m as-pair c r) / pivot]
				repeat i rows [
					if i <> r [
						v: pick m as-pair lead i
						unless zero? v [
							repeat c cols [
								poke m as-pair c i
									(pick m as-pair c i) - (v * pick m as-pair c r)
							]
						]
					]
				]
				r: r + 1
				lead: lead + 1
			]
		]
		m
	]
]