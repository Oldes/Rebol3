REBOL [
	System: "REBOL [R3] Language Interpreter and Run-time Environment"
	Title: "REBOL 3 HTTP protocol scheme"
	Rights: {
		Copyright 2012 REBOL Technologies
		REBOL is a trademark of REBOL Technologies
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
	Name: 'http
	Type: 'module
	Version: 0.2.1
	File: %prot-http.r
	Purpose: {
		This program defines the HTTP protocol scheme for REBOL 3.
	}
	Author: ["Gabriele Santilli" "Richard Smolak" "Oldes"]
	Date: 02-Apr-2019
	History: [
		0.1.1 22-Jun-2007 "Gabriele Santilli" "Version used in R3-Alpha"
		0.1.4 26-Nov-2012 "Richard Smolak"    "Version from Atronix's fork"
		0.1.5 10-May-2018 "Oldes" "FIX: Query on URL was returning just none"
		0.1.6 21-May-2018 "Oldes" "FEAT: Added support for basic redirection"
		0.1.7 03-Dec-2018 "Oldes" "FEAT: Added support for QUERY/MODE action"
		0.1.8 21-Mar-2019 "Oldes" "FEAT: Using system trace outputs"
		0.1.9 21-Mar-2019 "Oldes" "FEAT: Added support for transfer compression"
		0.2.0 28-Mar-2019 "Oldes" "FIX: close connection in case of errors"
		0.2.1 02-Apr-2019 "Oldes" "FEAT: Reusing connection in redirect when possible"
	]
]

sync-op: func [port body /local state encoding content-type code-page tmp] [
	unless port/state [open port port/state/close?: yes]
	state: port/state
	state/awake: :read-sync-awake
	do body
	if state/state = 'ready [do-request port]
	;NOTE: We'll wait in a WHILE loop so the timeout cannot occur during 'reading-data state.
	;The timeout should be triggered only when the response from other side exceeds the timeout value.
	;--Richard
	while [not find [ready close] state/state][
		unless port? wait [state/connection port/spec/timeout] [
			state/error: make-http-error "HTTP(s) Timeout"
			break
		]
		switch state/state [
			inited [
				if not open? state/connection [
					state/error: make-http-error rejoin ["Internal " state/connection/spec/ref " connection closed"]
					break
				]
			]
			reading-data [
				;? state/connection
				read state/connection
			]
			redirect [
				do-redirect port port/state/info/headers/location
				state: port/state
				state/awake: :read-sync-awake
			]
		]
	]
	if state/error [
		state/awake make event! [type: 'error port: port]
	]

	body: copy port

	sys/log/info 'HTTP ["Done reading:^[[22m" length? body "bytes"]

	if encoding: select port/state/info/headers 'Content-Encoding [
		sys/log/info 'HTTP 
		either find ["gzip" "deflate"] encoding [
			either error? try [
				body: switch encoding [
					"gzip"    [ decompress/gzip    body ]
					"deflate" [ decompress/deflate body ]
				]
			][
				["Failed to decode data using:^[[22m" encoding]
			][
				["Extracted using:^[[22m" encoding "^[[1mto:^[[22m" length? body "bytes"]
			]
		][
			["Unknown Content-Encoding:^[[m" encoding]
		]
	]

	if all [
		content-type: select port/state/info/headers 'Content-Type
		parse content-type ["text/" [thru "charset=" copy code-page to end] to end]
	][
		unless code-page [code-page: "utf-8"]
		sys/log/info 'HTTP ["trying to decode from code-page:^[[m" code-page]
		if string? tmp: try [iconv body code-page][	body: tmp ]
	]

	if state/close? [
		sys/log/more 'HTTP ["closing port for:^[[m" port/spec/ref]
		close port
	]
	body
]
read-sync-awake: func [event [event!] /local error] [
	sys/log/debug 'HTTP ["read-sync-awake:" event/type]
	switch/default event/type [
		connect ready [
			do-request event/port
			false
		]
		done [
			true
		]
		close [
			true
		]
		custom [
			if all [event/offset event/offset/x = 300] [
				event/port/state/state: 'redirect
				return true
			]
			false
		]
		error [
			error: event/port/state/error
			event/port/state/error: none
			try [
				sys/log/debug 'HTTP ["Closing (sync-awake):^[[1m" event/port/spec/ref]
				close event/port
			]
			if error? error [do error]
		]
	] [
		false
	]
]
http-awake: func [event /local port http-port state awake res] [
	port: event/port
	http-port: port/locals
	state: http-port/state
	if any-function? :http-port/awake [state/awake: :http-port/awake]
	awake: :state/awake

	sys/log/debug 'HTTP ["awake:^[[1m" event/type "^[[22mstate:^[[1m" state/state]

	switch/default event/type [
		read [
			awake make event! [type: 'read port: http-port]
			check-response http-port
		]
		wrote [
			awake make event! [type: 'wrote port: http-port]
			state/state: 'reading-headers
			read port
			false
		]
		lookup [open port false]
		connect [
			state/state: 'ready
			awake make event! [type: 'connect port: http-port]
		]
		close
		error [
			;?? state/state
			res: switch state/state [
				ready [
					awake make event! [type: 'close port: http-port]
				]
				doing-request reading-headers [
					state/error: make-http-error "Server closed connection"
					awake make event! [type: 'error port: http-port]
				]
				reading-data [
					either any [integer? state/info/headers/content-length state/info/headers/transfer-encoding = "chunked"] [
						state/error: make-http-error "Server closed connection"
						awake make event! [type: 'error port: http-port]
					] [
						;set state to CLOSE so the WAIT loop in 'sync-op can be interrupted --Richard
						state/state: 'close
						any [
							awake make event! [type: 'done port: http-port]
							awake make event! [type: 'close port: http-port]
						]
					]
				]
			]
			sys/log/debug 'HTTP ["Closing:^[[1m" http-port/spec/ref]
			close http-port
			if error? state/error [ do state/error ]
			res
		]
	] [true]
]
make-http-error: func [
	"Make an error for the HTTP protocol"
	message [string! block!]
] [
	if block? message [message: ajoin message]
	make error! [
		type: 'Access
		id: 'Protocol
		arg1: message
	]
]

make-http-request: func [
	"Create an HTTP request (returns string!)"
	method [word! string!] "E.g. GET, HEAD, POST etc."
	target [file! string!] {In case of string!, no escaping is performed (eg. useful to override escaping etc.). Careful!}
	headers [block!] "Request headers (set-word! string! pairs)"
	content [any-string! binary! none!] {Request contents (Content-Length is created automatically). Empty string not exactly like none.}
	/local result
] [
	result: rejoin [
		uppercase form method #" "
		either file? target [next mold target] [target]
		" HTTP/1.1" CRLF
	]
	foreach [word string] headers [
		repend result [mold word #" " string CRLF]
	]
	if content [
		content: to binary! content
		repend result ["Content-Length: " length? content CRLF]
	]
	sys/log/info 'HTTP ["Request:^[[22m" mold result]

	append result CRLF
	result: to binary! result
	if content [append result content]
	result
]
do-request: func [
	"Perform an HTTP request"
	port [port!]
	/local spec info
] [
	spec: port/spec
	info: port/state/info
	spec/headers: body-of make make object! [
		Accept: "*/*"
		Accept-Charset: "utf-8"
		Accept-encoding: "gzip,deflate"
		Host: either not find [80 443] spec/port-id [
			rejoin [form spec/host #":" spec/port-id]
		] [
			form spec/host
		]
		User-Agent: any [system/schemes/http/User-Agent "REBOL"]
	] spec/headers
	port/state/state: 'doing-request
	info/headers: info/response-line: info/response-parsed: port/data:
	info/size: info/date: info/name: none

	;sys/log/info 'HTTP ["do-request:^[[22m" spec/method spec/host spec/path]

	write port/state/connection make-http-request spec/method to file! any [spec/path %/] spec/headers spec/content
]
parse-write-dialect: func [port block /local spec] [
	spec: port/spec
	parse block [[set block word! (spec/method: block) | (spec/method: 'POST)]
		opt [set block [file! | url!] (spec/path: block)] [set block block! (spec/headers: block) | (spec/headers: [])] [set block [any-string! | binary!] (spec/content: block) | (spec/content: none)]
	]
]
check-response: func [port /local conn res headers d1 d2 line info state awake spec] [
	state: port/state
	conn: state/connection
	info: state/info
	headers: info/headers
	line: info/response-line
	awake: :state/awake
	spec: port/spec
	
	if all [
		not headers
		any [
			all [
				d1: find conn/data crlfbin
				d2: find/tail d1 crlf2bin
				;sys/log/debug 'HTML "server using standard content separator of #{0D0A0D0A}"
			]
			all [
				d1: find conn/data #{0A}
				d2: find/tail d1 #{0A0A}
				sys/log/debug 'HTML "server using malformed line separator of #{0A0A}"
			]
		]
	] [
		info/response-line: line: to string! copy/part conn/data d1
		sys/log/more 'HTTP line
		;probe to-string copy/part d1 d2
		info/headers: headers: construct/with d1 http-response-headers
		sys/log/info 'HTTP ["Headers:^[[22m" mold headers]
		info/name: spec/ref
		if state/error: try [
			; make sure that values bellow are valid
			if headers/content-length [info/size: headers/content-length: to integer! headers/content-length]
			if headers/last-modified  [info/date: to-date/utc headers/last-modified]
			none ; no error
		][
			awake make event! [type: 'error port: port]
		]
		remove/part conn/data d2
		state/state: 'reading-data
	]
	;? state
	;?? headers
	unless headers [
		read conn
		return false
	]
	res: false
	unless info/response-parsed [
		;?? line
		parse/all line [
			"HTTP/1." [#"0" | #"1"] some #" " [
				#"1" (info/response-parsed: 'info)
				|
				#"2" [["04" | "05"] (info/response-parsed: 'no-content)
					| (info/response-parsed: 'ok)
				]
				|
				#"3" [
					"03" (info/response-parsed: 'see-other)
					|
					"04" (info/response-parsed: 'not-modified)
					|
					"05" (info/response-parsed: 'use-proxy)
					| (info/response-parsed: 'redirect)
				]
				|
				#"4" [
					"01" (info/response-parsed: 'unauthorized)
					|
					"07" (info/response-parsed: 'proxy-auth)
					| (info/response-parsed: 'client-error)
				]
				|
				#"5" (info/response-parsed: 'server-error)
			]
			| (info/response-parsed: 'version-not-supported)
		]
	]

	sys/log/debug 'HTTP ["check-response:" info/response-parsed]

	;?? info/response-parsed
	;?? spec/method

	switch/all info/response-parsed [
		ok [
			either spec/method = 'HEAD [
				state/state: 'ready
				res: awake make event! [type: 'done port: port]
				unless res [res: awake make event! [type: 'ready port: port]]
			] [
				res: check-data port
				;?? res
				;?? state/state
				if all [not res state/state = 'ready] [
					res: awake make event! [type: 'done port: port]
					unless res [res: awake make event! [type: 'ready port: port]]
				]
			]
		]
		redirect see-other [
			either spec/method = 'HEAD [
				state/state: 'ready
				res: awake make event! [type: 'custom port: port code: 0]
			] [
				res: check-data port
				unless open? port [
					;NOTE some servers(e.g. yahoo.com) don't supply content-data in the redirect header so the state/state can be left in 'reading-data after check-data call
					;I think it is better to check if port has been closed here and set the state so redirect sequence can happen. --Richard
					state/state: 'ready
				]
			]
			;?? res
			;?? headers
			;?? state/state

			if all [not res state/state = 'ready] [
				either all [
					any [
						find [get head] spec/method
						all [
							info/response-parsed = 'see-other
							spec/method: 'GET
						]
					]
					in headers 'Location
				] [
					return awake make event! [type: 'custom port: port code: 300]
				] [
					state/error: make-http-error "Redirect requires manual intervention"
					res: awake make event! [type: 'error port: port]
				]
			]
		]
		unauthorized client-error server-error proxy-auth [
			either spec/method = 'HEAD [
				state/state: 'ready
			] [
				check-data port
			]
		]
		unauthorized [
			state/error: make-http-error "Authentication not supported yet"
			res: awake make event! [type: 'error port: port]
		]
		client-error server-error [
			state/error: make-http-error ["Server error: " line]
			res: awake make event! [type: 'error port: port]
		]
		not-modified [state/state: 'ready
			res: awake make event! [type: 'done port: port]
			unless res [res: awake make event! [type: 'ready port: port]]
		]
		use-proxy [
			state/state: 'ready
			state/error: make-http-error "Proxies not supported yet"
			res: awake make event! [type: 'error port: port]
		]
		proxy-auth [
			state/error: make-http-error "Authentication and proxies not supported yet"
			res: awake make event! [type: 'error port: port]
		]
		no-content [
			state/state: 'ready
			res: awake make event! [type: 'done port: port]
			unless res [res: awake make event! [type: 'ready port: port]]
		]
		info [
			info/headers: info/response-line: info/response-parsed: port/data: none
			state/state: 'reading-headers
			read conn
		]
		version-not-supported [
			state/error: make-http-error "HTTP response version not supported"
			res: awake make event! [type: 'error port: port]
			close port
		]
	]
	res
]
crlfbin: #{0D0A}
crlf2bin: #{0D0A0D0A}
crlf2: to string! crlf2bin
http-response-headers: context [
	Content-Length:
	Content-Encoding:
	Transfer-Encoding:
	Last-Modified: none
]

do-redirect: func [port [port!] new-uri [url! string! file!] /local spec state] [
	spec: port/spec
	state: port/state

	clear spec/headers
	port/data: none

	sys/log/info 'HTTP ["Redirect to:^[[m" mold new-uri]

	state/redirects: state/redirects + 1
	if state/redirects > 10 [
		state/error: make-http-error {Too many redirections}
		return state/awake make event! [type: 'error port: port]
	]

	if #"/" = first new-uri [
		; if it's redirection under same url, we can reuse the opened connection
		if "keep-alive" = select state/info/headers 'Connection [
			spec/path: new-uri
			do-request port
			return true
		]
		new-uri: to url! ajoin [spec/scheme "://" spec/host #":" spec/port-id new-uri]
	]
	new-uri: decode-url new-uri

	unless select new-uri 'port-id [
		switch new-uri/scheme [
			'https [append new-uri [port-id: 443]]
			'http [append new-uri [port-id: 80]]
		]
	]
	new-uri: construct/with new-uri port/scheme/spec
	new-uri/method: spec/method
	new-uri/ref: to url! ajoin either find [#[none] 80 443] new-uri/port-id [
		[new-uri/scheme "://" new-uri/host new-uri/path]
	][	[new-uri/scheme "://" new-uri/host #":" new-uri/port-id new-uri/path]]

	unless find [http https] new-uri/scheme [
		state/error: make-http-error {Redirect to a protocol different from HTTP or HTTPS not supported}
		return state/awake make event! [type: 'error port: port]
	]

	;we need to reset tcp connection here before doing a redirect
	close port/state/connection
	port/spec: spec: new-uri
	port/state: none
	open port
]

check-data: func [port /local headers res data out chunk-size mk1 mk2 trailer state conn] [
	state: port/state
	headers: state/info/headers
	conn: state/connection
	res: false

	sys/log/more 'HTTP ["check-data; bytes:^[[m" length? conn/data]
	;? conn

	case [
		headers/transfer-encoding = "chunked" [
			data: conn/data
			;clear the port data only at the beginning of the request --Richard
			unless port/data [port/data: make binary! length? data]
			out: port/data
			until [
				either parse/all data [
					copy chunk-size some hex-digits thru crlfbin mk1: to end
				] [
					chunk-size: to integer! to issue! to string! chunk-size
					either chunk-size = 0 [
						if parse/all mk1 [
							crlfbin (trailer: "") to end | copy trailer to crlf2bin to end
						] [
							trailer: construct trailer
							append headers body-of trailer
							state/state: 'ready
							res: state/awake make event! [type: 'custom port: port code: 0]
							clear data
						]
						true
					] [
						either parse/all mk1 [
							chunk-size skip mk2: crlfbin to end
						] [
							insert/part tail out mk1 mk2
							remove/part data skip mk2 2
							empty? data
						] [
							true
						]
					]
				] [
					true
				]
			]
			unless state/state = 'ready [
				;Awake from the WAIT loop to prevent timeout when reading big data. --Richard
				res: true
			]
		]
		integer? headers/content-length [
			port/data: conn/data
			either headers/content-length <= length? port/data [
				state/state: 'ready
				conn/data: make binary! 32000 ;@@ Oldes: why not just none?
				res: state/awake make event! [type: 'custom port: port code: 0]
			] [
				;Awake from the WAIT loop to prevent timeout when reading big data. --Richard
				res: true
			]
		]
		true [
			port/data: conn/data
			either state/info/response-parsed = 'ok [
				;Awake from the WAIT loop to prevent timeout when reading big data. --Richard
				res: true
			][
				;On other response than OK read all data asynchronously (assuming the data are small). --Richard
				read conn
			]
		]
	]
	res
]
hex-digits: charset "1234567890abcdefABCDEF"
sys/make-scheme [
	name: 'http
	title: "HyperText Transport Protocol v1.1"
	spec: make system/standard/port-spec-net [
		path: %/
		method: 'GET
		headers: []
		content: none
		timeout: 15
	]
	info: make system/standard/file-info [
		response-line:
		response-parsed:
		headers: none
	]
	actor: [
		read: func [
			port [port!]
		] [
			sys/log/debug 'HTTP "read"
			either any-function? :port/awake [
				unless open? port [cause-error 'Access 'not-open port/spec/ref]
				if port/state/state <> 'ready [http-error "Port not ready"]
				port/state/awake: :port/awake
				do-request port
				port
			] [
				sync-op port []
			]
		]
		write: func [
			port [port!]
			value
		] [
			sys/log/debug 'HTTP "write"
			;?? port
			unless any [block? :value binary? :value any-string? :value] [value: form :value]
			unless block? value [value: reduce [[Content-Type: "application/x-www-form-urlencoded; charset=utf-8"] value]]

			either any-function? :port/awake [
				unless open? port [cause-error 'Access 'not-open port/spec/ref]
				if port/state/state <> 'ready [http-error "Port not ready"]
				port/state/awake: :port/awake
				parse-write-dialect port value
				do-request port
				port
			] [
				sync-op port [parse-write-dialect port value]
			]
		]
		open: func [
			port [port!]
			/local conn
		] [
			sys/log/debug 'HTTP ["open, state:" port/state]
			if port/state [return port]
			if none? port/spec/host [http-error "Missing host address"]
			port/state: context [
				state: 'inited
				connection:
				error: none
				close?: no
				info: make port/scheme/info [type: 'url]
				awake: :port/awake
				redirects: 0
			]
			;? port/state/info
			port/state/connection: conn: make port! compose [
				scheme: (to lit-word! either port/spec/scheme = 'http ['tcp]['tls])
				host: port/spec/host
				port-id: port/spec/port-id
				ref: to url! ajoin [scheme "://" host #":" port-id]
			]
			;?? conn 
			conn/awake: :http-awake
			conn/locals: port
			sys/log/info 'HTTP ["Opening connection:^[[22m" conn/spec/ref]
			;?? conn
			open conn
			port
		]
		open?: func [
			port [port!]
		] [
			found? all [port/state open? port/state/connection]
		]
		close: func [
			port [port!]
		] [
			sys/log/debug 'HTTP "close"
			if port/state [
				close port/state/connection
				port/state/connection/awake: none
				port/state: none
			]
			port
		]
		copy: func [
			port [port!]
		] [
			either all [port/spec/method = 'HEAD port/state] [
				reduce bind [name size date] port/state/info
			] [
				if port/data [copy port/data]
			]
		]
		query: func [
			port [port!]
			/mode
			field [word! block! none!]
			/local error state result
		] [
			if all [mode none? field] [ return words-of system/schemes/http/info]
			if none? state: port/state [
				open port ;there is port opening in sync-op, but it would also close the port later and so clear the state
				attempt [sync-op port [parse-write-dialect port [HEAD]]]
				state: port/state
				close port
			]
			;?? state
			either all [
				state
				state/info/response-parsed
			][
				either field [
					either word? field [
						select state/info field
					][
						result: make block! length? field
						foreach word field [
							if any-word? word [
								if set-word? word [ append result word ]
								append result state/info/(to word! word)
							]
						]
						result
					]
				][	state/info ]
			][	none ]
		]
		length?: func [
			port [port!]
		] [
			either port/data [length? port/data] [0]
		]
	]
	User-Agent: none
	;@@ One can set above value for example to: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/73.0.3683.103 Safari/537.36"
	;@@ And so pretend that request is coming from Chrome on Windows10
]

sys/make-scheme/with [
	name: 'https
	title: "Secure HyperText Transport Protocol v1.1"
	spec: make spec [
		port-id: 443
	]
] 'http
