Rebol [
	Title:   "Rebol3 port! test script"
	Author:  "Oldes, Peter W A Wood"
	File: 	 %port-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "port-net"



;- "HTTP scheme" moved to %port-http-test.r3

===start-group=== "Listen port in use"
	--test-- "UDP port cannot be bound twice"
		p1: open udp://:45124
		--assert all [error? e: try [p2: open udp://:45124]  e/id = 'no-connect]
		try [close p1]
	--test-- "TCP port cannot be bound twice"
		p1: open tcp://:45125
		--assert all [error? e: try [p2: open tcp://:45125]  e/id = 'no-connect]
		try [close p1]
===end-group===


===start-group=== "Listen on a local address"
	--test-- "TCP listen on 127.0.0.1"
		accepted: none
		tcp-server: open [scheme: 'tcp port: 45126 local-ip: 127.0.0.1]
		tcp-server/awake: func [event] [
			if event/type = 'accept [accepted: first event/port]
			false
		]
		--assert 127.0.0.1 = query tcp-server 'local-ip
		tcp-client: open tcp://127.0.0.1:45126
		tcp-client/awake: func [event] [
			if event/type = 'lookup [open event/port]
			false
		]
		loop 20 [if all [accepted open? tcp-client] [break] wait 0.05]
		--assert port? accepted
		--assert 127.0.0.1 = query accepted 'local-ip
		try [close tcp-client]
		try [close accepted]
		try [close tcp-server]

	--test-- "UDP listen on 127.0.0.1"
		udp-server: open [scheme: 'udp port: 45127 local-ip: 127.0.0.1]
		--assert 127.0.0.1 = query udp-server 'local-ip
		try [close udp-server]

	--test-- "Listen on all interfaces by default"
		tcp-server: open tcp://:45128
		--assert 0.0.0.0 = query tcp-server 'local-ip
		try [close tcp-server]

	--test-- "Listen on an address which is not local"
		;; 203.0.113.0/24 is reserved for documentation (TEST-NET-3)
		--assert all [
			error? e: try [open [scheme: 'tcp port: 45129 local-ip: 203.0.113.77]]
			e/id = 'no-connect
		]

	--test-- "Invalid local-ip value"
		--assert all [
			error? e: try [open [scheme: 'tcp port: 45130 local-ip: "localhost"]]
			e/id = 'invalid-spec
		]

	--test-- "Failed OPEN does not leave its socket open"
		;; Each leaked socket keeps a file descriptor (the default limit is 1024 on Linux).
		loop 1100 [try [open [scheme: 'tcp port: 45131 local-ip: "x"]]]
		--assert port? p1: try [open tcp://:45131]
		try [close p1]
===end-group===


===start-group=== "UDP"
	;; Opens a client port and waits until it is usable. The host is a string,
	;; so OPEN starts a lookup, which is async on Windows.
	open-udp-client: func [url [url!] /local port] [
		port: open url
		port/awake: func [event] [
			if event/type = 'lookup [open event/port]
			event/type = 'connect
		]
		wait [port 1]
		port
	]

	--test-- "WRITE while the host lookup is pending"
		;; On Windows the lookup is async; on POSIX it is done in OPEN.
		udp-client: open udp://127.0.0.1:45123
		--assert any [
			not error? e: try [write udp-client "x"]
			e/id = 'not-connected
		]
		try [close udp-client]

	--test-- "UDP read length is applied only once"
		;; When a datagram is already waiting, READ completes immediately.
		;; Its length must not be added again by UPDATE when the event is processed.
		udp-server: open udp://:45117
		udp-server/awake: func [event] [event/type = 'read]
		udp-client: open-udp-client udp://127.0.0.1:45117
		--assert open? udp-client
		write udp-client "Hello"
		wait 0.1 ;; let the datagram arrive, so the READ completes immediately
		read udp-server
		--assert 5 = length? udp-server/data ;; the immediate path was taken
		--assert all [
			port? wait [udp-server 1]
			"Hello" = to string! udp-server/data
		]
		try [close udp-client]
		try [close udp-server]

	--test-- "UDP listen port does not produce an error event"
		;; A bound UDP port has nothing to accept, so polling it must not fail.
		udp-events: copy []
		udp-server: open udp://:45118
		udp-server/awake: func [event] [append udp-events event/type event/type = 'read]
		wait 0.1 ;; lets the device poll the port
		--assert empty? udp-events
		--assert 45118 = query udp-server 'local-port
		udp-client: open-udp-client udp://127.0.0.1:45118
		--assert open? udp-client
		write udp-client "Hi"
		read udp-server
		--assert all [
			port? wait [udp-server 1]
			"Hi" = to string! udp-server/data
			udp-events = [read]
		]
		try [close udp-client]
		try [close udp-server]

	--test-- "FIRST on a UDP listen port after READ"
		;; sock->data points into the read buffer, it is not an accept queue.
		udp-server: open udp://:45119
		udp-server/awake: func [event] [event/type = 'read]
		udp-client: open-udp-client udp://127.0.0.1:45119
		--assert open? udp-client
		write udp-client "Hi"
		read udp-server
		--assert port? wait [udp-server 1]
		--assert all [error? e: try [first udp-server]  e/id = 'out-of-range]
		try [close udp-client]
		try [close udp-server]

	--test-- "UPDATE applies the read length only once"
		;; Every event of the port calls UPDATE, not only the READ event.
		;; DELETE on a net port queues a test ERROR event for it.
		udp-server: open udp://:45120
		udp-server/awake: func [event] [event/type = 'read]
		udp-client: open-udp-client udp://127.0.0.1:45120
		--assert open? udp-client
		write udp-client "Hi"
		read udp-server
		--assert port? wait [udp-server 1]
		delete udp-server
		wait [udp-server 0.1] ;; processes the ERROR event (awake returns false)
		--assert "Hi" = to string! udp-server/data
		try [close udp-client]
		try [close udp-server]
===end-group===


===start-group=== "TCP full duplex"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2143
	--test-- "READ while a WRITE is still pending"
		;; The READ must not overwrite the pending WRITE request.
		big: append/dup make binary! 2000000 #{01020304} 500000
		received: 0
		reply: none
		tcp-server: open tcp://:45121
		tcp-server/awake: func [event /local client] [
			if event/type = 'accept [
				client: first event/port
				client/awake: func [event /local port] [
					port: event/port
					if event/type = 'read [
						received: received + length? port/data
						clear port/data
						either received >= length? big [write port "done"][read port]
					]
					false
				]
				read client
			]
			false
		]
		tcp-client: open tcp://127.0.0.1:45121
		tcp-client/awake: func [event /local port] [
			port: event/port
			switch event/type [
				lookup [open port]
				connect [
					write port big ;; 2MB is sent in 32kB parts, so it stays pending
					read port      ;; issued while the WRITE is pending
				]
				read [reply: to string! port/data return true]
			]
			false
		]
		wait [tcp-client 5]
		--assert received = length? big
		--assert reply = "done"
		try [close tcp-client]
		try [close tcp-server]

	--test-- "READ before the previous READ's event was processed"
		;; The first READ completes immediately (data already received),
		;; the second must not overwrite its data.
		conn: none
		tcp-server: open tcp://:45122
		tcp-server/awake: func [event] [
			if event/type = 'accept [
				conn: first event/port
				conn/awake: func [event] [event/type = 'read]
			]
			false
		]
		tcp-client: open tcp://127.0.0.1:45122
		tcp-client/awake: func [event] [
			if event/type = 'lookup [open event/port]
			false
		]
		loop 20 [if all [conn open? tcp-client] [break] wait 0.05]
		write tcp-client "Hello"
		wait 0.1 ;; let the data arrive (no READ is pending on conn)
		read conn
		read conn
		--assert all [
			port? wait [conn 1]
			"Hello" = to string! conn/data
		]
		try [close tcp-client]
		try [close conn]
		try [close tcp-server]
===end-group===



===start-group=== "WHOIS scheme"
	--test-- "read WHOIS"
		--assert  string? probe try [read whois://google.com]
	--test-- "write WHOIS"
		--assert string? try [write whois://whois.nic.cz "seznam.cz"]
===end-group===


;- not using this test, because the serives limits number of requests from the IP
;- and on CI it may return "Access denied -- too many requests"
;import 'daytime
;if find system/schemes 'daytime [
;===start-group=== "DAYTIME scheme"
;	--test-- "read DAYTIME"
;		--assert  all [
;			block? res: try [read daytime://]
;			res/2/date = now/date
;		]
;
;===end-group===
;]

===start-group=== "DNS"
;@@ https://github.com/Oldes/Rebol-issues/issues/1827
;@@ https://github.com/Oldes/Rebol-issues/issues/1860
;@@ https://github.com/Oldes/Rebol-issues/issues/1935
	--test-- "read dns://"
		--assert string? try [probe read dns://] ;- no crash!

	--test-- "Using just a name of the dns scheme"
	;@@ https://github.com/Oldes/Rebol-issues/issues/826
		--assert string? try [read 'dns]

	--test-- "read dns://8.8.8.8"
		--assert "dns.google" = try [probe read dns://8.8.8.8]
	--test-- "read dns://google.com"
		--assert tuple? try [read dns://google.com]

	;; QUERY is optional (include-net-interfaces), so detect it first:
	net-interfaces?: not error? try [query dns:// none]

	--test-- "query dns://"
	;@@ https://github.com/Oldes/rebol-issues/issues/1826
		--assert all [
			error? e: try [query dns:// object!]
			e/id = either net-interfaces? ['invalid-arg]['no-port-action]
		]

if net-interfaces? [
	--test-- "query dns:// none"
		--assert [local-ip interfaces] = query dns:// none

	--test-- "query dns:// with an unsupported field"
		--assert all [error? e: try [query dns:// 'remote-ip]  e/id = 'invalid-arg]

	--test-- "query dns:// 'local-ip"
		--assert all [
			block? ips: query dns:// 'local-ip
			parse ips [any tuple!]
			not find ips 127.0.0.1
		]

	--test-- "query dns:// 'interfaces"
		--assert all [
			block? ifs: query dns:// 'interfaces
			parse ifs [some map!]  ;; at least the loopback
			string? ifs/1/name
			tuple?  ifs/1/local-ip
			tuple?  ifs/1/netmask
			block?  ifs/1/flags
		]
		;; The loopback is listed, but its address is not in local-ip.
		lo: none
		foreach i ifs [if find i/flags 'loopback [lo: i break]]
		--assert all [
			map? lo
			string? lo/name
			127 = lo/local-ip/1
			255.0.0.0 = lo/netmask
			not find query dns:// 'local-ip lo/local-ip
		]
]

	--test-- "read dns://not-exists"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2498
		--assert none? try [read dns://not-exists]
===end-group===


===start-group=== "TCP"
	--test-- "query net info"
		;@@ https://github.com/Oldes/Rebol-issues/issues/1712
		port: open tcp://8.8.8.8:80
		--assert (words-of system/standard/net-info) = query port none
		--assert 0.0.0.0 = query port 'local-ip
		--assert       0 = query port 'local-port
		--assert not none? find [0.0.0.0 8.8.8.8] query port 'remote-ip ;; on posix there is sync lookup and so it reports 8.8.8.8 even without wait
		--assert      80 = query port 'remote-port
		--assert all [
			port? wait [port 1] ;= wait for lookup, so remote-ip is resolved
			8.8.8.8 = query port 'remote-ip
			[80 8.8.8.8] = query port [:remote-port :remote-ip]
			[local-ip: 0.0.0.0 local-port: 0] = query port [local-ip local-port]
		]
		try [close port]
===end-group===


~~~end-file~~~