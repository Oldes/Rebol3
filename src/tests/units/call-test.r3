Rebol [
	Title:   "Rebol3 CALL test script"
	Author:  "Oldes"
	File: 	 %call-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "CALL"

out-buffer: copy ""
err-buffer: copy ""

rebol-cmd: func[cmd][
	clear out-buffer
	clear err-buffer
	cmd: rejoin [to-local-file system/options/boot #" " cmd]
	;; stdin is not inherited (no TTY), so a security request is denied
	;; automatically instead of waiting for a key forever
	call/shell/output/error/input cmd out-buffer err-buffer none
]

===start-group=== "Command-Line Interface (/shell)"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2228
	;@@ https://github.com/Oldes/Rebol-issues/issues/2519
	--test-- "--do"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2467
		--assert 0 = rebol-cmd {--do "print 1 + 2"}
		--assert out-buffer = "3^/"
	--test-- "script args 1"
		;@@ https://github.com/Oldes/Rebol-issues/issues/1890
		--assert 0 = rebol-cmd {units/files/print-args.r3 a}
		--assert out-buffer = {["a"]^/["a"]^/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 a b}
		--assert out-buffer = {["a" "b"]^/["a" "b"]^/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 a "b c"}
		--assert out-buffer = {["a" "b c"]^/["a" "b c"]^/}
		either find [Macintosh Linux] system/platform [
			; single quotes are not used in Windows' command line
			--assert 0 = rebol-cmd {units/files/print-args.r3 a 'b " c'}
			--assert out-buffer = {["a" {b " c}]^/["a" {b " c}]^/}
		][
			--assert 0 = rebol-cmd {units/files/print-args.r3 a "b \" c"}
			--assert out-buffer = {["a" {b " c}]^/["a" {b " c}]^/}
		]
	--test-- "script args 2"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2227
		--assert 0 = rebol-cmd {-v}
		--assert not none? find/match out-buffer {Rebol/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 -v}
		--assert out-buffer = {["-v"]^/["-v"]^/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 -x}
		--assert out-buffer = {["-x"]^/["-x"]^/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 -- -x}
		--assert out-buffer = {["--" "-x"]^/["--" "-x"]^/}
		--assert 0 = rebol-cmd {units/files/print-args.r3 -v -- -x}
		--assert out-buffer = {["-v" "--" "-x"]^/["-v" "--" "-x"]^/}
		--assert 0 = rebol-cmd {--args "a b" units/files/print-args.r3 -v}
		--assert out-buffer = {["a b" "-v"]^/["a b"]^/}
		--assert 0 = rebol-cmd {--args "á b" units/files/print-args.r3 -v}
		--assert out-buffer = {["á b" "-v"]^/["á b"]^/}

		; providing script using --script option
		;@@ https://github.com/Oldes/Rebol-issues/issues/2469
		--assert 0 = rebol-cmd {--script units/files/print-args.r3 --args "a b" -- -v}
		--assert out-buffer = {["a b" "-v"]^/["a b"]^/}
		--assert 0 = rebol-cmd {--script units/files/print-args.r3 1 2}
		--assert out-buffer = {["1" "2"]^/["1" "2"]^/}
		--assert 0 = rebol-cmd {--args 1 --script units/files/print-args.r3 2}
		--assert out-buffer = {["1" "2"]^/["1"]^/}
	--test-- "option values"
		;; empty value must not be read past its end
		--assert 0 = rebol-cmd {--do ""}
		--assert out-buffer = ""
		;; values with `-` as the second char are valid values
		--assert 0 = rebol-cmd {--do "a-b: 3 print a-b"}
		--assert out-buffer = "3^/"
		--assert 0 = rebol-cmd {--args "a-b" units/files/print-args.r3}
		--assert out-buffer = {["a-b"]^/["a-b"]^/}
		;; single dash values are still accepted
		--assert 0 = rebol-cmd {--args "-x" units/files/print-args.r3}
		--assert out-buffer = {["-x"]^/["-x"]^/}
		;; repeated option: the last value is used (the previous is released)
		--assert 0 = rebol-cmd {--do "print 1" --do "print 2"}
		--assert out-buffer = "2^/"
		--assert 0 = rebol-cmd {--args "a" --args "b" units/files/print-args.r3}
		--assert out-buffer = {["b"]^/["b"]^/}
	--test-- "missing option value"
		;; value option followed by another --option is a usage error
		;; (help is shown and the script is not evaluated)
		--assert 0 = rebol-cmd {--args --quiet units/files/print-args.r3 a b}
		--assert not find out-buffer {["a" "b"]}
		--assert 0 = rebol-cmd {--script --quiet units/files/print-args.r3 a}
		--assert not find out-buffer {["a"]}
	--test-- "output into a cleared buffer"
		;; stale bytes after the new tail must not be counted
		buf: append/dup copy "" "x" 200
		clear buf
		--assert 0 = call/shell/output rejoin [
			to-local-file system/options/boot { --args "á b" units/files/print-args.r3}
		] buf
		--assert 16 = length? buf
		--assert buf = {["á b"]^/["á b"]^/}
	--test-- "script args 3"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2140
		cmd: "units/files/print-args.r3"
		repeat i 1000 [append append cmd #" " i]
		--assert 0 = rebol-cmd cmd
		--assert #{F41512F56B85D5805C62E6587C26A969DC3D41AE} = checksum out-buffer 'sha1
		if find [Macintosh Linux] system/platform [
			--assert 0 = rebol-cmd {--do "print system/options/args quit" `seq 1 1000`}
			--assert #{17900469C3C78A614B60FEE4E3851EF6BAF9D876} =  checksum out-buffer 'sha1
		]
	--test-- "--secure"
		--assert 0 = rebol-cmd {--secure none --do "probe system/options/secure"}
		--assert out-buffer == {none^/}
		;; invalid word is ignored (-s used, so the policy is not set from it)
		--assert 0 = rebol-cmd {-s --secure "foo bar" --do "probe system/options/secure"}
		--assert out-buffer == {_^/}
		--assert 0 = rebol-cmd {-s --secure 1 --do "probe system/options/secure"}
		--assert out-buffer == {_^/}
		;; invalid value without -s is a usage error
		--assert 1 = rebol-cmd {--secure "foo bar" --do "print {unexpected}"}
		--assert not find out-buffer "unexpected"
	--test-- "SECURE file policy of not existing path"
		;; must be stored as an exception, not replace the whole file policy
		--assert 0 = rebol-cmd {-s --do "d: to-file {/no/such/dir/} secure (reduce [d [ask write]]) probe select secure query d"}
		--assert out-buffer == "[allow read ask write allow execute]^/"
	--test-- "SECURE file policy reduction"
		;; exception is redundant only when equal to its nearest parent (or global) policy
		--assert 0 = rebol-cmd {-s --do "a: to-file {/a/} b: to-file {/a/b/} c: to-file {/a/b/c.reb} secure (reduce [a [ask write] b 'allow c [ask write]]) p: secure query probe select p b probe select p c"}
		--assert out-buffer == "allow^/[allow read ask write allow execute]^/"
	--test-- "SECURE startup policies"
		;; default policy protects startup script and modules from silent modification
		if all [system/options/data system/options/modules] [
			--assert 0 = rebol-cmd {--do "p: secure query probe select p system/options/data/user.reb probe select p system/options/modules"}
			--assert out-buffer == "[allow read ask write allow execute]^/[allow read ask write allow execute]^/"
		]
	--test-- "default SECURE policy without HOME"
		;; home is NONE, so it must not be used in the default policy
		;; (REBOL_HOME is set, so there is still a valid data directory)
		if find [Macintosh Linux] system/platform [
			clear out-buffer
			clear err-buffer
			--assert 0 = call/shell/output/error/input rejoin [
				{env -u HOME REBOL_HOME="} to-local-file first split-path system/options/boot {" }
				to-local-file system/options/boot { --do "print 1"}
			] out-buffer err-buffer none ;; no TTY, so it cannot wait on a security request
			--assert out-buffer = "1^/"
		]
	--test-- "NO_COLOR env variable"
		;@@ https://no-color.org/
		old: get-env "NO_COLOR"
		set-env "NO_COLOR" "1"
		--assert 0 = rebol-cmd {--do "probe system/options/no-color"}
		--assert out-buffer = "#(true)^/"
		if find [Macintosh Linux] system/platform [
			;; empty value must be ignored (not possible to set it on Windows)
			set-env "NO_COLOR" ""
			--assert 0 = rebol-cmd {--do "probe system/options/no-color"}
			--assert out-buffer = "#(false)^/"
		]
		;; --no-color option still works
		set-env "NO_COLOR" none
		--assert 0 = rebol-cmd {--no-color --do "probe system/options/no-color"}
		--assert out-buffer = "#(true)^/"
		set-env "NO_COLOR" old
	--test-- "--help"
		;; prints the version banner followed by the usage
		--assert 0 = rebol-cmd {--help}
		--assert did find/match out-buffer join version newline
		--assert 0 = rebol-cmd {--quiet --help}
		--assert not find/match out-buffer version
===end-group===


===start-group=== "Command-Line Interface"
	--test-- "Block input"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2582
	--assert all [
		file? try [write %issue-2582.r3 {Rebol [] print now}]
		tmp: clear ""
		0 = call/wait/output reduce [system/options/boot %issue-2582.r3] tmp
		date? transcode/one tmp
	]
	delete %issue-2582.r3
	--test-- "Block input with word!, get-word! and get-path!"
	--assert all [
		file? try [write %probe-args.r3 {Rebol [] probe system/options/args}]
		tmp: clear ""
		url: http://example.org
		;; not using reduce in this test...
		0 = call/wait/output [:system/options/boot %probe-args.r3 :url foo] tmp
		["http://example.org" "foo"] = transcode/one tmp
	]
	delete %probe-args.r3

===end-group===


===start-group=== "Error pipe"
	--test-- "User controlled error output"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2468
		--assert 0 = rebol-cmd {--do "prin 1 modify system/ports/output 'error on prin 2 modify system/ports/output 'error off prin 3"}
		--assert "13" = out-buffer
		--assert "2"  = err-buffer

	--test-- "Error printed to stderr"
		;@@ https://github.com/Oldes/Rebol-issues/issues/1862
		--assert 1 = rebol-cmd {--do "prin 2 1 / 0"}
		--assert "2" = out-buffer
		--assert not none? find err-buffer "Math error"
===end-group===


===start-group=== "LAUNCH"
	--test-- "launch"
		;@@ https://github.com/Oldes/Rebol-issues/issues/1403
		--assert 0 < launch %units/files/launched.r3 ;; returns a process id if not used /wait 
	--test-- "do launch"
		;@@ https://github.com/Oldes/Rebol-issues/issues/914
		--assert 0:0:1 > delta-time [do %units/files/launch.r3] ;; should not wait
	--test-- "do launch/wait"
		--assert 0:0:2 < delta-time [do %units/files/launch-wait.r3] ;; should wait
		--assert 6 = try [length? read/lines %units/files/launched.txt] ;; 6 because 3x launched!

	--test-- "do launch/with"
		--assert all [
			file? write %args.r3 {Rebol[] args: system/options/args forall args [args/1: transcode/one args/1] save %temp args}
			0 == launch/with/wait %args.r3 args: [2 {a "b" c} %"foo space"]
			args == load %temp
		]
		delete %temp
		delete %args.r3

	try [delete %units/files/launched.txt]
===end-group===


===start-group=== "Raw input"
	--test-- "Pipe input"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2613
		;; Reading stdin returns only data available at the moment (like POSIX `read`),
		;; and the writer may send its output in more chunks (`print` = value + newline),
		;; so read until the end of input (an empty result) to avoid a race.
		read-stdin: { --cgi --do "bin: copy #{} while [not empty? b: read/binary system/ports/input][append bin b] probe bin"}
		--assert all [
			0 = rebol-cmd rejoin [
				{--do "print 123 flush system/ports/output" | }
				to-local-file system/options/boot 
				read-stdin
			]
			find ["#{3132330A}^/" "#{313233}^/"] out-buffer ;; on Windows there is the CR char! 
			err-buffer == ""
		]
		--assert all [
			0 = rebol-cmd rejoin [
				{--do "prin 123 flush system/ports/output" | }
				to-local-file system/options/boot 
				read-stdin
			]
			out-buffer == "#{313233}^/"
			err-buffer == ""
		]
		--assert all [
			0 = rebol-cmd rejoin [
				{--do "prin {} flush system/ports/output" | }
				to-local-file system/options/boot 
				read-stdin
			]
			out-buffer == "#{}^/"
			err-buffer == ""
		]
	--test-- "Resolve length of bytes available on stdin"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2614
		;; The reader may query stdin before the writer has written anything,
		;; so poll for a while until some bytes are available (max ~1s).
		query-stdin: { --cgi --do "n: 0 loop 100 [if 0 < n: query system/ports/input 'length [break] wait 0.01] prin n"}
		--assert all [
			0 = rebol-cmd rejoin [
				{ --cgi --do "prin {}" | }
				to-local-file system/options/boot 
				query-stdin
			]
			out-buffer == "0"
			err-buffer == ""
		]
		--assert all [
			0 = rebol-cmd rejoin [
				{ --cgi --do "prin {1}" | }
				to-local-file system/options/boot 
				query-stdin
			]
			out-buffer == "1"
			err-buffer == ""
		]
	--test-- "Input with null byte"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2668
		clear out-buffer
		--assert all [
			0 = call/shell/output/input rejoin [
				to-local-file system/options/boot
				{ --cgi --do "prin length? read system/ports/input"}
			] out-buffer #{cafe001e} 
			out-buffer == "4"
		]
		clear out-buffer
		--assert all [
			0 = call/shell/output/input rejoin [
				to-local-file system/options/boot
				{ --cgi --do "prin length? read system/ports/input"}
			] out-buffer "one^@two"
			out-buffer == "7"
		]
	--test-- "Large binary input"
		;; input larger than the pipe buffer is written in more chunks
		bin: append/dup copy #{} #{0102030405060708} 100000 ;; 800kB
		clear out-buffer
		--assert all [
			0 = call/shell/output/input rejoin [
				to-local-file system/options/boot
				{ --cgi --do "bin: copy #{} while [not empty? b: read/binary system/ports/input][append bin b] prin checksum bin 'sha1"}
			] out-buffer bin
			out-buffer == form checksum bin 'sha1
		]
===end-group===

~~~end-file~~~