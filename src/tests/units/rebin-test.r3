Rebol [
	Title:   "Rebol3 Rebin test script"
	Author:  "Oldes"
	File: 	 %rebin-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3 %../fuzz-module.r3]
	type:    private
]

~~~start-file~~~ "REBIN"

if find system/codecs 'rebin [

;; Rebin stores datatypes as their internal numbers, which are not stable,
;; so the test data cannot be written as plain binaries.
rebin-types: make map! 64
repeat i length? system/catalog/datatypes [
	put rebin-types to word! pick system/catalog/datatypes i  i - 1
]
;; Header: "REBIN" version types flags
rebin-head: append append copy #{524542494E01} length? system/catalog/datatypes 0

rb: func [
	"Builds Rebin data (header is included)"
	spec [block!] "Binaries, datatype words (integer!), paths for a value with the new-line flag (integer!/line) or parens (evaluated)"
	/local out
][
	out: copy rebin-head
	foreach v spec [
		append out case [
			binary? v [v]
			word?   v [rebin-types/:v]
			path?   v [reduce [128 + rebin-types/(v/1) 1]]
			paren?  v [do v]
		]
	]
	out
]
;; Makes a varint (it is the same as Bincode's ENCODEDU64)
leb: func [n [integer!] /local b][b: binary 16  binary/write b [ENCODEDU64 :n]  b/buffer]

;; The first binary of the spec is always: symbol table (starts with number
;; of symbols) and number of values. So #{0002} is: no symbols and 2 values.

===start-group=== "Rebin header"
	--test-- "identify"
		bin: rb [#{0000}]
		--assert     do-codec system/codecs/rebin/entry 'identify bin
		--assert not do-codec system/codecs/rebin/entry 'identify head change copy bin #{524542494F} ;; magic
		--assert not do-codec system/codecs/rebin/entry 'identify head change skip copy bin 5 #{02}  ;; version
		--assert not do-codec system/codecs/rebin/entry 'identify copy/part bin 6                    ;; too short
	--test-- "data made for different datatypes are refused"
		bin: rb [#{0000}]
		--assert bin/7 = length? system/catalog/datatypes
		bin/7: bin/7 + 1
		--assert not do-codec system/codecs/rebin/entry 'identify bin
		--assert error? try [decode 'rebin bin]
	--test-- "no values"
		--assert [] == decode 'rebin rb [#{0000}]
===end-group===


===start-group=== "Rebin scalars"
	--test-- "unset none logic"
		blk: decode 'rebin rb [#{0004} unset! none! logic! #{01} logic! #{00}]
		--assert 4 = length? blk
		--assert unset? first blk
		--assert none?  second blk
		--assert true  == third blk
		--assert false == fourth blk
		;; logic must be 0 or 1
		--assert error? try [decode 'rebin rb [#{0001} logic! #{02}]]
		--assert error? try [decode 'rebin rb [#{0001} logic!]]
	--test-- "integer"
		--assert [0 1 -1 127 128 -129 9223372036854775807 -9223372036854775808]
			== decode 'rebin rb [
				#{0008}
				integer! #{00}
				integer! #{01}
				integer! #{FFFFFFFFFFFFFFFFFF01}
				integer! #{7F}
				integer! #{8001}
				integer! #{FFFEFFFFFFFFFFFFFF01}
				integer! #{FFFFFFFFFFFFFFFF7F}
				integer! #{80808080808080808001}
			]
	--test-- "decimal and percent"
		--assert [1.5 -0.1 50%]
			== decode 'rebin rb [
				#{0003}
				decimal! #{000000000000F83F}
				decimal! #{9A9999999999B9BF}
				percent! #{000000000000E03F}
			]
	--test-- "money"
		;; the 12 bytes are the same as `to binary! money`
		blk: decode 'rebin rb [
			#{0006}
			money! #{000000000000000000000000}
			money! #{000000000000000000000001}
			money! #{FF800000000000000000000F}
			money! #{7E800000000000000001E240}
			money! #{7F0000000000000000000064}
			money! #{800000000000000000000000}
		]
		--assert blk == [$0 $1 -$1.5 $123.456 $1.00 -$0]
		--assert "[$0 $1 -$1.5 $123.456 $1.00 -$0]" = mold blk
		;; the binary form of any loaded money value is decoded back to the same value
		foreach m [$0 -$0 $1.00 $0.000001 -$12345678901234567890.12345 $1e100 -$1e-100][
			--assert m == first decode 'rebin rb [#{0001} money! (to binary! m)]
		]
		;; the highest value
		--assert (to money! #{0052B7D2DCC80CD2E3FFFFFF})
			== first decode 'rebin rb [#{0001} money! #{0052B7D2DCC80CD2E3FFFFFF}]
	--test-- "invalid money"
		;; above the highest value
		--assert error? try [decode 'rebin rb [#{0001} money! #{0052B7D2DCC80CD2E4000000}]]
		--assert error? try [decode 'rebin rb [#{0001} money! #{0052B7D2DCC80CD300000000}]]
		--assert error? try [decode 'rebin rb [#{0001} money! #{0052B7D30000000000000000}]]
		--assert error? try [decode 'rebin rb [#{0001} money! #{007FFFFFDCC80CD2E3FFFFFF}]]
		--assert error? try [decode 'rebin rb [#{0001} money! #{0000000000000000000000}]] ;; short
	--test-- "datatype"
		blk: decode 'rebin rb [
			#{0004}
			datatype! (rebin-types/integer!)
			datatype! (rebin-types/end!)
			datatype! (rebin-types/datatype!)
			datatype! (rebin-types/utype!)
		]
		--assert blk == load "#(integer!) #(end!) #(datatype!) #(utype!)"
		--assert integer!  == blk/1
		--assert datatype? blk/1
		--assert datatype? blk/3
		--assert "#(integer!)" = mold blk/1
		--assert 2 = length? spec-of blk/1
		;; every datatype
		foreach t system/catalog/datatypes [
			--assert t == first decode 'rebin rb [#{0001} datatype! (rebin-types/(to word! t))]
		]
	--test-- "invalid datatype"
		--assert error? try [decode 'rebin rb [#{0001} datatype! (length? system/catalog/datatypes)]]
		--assert error? try [decode 'rebin rb [#{0001} datatype! #{FF}]]
		--assert error? try [decode 'rebin rb [#{0001} datatype!]]
	--test-- "typeset"
		;; bit n is the datatype n
		ts-bits: func [types /local n][
			n: 0
			foreach t types [n: n or shift 1 rebin-types/(to word! t)]
			n
		]
		blk: decode 'rebin rb [
			#{0004}
			typeset! #{00}
			typeset! (leb ts-bits [integer! string!])
			typeset! (leb ts-bits to block! any-type!)
			typeset! (leb ts-bits system/catalog/datatypes)
		]
		--assert typeset? blk/1
		--assert blk/1 == make typeset! []
		--assert blk/2 == make typeset! [integer! string!]
		--assert blk/2 == load "#(typeset! [integer! string!])"
		--assert blk/3 == any-type!
		--assert blk/4 == make typeset! system/catalog/datatypes
		--assert     find blk/2 integer!
		--assert not find blk/2 decimal!
		--assert (length? system/catalog/datatypes) = length? to block! blk/4
	--test-- "invalid typeset"
		;; a bit of a not existing datatype
		--assert error? try [decode 'rebin rb [#{0001} typeset! (leb shift 1 length? system/catalog/datatypes)]]
		--assert error? try [decode 'rebin rb [#{0001} typeset! (leb -1)]]
		--assert error? try [decode 'rebin rb [#{0001} typeset!]]
	--test-- "char"
		--assert [#"a" #"š" #"^(10FFFF)"]
			== decode 'rebin rb [#{0003} char! #{61} char! #{E102} char! #{FFFF43}]
		--assert error? try [decode 'rebin rb [#{0001} char! #{808044}]] ;; above the limit
		--assert error? try [decode 'rebin rb [#{0001} char! #{80B003}]] ;; surrogate
	--test-- "pair"
		--assert [1x2 -1.5x0.25]
			== decode 'rebin rb [#{0002} pair! #{0000803F00000040} pair! #{0000C0BF0000803E}]
	--test-- "tuple"
		--assert [1.2.3 255.0.128.64]
			== decode 'rebin rb [#{0002} tuple! #{03010203} tuple! #{04FF008040}]
		;; tuple longer than 12 bytes
		--assert error? try [decode 'rebin rb [#{0001} tuple! #{0D01020304050607080910111213}]]
	--test-- "time"
		--assert [0:00:01 -1:01:02.5]
			== decode 'rebin rb [#{0002} time! #{8094EBDC03} time! #{809EF48FB495FFFFFF01}]
	--test-- "date"
		;; year month day has-time [time zone]
		blk: decode 'rebin rb [
			#{0005}
			date! (leb 2000) #{01 01 00}
			date! (leb 2024) #{02 1D 00}
			date! (leb 2000) #{01 01 01} (leb 36'000'000'000'000) #{00}
			date! (leb 2000) #{01 01 01} (leb 36'000'000'000'000) #{08}
			date! (leb 1999) #{0C 1F 01} (leb 86'399'500'000'000) #{DE}
		]
		--assert blk == [1-Jan-2000 29-Feb-2024 1-Jan-2000/10:00 1-Jan-2000/10:00+2:00 31-Dec-1999/23:59:59.5-8:30]
		--assert blk == transcode "1-Jan-2000 29-Feb-2024 1-Jan-2000/10:00 1-Jan-2000/10:00+2:00 31-Dec-1999/23:59:59.5-8:30"
		--assert none?  blk/1/time
		--assert none?  blk/1/zone
		--assert 10:00  = blk/3/time
		--assert 0:00   = blk/3/zone
		--assert 10:00  = blk/4/time
		--assert 2:00   = blk/4/zone
		--assert 8:00   = blk/4/utc/time
		--assert -8:30  = blk/5/zone
		--assert 1-Jan-2000/8:29:59.5 = blk/5/utc
		--assert "31-Dec-1999/23:59:59.5-8:30" = mold blk/5
	--test-- "date limits"
		--assert [1-Jan-0000 31-Dec-16383] == decode 'rebin rb [
			#{0002}
			date! (leb 0)     #{01 01 00}
			date! (leb 16383) #{0C 1F 00}
		]
		--assert [1-Jan-2000/0:00+15:00 1-Jan-2000/23:59:59-15:00] == decode 'rebin rb [
			#{0002}
			date! (leb 2000) #{01 01 01} (leb 0)                  #{3C}
			date! (leb 2000) #{01 01 01} (leb 86'399'000'000'000) #{C4}
		]
	--test-- "invalid date"
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 16384) #{01 01 00}]] ;; year
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{00 01 00}]] ;; month
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{0D 01 00}]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 00 00}]] ;; day
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 20 00}]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{02 1E 00}]] ;; 30-Feb
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2023)  #{02 1D 00}]] ;; not a leap year
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 1900)  #{02 1D 00}]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 01 02}]] ;; has-time
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 01 01} (leb 86'400'000'000'000) #{00}]] ;; 24:00
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 01 01} (leb -1) #{00}]]                 ;; negative time
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 01 01} (leb 0)  #{3D}]]                 ;; zone +15:15
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)  #{01 01 01} (leb 0)  #{C3}]]                 ;; zone -15:15
		;; truncated
		--assert error? try [decode 'rebin rb [#{0001} date!]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000)]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000) #{01 01}]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000) #{01 01 01}]]
		--assert error? try [decode 'rebin rb [#{0001} date! (leb 2000) #{01 01 01} (leb 0)]]
===end-group===


===start-group=== "Rebin strings"
	--test-- "any-string"
		blk: decode 'rebin rb [
			#{0008}
			string! #{03616263}
			string! #{04C48D6175}
			string! #{00}
			file!   #{056120622F63}
			email!  #{03614062}
			ref!    #{03726566}
			url!    #{0C687474703A2F2F612E622F63}
			tag!    #{076220783D223122}
		]
		--assert blk == ["abc" "čau" "" %"a b/c" a@b @ref http://a.b/c <b x="1">]
		;; a not ASCII string must be handled as UTF-8
		--assert 3 = length? blk/2
		--assert #"č" = blk/2/1
		--assert "ČAU" = uppercase blk/2
	--test-- "binary"
		--assert [#{DEADBEEF} #{}] == decode 'rebin rb [#{0002} binary! #{04DEADBEEF} binary! #{00}]
		;; binary may hold any bytes
		--assert [#{61FA63}] == decode 'rebin rb [#{0001} binary! #{0361FA63}]
	--test-- "strings must be valid UTF-8"
		--assert error? try [decode 'rebin rb [#{0001} string! #{0361FA63}]] ;; invalid byte
		--assert error? try [decode 'rebin rb [#{0001} string! #{0261C4}]]   ;; incomplete sequence
		--assert error? try [decode 'rebin rb [#{0001} string! #{03EDA080}]] ;; surrogate
		--assert error? try [decode 'rebin rb [#{0001} file!   #{0361FA63}]]
		;; not ASCII part may start anywhere
		--assert ["abcč" "čabc" "aččb"] == decode 'rebin rb [
			#{0003}
			string! #{05616263C48D}
			string! #{05C48D616263}
			string! #{0661C48DC48D62}
		]
		--assert error? try [decode 'rebin rb [#{0001} string! #{03C48DFA}]]   ;; valid, then invalid
		--assert error? try [decode 'rebin rb [#{0001} string! #{04616263C4}]] ;; incomplete at the end
		--assert error? try [decode 'rebin rb [#{0001} string! #{0180}]]       ;; lone continuation byte
	--test-- "bitset"
		;; not length bytes
		blk: decode 'rebin rb [
			#{0004}
			bitset!      #{00 02 FF00}
			bitset!      #{01 02 FF00}
			bitset!      #{00 00}
			bitset!/line #{00 0D 00000000000000000000000060}
		]
		--assert blk == reduce [
			#(bitset! #{FF00})
			#(bitset! not #{FF00})
			#(bitset! #{})
			charset "ab"
		]
		--assert "#(bitset! #{FF00})"     = mold blk/1
		--assert "#(bitset! not #{FF00})" = mold blk/2
		--assert not complement? blk/1
		--assert     complement? blk/2
		--assert     blk/1/0
		--assert not blk/1/8
		--assert not blk/2/0
		--assert     blk/2/8
		--assert     blk/2/1000
		--assert     find blk/4 #"a"
		--assert not find blk/4 #"c"
		--assert new-line? skip blk 3
		;; the bitset may be modified
		blk/3/9: true
		--assert #(bitset! #{0040}) == blk/3
	--test-- "invalid bitset"
		--assert error? try [decode 'rebin rb [#{0001} bitset!]]
		--assert error? try [decode 'rebin rb [#{0001} bitset! #{02 01 FF}]] ;; not must be 0 or 1
		--assert error? try [decode 'rebin rb [#{0001} bitset! #{00}]]       ;; no length
		--assert error? try [decode 'rebin rb [#{0001} bitset! #{00 02 FF}]] ;; short data
	--test-- "decoded series are not shared with the input"
		bin: rb [#{0002} string! #{03616263} binary! #{04DEADBEEF}]
		blk: decode 'rebin bin
		clear bin
		--assert blk == ["abc" #{DEADBEEF}]
		--assert "abcd" == append blk/1 #"d"
===end-group===


===start-group=== "Rebin words"
	--test-- "any-word"
		;; symbols: a A b
		blk: decode 'rebin rb [
			#{03 0161 0141 0162  08}
			word!       #{00}
			set-word!   #{00}
			get-word!   #{00}
			lit-word!   #{00}
			refinement! #{00}
			issue!      #{00}
			word!       #{01}
			word!       #{02}
		]
		--assert blk == [a a: :a 'a /a #a A b]
		--assert "[a a: :a 'a /a #a A b]" = mold blk
		--assert blk/1 = blk/7           ;; the same word, just a different spelling
		--assert not strict-equal? blk/1 blk/7
		--assert none? context? blk/1    ;; not bound
	--test-- "decoded words may be bound"
		blk: decode 'rebin rb [#{01 0161  01} word! #{00}]
		a: 42
		--assert 42 = get first bind blk 'a
	--test-- "new symbols"
		;; symbols: rebin-new-word-2 rebin-new-word-1
		blk: decode 'rebin rb [
			#{02 10726562696E2D6E65772D776F72642D32 10726562696E2D6E65772D776F72642D31  03}
			word!      #{00}
			word!      #{01}
			word!/line #{00}
		]
		--assert "[rebin-new-word-2 rebin-new-word-1^/rebin-new-word-2]" = mold blk
===end-group===


===start-group=== "Rebin blocks"
	--test-- "block and paren"
		;; symbols: a b c
		blk: decode 'rebin rb [
			#{03 0161 0162 0163  03}
			word!   #{00}
			block!  #{03}
				word!    #{01}
				paren!   #{02}
					word!    #{02}
					integer! #{01}
				block!   #{00}
			string! #{0178}
		]
		--assert blk == [a [b (c 1) []] "x"]
		--assert paren? blk/2/2
		--assert 0 = length? blk/2/3
		;; the block may be modified
		--assert [a [b (c 1) []] "x" 2] == append blk 2
	--test-- "paths"
		;; symbols: a b
		blk: decode 'rebin rb [
			#{02 0161 0162  05}
			path!     #{02} word! #{00} word! #{01}
			set-path! #{02} word! #{00} word! #{01}
			get-path! #{02} word! #{00} word! #{01}
			lit-path! #{02} word! #{00} word! #{01}
			path!     #{03} word! #{00} integer! #{01} paren! #{01} word! #{01}
		]
		--assert blk == [a/b a/b: :a/b 'a/b a/1/(b)]
		--assert path?     blk/1
		--assert set-path? blk/2
		--assert get-path? blk/3
		--assert lit-path? blk/4
		--assert "[a/b a/b: :a/b 'a/b a/1/(b)]" = mold blk
	--test-- "image"
		;; width height RGBA
		blk: decode 'rebin rb [
			#{0004}
			image! #{02 01} #{FF0000FF00FF00FF}
			image! #{02 01} #{FF00008000FF00FF}
			image! #{00 00}
			image! #{01 02} #{0102030A0405060B}
		]
		--assert blk == load {[
			#(image! 2x1 #{FF000000FF00})
			#(image! 2x1 #{FF000000FF00} #{80FF})
			#(image! 0x0 #{})
			#(image! 1x2 #{010203040506} #{0A0B})
		]}
		--assert 2x1 = blk/1/size
		--assert 255.0.0.255 = blk/1/1
		--assert 255.0.0.128 = blk/2/1
		--assert 4.5.6.11 = blk/4/2
		--assert #{0102030A0405060B} = to binary! blk/4
		;; the image may be modified
		blk/1/2: 1.2.3.4
		--assert 1.2.3.4 = blk/1/2
	--test-- "invalid image"
		--assert error? try [decode 'rebin rb [#{0001} image! #{02 01} #{FF0000FF00FF00}]] ;; short data
		--assert error? try [decode 'rebin rb [#{0001} image! (leb 65536) #{00}]]           ;; too wide
		--assert error? try [decode 'rebin rb [#{0001} image! #{00} (leb 65536)]]           ;; too high
		--assert error? try [decode 'rebin rb [#{0001} image! (leb 65535) (leb 65535)]]     ;; no data
		--assert error? try [decode 'rebin rb [#{0001} image! #{01}]]
		--assert error? try [decode 'rebin rb [#{0001} image!]]
	--test-- "vector"
		;; type rows count data
		;; type: int8 int16 int32 int64 uint8 uint16 uint32 uint64 - - float32 float64
		blk: decode 'rebin rb [
			#{000B}
			vector! #{00 01 03} #{01FE03}
			vector! #{01 01 02} #{0100FFFF}
			vector! #{02 01 01} #{FFFFFFFF}
			vector! #{03 01 01} #{FFFFFFFFFFFFFFFF}
			vector! #{04 01 02} #{01FF}
			vector! #{05 01 02} #{0100FFFF}
			vector! #{06 01 01} #{FFFFFFFF}
			vector! #{07 01 01} #{0100000000000000}
			vector! #{0A 01 02} #{0000C03F000000C0}
			vector! #{0B 01 01} #{9A9999999999B93F}
			vector! #{02 01 00}
		]
		--assert blk == load {[
			#(int8! [1 -2 3])
			#(int16! [1 -1])
			#(int32! [-1])
			#(int64! [-1])
			#(uint8! [1 255])
			#(uint16! [1 65535])
			#(uint32! [4294967295])
			#(uint64! [1])
			#(float32! [1.5 -2.0])
			#(float64! [0.1])
			#(int32! [])
		]}
		--assert [int8! int16! int32! int64! uint8! uint16! uint32! uint64! float32! float64! int32!]
			== collect [foreach v blk [keep v/element-type]]
		--assert 0.1 = blk/10/1
		--assert empty? blk/11
		;; the vector may be modified
		--assert #(int32! [1]) == append blk/11 1
	--test-- "vector shape"
		blk: decode 'rebin rb [
			#{0002}
			vector! #{04 02 06} #{010203040506}
			vector! #{04 01 02} #{0102}
		]
		--assert blk/1 == load "#(uint8! 3x2 [1 2 3 4 5 6])"
		--assert 3x2 = query blk/1 'shape
		--assert 2x1 = query blk/2 'shape
		--assert 6 = pick blk/1 3x2
		;; a shaped vector cannot change its length
		--assert error? try [append blk/1 7]
		--assert #(uint8! [1 2 3]) == append blk/2 3
	--test-- "invalid vector"
		--assert error? try [decode 'rebin rb [#{0001} vector! #{08 01 01} #{00}]]   ;; float8 is not used
		--assert error? try [decode 'rebin rb [#{0001} vector! #{09 01 01} #{0000}]] ;; float16 is not used
		--assert error? try [decode 'rebin rb [#{0001} vector! #{0C 01 01} #{00}]]   ;; struct elements
		--assert error? try [decode 'rebin rb [#{0001} vector! #{FF 01 01} #{00}]]
		--assert error? try [decode 'rebin rb [#{0001} vector! #{04 00 01} #{00}]]   ;; no rows
		--assert error? try [decode 'rebin rb [#{0001} vector! #{04 02 03} #{000000}]] ;; not full rows
		--assert error? try [decode 'rebin rb [#{0001} vector! #{04 02 00}]]          ;; empty with rows
		--assert error? try [decode 'rebin rb [#{0001} vector! #{04 8080808001 01} #{00}]] ;; rows above the limit (also not full)
		--assert error? try [decode 'rebin rb [#{0001} vector! #{02 01 02} #{00000000}]]   ;; short data
		--assert error? try [decode 'rebin rb [#{0001} vector! #{02 01 FFFFFFFF0F}]]
		--assert error? try [decode 'rebin rb [#{0001} vector! #{02 01}]]
		--assert error? try [decode 'rebin rb [#{0001} vector!]]
	--test-- "hash"
		;; symbols: a b
		blk: decode 'rebin rb [
			#{02 0161 0162  02}
			hash! #{00}
			hash! #{05}
				word!    #{00} integer! #{01}
				word!    #{01} integer! #{02}
				decimal! #{000000000000F03F}
		]
		--assert hash? blk/1
		--assert empty? blk/1
		--assert hash? blk/2
		--assert blk/2 == load "#(hash! [a 1 b 2 1.0])"
		--assert "#(hash! [a 1 b 2 1.0])" = mold/all blk/2
		--assert 2 = select blk/2 'b
		--assert 3 = index? find blk/2 'b
		;; hash lookups use exact equality
		--assert 2 = index? find blk/2 1
		--assert 5 = index? find blk/2 1.0
		;; the hash may be modified (and used after it)
		append blk/2 [c 3]
		--assert 3 = select blk/2 'c
		insert blk/2 [x 0]
		--assert 2 = select blk/2 'b
		--assert 0 = select blk/2 'x
	--test-- "map"
		;; symbols: a b A
		blk: decode 'rebin rb [
			#{03 0161 0162 0141  03}
			map! #{00}
			map! #{06}
				word!     #{00} integer! #{01}
				set-word! #{01} integer! #{02}
				string!   #{0173} block! #{01} word! #{00}
			map! #{08}
				word! #{00} integer! #{01}
				word! #{00} integer! #{02} ;; the later one is used
				word! #{02} integer! #{03} ;; keys are case-sensitive
				none!       integer! #{04} ;; none key is not stored
		]
		--assert map? blk/1
		--assert empty? blk/1
		--assert blk/1 == make map! []
		--assert blk/2 == load {#[a 1 b: 2 "s" [a]]}
		--assert blk/3 == load {#[a 1 a 2 A 3 #(none) 4]}
		--assert 2 = length? blk/3
		--assert 2 = select/case blk/3 'a
		--assert 3 = select/case blk/3 'A
		--assert [a] == select blk/2 "s"
		;; keys are protected like in a loaded map
		--assert all [error? e: try [append pick keys-of blk/2 3 "x"]  e/id = 'protected]
		;; the map may be modified
		blk/1/c: 1
		--assert 1 = blk/1/c
	--test-- "invalid map"
		--assert error? try [decode 'rebin rb [#{01 0161  01} map! #{01} word! #{00}]] ;; odd number of values
		--assert error? try [decode 'rebin rb [#{0001} map! #{02} integer! #{01}]]       ;; missing value
		--assert error? try [decode 'rebin rb [#{0001} map!]]
	--test-- "new-line flags"
		;; [a^/b^/1 "x"^/[c^/d]]
		blk: decode 'rebin rb [
			#{04 0161 0162 0163 0164  05}
			word!         #{00}
			word!/line    #{01}
			integer!/line #{01}
			string!       #{0178}
			block!/line   #{02}
				word!      #{02}
				word!/line #{03}
		]
		--assert blk == [a b 1 "x" [c d]]
		--assert not new-line? blk
		--assert     new-line? next blk
		--assert     new-line? skip blk 2
		--assert not new-line? skip blk 3
		--assert     new-line? skip blk 4
		--assert not new-line? blk/5
		--assert     new-line? next blk/5
	--test-- "value flags"
		;; tag with the highest bit set is followed by the flags byte
		tag: 128 + rebin-types/integer!
		bin: rb [#{0002} integer! #{01}]
		blk: decode 'rebin append copy bin reduce [tag 1 2] ;; new-line flag
		--assert blk == [1 2]
		--assert new-line? next blk
		blk: decode 'rebin append copy bin reduce [tag 0 2] ;; no flag set
		--assert blk == [1 2]
		--assert not new-line? next blk
		;; not known flags are refused
		--assert error? try [decode 'rebin append copy bin reduce [tag 2 2]]
		--assert error? try [decode 'rebin append copy bin reduce [tag 3 2]]
		--assert error? try [decode 'rebin append copy bin reduce [tag 128 2]]
		;; missing flags byte or the value after it
		--assert error? try [decode 'rebin append copy bin reduce [tag]]
		--assert error? try [decode 'rebin append copy bin reduce [tag 1]]
	--test-- "deep nesting"
		;; 1000 nested blocks
		bin: rb [#{0001}]
		append/dup bin (rb-level: skip rb [block! #{01}] 8) 999
		append bin skip rb [block! #{00}] 8
		blk: decode 'rebin bin
		n: 0
		while [not empty? blk][blk: first blk ++ n]
		--assert n = 1000
	--test-- "too deep nesting is an error, not a crash"
		bin: rb [#{0001}]
		append/dup bin rb-level 5'000'000
		append bin skip rb [block! #{00}] 8
		--assert all [
			error? e: try [decode 'rebin bin]
			e/id = 'stack-overflow
		]
===end-group===


===start-group=== "Rebin invalid data"
	--test-- "bad header"
		--assert error? try [decode 'rebin #{}]
		--assert error? try [decode 'rebin #{524542494E}]
		--assert error? try [decode 'rebin head change rb [#{0000}] #{524542494F}]
		--assert error? try [decode 'rebin head change skip rb [#{0000}] 5 #{02}]
	--test-- "truncated"
		--assert error? try [decode 'rebin rb []]                                  ;; no symbol table
		--assert error? try [decode 'rebin rb [#{00}]]                             ;; no values
		--assert error? try [decode 'rebin rb [#{0001} integer!]]                  ;; integer without data
		--assert error? try [decode 'rebin rb [#{0001} decimal! #{00000000}]]      ;; short decimal
		--assert error? try [decode 'rebin rb [#{0001} pair! #{0000803F}]]         ;; short pair
		--assert error? try [decode 'rebin rb [#{0001} tuple!]]                    ;; tuple without data
		--assert error? try [decode 'rebin rb [#{0001} tuple! #{030102}]]          ;; short tuple
		--assert error? try [decode 'rebin rb [#{0001} string! #{05616263}]]       ;; short string
		--assert error? try [decode 'rebin rb [#{0002} integer! #{00}]]            ;; missing value
		--assert error? try [decode 'rebin rb [#{0001} block! #{02} integer! #{00} integer!]] ;; missing value in a block
	--test-- "data after the values"
		--assert error? try [decode 'rebin rb [#{0000FF}]]
		--assert error? try [decode 'rebin rb [#{0001} integer! #{00} integer! #{00}]]
	--test-- "not valid or not supported datatype"
		--assert error? try [decode 'rebin rb [#{0001} end!]]
		--assert error? try [decode 'rebin rb [#{0001} #{7F}]]
		--assert error? try [decode 'rebin rb [#{0001} object! #{00}]]
		--assert error? try [decode 'rebin rb [#{0001} handle! #{00}]]
	--test-- "count higher than the data size"
		--assert error? try [decode 'rebin rb [#{00FFFFFFFF0F}]]
		--assert error? try [decode 'rebin rb [#{00FFFFFFFFFFFFFFFFFF01}]]
		--assert error? try [decode 'rebin rb [#{FFFFFFFF0F00}]]
		--assert error? try [decode 'rebin rb [#{0001} string! #{FFFFFFFF0F}]]
		--assert error? try [decode 'rebin rb [#{0001} block!  #{FFFFFFFF0F}]]
		;; varint longer than 10 bytes
		--assert error? try [decode 'rebin rb [#{0001} integer! #{FFFFFFFFFFFFFFFFFFFF01}]]
	--test-- "invalid symbols"
		--assert error? try [decode 'rebin rb [#{01 0161  01} word! #{01}]] ;; index out of range
		--assert error? try [decode 'rebin rb [#{00  01} word! #{00}]]      ;; no symbols at all
		--assert error? try [decode 'rebin rb [#{01 00  01} word! #{00}]]   ;; zero length symbol
		--assert error? try [decode 'rebin rb [#{02 0161 00}]]              ;; truncated table
		--assert error? try [decode 'rebin rb [#{01 01FA  01} word! #{00}]] ;; not UTF-8
	--test-- "errors do not break following decodes"
		loop 1000 [
			try [decode 'rebin rb [#{01 0161  03} word! #{00} block! #{02} word! #{00} integer! #{00}]]
		]
		recycle
		--assert [a] == decode 'rebin rb [#{01 0161  01} word! #{00}]
===end-group===

===start-group=== "Rebin fuzz"
	--test-- "damaged data are refused, never crash"
		;; one valid input with each supported datatype (and the new-line flag)
		seeds: reduce [
			rb [#{0004} unset! none! logic! #{01} logic!/line #{00}]
			rb [#{0006} integer! #{7F} integer! #{FFFFFFFFFFFFFFFFFF01} decimal! #{000000000000F83F} percent! #{000000000000E03F} char! #{E102} pair! #{0000803F00000040}]
			rb [#{0004} tuple! #{03010203} time! #{8094EBDC03} money! #{FF800000000000000000000F} date!/line #{CF0F 0C 1F 01 8094EBDC03 DE}]
			rb [#{0008} binary! #{03010203} string! #{04C48D6175} file! #{0161} email! #{03614062} ref! #{03726566} url! #{03612F62} tag! #{0162} bitset! #{01 02 FF00}]
			rb [#{0004} datatype! (rebin-types/integer!) typeset! #{8001} image! #{0201 FF00008000FF00FF} vector! #{04 02 04 01020304}]
			rb [#{03 0161 0162 0141  06} word! #{00} set-word! #{01} get-word! #{02} lit-word! #{00} refinement! #{01} issue!/line #{02}]
			rb [#{02 0161 0162  05}
				block! #{02} word! #{00} paren! #{01} integer! #{01}
				path! #{02} word! #{00} word! #{01}
				set-path! #{02} word! #{00} integer! #{02}
				hash! #{02} word! #{00} integer! #{01}
				map! #{04} word! #{00} integer! #{01} string! #{0178} block! #{00}
			]
		]
		foreach s seeds [--assert block? decode 'rebin s] ;; seeds must be valid
		r: fuzz/count/keep func [b][mold decode 'rebin b] seeds 20000 8
		--assert r/inputs = 20000
		--assert r/ok > 1000          ;; many damaged inputs are still valid
		--assert r/errors/bad-media > 1000
		--assert [bad-media] = keys-of r/errors
===end-group===

] ;; end of codec check

~~~end-file~~~
