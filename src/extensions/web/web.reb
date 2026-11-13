REBOL [
	Title:   "Web browser bridge"
	Name:    web
	Version: 0.1.0
	Needs:   3.22.11
	Author:  @Oldes
	License: Apache-2.0
	Exports: [js dom show]
	Purpose: {
		Network access for the WebAssembly (browser) build.

		The browser does not allow sockets, so the HTTP and HTTPS schemes
		defined here use the browser's own XMLHttpRequest instead of TCP.
		The request is synchronous, which fits the synchronous wasm build;
		the JS side may later switch to fetch() without changing the Rebol
		side.

		Only binary! values cross the C boundary: the mezzanine converts
		strings to UTF-8 before calling the commands, and converts the
		results back.

		Cross-origin rules (CORS) of the browser apply. A proxy URL can be
		set from the page in Module.rebolHttpProxy - the requested URL is
		then appended to it (URI encoded).

		The exported `js` function evaluates JavaScript in the page's
		global scope and converts the result into a Rebol value.

		The exported `dom` object manipulates the page using CSS selectors
		(texts, HTML, properties, attributes, styles, classes, elements)
		and runs Rebol code on DOM events (`dom/on`, `dom/off`).

		The exported `show` function displays rich output (HTML, images,
		tables) in the console, through the page's Module.onRebolShow.
		An image! value is encoded as PNG; when the PNG codec is not
		available, it is passed as raw RGBA pixels and drawn on a canvas.
	}
]

;; ---------------------------------------------------------------------------
;; C-side configuration
c-header: {
// Indexes of the parts of the last response/evaluation kept on the JS side.
#define WEB_PART_BODY    0
#define WEB_PART_HEADERS 1
#define WEB_PART_ERROR   2
#define WEB_PART_RESULT  3

// Installs the JS side of the extension (web-commands.c).
void Web_Install_JS(void);
}

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
commands: [
	request: [
		"Sends an HTTP request; returns the status code (0 when no response, see `response-error`)"
		method  [binary!] "Method name (UTF-8)"
		url     [binary!] "Full URL (UTF-8)"
		headers [binary!] "Request headers as `Name: value` lines (UTF-8)"
		data    [binary! none!] "Request body"
	]
	response-body:    ["Returns the body of the last response" ]
	response-headers: ["Returns the headers of the last response as `Name: value` lines"]
	response-error:   ["Returns the error message of the last request or evaluation (empty if none)"]
	js-eval:   [
		"Evaluates JavaScript code; returns false when it threw (see `response-error`)"
		code [binary!] "JavaScript source (UTF-8)"
	]
	js-result: ["Returns the result of the last `js-eval` as Rebol source (UTF-8)"]
	show-rich: [
		"Passes rich output to the page; returns false when the page cannot show it"
		kind [binary!] {"html", "image" or "rgba:<width>" (UTF-8)}
		data [binary!] "HTML source (UTF-8), encoded image data, or RGBA pixels"
	]
]

;; ---------------------------------------------------------------------------
;; Module body.
mezzanine: [
	js: func [
		"Evaluates JavaScript code in the page's global scope and returns its result"
		code [string!] "JavaScript source"
	][
		unless js-eval to binary! code [
			do make error! ajoin ["JavaScript: " to string! response-error]
		]
		transcode/one js-result
	]

	html-escape: function/with [
		"Escapes HTML special characters"
		text [string!]
	][
		out: copy ""
		parse text [
			collect into out [
				any [
					keep some normal-chars
					| #"&" keep ("&amp;")
					| #"<" keep ("&lt;")
					| #">" keep ("&gt;")
					| #"^"" keep ("&quot;")
			]]
		]
		out
	][  normal-chars: #(bitset! not #{000000002200000A}) ] ;= complement charset {&<>"}

	html-table: function [
		"Converts a block of rows (blocks of values) to an HTML table"
		rows [block!]
		header [logic! none!] "The first row is a header"
	][
		out: copy "<table>"
		cell: either header ["th"]["td"]
		foreach row rows [
			append out "<tr>"
			foreach value either block? :row [row][reduce [:row]] [
				append out ajoin [#"<" cell #">" html-escape form :value "</" cell #">"]
			]
			append out "</tr>"
			cell: "td"
		]
		append out "</table>"
	]

	show: function [
		"Shows rich output in the console: HTML, an image, or a table"
		value [string! binary! file! url! block! image!] {HTML source, image (data, file or url), or a block of rows}
		/header "Table: the first row is a header"
	][
		case [
			image? value [
				;; the whole image is shown, not only from the current index
				value: head value
				either data: attempt [encode 'png value] [
					kind: "image"
				][
					;; no PNG codec: raw pixels in RGBA order (straight alpha),
					;; as canvas uses
					kind: ajoin ["rgba:" value/width]
					data: value/rgba
				]
			]
			any [binary? value file? value url? value] [
				kind: "image"
				data: either binary? value [value][read/binary value]
			]
			true [
				kind: "html"
				data: to binary! either block? value [html-table value header][value]
			]
		]
		unless show-rich to binary! kind data [
			;; the page cannot show it (for example when running in Node.js)
			print ajoin ["[" kind ": " length? data " bytes]"]
		]
		()
	]

	;; NOTE: inside `dom`, the words `append`, `remove`, `get`, `set` and
	;; `call` are the dom functions - helper code uses `lib/` versions.
	dom: make object! [
		handlers: make map! []
		last-id: 0

		js-string: function/with [
			"Converts a value to a JavaScript string literal"
			value
		][
			out: make string! 10 + length? value: form :value
			parse value [
				collect into out [
					keep (#"^"")
					any [
						keep some normal-chars
						| #"\"   keep ({\\})
						| #"^""  keep ({\"})
						| #"^/"  keep ({\n})
						| #"^M"  keep ({\r})
						| #"^-"  keep ({\t})
						| set c: skip keep ("\u") keep (to-hex/size c 4)
					]
					keep (#"^"")
				]
			]
			out
		][
			normal-chars: #(bitset! not #{FFFFFFFF2000000000000008}) ;= complement charset [0 - 31 {\"^/^M^-}]
		]

		js-value: func [
			"Converts a Rebol value to a JavaScript literal"
			value
		][
			switch/default type? :value [
				#(none!)    ["null"]
				#(logic!)   [either value ["true"]["false"]]
				#(integer!) [form value]
				#(decimal!) [
					case [
						value <> value                  ["NaN"]
						value =  1.#INF ["Infinity"]
						value = -1.#INF ["-Infinity"]
						true                            [form value]
					]
				]
			][  js-string :value ]
		]

		call: func [
			"Calls rebolDom.<name> with the arguments converted to JavaScript"
			name [word!]
			args [block!] "Arguments (will be reduced)"
		][
			args: reduce args
			forall args [change args js-value args/1]
			js ajoin ["rebolDom." name "(" ajoin/with args #"," #")"]
		]

		;-- getters and setters (applied to all matching elements) -----------

		text: func [
			"Sets the text of the matching elements; returns their count"
			selector [string!] value
		][	call 'set [selector "textContent" form :value] ]

		html: func [
			"Sets the inner HTML of the matching elements; returns their count"
			selector [string!] value [string!]
		][	call 'set [selector "innerHTML" value] ]

		get: func [
			"Returns a property of the first matching element (none if none matches)"
			selector [string!] property [word! string!] "For example: 'value 'checked 'textContent"
		][	call 'get [selector form property] ]

		set: func [
			"Sets a property of the matching elements; returns their count"
			selector [string!] property [word! string!] value
		][	call 'set [selector form property :value] ]

		value: func [
			"Returns the value of the first matching form element"
			selector [string!]
		][	call 'get [selector "value"] ]

		attr: func [
			"Sets an attribute of the matching elements (none removes it); returns their count"
			selector [string!] name [word! string!] value
		][	call 'attr [selector form name :value] ]

		style: function [
			"Sets CSS properties of the matching elements; returns their count"
			selector [string!]
			styles [block!] {Pairs of property and value, e.g.: [color: "red" padding: "8px"]}
		][
			out: copy "{"
			foreach [name val] styles [
				lib/append out ajoin [js-string to word! name #":" js-value form :val #","]
			]
			if #"," = last out [take/last out]
			lib/append out #"}"
			js ajoin ["rebolDom.style(" js-string selector #"," out #")"]
		]

		class: func [
			"Adds a CSS class to the matching elements; returns their count"
			selector [string!] name [word! string!]
			/remove "Removes the class instead"
			/toggle "Toggles the class instead"
		][
			call 'cls [
				selector form name
				case [remove ["remove"] toggle ["toggle"] true ["add"]]
			]
		]

		;-- structure ---------------------------------------------------------

		append: func [
			"Appends a new element to the first matching element; returns 1, or 0 when not found"
			selector [string!]
			tag      [word! string!] "Element name, e.g. 'li"
			content  [string! none!] "Text content"
			/html "The content is HTML"
		][	call 'append [selector form tag content html] ]

		remove: func [
			"Removes the matching elements; returns their count"
			selector [string!]
		][	call 'remove [selector] ]

		count: func [
			"Returns the number of matching elements"
			selector [string!]
		][	js ajoin ["document.querySelectorAll(" js-string selector ").length"] ]

		;-- events ------------------------------------------------------------

		on: func [
			"Evaluates a block on a DOM event of matching elements (also future ones); returns handler's id"
			selector [string!]
			type [word! string!] "Event type, e.g. 'click 'input 'keydown"
			body [block!] "Evaluated with `event` - a map! with type, id, value, checked, key, x and y"
			/local id ;; not `function` - it would make `last-id` local too
		][
			id: last-id: last-id + 1
			handlers/:id: func [event [map!]] body
			call 'on [selector form type id]
			id
		]

		off: func [
			"Removes an event handler; returns true when it existed"
			id [integer!]
		][
			lib/remove/key handlers id
			call 'off [id]
		]

		dispatch: func [
			"Called from JavaScript - evaluates the handler"
			id [integer!] event [map!]
			/local handler
		][
			if handler: handlers/:id [handler event]
			()
		]
	]

	http-response-headers: construct [
		Content-Length:
		Content-Encoding:
		Content-Type:
		Last-Modified:
	]

	form-headers: func [
		"Converts a block, map or object of headers to `Name: value` lines"
		headers [block! map! object! none!]
		/local out
	][
		out: make string! 100
		if headers [
			foreach [name value] to block! headers [
				append out ajoin [to word! name ": " value CRLF]
			]
		]
		out
	]

	do-http: func [
		"Performs the request; returns [status-code headers body]"
		port [port!]
		/local spec url status headers
	][
		spec: port/spec
		url: spec/ref
		status: request
			to binary! uppercase form spec/method
			to binary! form url
			to binary! form-headers spec/headers
			all [spec/content to binary! spec/content]
		if status = 0 [
			cause-error 'access 'no-connect reduce [url to string! response-error]
		]
		headers: construct/with response-headers http-response-headers
		if headers/content-length [
			try [headers/content-length: to integer! headers/content-length]
		]
		reduce [status headers response-body]
	]

	decode-result: func [
		"Converts the body to a string, when the content type says it is a text"
		result [block!] "[status-code headers body]"
		/local content-type
	][
		;; NOTE: the browser already decompressed the body, so Content-Encoding
		;; is ignored here (unlike in the TCP based scheme).
		if all [
			content-type: select result/2 'Content-Type
			any [
				find content-type "charset="
				parse content-type [["text/" | "application/json" | "application/xml"] to end]
			]
		][
			try [result/3: to string! result/3]
		]
		result
	]

	check-result: func [
		result [block!]
		all? [logic! none!]
	][
		either all? [result][
			if any [result/1 >= 400  result/1 == 300][
				cause-error 'access 'protocol any [
					select system/catalog/http-status-codes result/1
					ajoin ["Error response: " result/1]
				]
			]
			result/3
		]
	]

	unless select system/catalog 'http-status-codes [
		put system/catalog 'http-status-codes #[
			200 "OK" 201 "Created" 204 "No Content" 206 "Partial"
			300 "Multiple Choices" 301 "Moved Permanently" 302 "Found" 304 "Not Modified"
			400 "Bad Request" 401 "Unauthorized" 403 "Forbidden" 404 "Not Found"
			405 "Not Allowed" 408 "Request Timeout" 429 "Too Many Requests"
			500 "Internal Server Error" 502 "Bad Gateway" 503 "Service Unavailable"
			504 "Gateway Timeout"
		]
	]

	sys/make-scheme [
		name:  'http
		title: "HyperText Transport Protocol (browser)"
		spec: make system/standard/port-spec-net [
			path: %/
			method: 'GET
			headers: []
			content: none
		]
		actor: [
			read: func [
				port [port!]
				/string {Convert to text string}
				/binary {Preserves contents exactly}
				/lines  {Convert to block of strings (implies /string)}
				/all    {Returns [status-code headers body]}
				/local result
			][
				port/spec/method: 'GET
				port/spec/content: none
				result: do-http port
				unless binary [
					decode-result result
					case [
						string [try [result/3: to string! result/3]]
						lines  [try [result/3: split-lines to string! result/3]]
					]
				]
				check-result result all
			]
			write: func [
				port [port!]
				value {Data to write, or [method headers data] block}
				/binary {Preserves contents exactly}
				/all    {Returns [status-code headers body]}
				/local spec result
			][
				spec: port/spec
				either block? value [
					;; same dialect as the TCP based scheme: [method headers data]
					parse value [
						[set value word! (spec/method: value) | (spec/method: 'POST)]
						opt [set value [file! | url!] (
							spec/ref: either url? value [value][
								as url! ajoin [spec/scheme "://" spec/host value]
							]
						)]
						[set value [block! | map!] (spec/headers: value) | (spec/headers: [])]
						[set value [any-string! | binary!] (spec/content: value) | (spec/content: none)]
					]
				][
					spec/method: 'POST
					spec/headers: either binary? value [
						[Content-Type: "application/octet-stream"]
					][
						[Content-Type: "application/x-www-form-urlencoded; charset=utf-8"]
					]
					spec/content: either binary? value [value][form value]
				]
				result: do-http port
				unless binary [decode-result result]
				check-result result all
			]
			query: func [
				port [port!]
				field [word! block! none!]
				/local result info
			][
				if none? field [return copy [name size date type status-code headers]]
				port/spec/method: 'HEAD
				port/spec/content: none
				result: do-http port
				info: make object! [
					name: port/spec/ref
					size: result/2/content-length
					date: attempt [lib/to-date/utc result/2/last-modified]
					type: 'url
					status-code: result/1
					headers: result/2	
				]
				case [
					'object! = field [info]
					word?   field [select info field]
					block?  field [lib/collect [foreach w field [keep select info w]]]
				]
			]
		]
	]

	sys/make-scheme/with [
		name:  'https
		title: "Secure HyperText Transport Protocol (browser)"
		spec: make spec [port: 443]
	] 'http
]
