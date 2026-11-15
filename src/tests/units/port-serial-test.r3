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

~~~end-file~~~
