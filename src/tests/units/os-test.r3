Rebol [
	Title:   "Rebol OS test script"
	Author:  "Oldes"
	File: 	 %os-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "OS"

===start-group=== "set-env / get-env"
;@@ https://github.com/Oldes/Rebol-issues/issues/533
;@@ https://github.com/Oldes/Rebol-issues/issues/1307
--test-- "env-1"
	--assert "hello" = set-env 'test-temp "hello"
	--assert "hello" = get-env 'test-temp
	--assert "hello" = set-env "test-temp" "hello"
	--assert "hello" = get-env "test-temp"
	--assert all [
		map? env: list-env
		"hello" = pick env "test-temp"
	]
--test-- "env-2"
	--assert "" = set-env 'test-temp ""
	--assert "" = get-env 'test-temp
--test-- "env-3"
	--assert none? set-env 'test-temp none
	--assert none? get-env 'test-temp
	--assert none? pick list-env "test-temp"

===end-group===


===start-group=== "sys/find-in-path"
	--test-- "find-in-path"
		dlm: pick ";:" system/platform = 'Windows
		make-dir %tmp-fip/
		write %tmp-fip/foo.txt ""
		dir: to-local-file clean-path %tmp-fip/
		res: clean-path %tmp-fip/foo.txt
		;; the only entry (no delimiter at all)
		--assert res = sys/find-in-path/with %foo.txt dir
		;; the last entry (no trailing delimiter)
		--assert res = sys/find-in-path/with %foo.txt rejoin ["not-exists" dlm dir]
		;; the first entry
		--assert res = sys/find-in-path/with %foo.txt rejoin [dir dlm "not-exists"]
		;; empty entries and trailing delimiter
		--assert res = sys/find-in-path/with %foo.txt rejoin [dlm dlm dir dlm]
		;; not found
		--assert none? sys/find-in-path/with %foo.txt rejoin ["not-exists" dlm]
		--assert none? sys/find-in-path/with %foo.txt ""
		--assert none? sys/find-in-path/with %foo.txt form dlm
		;; empty entry must not be resolved as a root directory
		--assert none? sys/find-in-path/with
			either system/platform = 'Windows [%Windows][%bin]
			rejoin [dlm dlm]
		;; default uses PATH env variable
		--assert none? sys/find-in-path %not-existing-file.foo
		p: get-env 'PATH
		--assert none? sys/find-in-path %foo.txt
		d: to-real-file %tmp-fip
		set-env 'PATH rejoin [p dlm to-local-file d]
		--assert d/foo.txt = sys/find-in-path %foo.txt
		set-env 'PATH p

		delete %tmp-fip/foo.txt
		delete %tmp-fip/
===end-group===


===start-group=== "others"
	--test-- "echo"
		;@@ https://github.com/Oldes/Rebol-issues/issues/1224
		echo %temp-echo
		print 123
		--assert not error? try [echo off]
		--assert not error? try [echo none]
		--assert 123 = try [load %temp-echo]
		delete %temp-echo

===end-group===


~~~end-file~~~
