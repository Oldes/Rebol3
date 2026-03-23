Rebol [
	title: "Custom console test"
	needs: 3.12.0
	purpose: {
		Console port in an async (char based) mode.
		Allows console input while having other async devices running.
	}
	issues: {
		* Using `wait` function inside another wait (like in this console) has strange results.
		* When using paste in this console, it processes all input as key presses, which is slow.
		* Does not allow changing the cursor position.
	}
]

my-console: context [
	port: system/ports/input
	port/data: make string! 32
	prompt: "## "
	port/awake: function/with [event /local res][
		;probe event
		;probe event/offset

		;; Using a buffer to print all output in one pass to avoid flickering.
		clear buffer
		switch event/type [
			key [
				;; debug:
				; print ["^[[G^[[K" mold event/key event/code]
				switch/default event/key [
					#"^~"
					#"^H" [
						take/last event/port/data
					]
					#"^M" [
						prin LF
						unless empty? event/port/data [
							set/any 'res try/all [do event/port/data]
							clear event/port/data
							unless unset? :res [
								emit as-green "== "
								emit mold res
								print buffer
								clear buffer
							]
						]
					]
					#"^-" [ ; Tab Completion
						last-part: any [
							find/last/tail event/port/data SP
							event/port/data
						]
						either last-part/1 == #"%" [ ; File completion
							last-part: as file! next last-part
							path-parts: split-path last-part
							files: sort read path-parts/1
							matching-part: none
							either perfect-match: find files last-part [
								append event/port/data SP
							][
								best-matches: clear []
								foreach file files [
									if parse file [last-part to end][
										append best-matches as string! file
									]
								]
								either single? best-matches [
									missing-part: skip best-matches/1 length? last-part
									append event/port/data join missing-part SP
								][
									print ["^[[G^[[K" mold best-matches]
									min-length: length? best-matches/1
									foreach match next best-matches [
										min-length: min min-length length? match
									]
									if match-count: catch [
										repeat char-count min-length [
											char: best-matches/1/:char-count
											foreach word best-matches [
												if char != word/:char-count [
													throw char-count - 1
												]
											]
										]
									][
										matching-part: skip copy/part best-matches/1 match-count length? last-part
									]
								]
							]
							if matching-part [
								append event/port/data matching-part
							]
						] [ ; Word completion
							all-words: sort union words-of system/contexts/lib words-of system/contexts/user

							written-text: copy event/port/data

							either perfect-match: find all-words to-word written-text [
								append event/port/data SP
							][
								best-matches: clear []
								foreach word all-words [
									if parse to-string word [ written-text to end ] [
										append best-matches word
									]
								]
								either single? best-matches [
									missing-part: skip to-string best-matches/1 length? written-text
									append event/port/data join missing-part SP
								] [
									print ["^[[G^[[K" mold best-matches]
								]
							]
						]
					]
				][
					append event/port/data event/key
				]
				emit "^[[G^[[K"
				emit as-red prompt
				emit event/port/data
			]
			control	[
				if find [shift control alt] event/key [ return false ]
				emit "^[[G^[[K"
				emit reform ["control:" event/key event/flags LF]
				emit as-red prompt
				emit event/port/data
				if event/key = 'escape [
					emit "[ESC]^/"
					return true
				]
			]
			;control-up [
			;	prin "^[[G^[[K"
			;	print ["control-up:" event/key]
			;	prin as-red prompt
			;	prin event/port/data
			;]
			resize    [
				;print ["^[[G^[[Ksize:" event/offset]
				emit as-red prompt
				emit event/port/data
			]
			interrupt [
				print "^/[INTERRUPT]^/"
				return true
			]
		]
		prin buffer
		false
	][
		buffer: make string! 1000
		emit: func[str][ append buffer str ]
	]
	modify port 'line false
	prin as-red prompt
	wait [port]
	modify port 'line true
]
