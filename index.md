## What is Rebol3 (Oldes' branch)?

This site is dedicated to Rebol3 fork maintained by **Oldes** ([Amanita Design](http://amanita-design.net/) programmer).

As [Carl Sassenrath](http://www.sassenrath.com/) abandoned Rebol a few years after releasing its [sources](https://github.com/rebol/rebol) in year 2012, [Oldes' fork](https://github.com/Oldes/Rebol3) is one of the last two actively maintained versions. The second one is Hostile Fork's [Ren-C](https://github.com/metaeducation/ren-c).

The main difference between these two versions is, that while Fork is focused on deep language changes, Oldes' version is focused on bringing Rebol from its alpha state by going thru all the [issues](https://github.com/Oldes/Rebol-issues/issues) while trying to resolve these, but keeping the original without not neccessary modifications. There is still mergeable [pull request](https://github.com/rebol/rebol/pull/251).

### What is Rebol

Rebol is a cross-platform data exchange language and a multi-paradigm dynamic programming language designed by Carl Sassenrath.

### What is new in Oldes' version?

_To be written._ Meanwhile there is [list of changes](https://github.com/Oldes/Rebol3/blob/master/CHANGES.md) available in the repository.

### Rebol code examples

Simple HTTP server:

```rebol
Rebol [
    Title: "HTTPD Scheme example"
]

import %httpd.reb

system/schemes/httpd/set-verbose 3 ; for verbose output

; make sure that there is the directory for logs
make-dir/deep %_logs/

serve-http [
    ;- Main server configuration
    port: 8082
    root: %./
    server-name: "nginx"  ;= it's possible to hide real server name
    keep-alive: [15 100]  ;= [timeout max-requests] or FALSE to turn it off
    list-dir?:  #(true)   ;= allow directory listing
    log-access: %_logs/test-access.log
    log-errors: %_logs/test-errors.log

    ;- Server's actor functions
    actor: [
        On-Accept: func [info [object!]][
            ; allow only connections from localhost
            ; TRUE = accepted, FALSE = refuse
            find [ 127.0.0.1 ] info/remote-ip 
        ]
    ]
]
```

Github API client:

```rebol
Rebol [
	title: "Github API"
	author: "Oldes"
	license: MIT
]

My-GitHub-authorization: "token ..." ;<--- replace ...  with your API token!

github: context [
	api.github: https://api.github.com/
	owner: repository: none
	authorization: :My-GitHub-authorization

	data: #()
	response: none

	use-repo: func[o r][ owner: o repository: r] 

	get: object [
		issues: func[][
			*do 'GET [%repos/ owner %/ repository %/issues] none
		]
		issue: func[number [integer!]][
			*do 'GET [%repos/ owner %/ repository %/issues/ number] none
		]
		issue-comments: func[
			{Gets all comments of an issue by its number}
			number [integer!]
		][
			*do 'GET [%repos/ owner %/ repository %/issues/ number %/comments] none
		]
		issue-labels: func[
			{Gets all labels of an issue by its number}
			number [integer!]
		][
			*do 'GET [%repos/ owner %/ repository %/issues/ number %/labels] none
		]

		current-user: does [*do 'GET %user none]
	]

	post: object [
		issue: func[
			data [map!] {title, body, labels etc..}
		][
			unless block? data/labels [ data/labels: reduce [labels] ]
			*do 'POST [%repos/ owner %/ repository %/issues] data
		]

		issue-comment: func[
			{Adds a comment to an issue by its number}
			number  [integer!]
			body    [string!]
		][
			clear data
			data/body: body
			*do 'POST [%repos/ owner %/ repository %/issues/ number %/comments] data
		]

		issue-label: func[
			{Adds a label to an issue by its number}
			number  [integer!]
			body    [string! block!]
		][
			clear data
			append data/labels: clear [] body
			*do 'POST [%repos/ owner %/ repository %/issues/ number %/labels] data
		]
	]

	edit: object [
		issue: func[number [integer!] data [map!]][
			*do 'PATCH [%repos/ owner %/ repository %/issues/ number] data
		]
	]

	*do: func[method [word!] path data [map! none!] /local url header][
		url: join api.github path
		;?? url
		header: clear #()
		header/Authorization: authorization
		header/X-OAuth-Scopes: "repo"
		header/Accept: "Accept: application/vnd.github.v3+json"

		if map? data [header/Content-Type:  "application/json"]
		response: write url reduce [method to-block header to-json data]
		try [response: load-json to string! response]
	]
]
```

and or some random function:
```rebol
unpack-bits: function [
	{Decompress data compressed by Apple's PackBits routine}
	c [binary!] {Data to decompress}
][
	;https://web.archive.org/web/20080705155158/http://developer.apple.com/technotes/tn/tn1023.html
	u: make binary! 4 * length? c
	i: c ;store position
	while [not tail? c][
		n: first+ c
		case [
			n < 128 [
				++ n
				append u copy/part c n
				c: skip c n
			]
			n > 128 [
				n: 257 - n
				append/dup u first+ c n
			]
			;n = 128 is ignored
		]
	]
	c: i ;restore position
	u
]
```


