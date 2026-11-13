Rebol [
	Title:  "HTTPd Scheme"
	Type:    module
	Name:    httpd
	Date:    21-Oct-2026
	Version: 0.12.0
	Author: ["Andreas Bolka" "Christopher Ross-Gill" "Oldes"]
	Exports: [serve-http http-server decode-target to-CLF-idate make-password-hash]
	Home:    https://github.com/Oldes/Rebol-HTTPd
	Rights:  http://opensource.org/licenses/Apache-2.0
	Purpose: {
		A Tiny Webserver Scheme for Rebol 3 (Oldes' branch)
		Features:
		* serve static files (GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS); large files sent in chunks
		* customizable request handling using _actors_ (per server)
		* decoded request bodies (form-urlencoded, multipart, JSON, text),
		  chunked uploads and `Expect: 100-continue`
		* optional file upload (PUT) and removal (DELETE) in allowed locations
		* authentication using Bearer tokens or Basic auth (PBKDF2 password hashes)
		* keep-alive (HTTP/1.1 and HTTP/1.0) and pipelined requests
		* RSP (Rebol Server Pages)
		* WebSocket connections (text/binary messages, ping/pong)
		* request paths restricted to the root; request size limits
		* client can stop the server and return data (useful for OAuth2)
	}
	TODO: {
		* support for multidomain serving using `Host` header field
		* assembling fragmented WebSocket messages
		* optional `Origin` check for WebSocket upgrades
		* request body charsets other than UTF-8
		* range requests (206 Partial Content), response compression
	}
	Usage: {See README.md and %server-test.r3 script how to start a server}
	History: [
		04-Nov-2009 "Andreas Bolka" {A Tiny HTTP Server
		https://github.com/earl/rebol3/blob/master/scripts/shttpd.r}
		04-Jan-2017 "Christopher Ross-Gill" {Adaptation to Scheme
		https://gist.github.com/rgchris/73510e7d643eb0a6b9fa69b849cd9880}
		01-Apr-2019 "Oldes" {Rewritten to be usable in real life situations.}
		10-May-2020 "Oldes" {Implemented directory listing, logging and multipart POST processing}
		02-Jul-2020 "Oldes" {Added possibility to stop the server from a client and return data to it (useful for OAuth2)}
		06-Dec-2022 "Oldes" {Added minimal support for WebSocket connections}
		09-Jan-2023 "Oldes" {New home: https://github.com/Oldes/Rebol-HTTPd}
		09-May-2023 "Oldes" {Root-less configuration possibility (default)}
		14-Dec-2023 "Oldes" {Deprecated the `http-server` function in favor of `serve-http` with a different configuration input}
		06-Mar-2025 "Oldes" {Initial implementation of RSP (Rebol Server Pages)}
		21-Oct-2026 "Oldes" {Security and protocol fixes: path checks, request limits, chunked uploads, 100-continue, pipelining, HTTP/1.0 keep-alive, WebSocket ping and binary frames, per-server actors, request-local RSP variables}
	]
	Needs: [3.16.0 mime-types] ; uses #(...) construction syntax
]

append system/options/log [httpd: 1]

;------------------------------------------------------------------------
;-- Net utils:                                                         --
;------------------------------------------------------------------------
;@@ move elsewhere and with different name?

decode-target: wrap [
	path-chars: complement charset "?#"
	key-chars:  complement charset "&=#"
	val-chars:  complement charset "&#"
	func [
		"Splits target into file part and key/pair query values if any"
		target [file! string! binary! none!]
		/query "Target is only a query string (like form-urlencoded data)"
		/local bin key val
	][
		
		result: object [file: none values: make block! 8 fragment: none original: target]
		unless target [return result]
		bin: to binary! target
		unless query [
			;; path ends only at `?` or `#`; `=` and `&` are valid in file names
			parse bin [copy val any path-chars opt #"?" bin:]
			result/file: to file! dehex val
		]
		parse bin [
			any [
				copy key some key-chars [#"=" copy val any val-chars | (val: none)] (
					key: to string! dehex/uri key
					;; keys which are not valid words are kept as strings
					append result/values any [attempt [to set-word! key] key]
					append result/values either val [to string! dehex/uri val][copy ""]
				)
				| #"=" any val-chars ;; pair without a key
				| #"&"
				| #"#" copy val to end (result/fragment: to string! dehex val)
			]
		]
		new-line/skip result/values true 2
		result
	]
]

;- Multipart data related code:
; token:       https://tools.ietf.org/html/rfc7230#section-3.2.6
;              ["!#$%&'*+-.^^_`|~" #"0"-#"9" #"a"-#"z" #"A"-#"Z"]
; whitespace:  https://tools.ietf.org/html/rfc7230#section-3.2.3
ch_token: make bitset! #{000000005F36FFC07FFFFFE3FFFFFFEA}
ch_ws:    make bitset! #{0040000080}

rl_q-string: [#"^"" any [ #"\" 1 skip | some ch_token ] #"^""]

trim-q-string: func[str][
	parse str [
		remove #"^""
		any [ some ch_token
			| remove #"\" 1 skip
		]
		remove #"^""
	]
]
decode-content-params: func[
	str
	/local params type subtype key val
][
	if none? str [ return reduce [none none] ]
	str: to string! str
	params: clear []
	either parse str [
		any ch_ws
		copy type some ch_token
		opt [#"/" copy subtype some ch_token]
		any [
			any ch_ws
			#";"
			any ch_ws
			  copy key some ch_token #"="
			[ copy val some ch_token
			| copy val rl_q-string (trim-q-string val)]
			( put params key val )
		]
	][
		reduce [type subtype copy params]
	][	str ] ;- returns original in case of error
]

decode-multipart-data: func[
	{Decodes multipart encoded data (used with POST method)}
	type [binary! string! block!] "Content-type header value"
	data [binary!] "Raw data after HTTP header"
	/local boundary header body key val result boundary-end
][	;@@NOTE: this may be used with emails too! Where it should live?

	unless block? type [ type: decode-content-params type ]

	if any [
		not block? type
		not block? type/3
		type/1 <> "multipart"
		not boundary: select type/3 "boundary"
	][ return data ]  ;- returns original in case of error

	boundary-end: join "^M^/--" boundary
	result: copy []
	parse data [
		any [
			"--" boundary CRLF
			(header: copy [])
			some [
				copy key some ch_token #":"
				copy val to CRLF 2 skip
				( put header to string! key decode-content-params val )
			]
			( append/only result header )
			CRLF
			copy body to boundary-end
			( append result either empty? body [none][body] )
			CRLF
		]
		"--" boundary "--" CRLF end
	]
	new-line/all result true
	result
]

to-CLF-idate: func [
	"Returns a standard Common Log Format date string."
	date [date!]
	/local zone
][
	zone: form date/zone
	remove find zone ":"
	if #"-" <> first zone [insert zone #"+"]
	if 4 >= length? zone [insert next zone #"0"]
	ajoin [
		pad/with date/day -2 #"0"
		#"/"
		pick ["Jan" "Feb" "Mar" "Apr" "May" "Jun" "Jul" "Aug" "Sep" "Oct" "Nov" "Dec"] date/month
		#"/"
		date/year
		#":"
		to-itime any [date/time 0:00]
		#" "
		zone
	]
]

make-password-hash: function [
	"Returns a salted PBKDF2 hash of a password (for `users` in the auth config)"
	password [string!]
	/iterations count [integer!] "Default 100000"
][
	count: any [count 100000]
	salt: make binary! 16
	loop 16 [append salt (random/secure 256) - 1]
	ajoin [
		"pbkdf2-sha256$" count #"$" enbase salt 64 #"$"
		enbase pbkdf2 password salt count 32 'sha256 64
	]
]

pbkdf2: function [
;@@ https://github.com/Oldes/Rebol-PBKDF2
    pass   [any-string! binary!] "Password"
    salt   [any-string! binary!] "Salt value"
    count  [integer!] "Number of iterations"
    length [integer!] "Requested length of the result (key) in bytes"
    method [word!]    "Checksum method to be used [sha1 sha256...]"
][
    ;; Convert input password and salt to binary format
    pass: to binary! pass
    salt: to binary! salt
    ;; Initialize output buffer for the derived key
    output: make binary! length
    ;; Initialize last block with the salt
    last: copy salt
    ;; Determine the length of the hash output for the selected method
    hash-len: select [sha1 20 sha224 28 sha256 32 sha384 48 sha512 64] :method
    ;; Calculate the number of hash blocks needed to meet the requested length
    block-cnt: round/ceiling (length / hash-len)

    repeat i block-cnt [
        ;; Write the salt and block index (as big-endian 32-bit) into last
        binary/write last [BYTES :salt UI32BE :i]
        ;; Compute the first hash value (U1) using the password
        last: xorsum: checksum/with last method pass
        ;; Perform additional iterations (U2, U3, ..., Ucount)
        repeat j (count - 1) [
            ;; Compute the next hash in the chain and XOR it with previous results
            xorsum: xorsum xor (last: checksum/with last method pass)
        ]
        ;; Append the derived key fragment to the output buffer
        append output xorsum
    ]
    ;; Crop the output to match the exact requested key length
    copy/part output length
]

rsp-context: context [
	ctx:  none
	app:  none ;= shared application context (write via path: app/x: ...)
	page: none ;= request-local context for page set-words
	print:   func[value][append append ctx/out/content value LF]
	prin:    func[value][append ctx/out/content value]
	probe:   func[value][append ctx/out/content mold value]
	include: func[path [file!] /local dir parts res][
		dir: what-dir
		parts: split-path path
		change-dir parts/1
		set/any 'res try [process/include parts/2 ctx]
		change-dir dir
		if error? :res [do :res] ;; re-throw after restoring the directory
		() ;= unset!
	]
	process: function[data [file! binary! string!] ctx [object!] /include /local res][
		self/ctx: ctx
		out: ctx/out/content
		unless include [
			clear out
			self/app:  ctx/config/app
			self/page: make object! [] ;; fresh per request, shared with includes
		]
		if file? data [data: read/string data]
		parse data [any [
			s: to "<%" e: (append out copy/part s e) 
			2 skip [
				#"@" copy code: to "%>" 2 skip (
					try/with [
						unless all [
							file? file: transcode/one code
							exists? file
						][
							do make error! ajoin ["RSP include not found: " mold :file]
						]
						append out read/string file
					] :probe
				)
				|
				set get?: opt [#"=" | #"@"] copy code: to "%>" 2 skip (
					try/with [
						code: transcode code
						;; page set-words are request-local (added to page in one call)
						unless empty? words: collect-words/set/deep/ignore code words-of page [
							vals: clear []
							foreach w words [repend vals [to set-word! w #(none)]]
							append page vals
						]
						bind code lib          ;; library
						if app [bind code app] ;; app values shadow library
						bind code self         ;; RSP context (print, include, app...)
						bind code page         ;; request-local set-words win
					] :probe
					catch/all/quit [
						set/any 'res try/with :code :lib/probe
						if get? [append out :res]
					]
				)
			]
			| s: to end e: (append out copy/part s e)
		]]
		out
	]
]
protect rsp-context
unprotect in rsp-context 'ctx
unprotect in rsp-context 'app
unprotect in rsp-context 'page


;------------------------------------------------------------------------
;-- Scheme definition:                                                 --
;------------------------------------------------------------------------
sys/make-scheme [
	Title: "HTTP Server"
	Name:  'httpd

	Actor: [
		Open: func [port [port!] /local spec][
			spec: port/spec
			log-more ["Opening server at port:^[[22m" spec/port]
			port/extra: make object! [
				subport: open compose [
					scheme: 'tcp
					port: (spec/port)
				]
				subport/awake: :port/scheme/awake-server
				subport/extra: make object! [
					parent: port
					config: none
					actor:  none
					clients: make block! 16
				]
				;; per-server copy of default actors (customized by serve-http)
				actor: subport/extra/actor: make port/scheme/actor []
				subport/extra/config:
				config: make map! reduce/no-set [
					root:        none
					index:       [%index.html %index.htm]
					keep-alive:  true
					list-dir?:   true
					allow-put:   false ; TRUE, or block of allowed paths (relative to root) like [%/upload/]
					allow-delete: false ; TRUE, or block of allowed paths (relative to root) like [%/upload/]
					server-name: "Rebol3-HTTPd"
					trust-ips:   [127.0.0.1]
					follow-symlinks?: false
					max-header-size:    32768    ; bytes, NONE = unlimited
					max-content-length: 10485760 ; bytes (10MB), NONE = unlimited
					max-websocket-frame: 1048576 ; bytes (1MB), NONE = unlimited
					header-timeout: 0:0:10 ; complete request header must arrive in time, NONE = unlimited
					body-timeout:   0:1:0  ; complete request body must arrive in time, NONE = unlimited
				]
			]
			port/state: port/extra/subport/extra/clients
			port/awake: :awake-client
			port
		]

		Close: func [port [port!] /local ctx file][
			log-more ["Closing server at port:^[[22m" port/spec/port]
			ctx: port/extra
			close ctx/subport
			;; closing log ports if there are any
			if port? file: ctx/config/log-access [ close file ctx/config/log-access: none]
			if port? file: ctx/config/log-errors [ close file ctx/config/log-errors: none]
			port/extra: none
			port
		]

		On-Accept: func [ctx [object!]][ true ]
		On-Header: func [ctx [object!]][] ;= placeholder; user can use it for early request processing

		On-Auth: func [
			"Verifies request credentials; returns the user name, or NONE when not accepted"
			ctx [object!]
			/local auth value type cred key user pass stored
		][
			auth: ctx/config/auth
			unless string? value: select ctx/inp/header 'Authorization [return none]
			unless parse value [copy type: to #" " some #" " copy cred: to end][return none]
			switch type [
				"Bearer" [
					if block? auth/tokens [
						cred: checksum cred 'sha256
						foreach [user stored] auth/tokens [
							if stored == cred [return user]
						]
					]
				]
				"Basic" [
					if block? auth/users [
						;; PBKDF2 is slow by design, so verified credentials are cached
						key: checksum value 'sha256
						if user: auth/cache/:key [return user]
						all [
							cred: attempt [to string! debase cred 64]
							parse cred [copy user: to #":" skip copy pass: to end]
							stored: select/case auth/users user
							Verify-Password pass stored
							(
								if 1000 < length? auth/cache [clear auth/cache]
								put auth/cache key user
							)
							return user
						]
					]
				]
			]
			;= none
		]

		On-Get: func [
			ctx [object!]
			/local target path info index modified If-Modified-Since follow loc
		][
			target: ctx/inp/target
			unless ctx/config/root [
				ctx/actor/On-Not-Found ctx target
				exit
			]
			follow: ctx/config/follow-symlinks?
			unless path: Safe-Path/:follow ctx/config/root target/file [
				ctx/actor/On-Not-Found ctx target
				exit
			]
			target/file: path
			;; directory requested without trailing slash -> redirect, so relative links work
			if all [
				'dir = exists? path
				#"/" <> last path
			][
				loc: to string! ctx/inp/target/original
				insert any [find loc #"?" tail loc] #"/"
				ctx/out/status: 301
				ctx/out/header/Location: loc
				exit
			]
			ctx/out/header/Date: to-idate/gmt now
			ctx/out/status: 200

			if all [
				ctx/config/rsp
				%.rsp == skip tail ctx/inp/target/file -4
			][
				Process-RSP ctx
				exit
			]

			either exists? path [
				if dir? path [
					foreach file ctx/config/index [
						if exists? index: path/:file [
							path: index
							log-debug ["using index file:" index]
							break
						]
					]
					if path <> index [
						; client is requesting directory, which don't have index file
						return ctx/actor/On-List-Dir ctx target
					]
				]
				info: query/mode path [date: size:]
				; prepare modified date of the target
				modified: info/date
				modified/timezone: 0 ; converted to UTC
				modified/second: to integer! modified/second ; remove miliseconds from time

				ctx/state: 'send-data
				ctx/out/status: 200
				ctx/out/target: path
				ctx/out/header/Content-Length: info/size
				ctx/out/header/Last-Modified:  to-idate/gmt info/date
				if ctx/inp/method = "GET" [
					;? ctx/inp/header
					either all [
						date? If-Modified-Since: try [to-date ctx/inp/header/If-Modified-Since]
						If-Modified-Since >= modified
					][
						ctx/out/status: 304 ;= not modified
					][
						ctx/out/content: either info/size <= 32000 [read path][open/read path]
					]
				]
			][
				ctx/actor/On-Not-Found ctx target
			]
		]

		On-Post: func [ctx [object!]][
			;@@ this is just a placeholder!
			true
		]

		On-Read-Post: func[ctx [object!] /local path content header type temp target follow][
			header: ctx/inp/header
			unless find header 'Content-Type [
				;make sure there is any Content-Type
				extend header 'Content-Type "application/octet-stream"
			]
			unless content: Read-Body ctx [exit]
			ctx/state: 'data-received
			ctx/out/status: 200
			type: header/Content-Type: decode-content-params header/Content-Type
			;@@ TODO: handle charset if not utf-8!
			case [
				type/2 = "x-www-form-urlencoded" [
					; using full target decoder, although only values are needed
					temp: decode-target/query content
					ctx/inp/content: reduce [
						select temp 'values   ; parsed [key value ...]
						select temp 'original ; raw data
					]
				]
				type/2 = "json" [
					ctx/inp/content: decode 'json content
				]
				type/1 = "multipart" [
					ctx/inp/content: decode-multipart-data type content
				]
				type/1 = "text" [
					ctx/inp/content: to string! content
				]
			]
			either all [
				ctx/config/rsp
				%.rsp == skip tail ctx/inp/target/file -4
				ctx/config/root
			][
				target: ctx/inp/target
				follow: ctx/config/follow-symlinks?
				either path: Safe-Path/:follow ctx/config/root target/file [
					target/file: path
					ctx/out/header/Date: to-idate/gmt now
					ctx/out/status: 200
					Process-RSP ctx
				][
					ctx/actor/On-Not-Found ctx target
				]
			][
				ctx/actor/On-Post ctx
			]
		]

		On-Read-Put: func [ctx [object!] /local content][
			unless content: Read-Body ctx [exit]
			ctx/state: 'data-received
			ctx/inp/content: content ;= raw binary
			ctx/actor/On-Put ctx
		]

		On-Put: func [
			"Default PUT handler: stores the body as a file in root (requires allow-put)"
			ctx [object!]
			/local target allow follow path dir existed?
		][
			target: ctx/inp/target
			unless all [allow: ctx/config/allow-put ctx/config/root][
				ctx/out/status: 405 ; Method Not Allowed
				ctx/out/header/Allow: Allowed-Methods ctx
				exit
			]
			unless Path-Allowed? allow target/file [
				ctx/out/status: 403 ; Forbidden
				exit
			]
			follow: ctx/config/follow-symlinks?
			;; the file may not exist yet, so check its parent directory as well
			unless all [
				path: Safe-Path/:follow ctx/config/root target/file
				dir:  Safe-Path/:follow ctx/config/root first split-path target/file
			][
				ctx/out/status: 403 ; Forbidden
				exit
			]
			if any [
				#"/" = last path
				'dir = exists? path
				'dir <> exists? dir
			][
				ctx/out/status: 409 ; Conflict (target is a directory or parent doesn't exist)
				exit
			]
			existed?: exists? path
			write path ctx/inp/content
			ctx/out/status: either existed? [204][201] ; No Content | Created
		]

		On-Delete: func [
			"Default DELETE handler: removes a file in root (requires allow-delete)"
			ctx [object!]
			/local target allow follow path
		][
			target: ctx/inp/target
			unless all [allow: ctx/config/allow-delete ctx/config/root][
				ctx/out/status: 405 ; Method Not Allowed
				ctx/out/header/Allow: Allowed-Methods ctx
				exit
			]
			unless Path-Allowed? allow target/file [
				ctx/out/status: 403 ; Forbidden
				exit
			]
			follow: ctx/config/follow-symlinks?
			unless path: Safe-Path/:follow ctx/config/root target/file [
				ctx/out/status: 403 ; Forbidden
				exit
			]
			switch/default exists? path [
				file [
					delete path
					ctx/out/status: 204 ; No Content
				]
				dir [ctx/out/status: 409] ; Conflict (directories are not deleted)
			][
				ctx/out/status: 404 ; Not Found
			]
		]

		On-Options: func [
			"Default OPTIONS handler: lists allowed methods"
			ctx [object!]
		][
			ctx/out/status: 204 ; No Content
			ctx/out/header/Allow: Allowed-Methods ctx
		]

		On-Read-Patch: func [ctx [object!] /local content][
			;; read the body even when not handled, so the connection stays in sync
			unless content: Read-Body ctx [exit]
			ctx/state: 'data-received
			either any-function? get in ctx/actor 'On-Patch [
				ctx/inp/content: content ;= raw binary
				ctx/actor/On-Patch ctx
			][
				ctx/out/status: 405 ; Method Not Allowed
				ctx/out/header/Allow: Allowed-Methods ctx
			]
		]

		On-Read: func[
			"Process READ action on client's port"
			ctx [object!]
		][
			switch/default ctx/inp/method [
				"HEAD" ; same like GET, but without sending any content
				"GET"     [ ctx/actor/on-get        ctx ]
				"POST"    [ ctx/actor/on-read-post  ctx ]
				"PUT"     [ ctx/actor/on-read-put   ctx ]
				"PATCH"   [ ctx/actor/on-read-patch ctx ]
				"DELETE"  [ ctx/actor/on-delete     ctx ]
				"OPTIONS" [ ctx/actor/on-options    ctx ]
			][
				ctx/state: 'data-received
				ctx/out/status: 405 ; Method Not Allowed
				ctx/out/header/Allow: Allowed-Methods ctx
			]
		]

		On-Read-Websocket: func[
			"Process READ action on client's port using websocket"
			ctx [object!]
			final? [logic!]   "Indicates that this is the final fragment in a message."
			opcode [integer!] "Defines the interpretation of the 'Payload data'."
		][
			;@@ this is just a placeholder!
		]

		On-Close-Websocket: func[
			"Process CLOSE action on client's port using websocket"
			ctx [object!] code [integer!]
			/local reason
		][
			reason: any [
				select [
					1000 "the purpose for which the connection was established has been fulfilled."
					1001 "a browser navigated away from a page."
					1002 "a protocol error."
					1003 "it has received a type of data it cannot accept."
					1007 "it has received data within a message that was not consistent with the type of the message."
					1008 "it has received a message that violates its policy."
					1009 "it has received a message that is too big for it to process."
					1010 "it has expected the server to negotiate one or more extension, but the server didn't return them in the response message of the WebSocket handshake."
					1011 "it encountered an unexpected condition that prevented it from fulfilling the request."
				] code
				ajoin ["an unknown reason (" code ")"]
			]
			log-info ["WS connection is closing because" reason]
			unless empty? reason: ctx/inp/content [
				;; optional client's reason
				log-info ["Client's reason:" as-red to string! reason]
			]
		]

		On-List-Dir: func[
			ctx [object!] target [object!]
			/local path dir out size date files dirs name
		][
			unless ctx/config/list-dir? [
				log-more ["Listing dir not allowed:^[[1m" mold target/file]
				ctx/out/status: 404 ; using not-found response!
				return false
			]
			dir: target/file
			path: join "/" find/match/tail dir ctx/config/root
		
			try/with [
				out: make string! 2000
				append out ajoin [
					{<html><head><title>Index of } Escape-HTML path
					{</title></head><body bgcolor="white"><h1>Index of } Escape-HTML path
					{</h1><hr><pre>^/}
				]
				unless empty? path [
					append out {<a href="../">../</a>^/}
				]

				files: copy []
				dirs:  copy []
				foreach file read dir [
					; reversed naming bellow intended!
					append either dir? file [files][dirs] file
				]
				append files dirs

				foreach file files [
					set [size date] query/mode dir/:file [:size :date]
					name: form file
					append out ajoin [
						{<a href="} enhex name {">} Escape-HTML name {</a> }
						pad copy "" 50 - length? name
						pad/with date/date  -11 #"0" #" "
						pad/with date/hour   -2 #"0" #":"
						pad/with date/minute -2 #"0" #" "
						pad  any [size "-"] -19
						lf
					]
				]
				append out {</pre><hr></body></html>}
				ctx/out/content: out
				return true
			][
				print system/state/last-error
				ctx/out/status: 404 ; using not-found response!
				return false
			]
		]

		On-Not-Found: func[ctx [object!] target [object!]][
			log-more ["Target not found:^[[1m" mold target/file]
			ctx/out/status: 404
		]

		WS-handshake: func[ctx /local key][
			if all [
				"websocket" = select ctx/inp/header 'Upgrade
				key: select ctx/inp/header 'Sec-WebSocket-Key
			][
				ctx/out/status: 101
				ctx/out/header/Upgrade: "websocket"
				ctx/out/header/Connection: "Upgrade"
				ctx/out/header/Sec-WebSocket-Accept: enbase checksum join key "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" 'sha1 64
			]
		]
	]

	Status-Codes: make map! [
		100 "Continue"
		101 "Switching Protocols"
		200 "OK"
		201 "Created"
		202 "Accepted"
		203 "Non-Authoritative Information"
		204 "No Content"
		205 "Reset Content"
		206 "Partial Content"
		207 "Multi-Status"
		208 "Already Reported"
		226 "IM Used"
		300 "Multiple Choices"
		301 "Moved Permanently"
		302 "Found"              ;Previously "Moved temporarily"
		303 "See Other"          ;since HTTP/1.1
		304 "Not Modified"       ;RFC 7232
		305 "Use Proxy"          ;since HTTP/1.1
		306 "Switch Proxy"
		307 "Temporary Redirect" ;since HTTP/1.1
		308 "Permanent Redirect" ;RFC 7538
		400 "Bad Request"
		401 "Unauthorized"
		402 "Payment Required"
		403 "Forbidden"
		404 "Not Found"
		405 "Method Not Allowed"
		406 "Not Acceptable"
		407 "Proxy Authentication Required"
		408 "Request Timeout"
		409 "Conflict"
		410 "Gone"
		411 "Length Required"
		412 "Precondition Failed"
		413 "Payload Too Large"
		414 "URI Too Long"
		415 "Unsupported Media Type"
		416 "Range Not Satisfiable"
		417 "Expectation Failed"
		418 "I'm a teapot"
		421 "Misdirected Request"
		422 "Unprocessable Entity"
		423 "Locked"
		424 "Failed Dependency"
		426 "Upgrade Required"
		428 "Precondition Required"
		429 "Too Many Requests"
		431 "Request Header Fields Too Large"
		451 "Unavailable For Legal Reasons" ;@@ https://tools.ietf.org/html/rfc7725
		500 "Internal Server Error"
		501 "Not Implemented"
		502 "Bad Gateway"
		503 "Service Unavailable"
		504 "Gateway Timeout"
		505 "HTTP Version Not Supported"
		506 "Variant Also Negotiates"
		507 "Insufficient Storage"
		508 "Loop Detected"
		510 "Not Extended"
		511 "Network Authentication Required"
	]

	Status-Line: func [
		"Returns HTTP response status line"
		version [string!] status [integer!]
	][
		ajoin ["HTTP/" version #" " status #" " any [status-codes/:status "Unknown"] CRLF]
	]

	Keep-Alive?: func [
		"Returns TRUE if the client connection should be kept open after the current request"
		ctx [object!]
		/local ka h conn
	][
		all [
			block? ka: ctx/config/keep-alive ;= [timeout max-requests] (normalized in Respond)
			not ctx/done?                    ; server is going to stop
			ctx/state <> 'error              ; after 413/431/500 the input stream is unreliable
			ctx/requests + 1 < ka/2          ; limit to max requests
			object? h: ctx/inp/header
			conn: any [select h 'Connection ""]
			either ctx/inp/version = "1.0" [
				find conn "keep-alive"       ; HTTP/1.0: close unless asked to keep
			][	not find conn "close" ]      ; HTTP/1.1: keep unless asked to close
			true
		]
	]
	
	Safe-Path: func [
		"Joins a request path with root; returns NONE if it could escape the root"
		root [file!] "Must be canonical (from to-real-file)"
		file [file!]
		/follow "Allow symbolic links that resolve outside of root"
		/local p full real
	][
		;; backslash is a separator on Windows; NUL truncates paths in the OS layer
		if any [find file #"\" find file #"^@"] [return none]
		p: clean-path/only file
		foreach seg split p #"/" [
			if ".." == to string! seg [return none]
		]
		if #"/" == first p [p: next p]
		full: join root p
		;; resolve symlinks; NONE means the file doesn't exist (POSIX) -> let caller 404
		if all [not follow real: to-real-file full][
			if dir? full [real: dirize real] ;; POSIX result has no trailing slash
			unless find/match real root [return none]
		]
		full
	]

	Verify-Password: function [
		"Checks a password against a hash made by `make-password-hash`"
		pass [string!] stored [string!]
	][
		all [
			parse stored [
				"pbkdf2-" copy method: to #"$" skip
				copy count: to #"$" skip
				copy salt:  to #"$" skip
				copy hash:  to end
			]
			attempt [
				hash: debase hash 64
				hash == pbkdf2 pass debase salt 64 to integer! count length? hash to word! method
			]
		]
	]

	Client-IP: func [
		"Returns client's IP; X-Real-IP is used only when sent by a trusted proxy"
		ctx [object!]
		/local ip ips
	][
		either all [
			ip: attempt [to tuple! select ctx/inp/header 'X-Real-IP]
			4 = length? ip
			any [none? ips: ctx/config/trust-ips find ips ctx/remote-ip]
		][ip][ctx/remote-ip]
	]

	Auth-Blocked?: func [
		"Returns seconds until the client's lockout ends, or NONE when not locked out"
		ctx [object!]
		/local auth f
	][
		auth: ctx/config/auth
		all [
			integer? auth/max-failures
			f: auth/failures/(Client-IP ctx)
			f/1 >= auth/max-failures
			now < f/2
			max 1 to integer! difference f/2 now
		]
	]

	Auth-Failed: func [
		"Counts a failed authentication of the client"
		ctx [object!]
		/local auth ip f
	][
		auth: ctx/config/auth
		ip: Client-IP ctx
		if any [none? f: auth/failures/:ip  now >= f/2][
			if 10000 < length? auth/failures [
				;; drop expired entries (rebuilt, so no `remove/key` is used)
				active: make map! 64
				foreach [key val] auth/failures [
					if now < val/2 [active/:key: val]
				]
				auth/failures: active
			]
			auth/failures/:ip: f: reduce [0 now]
		]
		f/1: f/1 + 1
		f/2: now + auth/lockout ;; each failure extends the lockout
	]

	Auth-Required?: func [
		"Returns TRUE if the request must be authenticated"
		ctx [object!] /local auth
	][
		all [
			auth: ctx/config/auth
			ctx/inp/method <> "OPTIONS" ;; CORS preflight never carries credentials
			any [
				find auth/methods to word! ctx/inp/method
				all [block? auth/paths Path-Allowed? auth/paths ctx/inp/target/file]
			]
			true
		]
	]

	Auth-Challenge: func [
		"Returns value for the `WWW-Authenticate` response header"
		auth [object!]
	][
		ajoin [
			if auth/tokens [ajoin [{Bearer realm="} auth/realm {"}]]
			if all [auth/tokens auth/users][", "]
			if auth/users  [ajoin [{Basic realm="} auth/realm {", charset="UTF-8"}]]
		]
	]

	Path-Allowed?: func [
		"Returns TRUE if the request path is in a location allowed by an allow-put/allow-delete setting"
		allow [logic! block! none!] "TRUE, or block of allowed paths like [%/upload/]"
		file  [file!]
		/local p
	][
		unless block? allow [return true? allow]
		;; cleaned path, so `/upload/../x` can't pass as `/upload/`
		p: clean-path/only file
		foreach dir allow [
			if find/match/case p dir [return true]
		]
		false
	]

	Allowed-Methods: func [
		"Returns value for the `Allow` response header (for the requested path)"
		ctx [object!]
		/local file root
	][
		file: ctx/inp/target/file
		root: ctx/config/root
		ajoin [
			"OPTIONS, GET, HEAD, POST"
			if all [root Path-Allowed? ctx/config/allow-put    file][", PUT"]
			if all [root Path-Allowed? ctx/config/allow-delete file][", DELETE"]
			;; PATCH has no default behavior, so only when a handler is provided
			if any-function? get in ctx/actor 'On-Patch [", PATCH"]
		]
	]

	Escape-HTML: function/with [
		"Escapes text for use in HTML content or attribute values"
		text [any-string!]
	][
		out: make string! length? text
		parse text [collect into out any [
			keep some normal-chars
			| #"&"  keep ("&amp;")
			| #"<"  keep ("&lt;")
			| #">"  keep ("&gt;")
			| #"^"" keep ("&quot;")
		]]
		out
	][
		normal-chars: #(bitset! not #{000000002200000A}) ;= complement charset {&<>"}
	]

	Decode-Chunked: function/with [
		"Decodes chunked transfer-encoded data; returns [body end], NONE if incomplete, or 'invalid"
		data [binary!]
	][
		out: make binary! length? data
		pos: data
		forever [
			if tail? pos [return none]
			unless parse pos [copy size: 1 8 hex-digits to end][return 'invalid]
			unless p: find/tail pos CRLF [return none] ;; size line incomplete (may have extensions)
			n: 0 foreach c size [n: n * 16 + either c > 64 [(c and 7) + 9][c - 48]] ;; hex -> integer
			pos: p
			if zero? n [
				;; optional trailer fields end with an empty line
				return either parse pos [[#{0D0A} | thru #{0D0A0D0A}] e: to end][
					reduce [out e]
				][	none ] ;; incomplete
			]
			if (length? pos) < (n + 2) [return none]
			append out copy/part pos n
			pos: skip pos n
			unless pos: find/match/tail pos #{0D0A} [return 'invalid]
		]
	][
		hex-digits: #(bitset! #{000000000000FFC07E0000007E}) ;= charset "0123456789abcdefABCDEF"
	]

	Read-Body: func [
		"Returns the complete request body, or NONE when more data is needed or on error (see ctx/state)"
		ctx [object!]
		/local header content length res limit
	][
		header:  ctx/inp/header
		content: ctx/inp/content
		length:  select header 'Content-Length
		if "chunked" = select header 'Transfer-Encoding [
			res: Decode-Chunked content
			limit: ctx/config/max-content-length
			case [
				word? res [ ;; invalid chunk framing
					ctx/state: 'error
					ctx/out/status: 400 ; Bad Request
					return none
				]
				none? res [ ;; incomplete
					either all [limit limit < length? content][
						ctx/state: 'error
						ctx/out/status: 413 ; Payload Too Large
					][	ctx/state: 'read-data ]
					return none
				]
				all [limit limit < length? res/1][
					ctx/state: 'error
					ctx/out/status: 413
					return none
				]
			]
			ctx/body-end: res/2 ;; for pipelining
			return res/1
		]
		if integer? length [
			if length > length? content [
				ctx/state: 'read-data
				return none
			]
			;; don't include bytes of a pipelined next request
			content: copy/part content length
		]
		content
	]

	Respond: function [port [port!]][
		ctx: port/extra
		out: ctx/out
		unless out/status [out/status: 200] ;; expect OK response if not set
		log-more ["Respond:^[[22m" out/status length? out/content]
		; send the response header
		buffer: make binary! 1024
		append buffer Status-Line ctx/inp/version out/status

		either "websocket" = out/header/upgrade [
			ctx/inp/method: "websocket"
			try [ctx/inp/version: to integer! ctx/inp/header/Sec-WebSocket-Version]
			port/awake: :Awake-Websocket
		][
			unless out/header/Content-Type [
				if out/target [
					out/header/Content-Type: mime-type? out/target
				]
				if all [
					none? out/header/Content-Type ; no mime found above
					string? out/content
				][
					out/header/Content-Type: "text/html; charset=UTF-8"
				]
			]		

			out/header/Content-Length: case [
				out/content [
					if string? out/content [
						;; must be converted to binary to have proper length if not ascii
						out/content: to binary! out/content
					]
					length? out/content
				]
				;; HEAD on a file: keep the size set by On-Get
				ctx/inp/method = "HEAD" [any [out/header/Content-Length 0]]
				'else [0]
			]
			if ctx/inp/method = "HEAD" [
				;; HEAD must never send a body
				if port? out/content [close out/content]
				out/content: none
			]
			if any [out/status < 200 out/status = 204][
				;; these responses must not have a body or Content-Length
				out/header/Content-Length: none
				if port? out/content [close out/content]
				out/content: none
			]

			if logic? keep-alive: ctx/config/keep-alive [
				if keep-alive [
					; using defaults
					ctx/config/keep-alive: [15 100] ; [timeout max-requests]
				]
			]
			either Keep-Alive? ctx [
				keep-alive: ctx/config/keep-alive
				ctx/out/header/Connection: "keep-alive"
				ctx/out/header/Keep-Alive: ajoin ["timeout=" keep-alive/1 ", max=" keep-alive/2]
			][
				ctx/out/header/Connection: "close"
			]
			out/header/Server: ctx/config/server-name
		]
		
		;probe out/header
		foreach [name value] out/header [
			if value [
				append buffer ajoin [mold to set-word! name #" " value CRLF]
			]
		]
		;print to-string buffer
		append buffer CRLF

		if all [out/content not port? out/content] [
			append buffer out/content
			out/content: none
		]

		try/with [
			write port buffer
		][
			log-error "Write failed!"
			;probe copy/part buffer 100
			Awake-Client make event! [type: 'close port: port ]
		]
	]

	Write-log: function [ctx][
		try/with [
			remote-ip: Client-IP ctx
			;? ctx/inp
			msg: ajoin [
				remote-ip
				{ - - [} to-CLF-idate now {] "}
				any [ctx/inp/method #"-"] #" "
				either ctx/inp/target [to string! ctx/inp/target/original][#"-"]
				" HTTP/" ctx/inp/version {" }
				ctx/out/status #" "
				any [ctx/out/header/Content-Length #"-"]
				#"^/"
			]
			if :log-info [prin msg]
			if port? file: ctx/config/log-access [
				write/append file msg
			]
			if all [
				ctx/out/status >= 400
				port? file: ctx/config/log-errors
			][
				write/append file msg
			]
		][
			log-error "Failed to write a log"
			log-error system/state/last-error
		]
	]

	Awake-Client: wrap [
		chars-method: charset [#"A" - #"Z"]
		;from-method: ["GET" | "POST" | "HEAD" | "PUT" | "DELETE" | "TRACE" | "CONNECT" | "OPTIONS"]
		chars: complement union space: charset " " charset [#"^@" - #"^_"]
		CRLF2BIN: #{0D0A0D0A}

		function [
			event [event!]
		][
			port: event/port
			ctx: port/extra
			inp: ctx/inp
			out: ctx/out

			log-more ["Awake:^[[1m" ctx/remote "^[[22m" event/type]

			ctx/timeout: now + 0:0:15

			switch event/type [
				READ [
					log-more ["bytes:^[[1m" length? port/data]
					;; first bytes of a new request: start the header deadline
					if all [none? ctx/state none? ctx/deadline t: ctx/config/header-timeout][
						ctx/deadline: now + t
					]
					header-end: find/tail port/data CRLF2BIN
					case [
						all [ctx/deadline now > ctx/deadline][
							log-more ["Request timeout:^[[22m" ctx/remote]
							inp/version: any [inp/version "1.1"]
							ctx/state: 'error
							out/status: 408 ; Request Timeout
							respond port
						]
						all [
							none? ctx/state ;; still reading the request header
							limit: ctx/config/max-header-size
							limit < either header-end [(index? header-end) - 1][length? port/data]
						][
							log-more ["Request header too large:^[[22m" ctx/remote]
							inp/version: any [inp/version "1.1"]
							ctx/state: 'error
							out/status: 431 ; Request Header Fields Too Large
							respond port
						]
						'else [
							either header-end [
								try/with [
									if none? ctx/state [
										with inp [
											parse copy/part port/data header-end [
												copy method some chars-method some space
												copy target some chars        some space
												"HTTP/" copy version some chars thru CRLF
												copy header to end
												(
													method:  to string! method
													target:  decode-target target
													version: to string! version
													header:  construct header
													try [header/Content-Length: to integer! header/Content-Length]
												)
											]
											content: header-end
										]
										log-more ["Request header:^[[22m" ctx/inp/method mold ctx/inp/header]
										;; header complete: the body has its own deadline
										ctx/deadline: if t: ctx/config/body-timeout [now + t]
										either all [
											integer? len: select inp/header 'Content-Length
											limit: ctx/config/max-content-length
											len > limit
										][
											log-more ["Request content too large:^[[22m" len]
											ctx/state: 'error
											out/status: 413 ; Payload Too Large
										][
											; on-header actor may be used for rewrite rules (redirection)
											ctx/actor/on-header ctx
											if all [none? ctx/out/status Auth-Required? ctx][
												creds: select inp/header 'Authorization
												case [
													;; lockout applies only to login attempts, so a browser still gets the 401 challenge
													all [creds secs: Auth-Blocked? ctx][
														ctx/state: 'error
														out/status: 429 ; Too Many Requests
														out/header/Retry-After: secs
													]
													ctx/user: ctx/actor/On-Auth ctx [
														;; successful login resets the failure counter
														if f: ctx/config/auth/failures/(Client-IP ctx) [f/1: 0]
													]
													'else [
														;; requests without credentials (browser's first try) are not counted
														if creds [Auth-Failed ctx]
														ctx/state: 'error ;; don't keep the connection, a body may follow
														out/status: 401 ; Unauthorized
														out/header/WWW-Authenticate: Auth-Challenge ctx/config/auth
													]
												]
											]
										]
									]
									; if status is already defined (probably redirection), respond without reading
									unless ctx/out/status [ctx/actor/on-read port/extra]
								][
									log-error system/state/last-error
									ctx/state: 'error
									ctx/out/status: 500 ; Internal Server Error
								]
								log-debug ["State:^[[1m" ctx/state "^[[22mstatus:^[[1m" out/status]
								either ctx/state = 'read-data [
									; posted data not fully read
									either all [
										not ctx/continued?
										inp/version = "1.1"
										"100-continue" = select inp/header 'Expect
									][
										;; client waits for permission to send the body
										ctx/continued?: true
										ctx/state: 'continue
										ctx/pending: head port/data ;; `write` replaces port/data
										write port ajoin [Status-Line inp/version 100 CRLF]
									][	read port ]
								][
									;; `write` replaces port/data, so keep bytes of a pipelined next request
									if all [ctx/state <> 'error object? inp/header][
										len: select inp/header 'Content-Length
										end: case [
											ctx/body-end        [ctx/body-end]
											integer? len        [skip header-end len]
	   										find ["POST" "PUT" "PATCH"] inp/method [tail header-end] ;; no length: all data is content
											'else               [header-end]
										]
										ctx/pending: unless tail? end [copy end]
									]
									respond port
								]
							][
								; request header not yet fully received
								read port
							]
						]
					]
				]
				WROTE [
					either ctx/state = 'continue [
						;; interim `100 Continue` sent; restore received data and read the body
						ctx/state: 'read-data
						port/data: ctx/pending
						ctx/pending: none
						read port
					][
						either all [
							out
							out/content
						][
							case [
								port? out/content [
									; streaming output from port
									buffer: copy/part out/content 32000 ;4096
									either empty? buffer [
										; end of stream
										close out/content ; closing source port
										End-Client port
									][
										try/with [
											write port buffer
										][
											log-error  "Write failed (2)!"
											End-Client port
										]
									]
								]
								true [
									; for now just remove the content which is written
									; no need to chunk data manually, these are handled internaly
									; just make sure you don't use any other `write` before receiving `wrote`
									write port out/content
									out/content: none
								]
							]
						][
							; there is no other content to write, so close connection
							End-Client port
						]
					]
					port
				]
				CLOSE [
					log-more ["Closing:^[[22m" ctx/remote]
					if pos: find ctx/parent/extra/clients port [ remove pos ]
					close port
				]
			]
		]
	]

	Awake-Server: func [event [event!] /local ctx client config] [
		log-debug ["Awake (server):^[[22m" event/type]
		switch event/type [
			ACCEPT [ New-Client event/port ]
			CLOSE  [
				close event/port
				close event/port/extra/parent
			]
		]
		true
	]

	WS-Encode-Frame: function [
		"Encodes data as a single final, unmasked WebSocket text frame (server -> client)"
		data [binary! string!]
		/op "Use given opcode instead of text frame (1)"
			code [integer!]
	][
		hdr:  128 or any [code 1] ;; FIN bit + opcode
		data: to binary! data
		len:  length? data
		bin:  binary len + 10
		binary/write bin case [
			len < 126    [[UI8 :hdr UI8          :len :data]]
			len <= 65535 [[UI8 :hdr UI8 126 UI16 :len :data]]
			'else        [[UI8 :hdr UI8 127 UI64 :len :data]]
		]
		head bin/buffer
	]

	Awake-Websocket: function [
		event [event!]
	][
		port: event/port
		ctx: port/extra

		log-more ["Awake Websocket:^[[1m" ctx/remote "^[[22m" event/type]

		ctx/timeout: now + 0:0:30

		switch event/type [
			READ [
				ready?: false
				replies: make binary! 64
				data: head port/data
				log-more ["bytes:^[[1m" length? data]
				try/with [
					while [2 <= length? data][
						final?: data/1 & 2#10000000 = 2#10000000
						opcode: data/1 & 2#00001111
						mask?:  data/2 & 2#10000000 = 2#10000000
						len:    data/2 & 2#01111111
						data: skip data 2
						;? final? ? opcode ? len

						;@@ Not removing bytes until we make sure, that there is enough data!
						case [
							len = 126 [
								if 2 > length? data [break]
								len: binary/read data 'UI16
								data: skip data 2
							]
							len = 127 [
								if 8 > length? data [break]
								len: binary/read data 'UI64
								data: skip data 8
							]
						]
						;; reject oversized frames before buffering their payload
						if all [
							limit: ctx/config/max-websocket-frame
							len > limit
						][
							log-more ["WS frame too large:^[[22m" len]
							ctx/state: 'closing
							write port WS-Encode-Frame/op #{03F1} 8 ;; close frame, 1009 = message too big
							exit
						]
						;; mask (if any) + payload must be fully buffered before we touch it
						if (length? data) < (len + pick [4 0] mask?) [break]
						data: truncate data ;; removes already processed bytes from the head
						either mask? [
							masks:   take/part data 4
							request-data: masks xor take/part data len
						][
							request-data: take/part data len
						]
						ready?: true
						ctx/inp/content: truncate/part request-data len
						if opcode = 8 [
							log-more "WS Connection Close Frame!"
							code: 0
							if all [
								2 <= len
								2 <= length? request-data
							][
								code: to integer! take/part request-data 2
								log-more ["WS Close reason:" as-red code]
							]
							ctx/actor/On-Close-Websocket ctx code
							event/type: 'CLOSE
							Awake-Websocket event
							exit
						]
						case [
							opcode = 9  [append replies WS-Encode-Frame/op ctx/inp/content 10] ;; ping -> pong
							opcode = 10 [] ;; unsolicited pong, ignore
							'else [
								ctx/actor/On-Read-Websocket ctx final? opcode
								if all [
									series? content: ctx/out/content
									not empty? content
								][
									append replies WS-Encode-Frame/op content either binary? content [2][1]
									clear content
								]
							]
						]
					]
				][
					log-error system/state/last-error
				]
				either ready? [
					;; `write` replaces port/data, so keep any unprocessed (partial frame) bytes
					ctx/pending: head port/data
					write port either empty? replies [""][replies]
				][
					;; needs more data!
					read port
				]
			]
			WROTE [
				either ctx/state = 'closing [
					;; close frame sent, drop the connection
					Awake-Websocket make event! [type: 'CLOSE port: port]
				][
					if ctx/pending [
						port/data: ctx/pending ;; next read appends to the partial frame
						ctx/pending: none
					]
					read port
				]
			]
			CLOSE [
				log-more ["Closing:^[[22m" ctx/remote]
				if pos: find ctx/parent/extra/clients port [ remove pos ]
				close port
			]
		]
	]


	New-Client: func[port [port!] /local client info err ctx][
		client: first port
		info: query/mode client [remote-ip: remote-port: local-ip: local-port:]
		client/extra: ctx: context [
			state: none
			parent: port
			remote-ip: info/remote-ip
			remote: rejoin [tcp:// info/remote-ip #":" info/remote-port]
			inp: object [
				method:
				version:
				target:
				header:
				content: none
			]
			out: object [
				Status: none
				Header: make map! 12
				Target: none
				Content: none
			]
			config: none
			actor: none
			timeout: none
			done?: none
			requests: 0   ; number of already served requests per connection
			pending: none ; unprocessed input bytes to restore after a write
			continued?: none ; `100 Continue` already sent for the current request
			body-end: none   ; end of a chunked request body (for pipelining)
			user: none       ; authenticated user (set by On-Auth)
			deadline: none   ; time by which the current request header/body must be received
			started: stats/timer
		]
		;? port
		ctx/config: port/extra/config
		ctx/actor:  port/extra/actor
		
		unless ctx/actor/On-Accept ctx [
			; connection not allowed
			log-more ["Client not accepted:^[[22m" info/remote-ip]
			close client
			return false
		]
		client/awake: :Awake-Client
		append port/extra/clients client



		log-more ["New client:^[[1;31m" ctx/remote]
		try/with [read client][
			log-error ["Failed to read new client:" ctx/remote]
			log-error system/state/last-error
		]
	]

	End-Client: function [port [port!]][
		ctx: port/extra
		Write-log ctx
		clients: ctx/parent/extra/clients
		keep-alive: ctx/config/keep-alive
		
		either all [
			open? port
			Keep-Alive? ctx
		][
			ctx/requests: ctx/requests + 1
			log-more ["Keep-alive:^[[22m" ctx/remote "requests:" ctx/requests]
			; reset client state
			foreach v ctx/inp [ctx/inp/:v: none]
			foreach v ctx/out [ctx/out/:v: none]
			ctx/out/Header: make map! 12
			ctx/state: ctx/continued?: ctx/body-end: ctx/user: ctx/deadline: none
			; set new timeout
			ctx/timeout: now + to time! ctx/config/keep-alive/1 
			either pending: ctx/pending [
				ctx/pending: none
				port/data: pending
				either find pending #{0D0A0D0A} [
					;; complete pipelined request already received
					Awake-Client make event! [type: 'read port: port]
				][	read port ]
			][	read port ]
		][
			; close client connection
			Awake-Client make event! [type: 'CLOSE port: port]
			;try [remove find clients port]
		]
		log-debug ["Ports open:" length? clients]
		if ctx/done? [
			log-more "Server's job done, closing initiated"
			;; close remaining (idle) connections, so they don't block the stop
			foreach client copy clients [
				Awake-Client make event! [type: 'CLOSE port: client]
			]
			ctx/parent/data: ctx/done?
			Awake-Server make event! [type: 'CLOSE port: ctx/parent]
		]
	]

	Check-Clients: func[
		"Checks if some client connection isn't open too long time"
		port [port!] /local tm tmc
	][
		tm: now
		;log-debug ["Check-Clients:" length? port/state #"-" now]
		if block? port/state [
			foreach client reverse copy port/state [
				;log-debug ["Checking:" client/extra/remote client/extra/timeout]
				try [
					if all [
						date? tmc: client/extra/timeout
						tm >= tmc
					][
						Awake-Client make event! [type: 'CLOSE port: client]
					]
				]
			]
		]
	]

	Process-RSP: func[ctx [object!] /local dir path res][
		log-more ["RSP input:^[[1m" mold ctx/inp/target/file]
		path: ctx/inp/target/file
		unless exists? path [
			ctx/actor/On-Not-Found ctx ctx/inp/target
			exit
		]
		dir: what-dir
		ctx/out/content: make string! 1000
		change-dir first split-path path
		set/any 'res try [rsp-context/process path ctx]
		change-dir dir
		if error? :res [
			ctx/out/content: none ;; don't send partial output with the 500 response
			do :res
		]
		unless ctx/out/header/Content-Type [
			ctx/out/header/Content-Type: "text/html; charset=UTF-8"
		]
	]

	anti-hacking-rules: [
		some [
			;; common scripts, which we don't use
			  #"." [
			  	  %php
			  	| %aspx
			  	| %cgi
			][end | #"?" | #"#"] reject
			; common hacking attempts to root folders...
			| #"/" [
				  %ecp/      ; we are not an exchange server
				| %mifs/     ; either not MobileIron (https://stackoverflow.com/questions/67901776/what-does-the-line-mifs-services-logservice-mean)
				| %GponForm/ ; nor Gpon router (https://www.vpnmentor.com/blog/critical-vulnerability-gpon-router/)
				| %.env end  ; https://stackoverflow.com/questions/64109005/do-these-env-get-requests-from-localhost-indicate-an-attack
			] reject
			| 1 skip
		]
	]

	;=====================================================================
	log-error: log-info: log-more: log-debug: none
	set-verbose: func[verbose [integer!]][
		log-error: log-info: log-more: log-debug: none
		case/all [
			verbose >= 0 [log-error: func[msg][sys/log/error 'HTTPD :msg]]
			verbose >= 1 [log-info:  func[msg][sys/log/info  'HTTPD :msg]]
			verbose >= 2 [log-more:  func[msg][sys/log/more  'HTTPD :msg]]
			verbose >= 3 [log-debug: func[msg][sys/log/debug 'HTTPD :msg]]
		]
		system/options/log/httpd: verbose
	]
	set-verbose system/options/log/httpd
]

http-server: function [
	"Initialize simple HTTP server (DEPRECATED)"
	port  [integer!]        "Port to listen"
	/config                 "Possibility to change default settings"
	spec  [block! object!]  "Can hold: root, index, keep-alive, server-name"
	/actor  "Redefine default actor actions"
	actions [block! object!] "Functions like: On-Get On-Post On-Post-Received On-Read On-List-Dir On-Not-Found"
	/no-wait "Will not enter wait loop"
][
	sys/log/error 'HTTPD "`http-server` function is deprecated, use `serve-http` instead!"
	spec: either config [
		either block? spec [copy spec][to block! spec]
	][	copy [] ]
	if actor [extend spec 'actor actions]
	extend spec 'port port
	serve-http/:no-wait spec
]

serve-http: function [
	"Initiate a HTTP server and handle HTTP requests"
	spec [integer! file! block! object! map!] "Can hold: port, root, index, keep-alive, server-name, actor callbacks"
	/no-wait "Will not enter wait loop"
][
	case [
		integer? port: spec [
			spec: make map! reduce/no-set [port: spec root: dirize to-real-file what-dir]
		]
		file? spec [
			root: dirize to-real-file clean-path spec
			port: 8000
			spec: make map! reduce/no-set [port: port root: root]
		]
		'else [
			unless block? spec [spec: body-of spec]
			spec: make map! reduce/no-set spec
			port: any [spec/port 8000] ;; default port
			root: spec/root
			if string? root [root: to-rebol-file root]
			if file?   root [
				;; to-real-file returns none when file does not exists on Posix
				;; that should be changed... also on Linux there is no trailing slash
				;; even when the source is a directory :-/ 
				spec/root: either exists? root [
					dirize to-real-file root
				][
					sys/log/error 'HTTPD ["Specified root not found:" as-red root]
					none
				]
			]
			if object? spec/app [
				spec/rsp: rsp-context
			]
		]
	]

	server: open join httpd://: :port
	sys/log/info 'HTTPD ["Listening on port:" :port "with root directory:" as-green spec/root]
	
	if actions: spec/actor [
		append server/extra/actor either block? actions [
			bind actions server/scheme
			reduce/no-set actions
		][	bind body-of actions server/scheme ]
		spec/actor: none ;; not including actor in the config
	]
	append config: server/extra/config body-of spec

	if block? config/auth [
		config/auth: make object! compose [
			realm:   (config/server-name)
			tokens:  none ;= [name sha256-hash ...]
			users:   none ;= [name pbkdf2-hash ...]
			methods: [PUT PATCH DELETE]
			paths:   none ;= [%/private/ ...] all methods require auth there
			max-failures: 5      ;= failed attempts per IP before lockout, NONE = no limit
			lockout:      0:5:0  ;= lockout duration (extended by each failure)
			(config/auth)
			cache:    make map! 16
			failures: make map! 16
		]
		foreach [name hash] any [config/auth/tokens []][
			unless binary? hash [cause-error 'script 'invalid-arg reduce [hash]]
		]
		foreach [name hash] any [config/auth/users []][
			unless string? hash [cause-error 'script 'invalid-arg reduce [hash]]
		]
	]

	if file? file: config/log-access [
		new: not exists? file
		config/log-access: open/:new/write/seek file
	]
	if file? file: config/log-errors [
		new: not exists? file
		config/log-errors: open/:new/write/seek file
	] 

	unless no-wait [
		forever [
			p: wait [server server/extra/subport 15]
			if all [port? p not open? p] [
				return p/data
			]
			server/scheme/Check-Clients server
		]
	]
	server
]
