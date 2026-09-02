REBOL [
	System: "REBOL [R3] Language Interpreter and Run-time Environment"
	Title: "Generate OS host API headers"
	Rights: {
		Copyright 2012 REBOL Technologies
		Copyright 2012-2026 Rebol Open Source Contributors
		REBOL is a trademark of REBOL Technologies
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
	Author: ["Carl Sassenrath" "Oldes"]
	Version: 3.1.0
	Needs: 3.5.0
	Note: {
		Originaly Host was open-sourced part of Rebol3 while Core was closed.
		This script was used to generate headers for both parts.
		Now Host should be part of the Rebol library and so this script
		was simplified just to collect OS_* functions.

		It also collects the OS_Init_Ext_* entry points of the embedded
		extensions (which the extension generator emits into gen-<name>.c)
		into %gen-ext-init.h, so that host-main.c does not need an #ifdef
		per extension.
	}
]

context [ ; wrapped to prevent colisions with other build scripts

cnt: 0

xlib: make string! 20000
;; Entry points of the embedded extensions, collected separately.
ext-inits: make block! 8

emit:  func [d] [append repend xlib d newline]

func-header: [
	[
		thru "/***" 10 100 "*" newline
		thru "*/"
		copy spec to newline
		(if all [
			spec
			trim spec
			not find spec "static"
			any [  ; make sure we got only functions with "OS_" at the beginning
				find spec " *OS_"
				find spec " OS_"
			]
			find spec #"("
		][
			emit [spec ";    // " the-file]
			cnt: cnt + 1
			;; An embedded extension's entry point? Collect it for the
			;; aggregate header used by host-main.c
			if parse spec [
				thru "OS_Init_Ext_" copy ext-name to #"(" to end
			][
				append ext-inits ajoin ["OS_Init_Ext_" ext-name]
			]
		]
		)
		newline
		[
			"/*" ; must be in func header section, not file banner
			any [
				thru "**"
				[#" " | #"^-"]
				copy line thru newline
			]
			thru "*/"
			| 
			none
		]
	] | thru "/*"
]

process: func [file] [
	unless exists? file [
		;; A generated extension source which has not been produced yet.
		print ["** make-os-ext: missing file:" mold file]
		exit
	]
	data: read-file file
	parse data [
		any func-header
	]
]

foreach file c-host-files [ process file ]

out: rejoin [
	form-header/gen "Host Access Library" %host-lib.h %make-os-ext.reb
	{#define Host_Crash(reason) OS_Crash(cb_cast("REBOL Host Failure"), cb_cast(reason))} LF
	xlib
]

print out ;read-key

if cnt > 0 [
	write-generated root-dir/src/include/host-lib.h out
]

;-- Embedded extension entry points -------------------------------------------
;; host-main.c includes this and calls INIT_EMBEDDED_EXTENSIONS() once. The
;; list contains only extensions actually compiled in, because a source file
;; reaches `host-files` only when its `include-ext-*` target is active - so
;; no #ifdef guards are needed here.
sort ext-inits

out: make string! 2000
append out form-header/gen "Embedded Extension Entry Points" %gen-ext-init.h %make-os-ext.reb
append out {#ifndef GEN_EXT_INIT_H^/#define GEN_EXT_INIT_H^/^/}

either empty? ext-inits [
	append out {// No embedded extensions in this build.^/^/#define INIT_EMBEDDED_EXTENSIONS()^/}
][
	foreach name ext-inits [
		repend out ["RL_API void " name "(void);^/"]
	]
	append out {^/#define INIT_EMBEDDED_EXTENSIONS() \}
	foreach name ext-inits [
		repend out ["^/^-" name "(); \"]
	]
	take/last out ;; removes the last /
]
append out {^/#endif // GEN_EXT_INIT_H^/}

write-generated root-dir/src/include/gen-ext-init.h out

] ; end of context