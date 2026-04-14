REBOL [
	System: "REBOL [R3] Language Interpreter and Run-time Environment"
	Title: "REBOL 3 Mezzanine: Startup Banner"
	Rights: {
		Copyright 2012 REBOL Technologies
		Copyright 2012-2025 Rebol Open Source Developers
		REBOL is a trademark of REBOL Technologies
	}
	License: {
		Licensed under the Apache License, Version 2.0
		See: http://www.apache.org/licenses/LICENSE-2.0
	}
]

make-banner: func [
	"Build startup banner."
	fmt /local str star spc a b s sf
][
	if string? fmt [return fmt] ; aleady built
	str: make string! 2000
	append str format/pad [$0 #"╔" 74 "╗^/"] "" #"═"

	spc: format [#"║" $30.107 74 $0 #"║"] ""
	sf: [#"║" $30.107 "  " $35 72 $30.107 $0 #"║"]
	parse fmt [
		some [
			[
				set a string! (s: format sf a)
			  | set a block!  (s: format sf ajoin a)
			  | '= set a [string! | word! | set-word!] [
						b:
						  path! (b: get b/1)
						| word! (b: get b/1)
						| block! (b: reform b/1)
						| string! (b: b/1)
					]
					(s: either none? b [none][format [#"║" $30.107 "    " $32 11 $31 59 $30 $0 #"║"] reduce [a b]])
			  | '* (s: star)
			  | '- (s: spc)
			]
			(unless none? s [append append str s newline])
		]
	]
	append str format/pad [#"╚" 74 "╝^/"] "" #"═"
	str
]

if all [
    system/options/home
    #"/" <> first system/options/home
][
	;make sure that home directory is absolute on all platforms
	system/options/home: clean-path join what-dir system/options/home
]

sys/boot-banner: make-banner [
	-
	["REBOL/" system/product #" " system/version " (Oldes branch)"]
	-
	= Copyright: "2012 REBOL Technologies"
	= "" "2012-2025 Rebol Open Source Contributors"
	= "" "Apache 2.0 License, see LICENSE."
	= Website:  "https://github.com/Oldes/Rebol3"
	-
	= Platform: [
		ajoin [
			system/platform " | " system/build/target
			any [all [system/build/compiler join " | " system/build/compiler] ()]
		]
	]
	= Build:    system/build/date
	-
	= Home: [to-local-file any [system/options/home %"_"]]
	= Data: [to-local-file any [system/options/data %"_"]]
	-
]

system/license: make-banner [
	-
	= Copyright: "2012 REBOL Technologies"
	= "" "2012-2025 Rebol Open Source Contributors"
	= "" "Licensed under the Apache License, Version 2.0."
	= "" "https://www.apache.org/licenses/LICENSE-2.0"
	-
	= Notice: "https://github.com/Oldes/Rebol3/blob/master/NOTICE"
	-
]

;sys/boot-banner: ajoin ["REBOL/" system/product #" " system/version " (Oldes branch)"]
;system/license: "Licensed under the Apache License, Version 2.0."

append sys/boot-banner
{^/^[[1;33mImportant notes^[[0m:

  * Sandbox and security are not fully available.
  * Direct access to TCP HTTP required (no proxies).
  * Use at your own risk.
  * ^[[1;32m//^[[0m is now used as ^[[1;31minteger-divide^[[0m, for ^[[1;31mremainder^[[0m use ^[[1;32m%^[[0m or ^[[1;32m%%^[[0m (Euclidean division)!
  * For Python compatible ^[[1;31mmodulo^[[0m use ^[[1;32mmodulo/floor^[[0m.

^[[1;33mSpecial functions^[[0m:

  ^[[1;32mHelp^[[0m  - show built-in help information
}

if system/options/no-color [
	sys/remove-ansi sys/boot-banner
	sys/remove-ansi system/license
]

;print make-banner boot-banner halt
;print boot-help
