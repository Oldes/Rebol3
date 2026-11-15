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
		--test-- "Invalid flow control"
			--assert all [
				error? e: try [open [scheme: 'serial path: %/tmp/r3-ttyA speed: 9600 flow-control: 'foo]]
				e/id = 'invalid-port-arg
			]
	===end-group===

	call/wait/shell join "kill " socat-pid
]

~~~end-file~~~
