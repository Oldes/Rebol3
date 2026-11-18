REBOL [
	Title:   "Rebin encoder"
	Author:  "Oldes"
	File:    %rebin.reb
	Rights:  "Copyright 2026 Rebol Open Source Contributors"
	License: "Apache License, Version 2.0"
	Needs:   3.22.17
	Purpose: {
		Encodes loaded Rebol values into the Rebin format, which is decoded
		by the native rebin codec (src/core/u-rebin.c, see its header for
		the format description).
	}
	Note: {
		This script is evaluated by the bootstrap executable (now 3.22.18,
		the oldest tested version is 3.22.17)!
		It must use only features available there.

		The datatypes are stored as REB_* numbers of the target, so the list
		of the target's datatypes must be used, when it differs from the
		running interpreter (see the /types refinement).

		Values are stored as a tree, so:
		* a series used more than once is stored more than once,
		* a series which contains itself (directly or not) cannot be stored,
		* all series must be at their head.
		Vector data are stored as they are in memory (the host must be
		little endian).
	}
]

encode-rebin: function/with [
	"Encodes values into Rebin binary"
	data [block!] "Values to encode (the block itself is not stored)"
	/types list [block!] "Datatypes of the target in their REB_* order (default are the running interpreter's)"
	return: [binary!]
][
	type-ids: make map! 64
	n: 0
	foreach t any [list system/catalog/datatypes][
		put type-ids to word! t n
		n: n + 1
	]
	if n > 127 [fail "too many datatypes"]

	symbols: make map! 1000  ;; spelling (binary!, so it is case-sensitive) -> index
	spellings: make block! 1000
	out: binary 64000

	emit-block data

	;; header and the symbol table
	header: binary 1000
	count: length? spellings
	binary/write header [BYTES #{524542494E} UI8 1 UI8 :n UI8 0 ENCODEDU64 :count]
	foreach s spellings [
		len: length? s
		binary/write header [ENCODEDU64 :len BYTES :s]
	]
	append header/buffer out/buffer
][
	;; values shared by the functions below (set by encode-rebin)
	type-ids: symbols: spellings: out: none

	;; Vector element types (VTSI08...) by their names
	vector-types: [
		int8!    0  int16!   1  int32!   2  int64!   3
		uint8!   4  uint16!  5  uint32!  6  uint64!  7
		float32! 10 float64! 11
	]

	fail: func [msg [block! string!]][do make error! join "Rebin: " msg]

	type-id-of: func [type [datatype!] /local id][
		unless id: select type-ids to word! type [
			fail ["datatype is not in the target: " type]
		]
		id
	]

	symbol-id: func [word /local spelling id][
		spelling: to binary! form to word! :word
		unless id: select symbols spelling [
			append spellings spelling
			id: (length? spellings) - 1
			put symbols spelling id
		]
		id
	]

	nanoseconds: func [time [time!] /local ns][
		;; Using the parts, because a decimal number would lose the precision.
		ns: ((time/hour * 3600) + (time/minute * 60)) * 1000000000
		ns: ns + to integer! round time/second * 1E9
		either time < 0:00 [negate ns][ns]
	]

	check-head: func [value][
		unless head? value [fail ["series is not at its head: " mold/part :value 40]]
	]

	;; Writes a length (or a count) followed by the bytes
	emit-bytes: func [bin [binary!] /local len][
		len: length? bin
		binary/write out [ENCODEDU64 :len BYTES :bin]
	]

	emit-block: func [block [any-block!] /local len][
		len: length? block
		binary/write out [ENCODEDU64 :len]
		;; only blocks and parens have new-line flags (new-line? refuses other types)
		either any [block? block paren? block][
			forall block [emit-value first block new-line? block]
		][
			forall block [emit-value first block false]
		]
	]

	emit-value: func [
		value [any-type!] line? [logic!]
		/local tag num x y bin vtype spec rows
	][
		tag: type-id-of type? :value
		either line? [
			tag: tag or 128 ;; the flags byte follows with the new-line flag
			binary/write out [UI8 :tag UI8 1]
		][	binary/write out [UI8 :tag] ]

		switch/default to word! type? :value [
			unset! none! []
			logic! [
				num: either value [1][0]
				binary/write out [UI8 :num]
			]
			integer! [binary/write out [ENCODEDU64 :value]]
			decimal!
			percent! [
				x: to decimal! value
				binary/write out [F64LE :x]
			]
			money! [
				bin: to binary! value ;; 12 bytes
				binary/write out [BYTES :bin]
			]
			char! [
				num: to integer! value
				binary/write out [ENCODEDU64 :num]
			]
			pair! [
				x: value/x y: value/y
				binary/write out [F32LE :x F32LE :y]
			]
			tuple! [
				bin: to binary! value
				binary/write out [UI8BYTES :bin]
			]
			time! [
				num: nanoseconds value
				binary/write out [ENCODEDU64 :num]
			]
			date! [
				num: value/year x: value/month y: value/day
				binary/write out [ENCODEDU64 :num UI8 :x UI8 :y]
				either value/time [
					num: nanoseconds value/time
					;; zone in 15 minutes steps, as a signed byte
					x: (to integer! (to decimal! value/zone) / 900) and 255
					binary/write out [UI8 1 ENCODEDU64 :num UI8 :x]
				][	binary/write out [UI8 0] ]
			]
			binary! string! file! email! ref! url! tag! [
				check-head value
				emit-bytes to binary! value
			]
			bitset! [
				;; to binary! returns the complemented view of a complemented bitset
				bin: to binary! value
				either complement? value [
					bin: complement bin
					binary/write out [UI8 1]
				][	binary/write out [UI8 0] ]
				emit-bytes bin
			]
			image! [
				check-head value
				x: to integer! value/size/x
				y: to integer! value/size/y
				bin: to binary! value ;; RGBA
				binary/write out [ENCODEDU64 :x ENCODEDU64 :y BYTES :bin]
			]
			vector! [
				check-head value
				;; spec is like: [uint8! 3x2] or [uint8! 6] (rows are the y of the shape)
				spec: spec-of value
				unless vtype: select vector-types first spec [
					fail ["not supported vector: " mold spec]
				]
				rows: either pair? second spec [to integer! spec/2/y][1]
				num: length? value
				bin: to binary! value
				binary/write out [UI8 :vtype ENCODEDU64 :rows ENCODEDU64 :num BYTES :bin]
			]
			datatype! [
				num: type-id-of value
				binary/write out [UI8 :num]
			]
			typeset! [
				num: 0
				foreach t to block! value [num: num or shift 1 type-id-of t]
				binary/write out [ENCODEDU64 :num]
			]
			block! paren! path! set-path! get-path! lit-path! hash! [
				check-head value
				emit-block value
			]
			map! [
				;; key value pairs (word keys are set-words here)
				emit-block body-of value
			]
			word! set-word! get-word! lit-word! refinement! issue! [
				num: symbol-id :value
				binary/write out [ENCODEDU64 :num]
			]
		][
			fail ["not supported datatype: " type? :value]
		]
	]
]
