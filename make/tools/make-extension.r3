REBOL [
	Title:   "Universal Rebol extension code generator"
	Name:    make-extension
	Version: 0.1.0
	Author:  @Oldes
	Purpose: {
		Generates the C header and command-table sources of a Rebol
		extension from a declarative specification file.

		The specification is data only (never evaluated) - see
		`src/extensions/<name>/<name>.reb`.
	}
]

;-- helpers --------------------------------------------------------------------

to-c-name: func [
	"Converts a Rebol word into a C identifier"
	word [any-word! string!]
][
	word: form word
	foreach [f t] [
		#"-" #"_"
		#"." #"_"
		#"?" #"q"
		#"!" #"x"
		#"~" ""
		#"*" "_p"
		#"+" "_add"
		#"|" "or_bar"
	][replace/all word f t]
	;; guard against a mangle that produced something unusable
	if any [empty? word  find charset [#"0" - #"9"] word/1][
		cause-error 'user 'message reduce [ajoin ["Invalid C name from: " word]]
	]
	word
]

field: func [
	"Safely reads an optional field of an object"
	spec [block! object!] word [word!]
][
	select spec word
]

to-c-string: func [
	"Converts Rebol source into a continued C string literal"
	code [string!]
	/local out
][
	out: copy ""
	foreach line split code lf [
		;; order matters - backslash first!
		replace/all line #"\" "\\"
		replace/all line #"^"" {\"}
		append out ajoin [{\^/^-"} line {\n"}]
	]
	out
]

;-- main -----------------------------------------------------------------------

build-extension: function [
	"Generates extension C sources from a specification file"
	file [file!] "Extension specification"
	/into
	 dir  [file!] "Output directory (default: the spec's own directory)"
][
	src:  load/header file
	hdr:  take src
	spec: construct src
	?? spec

	unless dir [dir: first split-path file]

	;-- names ------------------------------------------------------------------
	;; Every global emitted here is prefixed, because embedded extensions all
	;; link into a single binary - an unprefixed `Command[]` or `W_ARG_ID`
	;; collides with the next embedded extension.
	;;
	;; NOTE: Rebol words are case-insensitive, so these three cannot differ
	;; only by case - the case lives in the string values, not the names.
	ext-name: form hdr/name                          ;; matrix
	ext-id:   to-c-name ext-name                     ;; matrix
	ext-caps: uppercase copy ext-id                  ;; MATRIX
	ext-cap1: uppercase/part copy ext-id 1           ;; Matrix

	fn-prefix:    any [field spec 'c-function-prefix  ajoin ["cmd_" ext-id "_"]]
	typedef-name: ajoin [ext-cap1 "_CommandPointer"]
	table-name:   ajoin [ext-cap1 "_Command"]
	call-name:    ajoin [ext-cap1 "_RX_Call"]
	init-macro:   ajoin [ext-caps "_EXT_INIT_CODE"]
	cmd-max:      ajoin ["CMD_" ext-caps "_MAX"]
	header-file:  ajoin [%gen- ext-name %.h]
	table-file:   ajoin [%gen- ext-name %.c]

	needs: any [field hdr 'needs  0.0.0]

	;-- module header of the generated extension --------------------------------
	;; `Type:` and `Date:` are injected here, not carried by the spec.
	reb-code: ajoin [
		{REBOL [Title: }  mold hdr/title
		{ Name: }         ext-name
		{ Type: module}
		{ Version: }      any [field hdr 'version 0.0.1]
		{ Needs: }        needs
		reduce if/only v: field hdr 'author  [{ Author:} mold v]
		{ Date: }         now/utc
		reduce if/only v: field hdr 'license [{ License:} mold v]
		reduce if/only v: field hdr 'url     [{ Url:}     v]
		{ Exports: }      mold any [field hdr 'exports []]
		#"]"
	]

	;-- word lists --------------------------------------------------------------
	;; `words:` declares lists resolved at init time through RL_MAP_WORDS.
	;; The leading _0 sentinel is required: RL_Find_Word returns 0 for
	;; not-found and 1-based indices otherwise.
	words:         field spec 'words
	word-enums:    copy ""
	word-globals:  copy ""
	word-externs:  copy ""
	init-words-fn: copy ""
	commands:      copy spec/commands

	if words [
		;; the init-words command is injected, never hand-written,
		;; and always first so command indices stay stable
		args: copy []
		foreach [wname wlist] words [
			repend args [to word! wname copy [block!]]
		]
		insert commands reduce [to set-word! 'init-words args]

		n: 0
		body: copy ""
		foreach [wname wlist] words [
			n: n + 1
			w-id:   to-c-name wname                          ;; arg
			w-caps: uppercase copy w-id                      ;; ARG
			w-var:  ajoin [ext-cap1 "_" w-id "_words"]       ;; Matrix_arg_words

			append word-enums ajoin ["^/enum " ext-id "_" w-id "_words {^/^-W_" ext-caps "_" w-caps "_0"]
			foreach word wlist [
				append word-enums ajoin [",^/^-W_" ext-caps "_" w-caps "_" uppercase to-c-name word]
			]
			append word-enums "^/};^/"

			append word-globals ajoin ["u32* " w-var " = NULL;^/"]
			append word-externs ajoin ["extern u32* " w-var ";^/"]
			append body ajoin ["^-" w-var " = RL_MAP_WORDS(RXA_SERIES(frm, " n "));^/"]
		]

		init-words-fn: ajoin [
			"^/int " fn-prefix "init_words(RXIFRM *frm, void *ctx) {^/"
			body
			"^-return RXR_TRUE;^/}^/"
		]
	]

	;-- commands ---------------------------------------------------------------
	enu-commands: copy ""
	cmd-declares: copy ""
	cmd-dispatch: copy ""

	foreach [cmd cmd-spec] commands [
		append reb-code ajoin [lf cmd ": command "]
		new-line/all cmd-spec false
		append/only reb-code mold cmd-spec

		cmd: to-c-name cmd
		append enu-commands ajoin ["^/^-CMD_" ext-caps "_" uppercase copy cmd #","]
		append cmd-declares ajoin ["^/int " fn-prefix cmd "(RXIFRM *frm, void *ctx);"]
		append cmd-dispatch ajoin ["^-" fn-prefix cmd ",^/"]
	]
	;; terminator - lets RX_Call bounds-check from any translation unit
	;; (sizeof() cannot: `Command[]` is an incomplete type where it's extern)
	append enu-commands ajoin ["^/^-" cmd-max]

	;-- mezzanine --------------------------------------------------------------
	if words [
		append reb-code ajoin [lf "init-words"]
		foreach [wname wlist] words [
			append reb-code ajoin [sp mold/flat wlist]
		]
		append reb-code "^/protect/hide 'init-words"
	]
	if mezz: field spec 'mezzanine [
		append reb-code ajoin [lf mold/only mezz]
	]

	init-code: to-c-string reb-code

	;-- includes / extra C declarations -----------------------------------------
	includes: copy ""
	foreach inc any [field spec 'c-include []] [
		append includes ajoin [{#include "} inc {"^/}]
	]
	c-header: ajoin [any [field spec 'c-header ""] word-externs]

	min-ver: needs/1
	min-rev: needs/2
	min-upd: needs/3

	logo: any [field spec 'logo  ajoin [
		"// Project: Rebol/" ext-cap1 " extension^/"
		"// SPDX-License-Identifier: " any [field hdr 'license "MIT"] "^/"
		"// ===========================================================================^/"
		"// NOTE: auto-generated file, do not modify!"
	]]

	;-- templates ---------------------------------------------------------------
	header-template: {$logo
#ifdef REB_EXT
#include "rebol-extension.h"
$includes
#define MIN_REBOL_VER $min-ver
#define MIN_REBOL_REV $min-rev
#define MIN_REBOL_UPD $min-upd
#define VERSION(a, b, c) (a << 16) + (b << 8) + c
#define MIN_REBOL_VERSION VERSION(MIN_REBOL_VER, MIN_REBOL_REV, MIN_REBOL_UPD)
#else
#include "reb-host.h"
#include "host-lib.h"
#include "sys-value.h"
#include "reb-struct.h"
#endif
#include "reb-ext-common.h"

$c-header
enum ext_commands {$enu-commands
};
$cmd-declares
$word-enums
typedef int (*$typedef-name)(RXIFRM *frm, void *ctx);
extern $typedef-name $table-name[];
int $call-name(int cmd, RXIFRM *frm, void *ctx);

#define $init-macro $init-code
}

	table-template: {$logo
#include "$header-file"

$word-globals
$typedef-name $table-name[] = {
$cmd-dispatch};
$init-words-fn
int $call-name(int cmd, RXIFRM *frm, void *ctx) {
	if (cmd < 0 || cmd >= $cmd-max) return RXR_NO_COMMAND;
	return $table-name[cmd](frm, ctx);
}
}


	;-- output ------------------------------------------------------------------
	vars: object compose [
		logo:          (logo)
		includes:      (includes)
		min-ver:       (min-ver)
		min-rev:       (min-rev)
		min-upd:       (min-upd)
		c-header:      (c-header)
		enu-commands:  (enu-commands)
		cmd-declares:  (cmd-declares)
		cmd-dispatch:  (cmd-dispatch)
		cmd-max:       (cmd-max)
		word-enums:    (word-enums)
		word-globals:  (word-globals)
		init-words-fn: (init-words-fn)
		typedef-name:  (typedef-name)
		table-name:    (table-name)
		call-name:     (call-name)
		header-file:   (header-file)
		init-macro:    (init-macro)
		init-code:     (init-code)
	]
	write rejoin [dir header-file] reword header-template vars
	write rejoin [dir table-file]  reword table-template  vars

	;; TODO: README generation from `handles:` plus tagged comments read
	;; from the raw spec text (deferred - see notes).

	reduce [header-file table-file]
]

src: none

if all [
	probe src: first system/script/args
	probe src: attempt [to-real-file src]
][
	build-extension src
]

