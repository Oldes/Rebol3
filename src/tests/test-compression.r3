Rebol [
	Title:    "Test compression"
	Date:     19-Nov-2026
	Author:   "Oldes"
	File:     %test-compression.r3
	Version:  0.0.3
;;	Requires: 3.11.0
	Usage: {
		r3 test-compression.r3            ;; uses molded system as input
		r3 test-compression.r3 <file>     ;; uses content of the given file
	}
	Note: {}
]

foreach m [brotli zstd zlib-ng deflate][
	attempt [import (m)]
]

;; Optional input file specified as the first command line argument.
file: all [
	not empty? args: system/options/args
	to-rebol-file first args
]

either file [
	unless exists? file [
		print as-red ["File not found:" to-local-file file]
		quit/return 1
	]
	print as-green ["Using file:" to-local-file file]
	bin: read file
][
	;; Using molded system as a test input (large text data).
	;; When running this script from REPL console,
	;; make sure that we don't mold the system multiple times,
	;; else its size would be significantly bigger!
	unless binary? :bin [bin: to binary! mold system]
]
sum: checksum bin 'sha256 ;; Used to validate decompressed result
len: length? bin          ;; Used as a hint for the decompression

foreach level [1 5 9 22][
	print as-green ajoin ["^/Testing compression of " len " bytes with level " level ".^/"]

	print as-yellow {Method    c.size    factor    com.time    dec.time    valid}
	foreach m system/catalog/compressions [
		t1: attempt [ to decimal! dt [out: compress/level bin m level] ]
		sz: attempt [ length? out ]
		fc: attempt [ round/to (len / sz) 0.001 ]
		t2: attempt [ to decimal! dt [out: decompress/size out m len] ]
		ok: attempt [ equal? sum checksum out 'sha256 ]
		printf [10 10 10 12 12] [m sz fc t1 t2 ok]
	]
	print  "------------------------"
]

;if system/options/script [ask "DONE"]