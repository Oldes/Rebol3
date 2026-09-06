Rebol [
	Title:   "Rebol struct test script"
	Author:  "Oldes"
	File: 	 %struct-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "STRUCT"
;; Struct datatype was reimplemented and so this test is only
;; for the recent version!
if system/version >= 3.19.1 [
===start-group=== "Struct construction"
--test-- "Struct single value construction"
	--assert not error? try [
		i8:  make struct! [a [int8!]]
		i16: make struct! [a [int16!]]
		i32: make struct! [a [int32!]]
		i64: make struct! [a [int64!]]
		u8:  make struct! [a [uint8!]]
		u16: make struct! [a [uint16!]]
		u32: make struct! [a [uint32!]]
		u64: make struct! [a [uint64!]]
		f32: make struct! [a [float!]]
		f64: make struct! [a [double!]]
		w:   make struct! [a [word!]]
	]
	--assert 1 = length? i8
	--assert 2 = length? i16
	--assert 4 = length? i32
	--assert 8 = length? i64
	--assert 1 = length? u8
	--assert 2 = length? u16
	--assert 4 = length? u32
	--assert 8 = length? u64
	--assert 4 = length? f32
	--assert 8 = length? f64
	--assert 4 = length? w
	--assert (mold/all/flat i8 ) = "#(struct! [a [int8!]] [a: 0])"
	--assert (mold/all/flat i16) = "#(struct! [a [int16!]] [a: 0])"
	--assert (mold/all/flat i32) = "#(struct! [a [int32!]] [a: 0])"
	--assert (mold/all/flat i64) = "#(struct! [a [int64!]] [a: 0])"
	--assert (mold/all/flat u8 ) = "#(struct! [a [uint8!]] [a: 0])"
	--assert (mold/all/flat u16) = "#(struct! [a [uint16!]] [a: 0])"
	--assert (mold/all/flat u32) = "#(struct! [a [uint32!]] [a: 0])"
	--assert (mold/all/flat u64) = "#(struct! [a [uint64!]] [a: 0])"
	--assert (mold/all/flat f32) = "#(struct! [a [float32!]] [a: 0.0])"
	--assert (mold/all/flat f64) = "#(struct! [a [float64!]] [a: 0.0])"
	--assert (mold/all/flat w)   = "#(struct! [a [word!]] [a: _])"

--test-- "Struct single value (dimensional) construction"
	--assert not error? try [
		i8x2:  make struct! [a [int8!   [2]]]
		i16x2: make struct! [a [int16!  [2]]]
		i32x2: make struct! [a [int32!  [2]]]
		i64x2: make struct! [a [int64!  [2]]]
		u8x2:  make struct! [a [uint8!  [2]]]
		u16x2: make struct! [a [uint16! [2]]]
		u32x2: make struct! [a [uint32! [2]]]
		u64x2: make struct! [a [uint64! [2]]]
		f32x2: make struct! [a [float!  [2]]]
		f64x2: make struct! [a [double! [2]]]
		wx2:   make struct! [a [word!   [2]]]
	]
	--assert [a [int8!   [2]]] = spec-of i8x2
	--assert [a [int16!  [2]]] = spec-of i16x2
	--assert [a [int32!  [2]]] = spec-of i32x2
	--assert [a [int64!  [2]]] = spec-of i64x2
	--assert [a [uint8!  [2]]] = spec-of u8x2
	--assert [a [uint16! [2]]] = spec-of u16x2
	--assert [a [uint32! [2]]] = spec-of u32x2
	--assert [a [uint64! [2]]] = spec-of u64x2
	--assert [a [float32! [2]]] = spec-of f32x2
	--assert [a [float64! [2]]] = spec-of f64x2
	--assert [a [word!   [2]]] = spec-of wx2

	--assert 2  = length? i8x2
	--assert 4  = length? i16x2
	--assert 8  = length? i32x2
	--assert 16 = length? i64x2
	--assert 2  = length? u8x2
	--assert 4  = length? u16x2
	--assert 8  = length? u32x2
	--assert 16 = length? u64x2
	--assert 8  = length? f32x2
	--assert 16 = length? f64x2
	--assert 8  = length? wx2
	--assert (mold/all/flat i8x2 ) = "#(struct! [a [int8! [2]]] [a: [0 0]])"
	--assert (mold/all/flat i16x2) = "#(struct! [a [int16! [2]]] [a: [0 0]])"
	--assert (mold/all/flat i32x2) = "#(struct! [a [int32! [2]]] [a: [0 0]])"
	--assert (mold/all/flat i64x2) = "#(struct! [a [int64! [2]]] [a: [0 0]])"
	--assert (mold/all/flat u8x2 ) = "#(struct! [a [uint8! [2]]] [a: [0 0]])"
	--assert (mold/all/flat u16x2) = "#(struct! [a [uint16! [2]]] [a: [0 0]])"
	--assert (mold/all/flat u32x2) = "#(struct! [a [uint32! [2]]] [a: [0 0]])"
	--assert (mold/all/flat u64x2) = "#(struct! [a [uint64! [2]]] [a: [0 0]])"
	--assert (mold/all/flat f32x2) = "#(struct! [a [float32! [2]]] [a: [0.0 0.0]])"
	--assert (mold/all/flat f64x2) = "#(struct! [a [float64! [2]]] [a: [0.0 0.0]])"
	--assert (mold/all/flat wx2)   = "#(struct! [a [word! [2]]] [a: [_ _]])"

--test-- "Resolving multi-dimensional value"
	;; vectors for numeric values
	--assert i8x2/a  = #(i8!  [0 0])
	--assert i16x2/a = #(i16! [0 0])
	--assert i32x2/a = #(i32! [0 0])
	--assert i64x2/a = #(i64! [0 0])
	--assert u8x2/a  = #(u8!  [0 0])
	--assert u16x2/a = #(u16! [0 0])
	--assert u32x2/a = #(u32! [0 0])
	--assert u64x2/a = #(u64! [0 0])
	--assert f32x2/a = #(f32! [0.0 0.0])
	--assert f64x2/a = #(f64! [0.0 0.0])
	;; block for other types
	--assert wx2/a   = [#(none) #(none)]

--test-- "Setting multi-dimensional value using vectors"
	--assert all [attempt [i8x2/a:  #(i8!  [1 2])]      i8x2/a ==  #(i8!  [1 2])]
	--assert all [attempt [i16x2/a: #(i16! [1 2])]      i16x2/a == #(i16! [1 2])]
	--assert all [attempt [i32x2/a: #(i32! [1 2])]      i32x2/a == #(i32! [1 2])]
	--assert all [attempt [i64x2/a: #(i64! [1 2])]      i64x2/a == #(i64! [1 2])]
	--assert all [attempt [u8x2/a:  #(u8!  [1 2])]      u8x2/a ==  #(u8!  [1 2])]
	--assert all [attempt [u16x2/a: #(u16! [1 2])]      u16x2/a == #(u16! [1 2])]
	--assert all [attempt [u32x2/a: #(u32! [1 2])]      u32x2/a == #(u32! [1 2])]
	--assert all [attempt [u64x2/a: #(u64! [1 2])]      u64x2/a == #(u64! [1 2])]
	--assert all [attempt [f32x2/a: #(f32! [1.0 2.0])]  f32x2/a == #(f32! [1.0 2.0])]
	--assert all [attempt [f64x2/a: #(f64! [1.0 2.0])]  f64x2/a == #(f64! [1.0 2.0])]

--test-- "Struct construction with initial value (using named fields)"
	--assert all [struct? i8:  #(struct! [a [int8!]   b [int8!]] [a:  23 ])  i8/a  = 23  i8/b  = 0 ]
	--assert all [struct? i16: #(struct! [a [int16!]  b [int8!]] [a:  23 ])  i16/a = 23  i16/b = 0 ]
	--assert all [struct? i32: #(struct! [a [int32!]  b [int8!]] [a:  23 ])  i32/a = 23  i32/b = 0 ]
	--assert all [struct? i64: #(struct! [a [int64!]  b [int8!]] [a:  23 ])  i64/a = 23  i64/b = 0 ]
	--assert all [struct? u8:  #(struct! [a [uint8!]  b [int8!]] [a:  23 ])  u8/a  = 23  u8/b  = 0 ]
	--assert all [struct? u16: #(struct! [a [uint16!] b [int8!]] [a:  23 ])  u16/a = 23  u16/b = 0 ]
	--assert all [struct? u32: #(struct! [a [uint32!] b [int8!]] [a:  23 ])  u32/a = 23  u32/b = 0 ]
	--assert all [struct? u64: #(struct! [a [uint64!] b [int8!]] [a:  23 ])  u64/a = 23  u64/b = 0 ]
	--assert all [struct? f32: #(struct! [a [float!]  b [int8!]] [a:  23 ])  f32/a = 23  f32/b = 0 ]
	--assert all [struct? f64: #(struct! [a [double!] b [int8!]] [a:  23 ])  f64/a = 23  f64/b = 0 ]
	--assert all [struct? w:   #(struct! [a [word!]   b [int8!]] [a: foo ])  w/a = 'foo  w/b   = 0 ]

	--assert (mold/all/flat i8 ) = "#(struct! [a [int8!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i16) = "#(struct! [a [int16!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i32) = "#(struct! [a [int32!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i64) = "#(struct! [a [int64!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u8 ) = "#(struct! [a [uint8!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u16) = "#(struct! [a [uint16!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u32) = "#(struct! [a [uint32!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u64) = "#(struct! [a [uint64!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat f32) = "#(struct! [a [float32!] b [int8!]] [a: 23.0 b: 0])"
	--assert (mold/all/flat f64) = "#(struct! [a [float64!] b [int8!]] [a: 23.0 b: 0])"
	--assert (mold/all/flat w)   = "#(struct! [a [word!] b [int8!]] [a: foo b: 0])"

--test-- "Struct construction with initial value (using values only)"
	--assert all [struct? i8:  #(struct! [a [int8!]   b [int8!]] [ 23 ])  i8/a  = 23  i8/b  = 0 ]
	--assert all [struct? i16: #(struct! [a [int16!]  b [int8!]] [ 23 ])  i16/a = 23  i16/b = 0 ]
	--assert all [struct? i32: #(struct! [a [int32!]  b [int8!]] [ 23 ])  i32/a = 23  i32/b = 0 ]
	--assert all [struct? i64: #(struct! [a [int64!]  b [int8!]] [ 23 ])  i64/a = 23  i64/b = 0 ]
	--assert all [struct? u8:  #(struct! [a [uint8!]  b [int8!]] [ 23 ])  u8/a  = 23  u8/b  = 0 ]
	--assert all [struct? u16: #(struct! [a [uint16!] b [int8!]] [ 23 ])  u16/a = 23  u16/b = 0 ]
	--assert all [struct? u32: #(struct! [a [uint32!] b [int8!]] [ 23 ])  u32/a = 23  u32/b = 0 ]
	--assert all [struct? u64: #(struct! [a [uint64!] b [int8!]] [ 23 ])  u64/a = 23  u64/b = 0 ]
	--assert all [struct? f32: #(struct! [a [float!]  b [int8!]] [ 23 ])  f32/a = 23  f32/b = 0 ]
	--assert all [struct? f64: #(struct! [a [double!] b [int8!]] [ 23 ])  f64/a = 23  f64/b = 0 ]
	--assert all [struct? w:   #(struct! [a [word!]   b [int8!]] [foo ])  w/a = 'foo  w/b   = 0 ]

	--assert (mold/all/flat i8 ) = "#(struct! [a [int8!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i16) = "#(struct! [a [int16!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i32) = "#(struct! [a [int32!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat i64) = "#(struct! [a [int64!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u8 ) = "#(struct! [a [uint8!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u16) = "#(struct! [a [uint16!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u32) = "#(struct! [a [uint32!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat u64) = "#(struct! [a [uint64!] b [int8!]] [a: 23 b: 0])"
	--assert (mold/all/flat f32) = "#(struct! [a [float32!] b [int8!]] [a: 23.0 b: 0])"
	--assert (mold/all/flat f64) = "#(struct! [a [float64!] b [int8!]] [a: 23.0 b: 0])"
	--assert (mold/all/flat w)   = "#(struct! [a [word!] b [int8!]] [a: foo b: 0])"

--test-- "Construction from struct prototype (using named fields)"
	proto!: #(struct! [a [uint8!] b [uint8!]] [a: 1 b: 2])
	--assert all [proto!/a = 1 proto!/b = 2]
	s1: make proto! [a: 10]
	s2: make proto! [b: 20]
	s3: make proto! [b: 20 a: 10]
	--assert all [s1/a = 10 s1/b = 2 ]
	--assert all [s2/a = 1  s2/b = 20]
	--assert all [s3/a = 10 s3/b = 20]
	;; the block is evaluated like reduce/no-set
	--assert all [attempt [s: make proto! [3 * 10 4 * 10]]        s/a = 30 s/b = 40]
	--assert all [attempt [s: make proto! [b: 3 * 10 a: 4 * 10]]  s/b = 30 s/a = 40]

--test-- "Construction from struct prototype (using values only)"
	proto!: #(struct! [a [uint8!] b [uint8!]] [1 2])
	--assert all [proto!/a = 1 proto!/b = 2]
	s1: make proto! [10]
	--assert all [s1/a = 10 s1/b = 2 ]

--test-- "Struct with many fields"
	blk: copy []
	repeat i 32 [repend blk [to word! join 'a i [int8!]]]
	--assert all [
		not error? try [s: make struct! blk]
		[a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12 a13 a14 a15 a16 a17 a18 a19 a20 a21 a22 a23 a24 a25 a26 a27 a28 a29 a30 a31 a32] = words-of s
		[0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0] = values-of s
	]

--test-- "Struct with Rebol values"
	--assert not error? try [
		s: make struct! [a [rebval!] b [rebval!]]
		s/a: str: "Hello" s/b: now
	]
	--assert s/a == str
	--assert date? s/b
	--assert all [
		;; It is not allowed to modify struct's binary when there are Rebol values
		error? e: try [change s #{FF}]
		e/id = 'protected
	]
	--assert all [
		;; It is possible to modify series in struct's Rebol values
		not error? try [clear s/a]
		s/a == ""
		str == ""
	]
	--assert all [
		;; It is allowed to clear the struct with Rebol values
		not error? try [clear s]
		none? s/a
		none? s/b
	]

--test-- "Registering a struct"
	--assert not error? try [
		register pair8!: make struct! [x [uint8!] y [uint8!]]
	]

--test-- "Modifying field values using change"
	s: make pair8! [1 2]
	--assert all [attempt [change s [3 4]]       #{0304} == to binary! s]
	--assert all [attempt [change s [y: 3 x: 4]] #{0403} == to binary! s]
	--assert all [attempt [change s [5]]         #{0503} == to binary! s]
	--assert all [attempt [change s [y: 6]]      #{0506} == to binary! s]
	--assert all [attempt [change s #{07}]       #{0706} == to binary! s]
	--assert all [attempt [change s #{0101}]     #{0101} == to binary! s]
	--assert all [attempt [change s #{020202}]   #{0202} == to binary! s]

--test-- "Copy structs"
	s: make pair8! [1 2]
	--assert all [attempt [s2: copy s]  #{0102} == to binary! s2]
	s/x: 3 ;; modified original struct
	--assert s2/x == 1 ;; the new struct is unchanged
	--assert all [error? e: try [copy/part s 1]  e/id = 'bad-refines]
	--assert all [error? e: try [copy/deep s 1]  e/id = 'bad-refines]

--test-- "Nested structs"

	--assert all [
		not error? try [
			s1: make struct! [
				id  [uint16!]
				pos [struct! pair8!] ;; 8bit pair
			]
		]
		#{0000 00 00} == to binary! s1
		not error? try [change s1 #{0100 02 03}]
		s1/id == 1
		s1/pos/x == 2
		s1/pos/y == 3
		s1/pos/x: 22
		s1/pos/y: 33
		s1/pos/x == 22
		s1/pos/y == 33
		{#(struct! [id [uint16!] pos [struct! pair8!]] [id: 1 pos: #(struct! [x [uint8!] y [uint8!]] [x: 22 y: 33])])} = mold/flat/all s1
	]

	--assert all [
		not error? try [
			s2: make struct! [
				id  [uint16!]
				pos [struct! pair8! [2]] ;; to 8bit pairs
			]
		]
		s2/id: 2
		not error? try [s2/pos/1: s1/pos]
		not error? try [change s2/pos/2 #{0102}]
		#{0200 1621 0102} == to binary! s2
		s2/pos/2: s2/pos/1
		s2/pos/1 = s2/pos/2
		s2/pos/1/x: 222
		s2/pos/1/x = 222
	]

	--assert all [
		not error? try [s3: make s1 [3 #(struct! [x [uint8!] y [uint8!]] [3 4])]]
		#{0300 0304} == to binary! s3
		s3/pos/x == 3
		not error? try [change s3 #{0400 0506}]
		s3/id == 4
		s3/pos/x == 5
		s3/pos/y == 6
		not error? try [change s3 #{0500}]
		#{0500 0506} == to binary! s3
	]

--test-- "Nested structs (deep)"
	s: make struct! [a [uint32!] b [struct! [x [uint32!] y [struct! [yy [uint32!]]]]]]
	s/b/x: 1
	s/b/y/yy: 2
	--assert (to binary! s/b/y) == #{02000000}
	--assert (to binary! s/b  ) == #{0100000002000000}
	--assert (to binary! s    ) == #{000000000100000002000000}

--test-- "Nested structs with Rebol values"
	--assert all [
		attempt [s: make struct! [val [rebval!] pos [struct! pair8!]]]
		s/val: "Hello"
		s/val == "Hello"
		;; it is possible to change inner struct using raw binary data
		attempt [change s/pos #{0102}]
		s/pos/x == 1
		s/pos/y == 2
		;; but not the main struct with Rebol value
		error? e: try [change s #{0102}]
		e/id = 'protected
	]
	--assert all [
		attempt [
			s: make struct! [
				id    [uint8!]
				inner [struct! [val [rebval!]]]
			]
		]
		error? e: try [change s/inner #{0102}]
		e/id = 'protected
		error? e: try [change s #{0102}]
		e/id = 'protected
	]

--test-- "Nested structs with arrays"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2692
	--assert attempt [all [
		s: make struct! [a [struct! [n [int8!]][2]]]
		1 == s/a/1/n: 1
		2 == s/a/2/n: 2
		#{0102} == to binary! s
		struct? a1: s/a/1
		#{01}   == to binary! a1
		struct? s/a/1: s/a/2
		#{0202} == to binary! s
		#{02}   == to binary! a1
		3 == s/a/1/n: 3
		#{03}   == to binary! a1
		#{0302} == to binary! s
	]]
	--assert attempt [all [
		s: make struct! [n [int8!] val [rebval!]]
		[1 2] == s/val: [1 2]
		1 == s/val/1
		2 == s/val/2
		s/val/1: 10
		s/val/(1 + 1): 20
		[10 20] == s/val
		append s/val "abc"
		[10 20 "abc"] == s/val
		#"a" == s/val/3/1
		#"X" == s/val/3/1: #"X"
		;s/val/3 = "Xbc"
	]]
	--assert attempt [all [
		s1: make struct! [n [uint8!] s [struct! [a1 [struct! [x [int8!]]]]]]
		s2: make struct! [n [ int8!] s [struct! [a2 [struct! [x [int8!]]]]]]
		s1/s/a1/x: 1
		s2/s/a2/x: 2
		#{0001} == to binary! s1
		#{0002} == to binary! s2
		s1/s/a1: s2/s/a2
		#{0002} == to binary! s1
	]]
	--assert attempt [all [
		s: make struct! [n [int8!] val [rebval!]]
		block? s/val: [1 "abc"]
		char?  s/val/2/1: #"X"
		s/val == [1 "Xbc"]
	]]
	--assert attempt [all [
		s: make struct! [n [int8!] val [rebval!]]
		block? s/val: [1 "abc"]
		char?  s/val/2/(1): #"X"
		s/val == [1 "Xbc"]
	]]
	--assert attempt [all [
		s: make struct! [n [int8!] val [rebval!]]
		object? s/val: object [a: "123"]
		char? s/val/a/1: #"X"
		s/val/a == "X23"
	]]

--test-- "Setting an element of a struct's array"
	s: make struct! [a [struct! [x [uint32!] y [uint32!]] [2]]]
	src: make struct! [x [uint32!] y [uint32!]]
	src/x: 1 src/y: 2
	--assert all [
		not error? try [s/a/2: src]
		#{0000000000000000 0100000002000000} == to binary! s
	]
	;; the index must be inside the array's range
	--assert error? try [s/a/0: src]
	--assert error? try [s/a/3: src]
	--assert error? try [s/a/(-1): src]
	;; ... and nothing may be modified in such a case
	--assert #{0000000000000000 0100000002000000} == to binary! s
	;; the assigned struct must have the same size...
	--assert error? try [s/a/1: make struct! [n [int8!]]]
	;; ... and the same field types!
	--assert error? try [s/a/1: make struct! [x [int32!] y [int32!]]]
	;; a value which is not a struct is not accepted
	--assert error? try [s/a/1: 5]
	--assert error? try [s/a/1: "hello"]
	;; ... but a block with values is
	--assert all [
		not error? try [s/a/1: [3 4]]
		#{0300000004000000 0100000002000000} == to binary! s
	]

--test-- "Setting an element of a struct's array with Rebol values"
	;; raw data must never be stored into a `rebval!` field,
	;; because the GC would try to mark it as a Rebol value!
	n: length? make struct! [v [rebval!]] ;; size of the internal Rebol value
	s: make struct! [a [struct! [v [rebval!]] [2]]]
	raw: make struct! compose/deep [b [uint8! [(n)]]]
	change raw append/dup make binary! n #{FF} n
	--assert n = length? raw
	--assert error? try [s/a/1: raw]
	--assert none? s/a/1/v
	--assert not error? try [recycle]

--test-- "Setting inner struct"
	s: make struct! [
		id  [uint16!]
		pos [struct! pair8!] 
	]
	--assert all [
		attempt [s/pos: [1 2]]
		s/pos/x == 1
		s/pos/y == 2
	]
	--assert all [
		attempt [s/pos: [y: 1 x: 2]]
		s/pos/x == 2
		s/pos/y == 1
	]
	--assert all [
		attempt [s/pos: make pair8! [3 4]]
		s/pos/x == 3
		s/pos/y == 4
	]

--test-- "Compare structs"
	s1: make struct! [a [u8!] b [u8!]]
	s2: make struct! [a [uint8!] b [uint8!]]
	--assert s1 = s2        ;; compares only field types
	--assert not (s1 == s2) ;; compares alse field names
	s1/a: 1
	

===end-group===


===start-group=== "Struct reflection"
;@@ https://github.com/Oldes/Rebol-issues/issues/2577
s: #(struct! [
	a [uint16!]
	b [int32!]
	c [word!]
	d [uint8! [2]]
] [a: 1 b: -1 c: foo])
--test-- "spec-of struct"
	--assert [a [uint16!] b [int32!] c [word!] d [uint8! [2]]] == spec-of s
--test-- "body-of struct"
	--assert [a: 1 b: -1 c: foo d: [0 0]] == body-of s
--test-- "words-of struct"
	--assert [a b c d] == words-of s
	--assert [a b c d] == keys-of s
--test-- "values-of struct"
	--assert [1 -1 foo [0 0]] == values-of s
===end-group===


===start-group=== "Struct conversion"
--test-- "to binary! struct!"
	s: #(struct! [a [uint16!] b [int32!]] [1 -1])
	--assert #{0100FFFFFFFF} = to binary! s
===end-group===


===start-group=== "Struct construction using binary data"
--test-- "Struct constructed from binary data"
	s: transcode/one {#(struct! [a [uint8!] b [uint16!]] #{010200})}
	--assert struct? s
	--assert all [s/a = 1 s/b = 2]
	--assert #{010200} == to binary! s
	;; data longer than the struct are truncated
	s: transcode/one {#(struct! [a [uint8!] b [uint16!]] #{010200FFFF})}
	--assert #{010200} == to binary! s

--test-- "Struct's binary data must not be shorter than the struct"
	--assert error? try [transcode/one/error {#(struct! [a [uint8!] b [uint16!]] #{0102})}]

--test-- "Struct with Rebol values cannot be initialized using binary data"
	;; the GC would try to mark random bytes as Rebol values!
	bin: append/dup make binary! 64 #{FF} 64
	s: make struct! [n [uint8!] val [rebval!]]
	--assert all [
		error? e: try [make s bin]
		e/id = 'protected
	]
	--assert none? s/val
	;; the same must be true for a struct with a nested Rebol value
	s: make struct! [n [uint8!] inner [struct! [val [rebval!]]]]
	--assert all [
		error? e: try [make s bin]
		e/id = 'protected
	]
	--assert none? s/inner/val
	;; and also when using the construction syntax
	--assert error? try [transcode/one/error {#(struct! [val [rebval!]] #{FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF})}]
	--assert not error? try [recycle]

===end-group===


===start-group=== "Struct GC"
;; Rebol values stored in a struct are reachable only from the struct's data
;; series, so the GC must find them there. `flush` and `reuse` below make an
;; unmarked value really disappear - the value is first pushed out of the GC's
;; infant nursery and once collected, its memory is taken by other series.
flush: does [loop 100 [make binary! 64]]
reuse: does [loop 100 [append copy "" "0123456789abcdef"]]

--test-- "GC marks a rebval! field which is not at the struct's head"
	s: make struct! [n [int8!] val [rebval!]]
	s/n: 42
	--assert string? s/val: copy "keep me alive"
	flush
	--assert not error? try [recycle]
	reuse
	--assert s/n = 42
	--assert s/val == "keep me alive"

--test-- "GC marks all values of a rebval! array field"
	s: make struct! [n [int16!] vals [rebval! [2]]]
	--assert block? s/vals: reduce [copy "first" copy "second"]
	flush
	--assert not error? try [recycle]
	reuse
	--assert s/vals == ["first" "second"]

--test-- "GC marks rebval! fields of a nested struct"
	s: make struct! [
		id    [uint8!]
		inner [struct! [val [rebval!]]]
	]
	s/id: 7
	--assert string? s/inner/val: copy "nested value"
	flush
	--assert not error? try [recycle]
	reuse
	--assert s/id = 7
	--assert s/inner/val == "nested value"

--test-- "GC marks rebval! fields of a deeply nested struct"
	s: make struct! [
		a [struct! [b [struct! [val [rebval!]]]] [2]]
	]
	--assert string? s/a/1/b/val: copy "first"
	--assert string? s/a/2/b/val: copy "second"
	flush
	--assert not error? try [recycle]
	reuse
	--assert s/a/1/b/val == "first"
	--assert s/a/2/b/val == "second"

===end-group===
] ;>= 3.19.1

===start-group=== "Invalid struct construction"
either system/version < 3.19.1 [
	;; this syntax is no longer supported
	--test-- "Missing struct init value"
	;@@ https://github.com/zsx/r3/issues/50
		--assert all [
			error? e: try [make struct! [ c: [struct! [a [uint8!]]] ]]
			e/id = 'expect-val
		]
	--test-- "Don't allow evaluation inside struct construction"
	;@@ https://github.com/zsx/r3/issues/51
		--assert all [
			error? e: try [make struct! [ a: [uint8!] probe random 100 ]]
			e/id = 'invalid-type
		]
	--test-- "Invalid array type initialisation"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2567
		--assert all [
			error? e: try [ make struct! [a: [int8! [2]] 1] ] ;- No crash!
			e/id = 'expect-val
		]
][
	--test-- "Empty struct not allowed"
		--assert all [
			error? e: try [make struct! []]
			e/id = 'malconstruct
		]
		--assert all [
			error? e: try [make struct! [[]]]
			e/id = 'malconstruct
		]
	--test-- "No field specification"
		--assert all [
			error? e: try [make struct! [a]]
			e/id = 'malconstruct
		]
		--assert all [
			error? e: try [make struct! [[] a]]
			e/id = 'malconstruct
		]
		--assert all [
			error? e: try [make struct! ["test" []]]
			e/id = 'malconstruct
		]
		--assert all [
			error? e: try [make struct! ["test" "test"]]
			e/id = 'malconstruct
		]
	--test-- "Invalid field specification"
		--assert all [
			error? e: try [make struct! [a [23]]]
			e/id = 'invalid-arg
		]
		--assert all [
			error? e: try [make struct! [a [int8! foo]]]
			e/id = 'invalid-arg
		]
		--assert all [
			error? e: try [make struct! [a [int8! 23]]]
			e/id = 'invalid-arg
		]
		--assert all [
			error? e: try [make struct! [a [int8! [foo]]]]
			e/id = 'invalid-arg
		]
	--test-- "Construction does not support evaluation"
		--assert all [
			error? e: transcode/one/error {#(struct [a [uint8!]] [random 10])}
			e/id = 'malconstruct
		]
]
===end-group===



~~~end-file~~~