Rebol [
	Title:   "Rebol3 serial port! test script"
	Author:  "Oldes"
	File: 	 %port-serial-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "port-serial"

if all [
	find system/schemes 'serial
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
		--test-- "Open with unsupported baud rate"
			foreach speed [0 -9600 12345][
				--assert all [
					error? e: try [open join serial://ptmx/ speed]
					e/id = 'cannot-open
				]
			]
	===end-group===
]

if all [
	find system/schemes 'serial
	system/platform = 'Linux
	0 = call/wait/shell "command -v socat > /dev/null && command -v stty > /dev/null"
][
	;; socat provides a pair of connected virtual serial devices,
	;; and stty is used to see the real settings of an opened device.
	tty-a: %/tmp/r3-ttyA
	tty-b: %/tmp/r3-ttyB
	attempt [delete tty-a]
	attempt [delete tty-b]
	socat-pid: call ["socat" "pty,raw,echo=0,link=/tmp/r3-ttyA" "pty,raw,echo=0,link=/tmp/r3-ttyB"]
	loop 50 [if exists? tty-b [break] wait 0.1]

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
				port? p: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600]]
				tty-flag? "-crtscts"
				tty-flag? "-ixon"
				tty-flag? "-ixoff"
			]
			try [close p]
		--test-- "Open with hardware flow control"
			--assert all [
				port? p: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600 flow-control: 'hardware]]
				tty-flag? "crtscts"
				tty-flag? "-ixon"
				tty-flag? "-ixoff"
			]
			try [close p]
		--test-- "Open with software flow control"
			--assert all [
				port? p: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600 flow-control: 'software]]
				tty-flag? "-crtscts"
				tty-flag? "ixon"
				tty-flag? "ixoff"
			]
			try [close p]
		--test-- "Close restores prior settings"
			--assert tty-flag? "-crtscts"
			--assert all [
				port? p: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600 flow-control: 'hardware]]
				tty-flag? "crtscts"
				port? try [close p]
				tty-flag? "-crtscts"
			]
		--test-- "Invalid flow control"
			--assert all [
				error? e: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600 flow-control: 'foo]]
				e/id = 'invalid-port-arg
			]
	===end-group===

	call/wait/shell join "kill " socat-pid
]

~~~end-file~~~
