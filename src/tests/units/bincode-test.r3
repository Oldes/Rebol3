Rebol [
	Title:   "Rebol BinCode (binary) test script"
	Author:  "Oldes"
	File: 	 %bincode-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "BinCode"

is-range-error?: func[code][
	true? all [
		error? err: try code
		err/id = 'out-of-range
	] 
]
is-protected-error?: func[code][
	true? all [
		error? err: try code
		[
			err/id = 'protected
			err/id = 'locked-word
		]
	] 
]

===start-group=== "BinCode initialization"
	--test-- "BinCode-0"
		--assert object? b: binary none ; create bincode with default size
		--assert b/type = 'bincode
		--assert empty? b/buffer
		--assert empty? b/buffer-write
	--test-- "BinCode-1"
		--assert object? b: binary 16 ; pre-allocate buffer for 16 bytes
		--assert b/type = 'bincode
		--assert empty? b/buffer
		--assert empty? b/buffer-write
	--test-- "BinCode-2"
		--assert object? b: binary #{0badCafe} ; init with data already
		--assert not empty? b/buffer
		--assert not empty? b/buffer-write
		--assert 4 = length? b/buffer
		--assert 4 = length? b/buffer-write
		--assert object? binary/init b 1024 ; reinitialize existing bincode
		--assert empty? b/buffer
		--assert empty? b/buffer-write
		--assert object? binary/init b #{0badCafe} ; reinitialize with data
		--assert not empty? b/buffer
		--assert not empty? b/buffer-write
		--assert object? binary/init b none ; just clear existing bincode
		--assert empty? b/buffer
		--assert empty? b/buffer-write

===end-group===

===start-group=== "BinCode basic read/write functions"

	--test-- "BinCode - write positive unsigned integers (big endian)"
		b: binary 64
		--assert object? binary/write b [ui64 1 ui32 1 ui24 1 ui16 1 ui8 1]
		--assert #{000000000000000100000001000001000101} = b/buffer
		binary/init b none
		--assert object? binary/write b [ui64 1  ui32BE 1  ui24BE 1  ui16BE 1 ui8 1]
		--assert #{000000000000000100000001000001000101} = b/buffer
		--assert "11111" = rejoin binary/read b [ui64 ui32 ui24 ui16 ui8]
		--assert "11111" = rejoin binary/read b [AT 1 ui64be ui32be ui24be ui16be ui8]
		binary/init b none
		binary/write b [
			#{7FFFFFFFFFFFFFFF}
			#{7FFFFFFF}
			#{7FFFFFFFFFFFFFFF}
			#{7FFFFF}
			#{7FFFFFFFFFFFFFFF}
			#{7FFF}
			#{7FFFFFFFFFFFFFFF}
			#{7F}
		]
		--assert 9223372036854775807 = binary/read b 'UI64
		--assert 2147483647 = binary/read b 'UI32
		--assert 9223372036854775807 = binary/read b 'UI64
		--assert 8388607 = binary/read b 'UI24
		--assert 9223372036854775807 = binary/read b 'UI64
		--assert 32767 = binary/read b 'UI16
		--assert 9223372036854775807 = binary/read b 'UI64
		--assert 127 = binary/read b 'UI8
	--test-- "BinCode - read signed 32-bit with high bit set (UBSan)"
		--assert -2147483648 = binary/read #{80000000} 'si32
		--assert -2147483648 = binary/read #{80000000} 'si32be
		--assert -2147483648 = binary/read #{00000080} 'si32le
		--assert -1          = binary/read #{FFFFFFFF} 'si32
		--assert -32768.0    = binary/read #{00000080} 'fixed16
		--assert -1.0        = binary/read #{0000FFFF} 'fixed16
		;; reading floats from unaligned positions
		--assert [0 1.5] = binary/read #{000000C03F} [ui8 f32]
		--assert [0 1.5] = binary/read #{003FC00000} [ui8 f32be]
		--assert [0 1.5] = binary/read #{00000000000000F83F} [ui8 f64]
		--assert [0 1.5] = binary/read #{003FF8000000000000} [ui8 f64be]
	--test-- "BinCode - write negative unsigned integers (big endian)"
		binary/write b [UI8 -1 UI16 -1 UI24 -1 UI32 -1 UI64 -1]
		--assert #{FF FFFF FFFFFF FFFFFFFF FFFFFFFFFFFFFFFF} = b/buffer
		--assert "255 65535 16777215 4294967295 -1" = reform binary/read b [ui8 ui16 ui24 ui32 ui64]
	--test-- "BinCode - unsigned integers (little endian)"
		binary/init b none
		--assert object? binary/write b [ui8 1  ui16LE 1  ui24LE 1  ui32LE 1]
		--assert #{01010001000001000000} = b/buffer
		--assert "1111" = rejoin binary/read b [at 1 ui8 ui16LE ui24LE ui32LE]
	--test-- "BinCode - write signed integers over range"
		binary/init b none
		; max values... should be ok
		--assert object? binary/write b [SI8  127]
		--assert object? binary/write b [SI8 -128]
		--assert object? binary/write b [SI16  32767]
		--assert object? binary/write b [SI16 -32768]
		--assert object? binary/write b [SI24  8388607]
		--assert object? binary/write b [SI24 -8388608]
		--assert object? binary/write b [SI32  2147483647]
		--assert object? binary/write b [SI32 -2147483648]
		; over range values.. should be "out of range" error
		--assert is-range-error? [binary/write b [SI8  128]]
		--assert is-range-error? [binary/write b [SI8 -129]]
		--assert is-range-error? [binary/write b [SI16  32768]]
		--assert is-range-error? [binary/write b [SI16 -32769]]
		--assert is-range-error? [binary/write b [SI24  8388608]]
		--assert is-range-error? [binary/write b [SI24 -8388609]]
		--assert is-range-error? [binary/write b [SI32  2147483648]]
		--assert is-range-error? [binary/write b [SI32 -2147483649]]
		; results:
		--assert [127 -128 32767 -32768 8388607 -8388608 2147483647 -2147483648]
		         = binary/read b [SI8 SI8 SI16 SI16 SI24 SI24 SI32 SI32]

	--test-- "BinCode - write unsigned integers over range"
		binary/init b none
		; max values... should be ok
		--assert object? binary/write b [UI8  255]
		--assert object? binary/write b [UI16 65535]
		--assert object? binary/write b [UI24 16777215]
		--assert object? binary/write b [UI32 4294967295]
		; --assert object? binary/write b [UI64 18446744073709551615.0] ;- not yet suported

		; over range values.. should be "out of range" error
		--assert is-range-error? [binary/write b [UI8  256]]
		--assert is-range-error? [binary/write b [UI16 65536]]
		--assert is-range-error? [binary/write b [UI24 16777216]]
		--assert is-range-error? [binary/write b [UI32 4294967296]]
		; results:
		--assert [255 65535 16777215 4294967295] = binary/read b [UI8 UI16 UI24 UI32]

	--test-- "BinCode - SI16LE, SI16BE"
	     --assert 1 = binary/read #{0100} 'SI16LE
	     --assert 1 = binary/read #{0001} 'SI16BE

	b: binary 32
	--test-- "BinCode - strings"
	     --assert object? binary/write b [%ščř%20 "- " http://foo " " e@foo.com]
	     --assert "ščř - http://foo e@foo.com" = to string! b/buffer
	--test-- "BinCode - BYTES"
	b: binary 32
	     --assert object? binary/write b [#{cafe}]
	     --assert #{CAFE} = binary/read b 'bytes
	     --assert object? binary/write b [BYTES %ščř%20 BYTES http://foo]
	     --assert "ščř http://foo" = to string! binary/read b 'bytes
	--test-- "BinCode - UI8BYTES"
		--assert object? binary/write b [UI8BYTES #{cafe}]
		--assert #{02CAFE} = binary/read b 'bytes
		--assert #{CAFE} = binary/read #{02CAFE} 'UI8BYTES
	--test-- "BinCode - UI16BYTES"
		--assert object? binary/write b [UI16BYTES #{cafe}]
		--assert #{0002CAFE} = binary/read b 'bytes
		--assert #{CAFE} = binary/read #{0002CAFE} 'UI16BYTES
	--test-- "BinCode - UI16LEBYTES"
		--assert object? binary/write b [UI16LEBYTES #{cafe}]
		--assert #{0200CAFE} = binary/read b 'bytes
		--assert #{CAFE} = binary/read #{0200CAFE} 'UI16LEBYTES
	--test-- "BinCode - UI24BYTES"
		--assert object? binary/write b [UI24BYTES #{cafe}]
		--assert #{000002CAFE} = binary/read b 'bytes
		--assert #{CAFE} = binary/read #{000002CAFE} 'UI24BYTES
		--assert #{CAFE} = binary/read #{020000CAFE} 'UI24LEBYTES
	--test-- "BinCode - UI32BYTES"
		--assert object? binary/write b [UI32BYTES #{cafe}]
		--assert #{00000002CAFE} = binary/read b 'bytes
		--assert #{CAFE} = binary/read #{00000002CAFE} 'UI32BYTES
		--assert #{CAFE} = binary/read #{02000000CAFE} 'UI32LEBYTES

	--test-- "BinCode - read string (NUL terminated) - valid"
		--assert "ABC" = binary/read #{41424300} 'string
		--assert ""    = binary/read #{00} 'string
		--assert ["A" 66] = binary/read #{410042} [string ui8]
	--test-- "BinCode - read string (NUL terminated) - missing NUL"
		--assert error? try [binary/read #{414243} 'string]
		--assert error? try [binary/read #{} 'string]
		b: binary #{41}
		--assert error? try [binary/read b 'string]
		--assert 1 = binary/read b 'length?  ;; index not moved past tail

	--test-- "BinCode - write GET-WORD"
		a: 42 b: binary/write #{} [ui8 :a]
		--assert #{2A} = b/buffer

	--test-- "BinCode - write GET-PATH"
		i: 2 a: [42 255] b: binary/write #{} [UI8 :a/1 UI8 :a/:i]
		--assert #{2AFF} = b/buffer

	--test-- "BinCode - AT"
		;AT is using absolute positioning (one-based)
		b: binary 8
		
		binary/write b [AT 4 UI8 4]
		binary/write b [AT 3 UI8 3]
		binary/write b [AT 2 UI8 2]
		binary/write b [AT 1 UI8 1]
		--assert is-range-error? [binary/write b [AT -1 UI8 1]]
		--assert #{01020304} = b/buffer
		i: 0
		binary/read b [AT 1 i: UI8] --assert i = 1
		binary/read b [AT 2 i: UI8] --assert i = 2
		binary/read b [AT 3 i: UI8] --assert i = 3
		binary/read b [AT 4 i: UI8] --assert i = 4
		--assert is-range-error? [binary/read b [AT  5 i: UI8]]
		--assert is-range-error? [binary/read b [AT -1 i: UI8]]
	
	--test-- "BinCode - ATz"
		;ATz is using absolute positioning (zero-based)
		b: binary 8
		
		binary/write b [ATz 3 UI8 4]
		binary/write b [ATz 2 UI8 3]
		binary/write b [ATz 1 UI8 2]
		binary/write b [ATz 0 UI8 1]
		--assert is-range-error? [binary/write b [ATz -1 UI8 1]]
		--assert #{01020304} = b/buffer
		i: 0
		binary/read b [ATz 0 i: UI8] --assert i = 1
		binary/read b [ATz 1 i: UI8] --assert i = 2
		binary/read b [ATz 2 i: UI8] --assert i = 3
		binary/read b [ATz 3 i: UI8] --assert i = 4
		--assert is-range-error? [binary/read b [ATz  4 i: UI8]]
		--assert is-range-error? [binary/read b [ATz -1 i: UI8]]

	--test-- "BinCode - INDEX & INDEXz"
		b: binary #{01020304}
		--assert [1 0] = binary/read b [INDEX INDEXz]
		--assert [2 1] = binary/read b [AT  2 INDEX INDEXz]
		--assert [4 3] = binary/read b [ATz 3 INDEX INDEXz]
		binary/read b [AT 4 i: INDEX j: INDEXz]
		--assert all [i = 4 j = 3]

		b: binary #{0102030405}
		--assert 1 = binary/read b 'index
		--assert 0 = binary/read b 'indexz
		binary/read b 'ui8
		--assert 2 = binary/read b 'index
		--assert 1 = binary/read b 'indexz
		binary/read b 'ui16
		--assert [4 3] = binary/read b [index indexz]
		binary/read b [ui16]
		--assert [6 5] = binary/read b [index indexz]

	--test-- "BinCode - SKIP"
		;SKIP is using relative positioning
		b: binary #{01020304}
		i: 0
		binary/read b [SKIP  1 i: UI8] --assert i = 2
		binary/read b [SKIP -1 i: UI8] --assert i = 2
		binary/read b [SKIP  0 i: UI8] --assert i = 3
		binary/read b [SKIP -2 i: UI8] --assert i = 2
		binary/read b [AT 1 SKIP 3  i: UI8] --assert i = 4
		--assert is-range-error? [binary/read b [AT 1 SKIP  5 UI8]]
		--assert is-range-error? [binary/read b [AT 1 SKIP -1 UI8]]

	--test-- "BinCode - PAD"
		binary/write b: #{} [UI8 255 PAD 4 UI8 255]
		--assert b = #{FF000000FF}
		--assert [255 255] = binary/read b [UI8 PAD 4 UI8]
	--test-- "pad 0 / negative"
		--assert error? try [binary/write binary 8 [pad 0]]
		--assert error? try [binary/write binary 8 [pad -4]]
		--assert error? try [binary/read #{0102} [pad 0]]
		--assert error? try [binary/read #{0102} [pad -4]]
	--test-- "read pad beyond tail"
		--assert error? try [binary/read #{0102030405} [ui32 pad 8]]
	--test-- "read pad valid"
		--assert [1 5] = binary/read #{0102030405060708} [ui8 pad 4 ui8]
		--assert [1 2] = binary/read #{01020304} [ui8 pad 1 ui8]
	--test-- "write pad valid"
		bin: binary 8
		binary/write bin [ui8 1 pad 4 ui8 2]
		--assert #{0100000002} = binary/read bin 'bytes


	--test-- "BinCode - read length (DER) - short form"
		--assert 5   = binary/read #{05} 'length
		--assert 127 = binary/read #{7F} 'length
	--test-- "BinCode - read length (DER) - long form"
		--assert 128   = binary/read #{8180} 'length
		--assert 256   = binary/read #{820100} 'length
		--assert 65536 = binary/read #{83010000} 'length
		--assert 4294967295 = binary/read #{84FFFFFFFF} 'length
		--assert [256 255] = binary/read #{820100FF} [length ui8]
	--test-- "BinCode - read length (DER) - invalid"
		--assert error? try [binary/read #{80} 'length]           ;; indefinite
		--assert error? try [binary/read #{81} 'length]           ;; truncated
		--assert error? try [binary/read #{8201} 'length]         ;; truncated
		--assert error? try [binary/read #{850000000001} 'length] ;; too long


	--test-- "BinCode - LENGTH?"
		;LENGTH? returns number of bytes remaining in the buffer
		b: binary #{01020304}
		i: 0
		binary/read b [     i: LENGTH?] --assert i = 4
		binary/read b [UI16 i: LENGTH?] --assert i = 2
		binary/read b [UI16 i: LENGTH?] --assert i = 0

	--test-- "BinCode - UNIXTIME-NOW"
		;Writes UNIX time as UI32 value
		b: binary 4
		binary/write b [UNIXTIME-NOW UNIXTIME-NOW-LE]
		--assert 8 = length? b/buffer
		binary/read b [t1: UI32 t2: UI32LE]
		time: now/utc
		--assert time/date   = (1-Jan-1970 + (t1 // 86400))
		--assert time/hour   = (t1 % 86400 // 3600)
		--assert time/minute = (t1 % 86400 % 3600 // 60)
		;lets say that seconds will be ok too:)
		--assert time/date   = (1-Jan-1970 + (t2 // 86400))
		--assert time/hour   = (t2 % 86400 // 3600)
		--assert time/minute = (t2 % 86400 % 3600 // 60)

	--test-- "BinCode - overwrite protected values"
		out: copy #{} ;not yet protected
		blk: copy []
		--assert object? binary/write out #{babe}
		--assert (binary/read out [AT 1 i: ui8] i = 186)

		protect :out
		protect :blk
		protect/words [i]
		--assert is-protected-error? [binary/write out #{babe}]
		
		
		--assert is-protected-error? [binary/read out [AT 1 i: ui8]]
		--assert is-protected-error? [binary/read/into out [AT 1 ui8] blk]
		unprotect/words [blk out i]

	--test-- "BinCode - STRING"
		;@@ this is yet not fully implemented feature! So far only reading.
		b: binary #{74657374002A}
		binary/read b [str: STRING i: UI8]
		--assert str = "test"
		--assert   i = 42

	--test-- "BinCode - read using integer argument"
		b: #{010203}
		--assert #{0102}   = binary/read b 2
		--assert #{010203} = b
		--assert #{0203}   = binary/read next b 2
		b: binary #{01020304}
		--assert #{0102}   = binary/read b 2
		--assert #{0304}   = binary/read b 2
		--assert error? try [binary/read b 2] ;value out of range 


	--test-- "BinCode - bits (SB, UB, FB, ALIGN)"
		b: binary 2#{01011011 10110011 11111111}
		--assert [2 -2 3 -5 255] = binary/read b [SB 3 SB 3 UB 2 SB 4 ALIGN UI8]
		--assert [-2 6] = binary/read 2#{1110 0110} [SB 4 SB 4]
		--assert 14  = binary/read/with 2#{1110 0000} 'UB 4
		--assert [2.5] = binary/read #{500000} [FB 19]
		binary/read 2#{11111000 11111000} [x: SB 6 ALIGN y: FB 6]
		--assert -2   = x
		--assert -2.0 = (y * 65536.0)

	--test-- "BinCode - bits (variant using sigle value access)"
		bin: binary #{438E9438}
		--assert 1080 = binary/read/with bin 'SB 12
		--assert binary/read bin 'BIT
		--assert binary/read bin 'BIT
		--assert 10 = binary/read/with bin 'UB 4
		--assert not binary/read bin 'BIT
		--assert binary/read bin 'BIT
		--assert 1080 = binary/read/with bin 'SB 12
		--assert 2.5  = binary/read/with #{500000} 'FB 19

		bin: binary #{438E9438}
		binary/read bin [a: SB 12 BIT BIT b: UB 4 BIT BIT c: SB 12]
		--assert all [a = 1080 b = 10 c = 1080]

	--test-- "BinCode - bits with zero skip"
		; shuld not throw range error when bits number is 0
		--assert [0 0 0] = binary/read #{00} [UI8 SB 0 UB 0]

	--test-- "BinCode - EncodedU32"
		b: binary/init none 16
		binary/write b [
			EncodedU32 0
			EncodedU32 1
			EncodedU32 128
			EncodedU32 129
			EncodedU32 130
			EncodedU32 2214768806
		]
		--assert b/buffer = #{0001800181018201A6E18AA008}
		--assert [0 1 128 129 130 2214768806] = binary/read b [
			EncodedU32 EncodedU32 EncodedU32 EncodedU32 EncodedU32 EncodedU32]

	--test-- "BinCode - EncodedU64"
		b: binary/init none 16
		binary/write b [
			EncodedU64 0
			EncodedU64 0#0102030405
			EncodedU64 0#7FFFFFFFFFFF
			EncodedU64 0#7FFFFFFFFFFFFF
			EncodedU64 0#7FFFFFFFFFFFFFFF
		]
		--assert b/buffer = #{0085888C9010FFFFFFFFFFFF1FFFFFFFFFFFFFFF3FFFFFFFFFFFFFFFFF7F}
		--assert [0 0#0102030405 0#7FFFFFFFFFFF 0#7FFFFFFFFFFFFF 0#7FFFFFFFFFFFFFFF] = binary/read b [
			EncodedU64 EncodedU64 EncodedU64 EncodedU64 EncodedU64]

	--test-- "BinCode - VINT"
	;; Another variable-length integer encoding (used in EBML/Matroska files)
		b: binary/init none 16
		binary/write b [
			VINT 0
			VINT 1
			VINT 128
			VINT 129
			VINT 130
			VINT 2214768806
		]
		--assert b/buffer = #{8081408040814082088402B0A6}
		--assert [0 1 128 129 130 2214768806] = binary/read b [
			VINT VINT VINT VINT VINT VINT]

	--test-- "BinCode - BITSET8, BITSET16, BITSET32 (read)"
		binary/read #{81800180000001} [
			f8:  BITSET8
			f16: BITSET16
			f32: BITSET32
		]
		--assert all [
			all [f8/0 f8/7]
			not any [f8/1 f8/2 f8/3 f8/4 f8/5 f8/6]
		]
		--assert all [
			all [f16/0 f16/15]
			not any [f16/1 f16/2 f16/3 f16/4 f16/5 f16/6 f16/7]
		]
		--assert all [
			all [f32/0 f32/31]
			not any [f32/1 f32/2 f32/3 f32/4 f32/5 f32/6 f32/7 f32/15]
		]

	--test-- "BinCode - FIXED8 and FIXED16 (read)"
		binary/read #{800700800700} [
			f8:  FIXED8
			f16: FIXED16
		]
		--assert 7.5 = f8
		--assert 7.5 = f16

	--test-- "BinCode - TUPLE3 and TUPLE4 (read)"
		binary/read #{01020304050607} [
			rgb:  TUPLE3
			rgba: TUPLE4
		]
		--assert 1.2.3   = rgb
		--assert 4.5.6.7 = rgba

	--test-- "BinCode - SKIPBITS"
		--assert [2 3] = binary/read 2#{00000000 11000011} [
			SKIPBITS 9 UB 2
			SKIPBITS 3 UB 2
		]
	--test-- "BinCode - SKIPBITS then ub in separate call"
		b: binary #{A5}
		binary/read b [skipbits 4]
		--assert [5] = binary/read b [ub 4]
		b: binary #{A5}
		binary/read b [skipbits 4]
		--assert 5 = binary/read/with b 'ub 4
	--test-- "BinCode - SKIPBITS then bit in separate call"
		b: binary #{0F}
		binary/read b [skipbits 4]
		--assert binary/read b 'bit
	--test-- "BinCode - SKIPBITS over byte boundary"
		b: binary #{FF0F}
		binary/read b [skipbits 12]
		--assert [15] = binary/read b [ub 4]
	--test-- "BinCode - SKIPBITS in same block (unchanged)"
		b: binary #{A5}
		--assert [5] = binary/read b [skipbits 4 ub 4]

	--test-- "BinCode - ALIGN"
		--assert [0 1 2] = binary/read #{008002} [
			UB 8 ALIGN ; align on byte boundary is noop
			UB 1 ALIGN ; this align should move input pointer
			UI8 ; and this read should return value 2
		]

	--test-- "BinCode - FLOAT16, FLOAT, DOUBLE (read)"
		--assert 1.0 = binary/read #{003C} 'FLOAT16
		--assert 1.0 = binary/read #{0000803F} 'FLOAT
		--assert 1.0 = binary/read #{000000000000F03F} 'DOUBLE

	--test-- "BinCode - FLOAT16, FLOAT, DOUBLE"
		b: binary/write #{} [float16 0.5 float16 1000 float16 32.5 float16 -32.5]
		--assert b/buffer = #{0038D063105010D0}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [float16 float16 float16 float16]
		b: binary/write #{} [float   0.5 float   1000 float   32.5 float   -32.5]
		--assert b/buffer = #{0000003F00007A4400000242000002C2}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [float float float float]
		b: binary/write #{} [double  0.5 double  1000 double  32.5 double  -32.5]
		--assert b/buffer = #{000000000000E03F0000000000408F40000000000040404000000000004040C0}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [double double double double]

	--test-- "BinCode - F16LE, F16BE"
		b: binary/write #{} [f16be   0.5 f16be   1000 f16be   32.5 f16be   -32.5]
		--assert b/buffer = #{380063D05010D010}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f16be f16be f16be f16be]
		b: binary/write #{} [f16le   0.5 f16le   1000 f16le   32.5 f16le   -32.5]
		--assert b/buffer = #{0038D063105010D0}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f16le f16le f16le f16le]

	--test-- "BinCode - read f16 zero"
		--assert 0.0 = binary/read #{0000} 'f16
		--assert 0.0 = binary/read #{0080} 'f16
		--assert 0.0 = binary/read #{0000} 'f16be
	--test-- "BinCode - read f16 subnormals"
		--assert 5.9604644775390625e-8   = binary/read #{0100} 'f16
		--assert -5.9604644775390625e-8  = binary/read #{0180} 'f16
		--assert 6.0975551605224609375e-5 = binary/read #{FF03} 'f16
		--assert 5.9604644775390625e-8   = binary/read #{0001} 'f16be
	--test-- "BinCode - read f16 normals (unchanged)"
		--assert 6.103515625e-5 = binary/read #{0004} 'f16
		--assert 1.0     = binary/read #{003C} 'f16
		--assert 65504.0 = binary/read #{FF7B} 'f16

	--test-- "BinCode - F32LE, F32BE"
		b: binary/write #{} [f32be   0.5 f32be   1000 f32be   32.5 f32be   -32.5]
		--assert b/buffer = #{3F000000447A000042020000C2020000}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f32be f32be f32be f32be]
		b: binary/write #{} [f32le   0.5 f32le   1000 f32le   32.5 f32le   -32.5]
		--assert b/buffer = #{0000003F00007A4400000242000002C2}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f32le f32le f32le f32le]

	--test-- "BinCode - F64LE, F64BE"
		b: binary/write #{} [f64be   0.5 f64be   1000 f64be   32.5 f64be   -32.5]
		--assert b/buffer = #{3FE0000000000000408F4000000000004040400000000000C040400000000000}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f64be f64be f64be f64be]
		b: binary/write #{} [f64le   0.5 f64le   1000 f64le   32.5 f64le   -32.5]
		--assert b/buffer = #{000000000000E03F0000000000408F40000000000040404000000000004040C0}
		--assert [0.5 1000.0 32.5 -32.5] = binary/read b [f64le f64le f64le f64le]

	--test-- "BinCode - FLOAT16, FLOAT, DOUBLE (write/read NAN)"
		b: binary/write #{} [float16 1.#NaN float 1.#NaN double 1.#NaN]
		--assert not none? find [
			#{007E0000C07F000000000000F87F}
			#{007E0000C0FF000000000000F8FF} ; when used clang compiler (should it be normalized?)
		] b/buffer
		--assert tail? b/buffer-write
		;@@ using MOLD as: 1.#nan <> 1.#nan 
		--assert "[1.#NaN 1.#NaN 1.#NaN]" = mold binary/read b [float16 float double]

	--test-- "BinCode - MSDOS-DATETIME"
		;- old and not much precise format for storing date and time from MS-DOS times
		;- still used in some file formats, like ZIP

		--assert  4-Apr-2018/18:53:56  = binary/read #{BC96844C} 'MSDOS-DATETIME
		--assert [18:53:56 4-Apr-2018] = binary/read #{BC96844C} [MSDOS-TIME MSDOS-DATE]

		b: binary 64
		binary/write b [
			msdos-time 11:32:20
			msdos-time 21:23:55 ;<- will be stored as 21:23:54 (2sec resolution only)
			msdos-time 14-Mar-2019/15:29:52 ;<- only time is used
			msdos-time 14-Mar-2019 ;<- zero time is used

			msdos-date     14-Mar-2019/15:29:52 ;<- only date is used
			msdos-datetime 14-Mar-2019/15:29:52
			msdos-datetime 14-Mar-2019/15:33:18+1:00
		]
		--assert 11:32:20 = binary/read b 'MSDOS-TIME
		--assert 21:23:54 = binary/read b 'MSDOS-TIME
		--assert 15:29:52 = binary/read b 'MSDOS-TIME
		--assert 0:0:0    = binary/read b 'MSDOS-TIME
		--assert 14-Mar-2019 = binary/read b 'MSDOS-DATE
		--assert 14-Mar-2019/15:29:52 = binary/read b 'MSDOS-DATETIME
		--assert 14-Mar-2019/14:33:18 = binary/read b 'MSDOS-DATETIME

		--assert error? try [binary/write b [MSDOS-DATE 15:33:18]] ;<- date required

	--test-- "write msdos-datetime rejects time!"
		--assert error? try [binary/write binary 8 [msdos-datetime 10:20:30]]


	--test-- "BinCode - STRING-BYTES (read)"
		;- used for fixed size strings (used for example in TAR files)
		--assert ["test" 1]= binary/read #{746573740000000001} [STRING-BYTES 8 UI8]

	--test-- "BinCode - OCTAL-BYTES (read)"
		;- used for fixed size octal numbers (used for example in TAR files)
		--assert 8 = binary/read/with #{3130} 'OCTAL-BYTES 2

	--test-- "BinCode - match binary!"
		b: binary #{0badCafe}
		--assert binary/read b #{0bad}
		--assert binary/read b #{Cafe}
		--assert tail? b/buffer
		--assert [#(true) #(false) #(true)] = binary/read b [
			ATz 0   ; reset position to head
			#{0bad} ; true and advance
			#{F00D} ; false, no advance
			#{Cafe} ; true and advance
		]
		--assert [#(true) #{CAFE}] = binary/read b [ATz 0 #{0bad} BYTES 2]


	--test-- "write empty block returns ctx"
		b: binary #{0102}
		--assert object? binary/write b []
	--test-- "write/read with empty block still reads"
		b: binary #{0102}
		--assert 1 = binary/write/read b [] 'ui8
		--assert [2] = binary/write/read b [] [ui8]
	--test-- "write binary returns ctx"
		b: binary 16
		--assert object? binary/write b #{0102}
	--test-- "write/read with binary data"
		b: binary 16
		--assert [1 2] = binary/write/read b #{0102} [ui8 ui8]
		--assert 3 = binary/write/read b #{03} 'ui8
===end-group===


===start-group=== "BinCode - write set-word with index/indexz"
	--test-- "set-word positions"
		bin: binary 16
		binary/write bin [p1: ui8 1 p2: index ui8 2 p3: indexz ui8 3 p4:]
		--assert p1 = 1
		--assert p2 = 2
		--assert p3 = 2
		--assert p4 = 4
		--assert #{010203} = binary/read bin 'bytes
	--test-- "set-word value usable later in same block"
		bin: binary 16
		binary/write bin [ui8 0 p1: indexz ui8 :p1]
		--assert #{0001} = binary/read bin 'bytes
===end-group===


===start-group=== "BinCode special read/write functions"

	--test-- "BinCode - CROP"
	;; CROP shrinks buffers from the left side at the position of the read buffer
		b: binary #{010203}
		--assert [1] = binary/read b [UI8 CROP]
		--assert all [
			#{0203} = b/buffer
			#{0203} = b/buffer-write
		]
		--assert b = binary/write b [UI8 4]
		--assert all [
			#{0403} = b/buffer
			  #{03} = b/buffer-write
		]
		--assert [4] = binary/read b [UI8 CROP]
		--assert all [
			  #{03} = b/buffer
			  #{03} = b/buffer-write
		]
		--assert b = binary/write b [AT 2 UI8 5]
		--assert all [
			#{0305} = b/buffer
			    #{} = b/buffer-write
		]
		--assert [3 5] = binary/read b [UI8 UI8 CROP]
		--assert all [
			empty? head b/buffer
			empty? head b/buffer-write
		]

	--test-- "crop all then read"
		b: binary #{0102}
		--assert error? try [binary/read b [ui16 crop ui8]]
	--test-- "crop large (bias reset) then read"
		b: binary append/dup #{} 0 5000
		--assert error? try [binary/read b [skip 4096 crop bytes 905]]
		b: binary append/dup #{} 0 5000
		--assert [904] = binary/read b [skip 4096 crop length?]
	--test-- "crop small keeps data"
		b: binary #{01020304}
		--assert [1 2 3 4] = binary/read b [ui8 crop ui8 ui8 ui8]
===end-group===


===start-group=== "BinCode other issues"
	--test-- "Missing additional read value"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2601
	--assert all [error? e: try [binary/read #{010203} [UI8 UB]] e/arg1 = 'UB]
	--assert all [error? e: try [binary/read #{010203} [UI8 FB]] e/arg1 = 'FB]
	--assert all [error? e: try [binary/read #{010203} [UI8 SB]] e/arg1 = 'SB]
	--assert all [error? e: try [binary/read #{010203} [UI8 AT]] e/arg1 = 'AT]
	--assert all [error? e: try [binary/read #{010203} [UI8 ATz]] e/arg1 = 'ATz]
	--assert all [error? e: try [binary/read #{010203} [UI8 PAD]] e/arg1 = 'PAD]
	--assert all [error? e: try [binary/read #{010203} [UI8 SKIP]] e/arg1 = 'SKIP]
	--assert all [error? e: try [binary/read #{010203} [UI8 SKIPBITS]] e/arg1 = 'SKIPBITS]

	--test-- "Code block not at its head"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2603
	--assert [1 2] == try [binary/read #{0102} next [ignored UI8 UI8]]
	--assert all [
		not error? try [binary/write b: #{} next [ignored UI8 1 UI8 2]]
		b == #{0102}
	]

	--test-- "Write at tail"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2637
	bin: #{010203} binary/write at bin 2 [ui8 255]
	--assert bin == #{01FF03}
	bin: #{010203} binary/write at bin 2 [ui8 255 ui8 255 ui8 255 ui8 255]
	--assert bin == #{01FFFFFFFF}
	bin: #{010203} binary/write tail bin [ui8 255]
	--assert bin == #{010203FF}
	bin: #{010203} binary/write tail bin [ui8 255 ui8 255]
	--assert bin == #{010203FFFF}

	--test-- "Invalid read spec"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2671
	--assert all [error? e: try [binary/read #{01} ["foo"]]  e/id = 'invalid-spec]
	--assert all [error? e: try [binary/read #{01} [ui8 42]]  e/id = 'invalid-spec]

===end-group===


===start-group=== "BinCode - write gaps are zeroed"
	--test-- "at past tail on reused buffer"
		bin: binary 16
		binary/write bin #{FFFFFFFFFFFFFFFF}
		binary/init bin none
		binary/write bin [at 5 ui8 1]
		--assert #{0000000001} = binary/read bin 'bytes
	--test-- "at past tail after existing data"
		bin: binary 16
		binary/write bin #{0102030405060708}
		binary/init bin none
		binary/write bin [ui8 1 ui8 2]
		binary/write bin [at 5 ui8 9]
		--assert #{0102000009} = binary/read bin 'bytes
	--test-- "at back into existing data keeps it"
		bin: binary 16
		binary/write bin [ui32 0#01020304 at 2 ui8 255]
		--assert #{01FF0304} = binary/read bin 'bytes
===end-group===


===start-group=== "BinCode - unsigned write range"
	--test-- "negative values (two's complement) accepted"
		bin: binary 16
		binary/write bin [ui8 -1 ui16 -1 ui24 -1 ui16le -2 ui24le -2]
		--assert #{FFFFFFFFFFFFFEFFFEFFFF} = binary/read bin 'bytes
	--test-- "values not fitting the width rejected"
		--assert error? try [binary/write binary 8 [ui8 -256]]
		--assert error? try [binary/write binary 8 [ui16 -65536]]
		--assert error? try [binary/write binary 8 [ui24 -16777216]]
		--assert error? try [binary/write binary 8 [ui8 4294967297]]
		--assert error? try [binary/write binary 8 [ui16 4294967297]]
	--test-- "upper bounds"
		--assert error? try [binary/write binary 8 [ui8 256]]
		--assert error? try [binary/write binary 8 [ui16 65536]]
		--assert error? try [binary/write binary 8 [ui24 16777216]]
		--assert error? try [binary/write binary 8 [ui8 #"€"]]
	--test-- "valid values"
		bin: binary 16
		binary/write bin [ui8 255 ui16 65535 ui24le 16777215 ui8 #"A" ui16 #"€"]
		--assert #{FFFFFFFFFFFF4120AC} = binary/read bin 'bytes
===end-group===


===start-group=== "BinCode - read count validation"
	--test-- "read N"
		--assert #{0102} = binary/read #{010203} 2
		--assert #{}     = binary/read #{010203} 0
		--assert error? try [binary/read #{0102} 3]
		--assert error? try [binary/read #{0102} -1]
		--assert error? try [binary/read #{0102} 4294967297]
	--test-- "bytes N"
		--assert [#{0102}] = binary/read #{010203} [bytes 2]
		--assert error? try [binary/read #{0102} [bytes -1]]
		--assert error? try [binary/read #{0102} [bytes 4294967297]]
		--assert error? try [binary/read #{0102} [string-bytes 4294967297]]
===end-group===


===start-group=== "BinCode - random-bytes"
	--test-- "valid"
		bin: binary 16
		binary/write bin [ui8 1 random-bytes 4 ui8 2]
		--assert 6 = binary/read bin 'length?
		--assert [1 2] = binary/read bin [ui8 skip 4 ui8]
		bin: binary 16
		binary/write bin [random-bytes 0]
		--assert 0 = binary/read bin 'length?
	--test-- "invalid counts"
		--assert error? try [binary/write binary 8 [random-bytes -1]]
		--assert error? try [binary/write binary 8 [random-bytes 4294967297]]
===end-group===


===start-group=== "BinCode - ub/sb/fb"
	--test-- "ub"
		--assert [10 5] = binary/read #{A5} [ub 4 ub 4]
		--assert [511] = binary/read #{FF80} [ub 9]
		--assert [4294967295] = binary/read #{FFFFFFFF} [ub 32]
		--assert [-1] = binary/read #{FFFFFFFFFFFFFFFF} [ub 64]
		--assert [0] = binary/read #{FF} [ub 0]
	--test-- "sb"
		--assert [-6 5] = binary/read #{A5} [sb 4 sb 4]
		--assert [-1] = binary/read #{FFFFFFFF} [sb 32]
		--assert [-1] = binary/read #{FFFFFFFFFFFFFFFF} [sb 64]
	--test-- "fb"
		--assert [1.0]  = binary/read #{00010000} [fb 32]
		--assert [-1.0] = binary/read #{FFFF0000} [fb 32]
	--test-- "invalid / out of data"
		--assert error? try [binary/read #{FF} [ub -1]]
		--assert error? try [binary/read #{FF} [ub 65]]
		--assert error? try [binary/read #{FF} [ub 9]]
		--assert error? try [binary/read #{} [sb 1]]
		--assert error? try [binary/read #{} [fb 1]]
===end-group===


===start-group=== "BinCode - read encodedu32/u64"
	--test-- "valid"
		--assert 300 = binary/read #{AC02} 'encodedu32
		--assert 4294967295 = binary/read #{FFFFFFFF0F} 'encodedu32
		--assert -1 = binary/read #{FFFFFFFFFFFFFFFFFF01} 'encodedu64
		--assert 4294967295 = binary/read #{FFFFFFFFFFFFFFFFFF01} 'encodedu32
		--assert [300 1] = binary/read #{AC0201} [encodedu32 ui8]
	--test-- "invalid"
		--assert error? try [binary/read #{80} 'encodedu32]
		--assert error? try [binary/read #{8080808080808080808000} 'encodedu64]
		--assert error? try [binary/read #{8080808080808080808000} 'encodedu32]
===end-group===


===start-group=== "BinCode - vint"
	--test-- "write valid"
		bin: binary 32
		binary/write bin [vint 0 vint 126 vint 127 vint 128 vint 16382 vint 16383]
		--assert #{80FE407F40807FFE203FFF} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [vint 72057594037927934]
		--assert #{01FFFFFFFFFFFFFE} = binary/read bin 'bytes
	--test-- "write/read roundtrip"
		bin: binary 32
		binary/write bin [vint 0 vint 127 vint 16383 vint 72057594037927934]
		--assert [0 127 16383 72057594037927934] = binary/read bin [vint vint vint vint]
	--test-- "write out of range"
		--assert error? try [binary/write binary 16 [vint 72057594037927935]]
		--assert error? try [binary/write binary 16 [vint -1]]
	--test-- "read"
		--assert 127 = binary/read #{FF} 'vint   ;; reserved, but still decoded
		--assert error? try [binary/read #{00} 'vint]
		--assert error? try [binary/read #{0000000000000000FF} 'vint]
		--assert error? try [binary/read #{4080} [vint vint]]
===end-group===


===start-group=== "BinCode - msdos date year range"
	--test-- "valid bounds"
		bin: binary 8
		binary/write bin [msdos-date 1-Jan-1980 msdos-date 31-Dec-2107]
		--assert [1-Jan-1980 31-Dec-2107] = binary/read bin [msdos-date msdos-date]
		bin: binary 8
		binary/write bin [msdos-datetime 1-Jan-1980/0:00 msdos-datetime 31-Dec-2107/23:59:58]
		--assert [1-Jan-1980/0:00 31-Dec-2107/23:59:58] = binary/read bin [msdos-datetime msdos-datetime]
	--test-- "out of range"
		--assert error? try [binary/write binary 8 [msdos-date 31-Dec-1979]]
		--assert error? try [binary/write binary 8 [msdos-date 1-Jan-2108]]
		--assert error? try [binary/write binary 8 [msdos-datetime 31-Dec-1979/12:00]]
		--assert error? try [binary/write binary 8 [msdos-datetime 1-Jan-2108/12:00]]
===end-group===


===start-group=== "BinCode - signed BE/LE variants"
	--test-- "write signed BE/LE"
		bin: binary 64
		binary/write bin [si16be -2 si16le -2 si24be -2 si24le -2 si32be -2 si32le -2 si64be -2 si64le -2]
		--assert #{FFFEFEFFFFFFFEFEFFFFFFFFFFFEFEFFFFFFFFFFFFFFFFFFFFFEFEFFFFFFFFFFFFFF} = binary/read bin 'bytes
	--test-- "read signed BE/LE"
		bin: binary 64
		binary/write bin [si16be -2 si16le -2 si24be -2 si24le -2 si32be -2 si32le -2 si64be -2 si64le -2]
		--assert [-2 -2 -2 -2 -2 -2 -2 -2] = binary/read bin [si16be si16le si24be si24le si32be si32le si64be si64le]
	--test-- "positive values"
		bin: binary 16
		binary/write bin [si24le 1193046]
		--assert #{563412} = binary/read bin 'bytes
		--assert 1193046 = binary/read #{563412} 'si24le
		--assert 1 = binary/read #{0000000000000001} 'si64
		--assert 1 = binary/read #{0100000000000000} 'si64le
	--test-- "range"
		--assert error? try [binary/write binary 8 [si16le 32768]]
		--assert error? try [binary/write binary 8 [si24le 8388608]]
		--assert error? try [binary/write binary 8 [si32le 2147483648]]
===end-group===


===start-group=== "BinCode - *bebytes read aliases"
	--test-- "write/read ui16/24/32bebytes"
		bin: binary 32
		binary/write bin [ui16bebytes #{0102} ui24bebytes #{03} ui32bebytes #{0405}]
		--assert #{0002010200000103000000020405} = binary/read bin 'bytes
		bin: binary 32
		binary/write bin [ui16bebytes #{0102} ui24bebytes #{03} ui32bebytes #{0405}]
		--assert [#{0102} #{03} #{0405}] = binary/read bin [ui16bebytes ui24bebytes ui32bebytes]
	--test-- "same as plain forms"
		--assert [#{AA}] = binary/read #{0001AA} [ui16bebytes]
		--assert [#{AA}] = binary/read #{0001AA} [ui16bytes]
===end-group===


===start-group=== "BinCode - bit-level write"
	--test-- "ub/sb"
		bin: binary 16
		binary/write bin [ub 4 10 ub 4 5]
		--assert #{A5} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [sb 4 -6 sb 4 5]
		--assert #{A5} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [ub 9 511]
		--assert #{FF80} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [ub 64 -1]
		--assert #{FFFFFFFFFFFFFFFF} = binary/read bin 'bytes
	--test-- "fb"
		bin: binary 16
		binary/write bin [fb 32 1.0 fb 32 -1 fb 32 0.5]
		--assert #{00010000FFFF000000008000} = binary/read bin 'bytes
	--test-- "bit / not-bit / align"
		bin: binary 16
		binary/write bin [bit #(true) bit #(false) bit 1 not-bit #(false) align ui8 1]
		--assert #{B001} = binary/read bin 'bytes
	--test-- "implicit align before byte commands and raw data"
		bin: binary 16
		binary/write bin [ub 3 5 ui8 255 bit #(true) #{CC}]
		--assert #{A0FF80CC} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [ub 3 5]
		binary/write bin #{CC}
		--assert #{A0CC} = binary/read bin 'bytes
	--test-- "bits continue across write calls"
		bin: binary 16
		binary/write bin [ub 4 10]
		binary/write bin [ub 4 5]
		--assert #{A5} = binary/read bin 'bytes
	--test-- "roundtrip with read"
		bin: binary 16
		binary/write bin [ub 5 17 sb 7 -33 fb 20 -2.5 bit #(true) align ui16 1234]
		--assert [17 -33 -2.5 #(true) 1234] = binary/read bin [ub 5 sb 7 fb 20 bit align ui16]
	--test-- "/init resets bit masks"
		bin: binary 16
		binary/write bin [ub 3 5]
		binary/init bin none
		binary/write bin [ub 4 10 ub 4 5]
		--assert #{A5} = binary/read bin 'bytes
	--test-- "range / argument errors"
		--assert error? try [binary/write binary 8 [ub 4 16]]
		--assert error? try [binary/write binary 8 [ub 4 -9]]
		--assert error? try [binary/write binary 8 [sb 4 8]]
		--assert error? try [binary/write binary 8 [sb 4 -9]]
		--assert error? try [binary/write binary 8 [ub 65 1]]
		--assert error? try [binary/write binary 8 [ub -1 1]]
		--assert error? try [binary/write binary 8 [ub 0 1]]
		--assert error? try [binary/write binary 8 [bit 2]]
		--assert error? try [binary/write binary 8 [bit "x"]]
		--assert error? try [binary/write binary 8 [ub 4]]
		bin: binary 8
		try [binary/write bin [ui8 1 ub 4 16]]
		--assert 0 = binary/read bin 'length?   ;; nothing written on error
===end-group===


===start-group=== "BinCode - write string (NUL terminated)"
	--test-- "write"
		bin: binary 16
		binary/write bin [string "ABC" string "" string #{4142} ui8 1]
		--assert #{41424300004142 0001} = binary/read bin 'bytes
	--test-- "roundtrip"
		bin: binary 16
		binary/write bin [string "Hello" ui16 6523]
		--assert ["Hello" 6523] = binary/read bin [string ui16]
	--test-- "after bits (implicit align)"
		bin: binary 16
		binary/write bin [ub 3 5 string "A"]
		--assert #{A04100} = binary/read bin 'bytes
	--test-- "invalid argument"
		--assert error? try [binary/write binary 8 [string 1]]
		--assert error? try [binary/write binary 8 [string]]
===end-group===


===start-group=== "BinCode - missing argument at block end"
	--test-- "write command without argument"
		--assert error? try [binary/write binary 8 [ui8]]
		--assert error? try [binary/write binary 8 [string]]
		--assert error? try [binary/write binary 8 [bytes]]
		--assert error? try [binary/write binary 8 [ub 4]]
		--assert error? try [binary/write binary 8 [bit]]
===end-group===


===start-group=== "BinCode - signed write range"
	--test-- "min/max values"
		bin: binary 32
		binary/write bin [si8 -128 si8 127 si16 -32768 si16le 32767 si24 -8388608 si24le 8388607 si32 -2147483648 si32le 2147483647]
		--assert #{807F8000FF7F800000FFFF7F80000000FFFFFF7F} = binary/read bin 'bytes
		bin: binary 32
		binary/write bin [si8 -128 si16 -32768 si24 -8388608 si32 -2147483648]
		--assert [-128 -32768 -8388608 -2147483648] = binary/read bin [si8 si16 si24 si32]
	--test-- "out of range"
		--assert error? try [binary/write binary 8 [si8 -129]]
		--assert error? try [binary/write binary 8 [si8 128]]
		--assert error? try [binary/write binary 8 [si16le -32769]]
		--assert error? try [binary/write binary 8 [si32 2147483648]]
	--test-- "char!"
		bin: binary 8
		binary/write bin [si8 #"A" si16 #"€"]
		--assert #{4120AC} = binary/read bin 'bytes
===end-group===


===start-group=== "BinCode - range error leaves buffer untouched"
	--test-- "unsigned"
		bin: binary 16
		--assert error? try [binary/write bin [ui8 1 ui16 2 ui8 256]]
		--assert 0 = binary/read bin 'length?
	--test-- "signed"
		bin: binary 16
		--assert error? try [binary/write bin [ui8 1 si16le 40000]]
		--assert 0 = binary/read bin 'length?
	--test-- "existing data kept"
		bin: binary 16
		binary/write bin [ui8 1]
		--assert error? try [binary/write bin [ui8 2 ui32 4294967296]]
		--assert #{01} = binary/read bin 'bytes
===end-group===


===start-group=== "BinCode - struct!"
	p: make struct! [x [int16!] y [int16!]]
	--test-- "write struct"
		p/x: 1 p/y: -1
		bin: binary 16
		binary/write bin [ui8 7 :p]
		--assert #{070100FFFF} = binary/read bin 'bytes
	--test-- "read struct (top-level)"
		s: binary/read #{0200FEFF} :p
		--assert struct? s
		--assert all [s/x = 2 s/y = -2]
		--assert all [p/x = 1 p/y = -1]   ;; prototype untouched
		--assert not same? s p
	--test-- "read structs in block"
		r: binary/read #{070100FFFF0300FDFF} [ui8 :p :p]
		--assert 7 = r/1
		--assert all [r/2/x = 1 r/2/y = -1 r/3/x = 3 r/3/y = -3]
		--assert not same? r/2 r/3
	--test-- "roundtrip"
		bin: binary 16
		binary/write bin [:p ui8 9]
		r: binary/read bin [:p ui8]
		--assert all [r/1 = p r/2 = 9]
	--test-- "nested struct as prototype"
		n: make struct! [id [uint8!] pos [struct! [x [int16!] y [int16!]]]]
		s: binary/read #{0502000300} :n
		--assert all [s/id = 5 s/pos/x = 2 s/pos/y = 3]
	--test-- "errors"
		--assert error? try [binary/read #{0100} :p]                        ;; short data
		--assert error? try [binary/read #{0100} [:x]]                      ;; not a struct
		r: make struct! [n [uint8!] val [rebval!]]
		--assert all [error? e: try [binary/read #{FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF} :r] e/id = 'protected]
		--assert all [error? e: try [binary/write binary 64 [:r]] e/id = 'protected]
===end-group===


===start-group=== "bincode - read/into struct"
	p: make struct! [x [int16!] y [int16!]]
	--test-- "fill struct in place"
		bin: binary #{0100FFFF0200FEFF}
		r: binary/read/into bin :p :p
		--assert same? r p
		--assert all [p/x = 1 p/y = -1]
		binary/read/into bin :p :p
		--assert all [p/x = 2 p/y = -2]
		--assert 0 = binary/read bin 'length?
	--test-- "fill other struct with same spec"
		q: make struct! [x [int16!] y [int16!]]
		binary/read/into #{0300FDFF} :p :q
		--assert all [q/x = 3 q/y = -3]
	--test-- "fill nested struct view"
		n: make struct! [id [uint8!] pos [struct! [x [int16!] y [int16!]]]]
		v: n/pos
		binary/read/into #{04000500} :v :v
		--assert all [n/pos/x = 4 n/pos/y = 5]
	--test-- "into block (unchanged behavior)"
		b: [] binary/read/into #{0102} [ui8 ui8] :b
		--assert b = [1 2]
		b: [] binary/read/into #{0600FAFF} :p :b
		--assert all [1 = length? b struct? b/1 b/1/x = 6 b/1/y = -6]
		--assert not same? b/1 p
	--test-- "errors"
		other: make struct! [a [int32!]]
		--assert error? try [binary/read/into #{01000200} :other :p]   ;; different spec
		--assert error? try [binary/read/into #{0100} :p :p]           ;; short data
		--assert error? try [binary/read/into #{0100FFFF} [ui8] :p]    ;; code must be a struct
		r: make struct! [n [uint8!] val [rebval!]]
		--assert error? try [binary/read/into append/dup #{} #{FF} 64 :r :r]
===end-group===


===start-group=== "bincode - registered struct names"
	register point3d!: make struct! [x [f32!] y [f32!] z [f32!]]
	--test-- "read registered struct in block"
		r: binary/read #{0000803F000000400000404001} [p: point3d! ui8]
		--assert struct? p
		--assert all [p/x = 1.0 p/y = 2.0 p/z = 3.0]
		--assert same? p r/1
		--assert 1 = r/2
	--test-- "read registered struct as single word"
		s: binary/read #{000000000000803F00000000} 'point3d!
		--assert all [struct? s s/x = 0.0 s/y = 1.0 s/z = 0.0]
	--test-- "new struct on each read"
		r: binary/read #{0000803F0000000000000000000000400000000000000000} [point3d! point3d!]
		--assert not same? r/1 r/2
		--assert all [r/1/x = 1.0 r/2/x = 2.0]
	--test-- "errors"
		--assert error? try [binary/read #{0000803F} [point3d!]]          ;; short data
		--assert error? try [binary/read #{00} [not-registered-struct!]]  ;; unknown word
===end-group===


===start-group=== "bincode - vectors"
	register point3d!: make struct! [x [f32!] y [f32!] z [f32!]]
	--test-- "read numeric vector"
		--assert  #(u16! [1 2 3 4])  = first binary/read #{0100020003000400} [vector uint16! 4]
		--assert [#(u16! [1 2 3 4])] =       binary/read #{0001000200030004} [vector-be uint16! 4]
		--assert  #(f32! [1.0 2.0])  = first binary/read #{3F80000040000000} [vector-be f32! 2]
		--assert  #(f32! [1.0 2.0])  = first binary/read #{0000803F00000040} [vector-le f32! 2]
	--test-- "count from data"
		r: binary/read #{020000803F00000040FF} [n: ui8 v: vector f32! :n ui8]
		--assert all [n = 2 v = #(f32! [1.0 2.0]) r/3 = 255]
	--test-- "empty vector"
		--assert 0 = length? first binary/read #{} [vector uint8! 0]
	--test-- "read vector of structs"
		probe v: first binary/read #{0000803F000000400000404000008040000000000000000000} [vector point3d! 2]
		--assert all [vector? v 2 = length? v v/1/x = 1.0 v/1/z = 3.0 v/2/x = 4.0]
		p: make struct! [a [uint8!] b [uint16!]]
		v: first binary/read #{01020003040005} [vector :p 2]
		--assert all [2 = length? v v/1/a = 1 v/1/b = 2 v/2/a = 3]
	--test-- "write vectors"
		bin: binary 32
		binary/write bin [#(u16! [1 2]) vector-be #(u16! [1 2]) vector-le #(u16! [3])]
		--assert #{01000200000100020300} = binary/read bin 'bytes
		bin: binary 32
		binary/write bin [ub 3 5 #(u8! [255])]
		--assert #{A0FF} = binary/read bin 'bytes   ;; aligned like other raw data
	--test-- "roundtrip"
		bin: binary 64
		binary/write bin [ui32 3 vector-be #(i32! [-1 0 1])]
		--assert [3 #(i32! [-1 0 1])] = binary/read bin [ui32 vector-be i32! 3]
	--test-- "read/into vector"
		v: #(u16! [0 0])
		r: binary/read/into #{01000200} 'vector v
		--assert all [same? r v v = #(u16! [1 2])]
		binary/read/into #{00030004} 'vector-be v
		--assert v = #(u16! [3 4])
	--test-- "errors"
		--assert error? try [binary/read #{0100} [vector uint16! 2]]            ;; short data
		--assert error? try [binary/read #{}     [vector uint64! 2147483647]]   ;; huge count, no allocation
		--assert error? try [binary/read #{00}   [vector uint8! -1]]
		--assert error? try [binary/read #{00}   [vector not-a-type! 1]]
		--assert error? try [binary/read #{00}   [vector uint8!]]               ;; missing count
		--assert error? try [binary/read #{000000000000000000000000} [vector-be point3d! 1]]
		--assert error? try [binary/read/into #{01} 'vector #(u16! [0])]
		--assert error? try [binary/read/into #{0100} 'ui8 #(u16! [0])]
===end-group===


===start-group=== "swap-endian"
	--test-- "widths"
		--assert #{0100}             = swap-endian #{0001}
		--assert #{03020100}         = swap-endian/width #{00010203} 4
		--assert #{0706050403020100} = swap-endian/width #{0001020304050607} 8
	--test-- "unaligned position"
		b: #{00 01020304 05060708 09}
		swap-endian/width next b 4
		--assert b = #{00 04030201 08070605 09}
	--test-- "trailing bytes untouched"
		--assert #{010002} = swap-endian #{000102}
	--test-- "/part"
		--assert #{01000203} = swap-endian/part #{00010203} 2
	--test-- "invalid width"
		--assert error? try [swap-endian/width #{0001} 3]
		--assert error? try [swap-endian/width #{0001} 1]
===end-group===


===start-group=== "bincode - char! in 32/64-bit writes"
	--test-- "char from a reused stack slot"
		;; SET_CHAR sets only 32 bits of the value data, so the upper bytes may
		;; hold a previous value (here the integer evaluated just before)
		c: do [0#7FFFFFFF00000000 first "A"]
		bin: binary 32
		binary/write bin [ui64 :c ui64le :c si64 :c]
		--assert #{0000000000000041 4100000000000000 0000000000000041} = binary/read bin 'bytes
	--test-- "big endian"
		bin: binary 32
		binary/write bin [ui32 #"A" ui64 #"€" si32 #"B" si64 #"C"]
		--assert #{00000041 00000000000020AC 00000042 0000000000000043} = binary/read bin 'bytes
	--test-- "little endian"
		bin: binary 32
		binary/write bin [ui32le #"A" ui64le #"€" si32le #"B" si64le #"C"]
		--assert #{41000000 AC20000000000000 42000000 4300000000000000} = binary/read bin 'bytes
	--test-- "max char"
		bin: binary 8
		binary/write bin [ui32 #"^(10FFFF)"]
		--assert #{0010FFFF} = binary/read bin 'bytes
===end-group===


===start-group=== "bincode - write UTF-8 strings"
	--test-- "raw string value"
		bin: binary 16
		binary/write bin ["ž€a"]
		--assert #{C5BE E282AC 61} = binary/read bin 'bytes
	--test-- "string at non-ASCII offset"
		bin: binary 16
		binary/write bin reduce [next "ž€a"]
		--assert #{E282AC 61} = binary/read bin 'bytes
	--test-- "string command and roundtrip"
		bin: binary 16
		binary/write bin compose [string (next "ž€a") ui16bytes "€"]
		--assert #{E282AC 61 00 0003 E282AC} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [string "ž€a"]
		--assert "ž€a" = binary/read bin 'string
===end-group===


===start-group=== "bincode - read get-path arguments"
	o: object [n: 2 i: 3 p: 4 b: 4 s: make struct! [a [uint8!]]]
	--test-- "integer arguments"
		--assert [#{0102}] = binary/read #{010203} [bytes :o/n]
		--assert [3]       = binary/read #{010203} [at :o/i ui8]
		--assert [1 5]     = binary/read #{01020304 05} [ui8 pad :o/p ui8]
		--assert [5]       = binary/read #{A5} [skipbits :o/b ub :o/b]
		--assert [#(u8! [1 2])] = binary/read #{0102} [vector uint8! :o/n]
	--test-- "struct prototype"
		r: binary/read #{07} [:o/s]
		--assert all [struct? r/1 r/1/a = 7]
		v: first binary/read #{0708} [vector :o/s :o/n]
		--assert all [2 = length? v v/1/a = 7 v/2/a = 8]
	--test-- "invalid"
		--assert error? try [binary/read #{01} [bytes :o/s]]
		--assert error? try [binary/read #{01} [:o/n]]
===end-group===


===start-group=== "bincode - unixtime"
	--test-- "read"
		--assert 1-Jan-1970/0:00     = binary/read #{00000000} 'unixtime
		--assert 19-Jan-2038/3:14:07 = binary/read #{7FFFFFFF} 'unixtime
		--assert 7-Feb-2106/6:28:15  = binary/read #{FFFFFFFF} 'unixtime
		--assert 19-Jan-2038/3:14:07 = binary/read #{FFFFFF7F} 'unixtime-le
		--assert [1-Jan-2020/0:00 1] = binary/read #{5E0BE100 01} [unixtime ui8]
	--test-- "write"
		bin: binary 16
		binary/write bin [unixtime 1-Jan-2020/0:00 unixtime-le 1-Jan-2020/0:00]
		--assert #{5E0BE100 00E10B5E} = binary/read bin 'bytes
		bin: binary 16
		binary/write bin [unixtime 1-Jan-2020/1:00+1:00 unixtime 1-Jan-2020]
		--assert #{5E0BE100 5E0BE100} = binary/read bin 'bytes   ;; zone and missing time
	--test-- "roundtrip"
		bin: binary 16
		d: 7-Nov-2026/13:30:15
		binary/write bin [unixtime :d unixtime-le :d]
		--assert [7-Nov-2026/13:30:15 7-Nov-2026/13:30:15] = binary/read bin [unixtime unixtime-le]
	--test-- "unixtime-now reads back"
		bin: binary 8
		binary/write bin [unixtime-now]
		--assert 5 > abs to integer! difference now/utc binary/read bin 'unixtime
	--test-- "errors"
		--assert error? try [binary/read #{000000} 'unixtime]
		--assert error? try [binary/write binary 8 [unixtime 31-Dec-1969/23:59:59]]
		--assert error? try [binary/write binary 8 [unixtime 7-Feb-2106/6:28:16]]
		--assert error? try [binary/write binary 8 [unixtime 0]]
		bin: binary 8
		try [binary/write bin [ui8 1 unixtime 1-Jan-1900]]
		--assert 0 = binary/read bin 'length?
===end-group===


===start-group=== "bincode - bitset writes"
	--test-- "write"
		bin: binary 16
		binary/write bin [bitset8 #(bitset! #{A5}) bitset16 #(bitset! #{A5}) bitset32 #(bitset! #{01020304})]
		--assert #{A5 A500 01020304} = binary/read bin 'bytes
	--test-- "trailing zero bytes allowed"
		bin: binary 8
		binary/write bin [bitset8 #(bitset! #{A50000})]
		--assert #{A5} = binary/read bin 'bytes
	--test-- "padding overwrites existing data"
		bin: binary 8
		binary/write bin [ui32 -1 at 1 bitset32 #(bitset! #{01})]
		--assert #{01000000} = binary/read bin 'bytes
	--test-- "roundtrip"
		bin: binary 8
		cs: charset [0 7 8 15]
		binary/write bin [bitset16 :cs]
		--assert #{8181} = binary/read bin 'bytes
		bin: binary 8
		binary/write bin [bitset16 :cs]
		--assert cs = binary/read bin 'bitset16
	--test-- "errors"
		--assert error? try [binary/write binary 8 [bitset8 #(bitset! #{A501})]]   ;; bit above width
		cs: complement charset "a"
		--assert error? try [binary/write binary 8 [bitset32 :cs]]                 ;; negated
		--assert error? try [binary/write binary 8 [bitset8 1]]
		bin: binary 8
		try [binary/write bin [ui8 1 bitset8 #(bitset! #{0001})]]
		--assert 0 = binary/read bin 'length?
===end-group===

~~~end-file~~~

