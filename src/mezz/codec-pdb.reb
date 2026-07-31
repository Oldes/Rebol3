REBOL [
	Title:  "Codec: PDB (Pilot	Database) file format"
	Name:    pdb
	Type:    module
	Version: 0.0.2
	Options: [delay]
	Author: "Oldes"
	History: [31-Jul-2026 "Oldes" {Initial version}]
	Purpose: {To decode a text from some PalmDoc book files.}
]

register-codec [
	name:  'pdb
	type:  'text
	title: "PDB (Pilot	Database) file format"
	suffixes: [%.pdb]

	decode: func [
		{Extract content of the PDB file}
		data  [binary! file! url!]
		/local bin start end text
	][
		clear pdb-info
		clear pdb-data
		unless binary? data [ data: read data ]
		bin: binary data
		set pdb-header binary/read bin [
			bytes 32  ;; Database name (null-padded)
			UI16      ;; Attributes
			UI16      ;; Version
			UI32      ;; Creation date (seconds since 1904-01-01)
			UI32      ;; Modification date
			UI32      ;; Last backup date
			UI32      ;; Modification number
			UI32      ;; App info offset
			UI32      ;; Sort info offset
			bytes 4   ;; Type (4-char code, e.g. "TEXt" or "BOOK")
			bytes 4   ;; Creator (4-char code, e.g. "REAd" or "MOBI" or "PDOC")
			UI32      ;; Unique ID seed
			UI32      ;; Next record list ID
			UI16      ;; Number of records
		]
		loop pdb-header/records [
			append pdb-info binary/read bin [UI32 UI8 UI24] ;; offset + attributes + uniqueID
		]
		with pdb-header [
			name:     to-text name
			type:     to-text type
			creator:  to-text creator
			created:  decode-palm-date created
			modified: decode-palm-date modified
		]
		forskip pdb-info 3 [
			start:    pdb-info/1
			end: any [pdb-info/4 length? data]
			append pdb-data copy/part atz data start end - start
		]
		pdb-record0: binary/read pdb-data/1 [
			UI16  ;compression: ;; 1 = No compression, 2 = PalmDOC compression, 17480 (0x4448) = HUFF/CD compression.
			UI16  ;reserved:    ;; Always set to 0.
			UI32  ;length:      ;; The total length of the uncompressed text in bytes.
			UI16  ;rec-count:   ;; The number of text records that follow (excluding Record 0).
			UI16  ;rec-size:    ;; Maximum size of an uncompressed text block (usually 4096 bytes).
			UI32  ;curr-pos:    ;; Usually 0 for unencrypted files.
			BYTES ;rest:        
		]
		if verbose [? pdb-header ? pdb-record0]
		text: rejoin switch/default pdb-record0/1 [
			2      [ map-each rec next pdb-data [depalmdoc rec] ]
			0#4448 [do make error! "HUFF/CD compression not supported!"]
		][	next pdb-data ]
		clear skip text pdb-record0/3 ;; clear possible padding
		try [text: iconv/to text 'cp1250 'utf8]
		text
	]

	identify: func [data [binary!]][
		parse/case data [
			60 skip
			["TEXt" | "BOOK"]
			["REAd" | "MOBI" | "PDOC"]
			to end
		]
	]

	pdb-header: construct [
		name:     ;; Database name (null-padded)
		attr:     ;; Attributes
		version:  ;; Version
		created:  ;; Creation date (seconds since 1904-01-01)
		modified: ;; Modification date
		backup:   ;; Last backup date
		mod:      ;; Modification number
		app-offs: ;; App info offset
		sort-offs:;; Sort info offset
		type:     ;; Type (4-char code, e.g. "TEXt" or "BOOK")
		creator:  ;; Creator (4-char code, e.g. "REAd" or "MOBI" or "PDOC")
		id-seed:  ;; Unique ID seed
		next-id:  ;; Next record list ID
		records:  ;; Number of records
		_
	]
	pdb-info: []
	pdb-data: []
	pdb-record0: _

	to-text: func[bin][iconv/to trim/tail bin 'cp1250 'utf8]

	decode-palm-date: function [timestamp [integer!]][
		;; seconds from 1904-01-01T00:00:00
		date: to date! (-2082844800 + timestamp)
		;; if too old, then the Unix epoch base time
		if date < 1-1-1970 [date: to date! timestamp]
		date
	]

	depalmdoc: function [
		"Decompresses one PalmDoc (LZ77-style) compressed record"
		data [binary!]
	][
		size: length? data
		out: clear #{}
		while [not tail? data][
			byte: data/1
			++ data
			case [
				byte = 0 [
					append out byte
				]
				byte <= 8 [
					append out copy/part data byte
					data: skip data byte
				]
				byte <= 127 [
					append out byte
				]
				byte <= 191 [ ;; 0x80-0xBF: back-reference
					val: byte << 8 | data/1
					src: (length? out) - (val >> 3 & 0#7FF)
					++ data
					repeat i (val & 7 + 3) [
						append out out/(src + i)
					]
				]
				true [ ;; 0xC0-0xFF
					append append out SP (byte xor 0#80)
				]
			]
		]
		copy out
	]
	verbose: not system/options/quiet
]