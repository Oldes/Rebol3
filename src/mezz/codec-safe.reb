REBOL [
	Title:   "Codec: SAFE"
	Name:    safe
	type:    module
	Version: 2.0.0
	Author:  "Oldes"
;	History: [
;		10-Jul-2022 "Oldes" "Initial version"
;		12-Jul-2022 "Oldes" "Included SAFE scheme"
;		26-Nov-2026 "Oldes" "SAFE 2.0 format: PBKDF2 keys, random salt/IV, HMAC-SHA256 (v1 still readable);"
;		                    "scheme with file locking, conflict detection and serializable values only"
;	]
	File:    https://raw.githubusercontent.com/Oldes/Rebol3/master/src/mezz/codec-safe.reb
	TODO: [
		"The scheme could require to enter the password again after some time of inactivity."
	]
]

register-codec [
	name:  'safe
	type:  'crypt
	title: "Encrypted file storage"
	suffixes: [%.safe]

	ciphers: [chacha20 aes-256-gcm]    ;; allowed ciphers, in order of preference
	default-iterations: 600000         ;; OWASP minimum for PBKDF2-HMAC-SHA256
	max-iterations:     10000000       ;; refuse hostile files

	derive-keys: function [
		"Returns [enc-key mac-key]: one PBKDF2 block, then an HMAC split"
		password [any-string! binary!]
		salt     [binary!]
		count    [integer!]
	][
		master: pbkdf2 password salt count 32 'sha256
		reduce [
			checksum/with "SAFE enc" 'sha256 master
			checksum/with "SAFE mac" 'sha256 master
		]
	]

	;; Layout:
	;; id(8) flags(UI16) iterations(UI32) salt(16) iv(16) method(UI8BYTES)
	;; plain-length(UI32) cipher-length(UI32) ciphertext HMAC-SHA256(32, over everything before it)
	encode: function [
		data [any-type!]
		/key        password [any-string! binary! none!]
		/as         cipher   [word!]
		/iterations count    [integer!]
		/cache      ctx      [block!] ;; Key cache [salt count enc-key mac-key]; reused if possible, else filled
	][
		unless cipher [
			foreach m ciphers [
				if find system/catalog/ciphers m [cipher: m break]
			]
		]
		unless all [cipher find ciphers cipher find system/catalog/ciphers cipher][
			cause-error 'script 'invalid-arg [cipher]
		]
		count: any [count default-iterations]
		unless all [count >= 1 count <= max-iterations][
			cause-error 'script 'invalid-arg [count]
		]

		either binary? data [
			flags: 0
		][
			;; refuse data which could not be loaded back
			unless serializable? :data [cause-error 'script 'invalid-arg reduce ['data]]
			data: compress mold/all data 'zlib
			flags: 3
		]
		plen: length? data

		either all [ctx 4 == length? ctx ctx/2 >= count][
			;; reuse the cached salt and keys (no KDF)
			set [salt: count: enc-key: mac-key:] ctx
		][
			salt: make binary! 16
			binary/write salt [random-bytes 16]
			unless key [password: ask-password/new]
			unless password [cause-error 'script 'need-value ['password]]
			keys:    derive-keys password salt count
			enc-key: keys/1
			mac-key: keys/2
			if ctx [repend clear ctx [salt count enc-key mac-key]]
		]
		;; the IV is always fresh, also when the keys are reused
		iv: make binary! 16
		binary/write iv [random-bytes 16]

		if plen > 0 [
			port: open [scheme: 'crypt algorithm: :cipher key: :enc-key]
			modify port 'init-vector iv
			data: take write port data
			close port
		]
		clen: length? data
		method: form cipher

		out: make binary! 128 + clen
		binary/write out [
			BYTES    "SAFE 2.0"
			UI16BE   :flags
			UI32BE   :count
			BYTES    :salt
			BYTES    :iv
			UI8BYTES :method
			UI32BE   :plen
			UI32BE   :clen
			BYTES    :data
		]
		append out checksum/with out 'sha256 mac-key
	]

	decode: function [
		data [binary! file! url!]
		/key   password [any-string! binary! none!]
		/cache ctx      [block!] ;; Key cache [salt count enc-key mac-key]; used when the salt matches, updated on success
	][
		unless binary? data [data: read/binary data]
		if parse data [
			"SAFE " [
			  "2.0" (fun: :decode-v2)
			| "1.0" (fun: :decode-v1)
			] to end
		][
			unless key [password: ask-password]
			attempt [fun data password ctx]
		]
	]

	decode-v2: function [
		data     [binary!]
		password [any-string! binary! none!]
		ctx      [block! none!]
	][
		total: length? data
		if total < 87 [return none]
		try/with [
			binary/read data [
				SKIP 8
				flags:  UI16BE
				count:  UI32BE
				salt:   BYTES 16
				iv:     BYTES 16
				method: UI8BYTES
				plen:   UI32BE
				clen:   UI32BE
				pos:    INDEX
			]
		][  return none ]

		unless all [
			find [0 3] flags
			count >= 1 count <= max-iterations
			plen  <= clen
			total = (pos - 1 + clen + 32)
			find ciphers cipher: transcode/one method
			find system/catalog/ciphers cipher
		][	return none ]

		either all [ctx 4 == length? ctx salt == ctx/1 count == ctx/2][
			;; same salt and count as cached, so no KDF needed
			enc-key: ctx/3
			mac-key: ctx/4
		][
			unless password [return none]
			keys:    derive-keys password salt count
			enc-key: keys/1
			mac-key: keys/2
		]

		;; verify before decrypting; compare through a second HMAC so the comparison
		;; does not leak where the values differ
		unless equal?
			checksum/with (skip tail data -32) 'sha256 mac-key
			checksum/with (checksum/part/with data 'sha256 (total - 32) mac-key) 'sha256 mac-key
		[return none]

		;; cache only keys which passed the HMAC check
		if ctx [append clear ctx reduce [salt count enc-key mac-key]]

		either plen > 0 [
			port: open [scheme: 'crypt direction: 'decrypt algorithm: :cipher key: :enc-key]
			modify port 'init-vector iv
			output: take write/part port at data pos clen
			close port
			clear skip output plen
		][
			output: make binary! 0
		]
		either flags = 3 [transcode/one decompress output 'zlib][output]
	]

	decode-v1: function [
		data     [binary!]
		password [any-string! binary! none!]
		ctx      [block! none!] ;; Not used (v1 has no key derivation)
	][
		binary/read data [
			SKIP 8
			flags:        UI16
			bytes:        UI32     ; real data size (ecrypted may be padded)
			expected-sum: BYTES 32 ; checksum using SHA256
			method:       UI8BYTES
			length:       UI32
			pos:          INDEX
		]
		cipher: to word! to string! method

		unless password [return none]
		unless binary? password [
			password: checksum password 'sha256
		]

		either bytes > 0 [
			port: open [
				scheme:    'crypt
				direction: 'decrypt
				algorithm: :cipher
				key:       :password
			]
			modify port 'init-vector copy/part head data 16

			output: take write/part port at head data :pos :length; decrypt the data
			clear skip output bytes ; remove optional padding
			close port
		][  output: copy #{} ]  ; there were no data
		
		real-sum: checksum output 'sha256
		unless equal? real-sum expected-sum [ return none ]
		if flags == 3 [output: transcode/one decompress output 'zlib]
		output
	]

	ask-password: func[
		;; Returns REBOL_SAFE_PASS (when not empty) or asks for the password
		/new ;; Ask twice when typed, as the password is used for new data
		/local pass confirm
	][
		if all [pass: get-env "REBOL_SAFE_PASS" not empty? pass][return pass]
		confirm: did new
		lib/ask-password/:confirm "Enter SAFE Password: "
	]
]


sys/make-scheme [
	title: "Persistent key/value storage"
	name: 'safe
	lock-wait: 0:00:01 ;; how long to wait for a file locked by another port or process
	actor: [
		open: func [port [port!] /local spec host pw][
			spec: port/spec
			spec/ref: rejoin either/only host: select spec 'host [
				https:// host
				select spec 'path
				select spec 'target
			][
				any [select spec 'path   system/options/home]
				any [select spec 'target %.safe]
			]
			if %.safe <> suffix? spec/ref [
				append spec/ref %.safe
			]
			;; REBOL_SAFE_PASS or a prompt (confirmed when the file is new)
			pw: any [
				select spec 'pass
				either exists? spec/ref [
					system/codecs/safe/ask-password
				][	system/codecs/safe/ask-password/new ]
			]
			unless pw [
				cause-error 'access 'cannot-open reduce [spec/ref "no password"]
			]

			port/extra: object [
				data: none
				pass: pw
				keys:  copy [] ;; derived key cache, so PBKDF2 runs once per salt
				file:  port/spec/ref
				stamp: none    ;; file modification time at our last load or save
				dirty: false   ;; true when there are local changes not saved yet
				;; Opens the file with an OS lock: shared for reading, exclusive for writing.
				;; A lock held by someone else fails immediately with the `locked` error,
				;; so it is retried until LOCK-WAIT expires. Locks are released
				;; by the OS when the process dies, so there are no stale locks.
				lock-file: func[write [logic!] /local deadline res][
					deadline: now/precise + system/schemes/safe/lock-wait
					forever [
						res: try [open/read/:write/lock file]
						unless error? res [return res]
						if any [
							res/id != 'locked
							now/precise > deadline
						][	do res ]
						wait 0.02
					]
				]
				conflict: does [
					print [
						as-purple "*** File was modified by another process; local changes not saved:"
						as-red file
					]
				]
				words: does [words-of data]
				get:  func[key /local value][
					; returned values are always copied, if possible, so
					; they are not modified by accident from outside
					either find copyable! type? value: select data :key [
						copy/deep :value
					][  :value ]
				]
				set:  func[key [word!] val [any-type!]][
					;; refuse values which could not be loaded back,
					;; otherwise the whole file could not be loaded later
					unless serializable? :val [cause-error 'script 'invalid-arg reduce [key]]
					dirty: true
					put data :key :val
				]
				rem:  func[key][
					dirty: true
					remove/key data :key #(unset)
				]
				;; Returns true on success. On failure (wrong password or corrupted file)
				;; nothing is changed, so already loaded data are kept.
				load: func[/local port bin modf new][
					either url? file [
						bin: file
						modf: modified? file
					][
						port: lock-file false
						bin: try [read port]
						modf: modified? file
						close port
						if error? bin [do bin]
					]
					new: system/codecs/safe/decode/key/cache :bin :pass keys
					if none? new [return false]
					data:  new
					stamp: modf
					dirty: false
					true
				]
				save: func[/local port bin err][
					if url? file [
						print as-purple "*** Saving to URL is not yet implemented!"
						return false
					]
					;; encrypt before locking, so the lock is held only briefly
					bin: system/codecs/safe/encode/key/cache :data :pass keys
					port: lock-file true
					;; someone else may have written the file since the check in SYNC
					;; (an empty file was just created by our open)
					if all [
						0 < length? port
						stamp <> modified? file
					][
						close port
						conflict
						return false
					]
					err: try [
						write port bin
						clear port ;; truncate the rest of a previously longer file
					]
					close port
					if error? err [do err]
					stamp: modified? file
					dirty: false
					true
				]
				reload: does [
					;; drops local changes
					either load [true][
						print [as-purple "*** Failed to decrypt data from:" as-red file]
						false
					]
				]
				overwrite: does [
					;; accepts the current file state as seen, so local data replace it
					stamp: modified? file
					dirty: true
					save
				]
				sync: func[/close /local modf] [
					if data [
						;; There are already some data..
						modf: modified? file ;; none when the file does not exist
						case [
							none? modf [ save ]
							stamp <> modf [
								;; the file was changed by someone else since our last load/save
								case [
									dirty    [ conflict ]
									not load [
										print [as-purple "*** Failed to decrypt data from:" as-red file]
									]
								]
							]
							dirty [ save ]
						]
					]
					case [
						close [
							data: stamp: pass: keys: none
							dirty: false
						]
						none? data [
							;; There are no data yet, so load it or make a new map!
							;; (when the load fails, data stay none and OPEN reports it)
							either exists? file [ load ][
								data: make map! 4
								save
							]
						]
					]
					file
				]
				open?: does [ map? data ]
				change-pass: func[
					new [string! binary! block!] "New password, or [old new] passwords"
					/local old
				][
					either block? new [
						unless parse new [2 [string! | binary!]][
							cause-error 'script 'invalid-arg reduce [new]
						]
						lib/set [old new] new
					][
						old: lib/ask-password "Old password: "
					]
					either pass == old [
						pass: new
						clear keys ;; forces a new salt and keys on the next save
						dirty: true
						sync
						true
					][
						sys/log/error 'REBOL "Password validation failed!"
						false
					]
				]
				
				sync

				protect/words/hide [data pass keys load save]
				protect/words [get set rem sync open? change-pass lock-file conflict reload overwrite words]
			]

			unless port/extra/open? [
				port/extra/sync/close ;; clears the password and other state
				cause-error 'access 'cannot-open reduce [spec/ref "invalid password or corrupted data"]
			]

			; make only these required values availeble in the spec:
			set port/spec: object [title: scheme: ref: none] spec

			;protect/words port/extra
			port
		]
		; We must use the inner functions bellow, because the inner data are hidden!
		open?: func[port][
			port/extra/open?
		]
		close: func[port][
			port/extra/sync/close
		]
		put:
		poke: func[port key value][
			port/extra/set :key :value
		]
		pick:
		select: func[port key][
			port/extra/get :key
		]
		remove: func[port /part range /key key-arg][
			if key [port/extra/rem :key-arg]
		]
		update: func[port][
			port/extra/sync
		]
		modify: func[port field value][
			switch field [
				password  [port/extra/change-pass :value]
				reload    [if value [port/extra/reload]]
				overwrite [if value [port/extra/overwrite]]
			]
		]
		query: func[port field][
			;@@ add more fields?
			either field = 'keys [port/extra/words][
				cause-error 'script 'invalid-arg reduce [field]
			]
		]
	]
]