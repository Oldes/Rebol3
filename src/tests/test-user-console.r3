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
						;all-words: union words-of system/contexts/lib words-of system/contexts/user
						parts: split event/port/data " "
						if not empty? parts [
							last-part: last parts
							either (first last-part) = #"%" [
								path-parts: split-path last-part
								files: sort either 1 = length? path-parts [ read %. ] [ read to-file first path-parts ]

								perfect-match: find files to-file skip last-part 1
								
								either perfect-match = none [
									last-part-as-string: to-string skip last-part 1
									best-matches: copy []
									
									foreach file files [
										file-as-string: to-string file

										if all [
											((length? file-as-string) >= (length? last-part-as-string))
											((copy/part file-as-string (length? last-part-as-string)) = last-part-as-string)
										] [
											;append best-matches replace file-as-string last-part-as-string ""
											append best-matches join "%" file-as-string
										]
									]

									either (length? best-matches) <> 1 [
										print rejoin [ "^/" mold best-matches ]

										min-length: length? first best-matches
										foreach match best-matches [
											min-length: min min-length (length? match)
										]

										match: true
										match-count: 0

										repeat char-count min-length [
											repeat word-count length? best-matches [
												if (pick (pick best-matches word-count) char-count) <> (pick (pick best-matches 1) char-count) [
													match: false
													break
												]
											]
											if (not match) [ break ]
											match-count: char-count
										]

										matching-part: skip copy/part first best-matches match-count length? last-part

										append event/port/data matching-part
									] [
										matching-part: skip first best-matches length? last-part

										append event/port/data matching-part
									]
								] [
									append event/port/data " "
								]
							] [
								print "^/Char"
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
				print ["^[[G^[[Ksize:" event/offset]

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