REBOL [
	title: "Codec: ZIP"
	name:  zip
	type:  module
	version: 0.0.6
	author: "Oldes"
	specification: https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
	;history: [
	;	7-Nov-2026 "Oldes" {
	;		* Fixed file name length of non-ASCII names (length in UTF-8 bytes, not chars)
	;		* Setting the UTF-8 flag (bit 11) for non-ASCII file names
	;		* Invalid data specification is skipped using `exit` (not relying on the caller's loop)
	;		* Decoder skips encrypted and ZIP64 entries (with an error log) and refuses ZIP64 archives
	;		* Using the `level` setting for deflate compression
	;		* Fixed LZMA input length (compressed size includes the 4-byte header)
	;		* Local header CRC read as unsigned (as in the central directory)
	;		* Root path is removed only as a prefix of the added file name
	;		* Decoder returns none (not an empty block) when the end of central directory record is missing
	;		* Stricter identify (checks the full 4-byte signature)
	;		* Optional alignment of stored files (Android ZIP Alignment Extra Field 0xD935)
	;		* Encoder is a plain function (no state kept between calls)
	;	}
	;	30-Nov-2021 "Oldes" {
	;		* Access to comment and extra field of uncompressed data
	;		* Added support to include file comments, extras or internal and external attributes
	;		* Added support to include uncompressed data (useful when making APK targeting version 30 and above)
	;	}
	;	24-Oct-2019 "Oldes" {
	;		* Refactored decoder so it is using central directory structure
	;		* Added decode/info refinement used to resolve just the info about files without decompressing
	;		* Added access to decompress-file function which may be used to decompress single file using info from the info records
	;		* Added basic extra field data decoder
	;	}
	;	15-Mar-2019 "Oldes" {Initial version of the ZIP decoder}]
]

register-codec [
	name:  'zip
	type:  'compression
	title: "ZIP File Format"
	suffixes: [%.zip %.aar %.jar %.apk %.zipx %.appx %.epub %.docx %.swc]

	decode: function [
		{Decompress all content of the ZIP file}
		zip-data  [binary! file! url!]
		/validate "Check if decompressed data has valid CRC"
		/only     "Extract only specified files if found in the achive"
			files [block!] "Block with file names to extract"
		/info              "Does not decode data. Instead of data there is reported uncompressed size."
		return:   [block! none!] "Result is in format: [NAME [MODIFIED DATA CRC] ...] or none if data are not supported"
	] [
		unless binary? zip-data [ zip-data: read zip-data ]
		if verbose [
			log-info 'ZIP ["^[[1;32mDecode ZIP data^[[m (^[[1m" length? zip-data "^[[mbytes )"]
		]
		bin: binary zip-data

		if only [ files-to-extract: length? files ]

		;-[ reading central directory end record ]-
		; it must be present in each zip file
		unless pos: find/last/tail bin/buffer #{504B0506} [
			log-error 'ZIP "End of central directory record not found!"
			return none
		]

		bin/buffer: pos
		if verbose [ log-trace 'ZIP "End of central directory record" ]
		data: binary/read bin [
			     UI16LE      ; number of this disk
			     UI16LE      ; number of the disk with the start of the central directory
			     UI16LE      ; total number of entries in the central directory on this disk
			     UI16LE      ; total number of entries in the central directory
			     UI32LE      ; size of the central directory
			pos: UI32LE      ; offset of start of central directory with respect to the starting disk number
			len: UI16LE      ; .ZIP file comment length
			com: BYTES :len  ; .ZIP file comment
		]
		if verbose [
			log-trace 'ZIP mold data
			unless empty? com [
				log-info 'ZIP ["Comment: ^[[33m" to-string com]
			]
		]
		
		unless all [zero? data/1 zero? data/2][
			log-error 'ZIP "Splitted zip files not supported!"
			return none
		] 
		if any [data/5 = 4294967295 pos = 4294967295][
			log-error 'ZIP "ZIP64 archives not supported!"
			return none
		]

		result: make block! 2 * data/4

		;-[ reading central directory records ]-
		bin/buffer: at head bin/buffer (pos + 1)

		while [
			33639248 = binary/read bin 'UI32LE ;#{02014B50}
		][
			cheader: binary/read bin [
				          UI16LE         ; version made by
				          UI16LE         ; version needed to extract
				flags:    UI16LE         ; general purpose bit flag
				method:   UI16LE         ; compression method
				modified: MSDOS-DATETIME ; last modified
				crc:      UI32LE         ; crc-32
				cmp-size: UI32LE         ; compressed size
				unc-size: UI32LE         ; uncompressed size
				len-name: UI16LE         ; file name length
				len-extr: UI16LE         ; extra field length
				len-comm: UI16LE         ; file comment length
				disk-num: UI16LE         ; disk number start
				att-int:  UI16LE         ; internal file attributes
				att-ext:  UI32LE         ; external file attributes
				offset:   UI32LE         ; relative offset of local header
				name:     BYTES :len-name
				extr:     BYTES :len-extr
				comm:     BYTES :len-comm
			]
			cheader/16: name: to file! name
			if verbose [
				log-trace 'ZIP "Central directory structure"
				log-trace 'ZIP mold cheader
				de-extra-fields extr
			]
			if all [only not find files name][
				if verbose [ log-debug 'ZIP ["Not extracting: ^[[33m" name] ]
				continue
			]
			if 1 = (flags and 1) [
				log-error 'ZIP ["Encrypted files not supported:^[[33m" name]
				continue
			]
			if any [
				cmp-size = 4294967295
				unc-size = 4294967295
				offset   = 4294967295
			][
				log-error 'ZIP ["ZIP64 files not supported:^[[33m" name]
				continue
			]

			either info [
				repend result [name reduce [modified offset cmp-size unc-size method crc extr comm]]
			][
				if verbose [
					log-info 'ZIP [
						"Extracting: ^[[33m" name
						" ^[[0mbytes:^[[33m" cmp-size "^[[0m->^[[33m" unc-size
					]
					unless empty? comm [log-info 'ZIP ["Comment: ^[[33m" to-string comm "^[[0m" mold name]]
				]

				either zero? unc-size [
					data: none
				][
					data: decompress-file (at head bin/buffer (offset + 1)) reduce [method cmp-size unc-size]

					if all [
						data
						any [validate validate-crc?]
						crc <> crc2: checksum data 'crc32
					][
						log-error 'ZIP ["CRC check failed!" crc "<>" crc2]
					]
				]
				data: reduce [modified data crc]
				unless empty? extr [append append data quote   extra: extr]
				unless empty? comm [append append data quote comment: comm]
				if att-int <> 0    [append append data quote att-int: att-int]
				if att-ext <> 0    [append append data quote att-ext: att-ext]
				append/only append result name data
			]

			if only [
				-- files-to-extract
				if files-to-extract = 0 [
					if verbose [ log-debug 'ZIP "All files extracted" ]
					break
				]
			]
		]

		new-line/all result true
		result
	]

	encode: func [
		"Compress given block of files."
		files [block! file!] "[file! binary! ..] or [file! [date! crc binary!] or [dir! none!] ..]"
		/local bin dir entries root add-file add-data dir-size bin-size
	][
		bin: binary 10000
		dir: binary 1000
		entries: 0

		add-file: func[file [file!] /local path spec file-name][
			try/with [
				spec: query file [type: date:]
				either spec [
					file-name: any [find/match/tail file root file]
					either spec/type = 'dir [
						add-data file-name spec
						foreach f read file [
							add-file file/:f
						]
					][
						add-data file-name reduce [spec/date read file]
					]
				][
					; wildcard?
					path: first split-path file
					foreach f read file [
						add-file path/:f
					]
				]
			][
				log-error 'ZIP ["Failed to add file:" as-green file]
			]
		]

		add-data: func[
			file spec
			/local data date compressed-data method compressed-size size crc offset
			       att-ext att-int no-compress? extra extra-length comm comm-length
			       name filename-length flags
			       file-align lfh-extra-length pad-length pad-size padding
		][
			if verbose [log-info 'ZIP ["Adding:" as-green file]]

			any [file? file cause-error 'user 'message reduce [reform ["found" type? file "where file! expected"]]]
			extra-length: comm-length: att-ext: att-int: 0
			unless any [
				all [
					block? spec 
					parse spec [any [
						spec:
						  date!   (date: spec/1)
						| string! (data: to binary! spec/1)
						| binary! (data: spec/1)
						| 'store  (no-compress?: true)
						| quote extra: set extra binary! (extra-length: length? extra)
						| quote comment: set comm [binary! | any-string!] (
							if string? comm [comm: to binary! comm]
							comm-length: length? comm
						)
						| quote att-int: set att-int integer!
						| quote att-ext: set att-ext integer!
						| quote align:   set file-align integer!
						| 1 skip
					]]
				]
				all [binary? spec data: spec]
				all [string? spec data: to binary! spec]
				none? spec ; just a directory
				spec = 'none
			][
				log-error 'ZIP ["Invalid zip file's data specification:" as-red mold/part spec 30]
				exit
			]
			data: any [data #{}]
			crc: checksum data 'CRC32
			size: length? data
			method: either any [
				no-compress?
				zero? size
				lesser-or-equal? size length? compressed-data: compress/level data 'deflate level
			][
				compressed-data: data
				0 ;store
			][
				8 ;deflate
			]

			compressed-size: length? compressed-data

			if any [
				none? date
				"?date?" = form date ; temp fix for invalid date!
			][	date: now ]

			;; file name is stored as UTF-8 bytes, so its length must be counted in bytes
			name: to binary! file
			filename-length: length? name
			;; general purpose bit 11: file name (and comment) is UTF-8 encoded
			flags: either filename-length = length? file [0][2048]
			offset: indexZ? bin/buffer-write

			;; optional alignment of stored data (required for some files in Android APKs)
			;; using the Android ZIP Alignment Extra Field (0xD935) in the local header only
			lfh-extra-length: 0
			file-align: any [file-align align]
			if all [method = 0 file-align > 1][
				;; data start = offset + local header (30) + name + alignment field (min 6)
				pad-length: offset + 36 + filename-length
				pad-length: (file-align - (pad-length % file-align)) % file-align
				pad-size: 2 + pad-length ; alignment value + padding
				padding: append/dup make binary! pad-length 0 pad-length
				lfh-extra-length: 6 + pad-length
			]

			unless extra [extra: #{}]
			unless comm  [comm:  #{}]

			binary/write bin [
				#{504B0304 1400} ;signature / version
				UI16LE         :flags
				UI16LE         :method
				MSDOS-DATETIME :date
				UI32LE         :crc
				UI32LE         :compressed-size
				UI32LE         :size
				UI16LE         :filename-length
				UI16LE         :lfh-extra-length
				BYTES          :name
			]
			if padding [
				binary/write bin [
					UI16LE         0#D935        ; Android ZIP Alignment Extra Field
					UI16LE         :pad-size
					UI16LE         :file-align
					BYTES          :padding
				]
			]
			binary/write bin [
				BYTES          :compressed-data
			]
			binary/write dir [
				#{504B0102 1703 1400} ; signature / version made / version needed
				UI16LE         :flags
				UI16LE         :method
				MSDOS-DATETIME :date
				UI32LE         :crc
				UI32LE         :compressed-size
				UI32LE         :size
				UI16LE         :filename-length
				UI16LE         :extra-length ; Extra field length
				UI16LE         :comm-length  ; File comment length
				UI16LE         0             ; Disk number where file starts
				UI16LE         :att-int      ; Internal file attributes
				UI32LE         :att-ext      ; External file attributes
				UI32LE         :offset       ; Relative offset of local file header
				BYTES          :name
				BYTES          :extra        ; Extra field
				BYTES          :comm         ; File comment
			]
			++ entries
		]

		;- ENCODE:
		either file? files [
			root: first split-path files
			add-file files
		][
			foreach [file spec] files [
				add-data file spec
			]
		]

		dir-size: length? dir/buffer
		bin-size: length? bin/buffer

		binary/write bin [
			BYTES :dir/buffer
			#{504B0506}      ; End of central directory signature
			UI16LE 0         ; Number of this disk
			UI16LE 0         ; Disk where central directory starts
			UI16LE :entries  ; Number of central directory records on this disk
			UI16LE :entries  ; Total number of central directory records
			UI32LE :dir-size ; Size of central directory
			UI32LE :bin-size ; Offset of start of central directory
			UI16LE 0         ; Comment length
			;#{}             ; Comment
		]
		bin/buffer
	]

	identify: function [data [binary!]][
		parse data [
			#"P" #"K" [
				  #"^(03)" #"^(04)" ; local file header
				| #"^(05)" #"^(06)" ; end of central directory (empty archive)
			] to end
		]
	]

	decompress-file: function [
		buffer [binary!] "Binary at position of the zip's local record"
		header [block!]  "[method cmp-size unc-size]"
	][
		bin: binary buffer
		unless 67324752 = binary/read bin 'UI32LE [ ;#{504B0304}
			log-error 'ZIP {Offset is not pointing to the "Local file header"}
			return none
		]

		if verbose [log-trace 'ZIP "Local file header"]
		local: binary/read bin [
			          UI16LE         ; version
			          BITSET16       ; flags
			          UI16LE         ; compression
			          MSDOS-DATETIME ; last modified
			          UI32LE         ; crc-32
			          UI32LE         ; compressed size
			          UI32LE         ; uncompressed size
			len-name: UI16LE
			len-extr: UI16LE
			name:     BYTES :len-name
			extr:     BYTES :len-extr
			data-pos: INDEX
		]

		;@@ O: does really have central record precedense as I use it now?
		;@@ O: should I compare cental record with the local one?

		method:   header/1
		cmp-size: header/2
		unc-size: header/3

		if verbose [log-trace 'ZIP mold local]

		data: at head buffer :data-pos

		switch/default method [
			8  [ ;- deflate
				output: decompress/size data 'deflate unc-size
			]
			14 [ ;- LZMA
				;; skip LZMA version (2 bytes) and properties size (2 bytes)
				output: decompress/part/size (skip data 4) 'lzma (cmp-size - 4) unc-size
			]
			0  [ ;- store
				output: copy/part data cmp-size
			]
		][
			log-error 'ZIP ["Unsupported compression method:^[[0;35m" method]
		]

		return output
	]

	de-extra-fields: function [
		"Decodes extra field data of the ZIP record"
		extra [binary!] "Extra field data"
	][
		log-trace 'ZIP ["Decode extra fields:" mold extra]
		bin: binary extra
		fields: copy []
		while [not tail? bin/buffer][
			binary/read bin [id: UI16LE len: UI16LE data: BYTES :len]
			repend fields [id data]
		]
		log-debug 'ZIP ["Extra fields:" mold fields]
		fields
		
		;- Extra field info:
		; https://opensource.apple.com/source/zip/zip-6/unzip/unzip/proginfo/extra.fld

		;- List of extra field ids as defined in Info-ZIP utility:
		;#define EF_PKSZ64    0x0001    /* PKWARE's 64-bit filesize extensions */
		;#define EF_AV        0x0007    /* PKWARE's authenticity verification */
		;#define EF_EFS       0x0008    /* PKWARE's extended language encoding */
		;#define EF_OS2       0x0009    /* OS/2 extended attributes */
		;#define EF_PKW32     0x000a    /* PKWARE's Win95/98/WinNT filetimes */
		;#define EF_PKVMS     0x000c    /* PKWARE's VMS */
		;#define EF_PKUNIX    0x000d    /* PKWARE's Unix */
		;#define EF_PKFORK    0x000e    /* PKWARE's future stream/fork descriptors */
		;#define EF_PKPATCH   0x000f    /* PKWARE's patch descriptor */
		;#define EF_PKPKCS7   0x0014    /* PKWARE's PKCS#7 store for X.509 Certs */
		;#define EF_PKFX509   0x0015    /* PKWARE's file X.509 Cert&Signature ID */
		;#define EF_PKCX509   0x0016    /* PKWARE's central dir X.509 Cert ID */
		;#define EF_PKENCRHD  0x0017    /* PKWARE's Strong Encryption header */
		;#define EF_PKRMCTL   0x0018    /* PKWARE's Record Management Controls*/
		;#define EF_PKLSTCS7  0x0019    /* PKWARE's PKCS#7 Encr. Recipient Cert List */
		;#define EF_PKIBM     0x0065    /* PKWARE's IBM S/390 & AS/400 attributes */
		;#define EF_PKIBM2    0x0066    /* PKWARE's IBM S/390 & AS/400 compr. attribs */
		;#define EF_IZVMS     0x4d49    /* Info-ZIP's VMS ("IM") */
		;#define EF_IZUNIX    0x5855    /* Info-ZIP's first Unix[1] ("UX") */
		;#define EF_IZUNIX2   0x7855    /* Info-ZIP's second Unix[2] ("Ux") */
		;#define EF_IZUNIX3   0x7875    /* Info-ZIP's newest Unix[3] ("ux") */
		;#define EF_TIME      0x5455    /* universal timestamp ("UT") */
		;#define EF_UNIPATH   0x7075    /* Info-ZIP Unicode Path ("up") */
		;#define EF_UNICOMNT  0x6375    /* Info-ZIP Unicode Comment ("uc") */
		;#define EF_MAC3      0x334d    /* Info-ZIP's new Macintosh (= "M3") */
		;#define EF_JLMAC     0x07c8    /* Johnny Lee's old Macintosh (= 1992) */
		;#define EF_ZIPIT     0x2605    /* Thomas Brown's Macintosh (ZipIt) */
		;#define EF_ZIPIT2    0x2705    /* T. Brown's Mac (ZipIt) v 1.3.8 and newer ? */
		;#define EF_SMARTZIP  0x4d63    /* Mac SmartZip by Marco Bambini */
		;#define EF_VMCMS     0x4704    /* Info-ZIP's VM/CMS ("\004G") */
		;#define EF_MVS       0x470f    /* Info-ZIP's MVS ("\017G") */
		;#define EF_ACL       0x4c41    /* (OS/2) access control list ("AL") */
		;#define EF_NTSD      0x4453    /* NT security descriptor ("SD") */
		;#define EF_ATHEOS    0x7441    /* AtheOS ("At") */
		;#define EF_BEOS      0x6542    /* BeOS ("Be") */
		;#define EF_QDOS      0xfb4a    /* SMS/QDOS ("J\373") */
		;#define EF_AOSVS     0x5356    /* AOS/VS ("VS") */
		;#define EF_SPARK     0x4341    /* David Pilling's Acorn/SparkFS ("AC") */
		;#define EF_TANDEM    0x4154    /* Tandem NSK ("TA") */
		;#define EF_THEOS     0x6854    /* Jean-Michel Dubois' Theos "Th" */
		;#define EF_THEOSO    0x4854    /* old Theos port */
		;#define EF_MD5       0x4b46    /* Fred Kantor's MD5 ("FK") */
		;#define EF_ASIUNIX   0x756e    /* ASi's Unix ("nu") */ 
	]

	validate-crc?: true
	verbose: true
	level: 9
	align: 0 ; default alignment of stored (uncompressed) files; 0 = no alignment
]
