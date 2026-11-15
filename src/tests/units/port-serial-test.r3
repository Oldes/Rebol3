Rebol [
	Title:   "Rebol3 serial port! test script"
	Author:  "Oldes"
	File: 	 %port-serial-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "port-serial"

;; The scheme is always defined, but it has the native actor only when
;; Rebol is built with the serial device (else OPEN returns just none).
serial?: all [
	find system/schemes 'serial
	native? select system/schemes/serial 'actor
]

if all [
	serial?
	system/platform = 'Linux
	exists? %/dev/ptmx
][
	;; The pty master is a tty device which accepts serial settings,
	;; so it can be used to test the port without any hardware.
	===start-group=== "SERIAL (pty)"
		--test-- "Open with standard baud rates"
			;; the numeric rate must be converted to the system's speed constant
			foreach speed [300 1200 2400 4800 9600 19200 38400 57600 115200 230400][
				--assert all [
					port? p: try [open join serial://ptmx/ speed]
					open? p
					port? try [close p]
					not open? p
				]
			]
		--test-- "Open with the default baud rate"
			;; the speed is optional in the url
			--assert all [
				port? p: try [open serial://ptmx]
				p/spec/path  = %ptmx
				p/spec/speed = 115200
				port? try [close p]
			]
			--assert all [
				port? p: try [open serial://ptmx/]
				p/spec/path  = %ptmx
				p/spec/speed = 115200
				port? try [close p]
			]
			--assert all [
				port? p: try [open serial://ptmx/9600]
				p/spec/path  = %ptmx
				p/spec/speed = 9600
				port? try [close p]
			]
		--test-- "Invalid url"
			foreach url [serial://ptmx/abc serial://ptmx/9600x serial://ptmx/9600/8 serial:// serial:///9600][
				--assert all [
					error? e: try [open url]
					e/id = 'invalid-spec
				]
			]
		--test-- "Open and close many times"
			;; no file descriptor may be left open
			--assert not error? try [
				loop 2000 [close open serial://ptmx/9600]
			]
		--test-- "Open a device which is not a terminal"
			--assert all [
				error? e: try [open serial://null/9600]
				e/id = 'cannot-open
			]
			--assert all [
				error? e: try [open serial://not-existing-device/9600]
				e/id = 'cannot-open
			]
			;; failed attempts must not break the following valid one
			--assert not error? try [
				loop 2000 [try [open serial://null/9600]]
				close open serial://ptmx/9600
			]
		--test-- "Device path as file, string and binary"
			foreach path reduce [%ptmx "ptmx" to binary! "ptmx" next "/ptmx" %/dev/ptmx][
				--assert all [
					port? p: try [open compose [scheme: 'serial path: (path) speed: 9600]]
					open? p
					port? try [close p]
				]
			]
		--test-- "Device path which is too long"
			--assert all [
				error? e: try [open compose [scheme: 'serial path: (append/dup copy "" #"a" 127) speed: 9600]]
				e/id = 'cannot-open ;; valid length, but there is no such device
			]
			--assert all [
				error? e: try [open compose [scheme: 'serial path: (append/dup copy "" #"a" 128) speed: 9600]]
				e/id = 'invalid-port-arg
			]
			--assert all [
				error? e: try [open compose [scheme: 'serial path: (append/dup copy "" #"a" 5000) speed: 9600]]
				e/id = 'invalid-port-arg
			]
		--test-- "Invalid device path"
			--assert all [
				error? e: try [open [scheme: 'serial path: 42 speed: 9600]]
				e/id = 'invalid-port-arg
			]
			--assert all [
				error? e: try [open [scheme: 'serial speed: 9600]]
				e/id = 'invalid-port-arg
			]
		--test-- "Close with a queued event"
			;; a finished write leaves an event in the system port's queue
			;; and this event must not survive the port's request
			events: system/ports/system/state ;; may hold events of other tests
			queued?: func [port][
				foreach event events [if port = event/port [return true]]
				false
			]
			n: length? events
			--assert all [
				port? p: try [open serial://ptmx/9600]
				port? try [write p #{01}]
				(n + 1) = length? events
				queued? p
				port? try [close p]
				n = length? events
				not error? try [wait 0.01]
			]
			;; events of other ports must not be removed
			n: length? events
			--assert all [
				port? p1: try [open serial://ptmx/9600]
				port? p2: try [open serial://ptmx/9600]
				port? try [write p1 #{01}]
				port? try [write p2 #{02}]
				port? try [write p1 #{03}]
				(n + 3) = length? events
				port? try [close p1]
				(n + 1) = length? events
				queued? p2
				not error? try [loop 10 [wait 0.01]]
				not queued? p2
				port? try [close p2]
			]
		--test-- "Modify control lines"
			p: open serial://ptmx/9600
			;; pty has no RTS and DTR lines, so the failure must be reported
			--assert none? try [modify p 'rts true]
			--assert none? try [modify p 'rts false]
			--assert none? try [modify p 'dtr true]
			--assert none? try [modify p 'dtr false]
			--assert not error? try [modify p 'brk true]
			--assert not error? try [modify p 'brk false]
			--assert all [
				error? e: try [modify p 'foo true]
				e/id = 'bad-file-mode
			]
			--assert all [
				error? e: try [modify p 'rts 1]
				e/id = 'invalid-value-for
			]
			;; the port must stay usable (and close must not crash)
			--assert open? p
			--assert port? try [close p]
			--assert all [
				error? e: try [modify p 'rts true]
				e/id = 'not-open
			]
		--test-- "Open with unsupported baud rate"
			foreach speed [0 -9600 12345][
				--assert all [
					error? e: try [open join serial://ptmx/ speed]
					e/id = 'cannot-open
				]
			]
	===end-group===
]

;; The following tests need a pair of connected serial devices.
;; The pair may be given using environment variables, for example
;; a virtual pair made by com0com on Windows:
;;     R3_SERIAL_A=CNCA0
;;     R3_SERIAL_B=CNCB0
;; On Linux the pair is made using socat (when it is available).
tty-a: tty-b: socat-pid: none
if serial? [
	case [
		all [
			tty-a: get-env "R3_SERIAL_A"
			tty-b: get-env "R3_SERIAL_B"
		][
			tty-a: to file! tty-a
			tty-b: to file! tty-b
		]
		all [
			system/platform = 'Linux
			0 = call/wait/shell "command -v socat > /dev/null && command -v stty > /dev/null"
		][
			tty-a: %/tmp/r3-ttyA
			tty-b: %/tmp/r3-ttyB
			attempt [delete tty-a]
			attempt [delete tty-b]
			socat-pid: call ["socat" "pty,raw,echo=0,link=/tmp/r3-ttyA" "pty,raw,echo=0,link=/tmp/r3-ttyB"]
			loop 50 [if exists? tty-b [break] wait 0.1]
		]
		'else [tty-a: tty-b: none]
	]
]
open-serial: func [
	"Opens the serial device using speed 9600"
	path [file!]
	/with spec [block!] "Additional port specification"
][
	open append compose [scheme: 'serial path: (path) speed: 9600] any [spec []]
]

if all [tty-a tty-b][
	===start-group=== "SERIAL (device pair)"
		--test-- "Read received data"
			a: open-serial tty-a
			b: open-serial tty-b
			--assert all [
				binary? try [read b] ;; nothing to read yet
				empty? b/data
			]
			write a #{010203}
			wait 0.1
			;; received data are available right after the read...
			--assert #{010203} == try [read b]
			;; ... and are not counted again when the read event is processed
			wait 0.1
			--assert #{010203} == b/data
			;; next data are appended to the port's buffer
			write a "abc"
			wait 0.1
			--assert #{010203616263} == try [read b]
			--assert same? b/data read b
			;; and the buffer may be cleared when the data are processed
			;; (even though the last read is still pending)
			clear b/data
			write a #{0405}
			wait 0.1
			--assert #{0405} == try [read b]
			clear b/data
		--test-- "Read data received while waiting"
			;; the read is pending when there are no data...
			--assert empty? read b
			write a #{0A0B0C}
			;; ... and it is finished while waiting for the port
			--assert all [
				b = wait [b 1]
				#{0A0B0C} == b/data
			]
			;; the buffer may be shortened while the read is pending
			--assert #{0A0B0C} == read b
			remove/part b/data 2
			write a #{0D0E}
			--assert all [
				b = wait [b 1]
				#{0C0D0E} == b/data
			]
			clear b/data
		--test-- "Modify does not cancel a pending read"
			--assert empty? read b ;; pending read
			modify b 'brk false
			modify b 'rts true
			write a #{0F}
			--assert all [
				b = wait [b 1]
				#{0F} == b/data
			]
			clear b/data
		--test-- "Write does not use the port's data"
			write a #{0102}
			wait 0.1
			read b
			data: copy #{AABB}
			--assert all [
				port? try [write b data]  ;; must not drop the received data
				#{0102} == b/data
			]
			wait 0.1
			--assert all [
				#{0102} == b/data
				#{AABB} == read a         ;; duplex
				#{AABB} == data           ;; received data are not appended to the written series
				not same? data a/data
			]
			;; the written series may be modified while its data are being sent
			write a data
			clear data
			wait 0.1
			--assert #{0102AABB} == read b
			clear a/data
			clear b/data
		--test-- "Close both devices"
			--assert port? try [close a]
			--assert port? try [close b]
			--assert not error? try [wait 0.01]
	===end-group===
]

if socat-pid [
	;; stty is used to see the real settings of an opened device.
	tty-flag?: func [
		"Returns true when the stty flag is set on the first device"
		flag [string!]
		/local out
	][
		out: copy ""
		call/wait/shell/output "stty -a -F /tmp/r3-ttyA" out
		replace/all out lf SP
		to logic! find rejoin [SP out SP] rejoin [SP flag SP]
	]

	===start-group=== "SERIAL (pty pair)"
		--test-- "Open without flow control"
			--assert all [
				port? p: try [open-serial tty-a]
				tty-flag? "-crtscts"
				tty-flag? "-ixon"
				tty-flag? "-ixoff"
			]
			try [close p]
		--test-- "Open with hardware flow control"
			--assert all [
				port? p: try [open-serial/with tty-a [flow-control: 'hardware]]
				tty-flag? "crtscts"
				tty-flag? "-ixon"
				tty-flag? "-ixoff"
			]
			try [close p]
		--test-- "Open with software flow control"
			--assert all [
				port? p: try [open-serial/with tty-a [flow-control: 'software]]
				tty-flag? "-crtscts"
				tty-flag? "ixon"
				tty-flag? "ixoff"
			]
			try [close p]
		--test-- "Close restores prior settings"
			--assert tty-flag? "-crtscts"
			--assert all [
				port? p: try [open-serial/with tty-a [flow-control: 'hardware]]
				tty-flag? "crtscts"
				port? try [close p]
				tty-flag? "-crtscts"
			]
		--test-- "Write binary and string data"
			;; data are collected from the second device by an external process
			call/shell "timeout 1 cat /tmp/r3-ttyB > /tmp/r3-serial-out"
			wait 0.3
			--assert all [
				port? p: try [open-serial tty-a]
				port? try [write p #{010203}]
				port? try [write p "abc"]
				port? try [write p "ěšč"] ;; sent as UTF-8
				port? try [write p %file]
				port? try [write p <tag>]
				port? try [write p skip #{0A0B0C0D} 2]
				port? try [write/part p #{0405060708} 2]
				port? try [write/part p "xyz" 100] ;; clipped to the data length
				port? try [write p #{}]
			]
		--test-- "Write value which has no bytes"
			;; these were sent as a raw memory or crashed!
			foreach value reduce [42 1.5 #"a" [1 2 "x"] 'word none true 1x2 now object [a: 1]][
				--assert all [
					error? e: try [write p :value]
					e/id = 'invalid-arg
				]
			]
			--assert port? try [write p #{FF}] ;; the port is still usable
			wait 1
			try [close p]
			--assert (read %/tmp/r3-serial-out) == rejoin [
				#{010203} #{616263} to binary! "ěšč" to binary! "file" to binary! "tag"
				#{0C0D} #{0405} #{78797A} #{FF}
			]
			try [delete %/tmp/r3-serial-out]
		--test-- "Write more data than the device accepts at once"
			;; the write stays pending and the following writes must keep the order
			a: open-serial tty-a
			b: open-serial tty-b
			data: make binary! 150000
			loop 50 [
				chunk: make binary! 3000
				loop 3000 [append chunk random 255]
				append data chunk
				write a chunk
			]
			loop 300 [
				read b
				if 150000 <= length? b/data [break]
				wait 0.01
			]
			--assert 150000 = length? b/data
			--assert data == b/data
			clear b/data
		--test-- "Close with pending read and write"
			--assert all [
				empty? read b                  ;; pending read
				port? try [loop 20 [write a data]] ;; nobody reads it, so it must stay pending
				port? try [close a]
				port? try [close b]
				not error? try [wait 0.1]
				not open? a
				not open? b
			]
		--test-- "Device which is gone"
			;; NOTE: this test must be the last one using the device pair!
			a: open-serial tty-a
			b: open-serial tty-b
			events: copy []
			b/awake: func [event][append events event/type true]
			--assert empty? read b ;; pending read
			call/wait/shell join "kill " socat-pid
			;; the pending read must be finished with an error event
			--assert all [
				b = wait [b 1]
				events = [error]
			]
			;; and it is not possible to read or write anymore
			--assert all [
				error? e: try [read b]
				e/id = 'read-error
			]
			--assert all [
				error? e: try [read a]
				e/id = 'read-error
			]
			--assert all [
				error? e: try [write a #{01}]
				e/id = 'write-error
			]
			--assert port? try [close a]
			--assert port? try [close b]
			--assert not error? try [wait 0.01]
		--test-- "Invalid flow control"
			--assert all [
				error? e: try [open-serial/with tty-a [flow-control: 'foo]]
				e/id = 'invalid-port-arg
			]
	===end-group===

	call/wait/shell join "kill " socat-pid
]

~~~end-file~~~
