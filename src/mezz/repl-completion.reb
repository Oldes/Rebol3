Rebol [
	Title: "Rebol Console - Completion"
	Home:  https://github.com/Oldes/Rebol-Console
	Type:  module
	Name:  rebol-completion
	exports: [completion!]
	Note:  {This source must not use any `print` calls!}
]

completion!: context [
	;- public ---
	matches:      []  ;; current matches (using index as a position!)
	count:        0   ;; total number of matches
	status-line:  ""  ;; holds possible matches as a trimed line

	reset: does [
		count: 0 suffix: last-input: kind: _
		clear head matches
		clear status-line
	]

	accept: func [/local pos] [
		;; Move accepted match to tail so it's served first next time.
		if all [
			kind = 'word
			pos: find/last words matches/1
		][
			append words take pos
		]
		reset
	]
	
	complete: func [
		;; Input completion function.
		input     [string! ] ;; Current line to be completed
		/local files dir n
	][
		if last-input = input [exit]
		count: 0 suffix: _
		
		partial: any [
			find/last/tail input SP
			input
		]
		
		matches: clear head matches
		case [
			partial/1 == #"%" [ ; File completion
				kind: 'file
				partial: next partial
				either empty? partial [
					files: read %.
					forall files [
						append matches as string! enhex files/1
					]
				][
					unless dir? dir: as file! partial [ dir: first split-path dir ]
					unless files: attempt [read dir][ exit ]
					foreach file files [
						file: dir/:file
						if apply :parse [
							file [partial to end]
							system/platform != 'Windows ;; Case-sensitive on Posix!
						][
							append matches as string! enhex file
						]
					]
				]
			]
			find partial #"/" [ ; Path completion
				kind: 'path
				append matches any [
					scan-context system/contexts/sys
					scan-context system/contexts/lib
					scan-context user-context
					[]
				]
			]
			not empty? partial [ ; Word completion
				kind: 'word
				n: length? words
				if lib-size < length? lib-context [
					foreach word reverse skip words-of lib-context lib-size [
						append words form word
					]
					lib-size: length? lib-context
				]
				if user-size < length? user-context [
					foreach word reverse skip words-of user-context user-size [
						append words form word
					]
					user-size: length? user-context
				]
				if n < length? words [ words: unique words ]

				;; Collect from tail (new words will be served first)
				n: length? words
				while [n > 0] [
					if parse words/:n [ partial to end ][
						append matches words/:n
					]
					-- n
				]
			]
			'else [last-input: kind: none exit]
		]
		
		unless zero? count: length? matches [last-input: input]
		matches: tail matches ;; starting with position at tail (so first match will be from the head)
	]

	get-match: func[
		back? [logic!]
		/local mark start-mark index index-found len mlen match-visible term-width
	][
		if zero? count [return ""]
		;; rotate left/right
		either back? [
			matches: back either head? matches [tail matches][matches]
		][
			++ matches
			if tail? matches [matches: head matches]
		]
		;; prepare matches with highlighted current match
		term-width: query system/ports/input 'window-cols
		clear status-line
		index-found: index? matches 
		index: 1 len: 0 match-visible: _
		foreach match head matches [
			if kind = 'path [
				match: any [find/last match #"/" match]
			]
			mlen: match/width
			if (len + mlen + 1) >= term-width [
				if match-visible [ break ]
				clear status-line
				len: 0
			]
			append status-line ajoin either index == index-found [
				match-visible: true
				["^[[7m" match "^[[27m "]
			][	[match SP]]
			len: len + mlen + 1
			++ index
		]
		any [
			suffix: find/match/tail matches/1 partial
			all [empty? partial suffix: matches/1]
			;print ["^/partial:" mold partial matches/1]
		]
	]

	;- private --
	partial:    _   ;; the partial word being completed (the fragment after the last space)
	suffix:     _   ;; the currently inserted completion suffix (the part appended after partial)
	last-input: _   ;; used to detect whether the input has changed since the last TAB press
	kind:       _   ;; the completion type: word / path / file
	words: copy []  ;; collected words for possible completion
	lib-size:   0   ;; number of collected words from the lib context
	user-size:  0   ;; number of collected words from the user context
	lib-context: system/contexts/lib
	user-context: context []

	;; Object/function completion support

	collect-refs: function [fn [any-function!]][
		parse spec-of :fn [ collect any [to refinement! set x: skip keep (form x)] ]
	]

	filter-matches: function [
		"From block of strings, return only those matching pattern"
		block   [block!]
		pattern [string!]
	][
		remove-each value block [ not find/match value pattern ]
	]

	scan-context: function/extern [
		ctx [object!]
	][
		path: split partial #"/"
		foreach [key val] ctx [
			switch type? :val [
				#(native!) #(action!) #(function!) #(closure!) [
					if equal? path/1 form key [
						matches: either empty? last path [ ; part is `word/` -> ["word" ""]
							collect-refs :val
						][
							; possible optimization:
							; if refinement is already present, do not offer it
							filter-matches collect-refs :val last path
						]
					]
				]
				#(object!) #(module!) #(error!) #(port!) #(block!) [
					if equal? path/1 form key [
						matches: case [
							; top level object
							all [ empty? last path 2 = length? path ][
								form-all words-of :val
							]
							; subobject
							empty? last path [
								take/last path
								result: get to path! load path
								case [
									any-object? result [ form-all words-of result ]
								;	block? result [ rejoin ["1 - " length? result ] ]
									'else ["???"]
								]
							]
							'else [
								either attempt [ get to path! load path ][
									; fully resolved path, nothing to add
									[]
								][
									; partial word from subobject
									partial2: take/last path
									result: either single? path [
										form-all words-of get load path/1
									][
										form-all words-of get to path! load path
									]
									filter-matches result partial2
								]
							]
						]
					]
				]
			]
		]
		either block? matches [
			prefix: combine/with path #"/"
			unless equal? #"/" last prefix [append prefix #"/"]
			forall matches [matches/1: join prefix matches/1]
			head matches
		][ none ]
	][	partial ]
]