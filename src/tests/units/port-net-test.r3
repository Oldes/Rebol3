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


===start-group=== "UDP"
	--test-- "UDP read length is applied only once"
		;; When a datagram is already waiting, READ completes immediately.
		;; Its length must not be added again by UPDATE when the event is processed.
		udp-server: open udp://:45117
		udp-server/awake: func [event] [event/type = 'read]
		udp-client: open udp://127.0.0.1:45117
		udp-client/awake: func [event] [false]
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
		udp-client: open udp://127.0.0.1:45118
		udp-client/awake: func [event] [false]
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
		udp-client: open udp://127.0.0.1:45119
		udp-client/awake: func [event] [false]
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
		udp-client: open udp://127.0.0.1:45120
		udp-client/awake: func [event] [false]
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

	--test-- "query dns://"
	;@@ https://github.com/Oldes/rebol-issues/issues/1826
		--assert all [error? e: try [query dns:// object!]  e/id = 'no-port-action]

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