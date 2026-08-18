Rebol [
	Title:   "Rebol vector test script"
	Author:  "Oldes"
	File: 	 %vector-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "VECTOR"

===start-group=== "VECTOR"

--test-- "issue/2346"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2346
	--assert [] = to-block make vector! 0

--test-- "issue/1036"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1036
	--assert 2 = index? load mold/all next make vector! [integer! 32 4 [1 2 3 4]]

--test-- "issue/1026"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1026
	--assert #(int32! []) == make vector! 0
	--assert #(int32! []) == make vector! []
	
--test-- "VECTOR can be initialized using a block with CHARs"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2348
	--assert vector? v: make vector! [integer! 8 [#"^(00)" #"^(01)" #"^(02)" #"a" #"b"]]
	--assert  0 = v/1
	--assert 98 = v/5

	--assert vector? v: make vector! [integer! 16 [#"^(00)" #"^(01)" #"^(02)" #"a" #"b"]]
	--assert  0 = v/1
	--assert 98 = v/5

--test-- "Make vector with get-words"
	data: [1 2 3 4]
	size: 2
	--assert {#(uint8! [1 2 3 4])}   == mold make vector! [uint8! :data]
	--assert {#(uint8! [1 2])}       == mold make vector! [uint8! :size :data] ;; truncates
	index: 3
	--assert {#(uint8! [3 4])}       == mold make vector! [uint8! :data :index]
	size: 4
	--assert {#(uint8! [3 4])}       == mold make vector! [uint8! :size [1 2 3 4 5] :index]
	--assert {#(uint8! [1 2 3 4] 3)} == mold/all make vector! [uint8! :size [1 2 3 4 5] :index]

	;; Using large data
	data: append/dup copy [] 1 10000
	--assert {#(uint8! [1 1])}   == mold make vector! [uint8! 2 :data]
	--assert {#(uint8! 2x2 [1 1 1 1])}   == mold/flat make vector! [uint8! 2x2 :data]

--test-- "Make vector using direct values"
	--assert (make vector! [1 2 3 4]) == #(int64! [1 2 3 4])
	--assert (make vector! [1.0 2]) == #(float64! [1.0 2.0])

--test-- "Make empty vector"
	--assert #(uint8! []) == transcode/one "#(uint8!)"
	--assert #(uint32! []) == transcode/one "#(uint32!)"
	--assert #(float32! []) == transcode/one "#(float32!)"

--test-- "Make vector with negative size is a range error"
	--assert all [error? e: try [make vector! [u8! -5]]   e/id = 'out-of-range]
	--assert all [error? e: try [make vector! -5]         e/id = 'out-of-range]
	--assert all [error? e: try [make vector! [u8! -2x3]] e/id = 'out-of-range]
	--assert all [error? e: try [make vector! [u8! 2x-3]] e/id = 'out-of-range]

--test-- "Make vector from binary"
	--assert #(uint8! []) == attempt [to vector! #{}]
	--assert #(uint8! [1 255]) == attempt [to vector! #{01FF}]

--test-- "Random shuffle of vector vs. block"
	;@@ https://github.com/Oldes/Rebol-issues/issues/910
	;@@ https://github.com/Oldes/Rebol-issues/issues/947
	v1: make vector! [integer! 32 5 [1 2 3 4 5]]
	v2: random v1
	--assert same? v1 v2
	b1: [1 2 3 4 5]
	b2: random b1
	--assert same? b1 b2

--test-- "Some vector! formats are invalid"
	;@@ https://github.com/Oldes/Rebol-issues/issues/350
	--assert error? try [make vector! [- decimal! 32]]
	--assert error? try [make vector! [- integer! 32]]

--test-- "FIRST, LAST on vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/459
	v: make vector! [integer! 8 [1 2 3]]
	--assert 1 = first v
	--assert 3 = last v
	--assert 1 = v/1
	--assert 3 = v/3

--test-- "HEAD, TAIL on vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/462
	v: #(u8! [1 2 3])
	--assert tail? tail v
	--assert head? head v

--test-- "to-block vector!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/865
	--assert [0 0] = to-block make vector! [integer! 32 2]
	--assert [1 2] = to block! #(u16! [1 2])

--test-- "to-binary vector!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2590
	--assert #{01000200} = to binary! #(u16! [1 2])
	--assert #{0100000002000000} = to binary! #(i32! [1 2])
	--assert #{0000803F00000040} = to binary! #(f32! [1 2])
	--assert #{01000000000000000200000000000000} = to binary! #(i64! [1 2])
	--assert #{000000000000F03F0000000000000040} = to binary! #(f64! [1 2])
	;@@ https://github.com/Oldes/Rebol-issues/issues/2518
	--assert #{0200} = to binary! next #(u16! [1 2])
	--assert #{02000000} = to binary! next #(i32! [1 2])
	--assert #{00000040} = to binary! next #(f32! [1 2])
	--assert #{0200000000000000} = to binary! next #(i64! [1 2])
	--assert #{0000000000000040} = to binary! next #(f64! [1 2])
	;@@ https://github.com/Oldes/Rebol-issues/issues/2458
	--assert #{01000200} = to binary! protect #(u16! [1 2])

--test-- "LOAD/MOLD on vector"
	--assert v = load mold/all v
	--assert v = do load mold v
	;@@ https://github.com/Oldes/Rebol-issues/issues/1036
	--assert 2 = index? load mold/all next make vector! [integer! 32 4 [1 2 3 4]]

--test-- "Conversion from VECTOR to BINARY"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2347
	--assert #{0102} = to binary! make vector! [integer! 8 [1 2]]
	--assert #{01000200} = to binary! make vector! [integer! 16 [1 2]]
	--assert #{0100000002000000} = to binary! make vector! [integer! 32 [1 2]]
	--assert 1 = to integer! head reverse to binary! make vector! [integer! 64 [1]]
	--assert #{0000803F} = to binary! make vector! [decimal! 32 [1.0]]
	--assert 1.0 = to decimal! head reverse to binary! make vector! [decimal! 64 [1.0]]

--test-- "VECTOR can be initialized using binary data"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1410
	--assert vector? v: make vector! [integer! 16 #{010002000300}]
	--assert 1 = v/1
	--assert 3 = v/3

	b: to binary! make vector! [decimal! 32 [1.0 -1.0]]
	v: make vector! compose [decimal! 32 (b)]
	--assert v/1 = 1.0
	--assert v/2 = -1.0
	--assert b = to binary! v

--test-- "Croping input specification when size and series is provided"
	--assert 2 = length? v: make vector! [integer! 16 2 [1 2 3 4]]
	--assert 2 = v/2
	--assert none? v/3
	--assert 1 = length? v: make vector! [integer! 16 1 #{01000200}]
	--assert none? v/2
	;- It's not supported to specify size with the construction syntax anymore
	;--assert 1 = length? v: #(i16! 1 #{01000200})
	;--assert none? v/2

--test-- "Extending input specification when size and series is provided"
	--assert 4 = length? v: make vector! [integer! 16 4 [1 2]]
	--assert 2 = v/2
	--assert 0 = v/4
	--assert none? v/5

--test-- "Vector created with specified index"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1038
	--assert 2 = index? v: make vector! [integer! 16 [1 2] 2]
	--assert 2 = index? v: make vector! [integer! 16 #{01000200} 2]
	--assert 2 = index? v: #(i16! [1 2] 2)
	--assert 2 = index? v: #(i16! #{01000200} 2)

--test-- "MOLD of unsigned vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/756
	--assert "#(int32! [0 0])" = mold make vector! [signed integer! 32 2]
	--assert "#(uint32! [0 0])" = mold make vector! [unsigned integer! 32 2]
	--assert "#(int32! [0 0])" = mold/all make vector! [signed integer! 32 2]
	--assert "#(uint32! [0 0])" = mold/all make vector! [unsigned integer! 32 2]

--test-- "MOLD/flat on vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2349
	--assert (mold/flat make vector! [integer! 8 12]) = {#(int8! [0 0 0 0 0 0 0 0 0 0 0 0])}
	--assert (mold/all/flat make vector! [integer! 8 12]) = "#(int8! [0 0 0 0 0 0 0 0 0 0 0 0])"
	--assert (mold make vector! [integer! 8  2]) = "#(int8! [0 0])"
	--assert (mold make vector! [integer! 8 20]) = {#(int8! [
    0 0 0 0 0 0 0 0 0 0
    0 0 0 0 0 0 0 0 0 0
])}
	v: make vector! [integer! 8 20]
	--assert (mold reduce [
	1 2
	v
	3 4
]) = {[
    1 2 #(int8! [
        0 0 0 0 0 0 0 0 0 0
        0 0 0 0 0 0 0 0 0 0
    ])
    3 4
]}

--test-- "MOLD/flat on shaped vector"
	v: #(u8! 3x2 [1 2 3 4 5 6])
	--assert (mold/flat v) == "#(uint8! 3x2 [1 2 3 4 5 6])"
	--assert (mold/all/flat v) == "#(uint8! 3x2 [1 2 3 4 5 6])"
	--assert not find mold/flat v "^/"


--test-- "QUERY on vector as object"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2352
	v: make vector! [unsigned integer! 16 2]
	o: query v object!
	--assert object? o
	--assert not o/signed
	--assert o/type = 'integer!
	--assert o/size = 16
	--assert o/length = 2
	--assert o/minimum = 0
	--assert o/maximum = 0
--test-- "QUERY on vector"
	--assert [element-type signed type size length shape shaped minimum maximum range sum mean median variance sample-variance population-deviation sample-deviation] = query v none
	--assert [16 integer!] = query v [:size :type]
	--assert block? b: query v [signed length]
	--assert all [not b/signed b/length = 2]
	--assert 16 = query v 'size
	--assert 16 = size? v
--test-- "REFLECT on vector"
	--assert 16 = reflect v 'size
	--assert  2 = reflect v 'length
	--assert 'integer! = reflect v 'type
	--assert false = reflect v 'signed
	--assert [uint16! 2] = reflect v 'spec
	--assert [uint16! 2] = spec-of v
	--assert [uint8! 2x2] = spec-of #(u8! 2x2)
	;; signed and float spellings round-trip too
	--assert (spec-of #(i16! 2x2 [1 2 3 4])) = [int16! 2x2]
	--assert (spec-of #(f32! 2x2 [1 2 3 4])) = [float32! 2x2]
--test-- "ACCESSORS on vector"
	--assert 16 = v/size
	--assert  2 = v/length
	--assert 'integer! = v/type
	--assert false     = v/signed

--test-- "REVERSE on vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2515
	--assert #(u8!  [3 2 1]) = reverse #(u8!  [1 2 3])
	--assert #(u16! [3 2 1]) = reverse #(u16! [1 2 3])
	--assert #(u32! [3 2 1]) = reverse #(u32! [1 2 3])
	--assert #(u64! [3 2 1]) = reverse #(u64! [1 2 3])
	--assert #(i8!  [3 2 1]) = reverse #(i8!  [1 2 3])
	--assert #(i16! [3 2 1]) = reverse #(i16! [1 2 3])
	--assert #(i32! [3 2 1]) = reverse #(i32! [1 2 3])
	--assert #(i64! [3 2 1]) = reverse #(i64! [1 2 3])
	--assert #(f32! [3.0 2.0 1.0]) = reverse #(f32! [1 2 3])
	--assert #(f64! [3.0 2.0 1.0]) = reverse #(f64! [1 2 3])

	--assert #(u8!  [2 1 3]) = reverse/part #(u8!  [1 2 3]) 2
	--assert #(u16! [2 1 3]) = reverse/part #(u16! [1 2 3]) 2
	--assert #(u32! [2 1 3]) = reverse/part #(u32! [1 2 3]) 2
	--assert #(u64! [2 1 3]) = reverse/part #(u64! [1 2 3]) 2
	--assert #(i8!  [2 1 3]) = reverse/part #(i8!  [1 2 3]) 2
	--assert #(i16! [2 1 3]) = reverse/part #(i16! [1 2 3]) 2
	--assert #(i32! [2 1 3]) = reverse/part #(i32! [1 2 3]) 2
	--assert #(i64! [2 1 3]) = reverse/part #(i64! [1 2 3]) 2
	--assert #(f32! [2.0 1.0 3.0]) = reverse/part #(f32! [1 2 3]) 2
	--assert #(f64! [2.0 1.0 3.0]) = reverse/part #(f64! [1 2 3]) 2

	--assert #(u8!  [1 3 2]) = head reverse next #(u8!  [1 2 3])
	--assert #(u16! [1 3 2]) = head reverse next #(u16! [1 2 3])
	--assert #(u32! [1 3 2]) = head reverse next #(u32! [1 2 3])
	--assert #(u64! [1 3 2]) = head reverse next #(u64! [1 2 3])
	--assert #(i8!  [1 3 2]) = head reverse next #(i8!  [1 2 3])
	--assert #(i16! [1 3 2]) = head reverse next #(i16! [1 2 3])
	--assert #(i32! [1 3 2]) = head reverse next #(i32! [1 2 3])
	--assert #(i64! [1 3 2]) = head reverse next #(i64! [1 2 3])
	--assert #(f32! [1.0 3.0 2.0]) = head reverse next #(f32! [1 2 3])
	--assert #(f64! [1.0 3.0 2.0]) = head reverse next #(f64! [1 2 3])
===end-group===


===start-group=== "VECTOR from binary data"

	--test-- "binary is copied verbatim in native byte order"
		;; the raw COPY_MEM means TO BINARY! round-trips exactly
		v: #(u16! [1 2 3])
		--assert v == make vector! compose [u16! (to binary! v)]
		v: #(i32! [-1 2 3])
		--assert v == make vector! compose [i32! (to binary! v)]
		v: #(f64! [1.5 -2.5])
		--assert v == make vector! compose [f64! (to binary! v)]
		;; ...and via the compact syntax
		--assert (transcode/one {#(u16! #{010002000300})}) == make vector! [u16! #{010002000300}]

	--test-- "binary length is clamped to the allocation"
		--assert 2 = length? v: make vector! [u16! 2 #{010002000300}]
		--assert v == #(u16! [1 2])
		--assert 4 = length? v: make vector! [u8! 2x2 #{0102030405}]
		--assert v == #(u8! 2x2 [1 2 3 4])

	--test-- "binary shorter than one element is rejected"
		;; APPEND already traps on this -- the constructors must agree
		--assert all [error? e: try [append #(i16! [1 2]) #{03}]  e/id = 'invalid-data]
		--assert error? try [make vector! [i16! #{03}]]
		--assert error? try [make vector! [i32! #{0102}]]
		--assert error? try [make vector! [f64! #{01020304}]]
		--assert error? transcode/one/error {#(i16! #{03})}
		;; but an empty binary stays legal
		--assert #(uint8! [])  == make vector! [u8! #{}]
		--assert #(uint16! []) == make vector! [u16! #{}]
		--assert #(uint8! [])  == to vector! #{}

	--test-- "partial trailing bytes are dropped, not rejected"
		;; 3 bytes into a u16! vector -- one whole element, one stray byte
		--assert 1 = length? v: make vector! [u16! #{010002}]
		--assert v == #(u16! [1])

	--test-- "shape and binary data together"
		--assert (make vector! [u8! 2x2 #{01020304}]) == #(u8! 2x2 [1 2 3 4])
		--assert all [
			m: make vector! [u16! 2x2 #{0100020003000400}]
			m/shape = 2x2
			(pick m 2x2) == 4
		]

===end-group===


===start-group=== "VECTOR compact construction"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2396
	--test-- "Compact construction syntax (empty)"
		;- Not supported anymore!
		;--assert (mold #(i8! ))  == "#(int8! [])"
		;--assert (mold #(i16!))  == "#(int16! [])"
		;--assert (mold #(i32!))  == "#(int32! [])"
		;--assert (mold #(i64!))  == "#(int64! [])"
		;--assert (mold #(u8! ))  == "#(uint8! [])"
		;--assert (mold #(u16!))  == "#(uint16! [])"
		;--assert (mold #(u32!))  == "#(uint32! [])"
		;--assert (mold #(u64!))  == "#(uint64! [])"
		;--assert (mold #(f32! )) == "#(float32! [])"
		;--assert (mold #(f64! )) == "#(float64! [])"

	--test-- "Compact construction syntax (size)"
		;- Not supported anymore!
		;--assert (mold #(i8!  3)) == "#(int8! [0 0 0])"
		;--assert (mold #(i16! 3)) == "#(int16! [0 0 0])"
		;--assert (mold #(i32! 3)) == "#(int32! [0 0 0])"
		;--assert (mold #(i64! 3)) == "#(int64! [0 0 0])"
		;--assert (mold #(u8!  3)) == "#(uint8! [0 0 0])"
		;--assert (mold #(u16! 3)) == "#(uint16! [0 0 0])"
		;--assert (mold #(u32! 3)) == "#(uint32! [0 0 0])"
		;--assert (mold #(u64! 3)) == "#(uint64! [0 0 0])"
		;--assert (mold #(f32! 3)) == "#(float32! [0.0 0.0 0.0])"
		;--assert (mold #(f64! 3)) == "#(float64! [0.0 0.0 0.0])"

	--test-- "Compact construction syntax (data)"
		--assert (mold #(i8!  [1 2])) == "#(int8! [1 2])"
		--assert (mold #(i16! [1 2])) == "#(int16! [1 2])"
		--assert (mold #(i32! [1 2])) == "#(int32! [1 2])"
		--assert (mold #(i64! [1 2])) == "#(int64! [1 2])"
		--assert (mold #(u8!  [1 2])) == "#(uint8! [1 2])"
		--assert (mold #(u16! [1 2])) == "#(uint16! [1 2])"
		--assert (mold #(u32! [1 2])) == "#(uint32! [1 2])"
		--assert (mold #(u64! [1 2])) == "#(uint64! [1 2])"
		--assert (mold #(f32! [1 2])) == "#(float32! [1.0 2.0])"
		--assert (mold #(f64! [1 2])) == "#(float64! [1.0 2.0])"

	--test-- "Compact construction syntax (data with index)"
		--assert (mold v: #(i8!  [1 2] 2)) == "#(int8! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i16! [1 2] 2)) == "#(int16! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i32! [1 2] 2)) == "#(int32! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i64! [1 2] 2)) == "#(int64! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u8!  [1 2] 2)) == "#(uint8! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u16! [1 2] 2)) == "#(uint16! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u32! [1 2] 2)) == "#(uint32! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u64! [1 2] 2)) == "#(uint64! [2])"
		--assert 2 = index? v
		--assert (mold v: #(f32!  [1 2] 2)) == "#(float32! [2.0])"
		--assert 2 = index? v
		--assert (mold v: #(f64!  [1 2] 2)) == "#(float64! [2.0])"
		--assert 2 = index? v
===end-group===

===start-group=== "VECTOR semi-compact construction"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2396
	--test-- "Compact construction syntax (empty)"
		--assert (mold make vector! [i8! ]) == "#(int8! [])"
		--assert (mold make vector! [i16!]) == "#(int16! [])"
		--assert (mold make vector! [i32!]) == "#(int32! [])"
		--assert (mold make vector! [i64!]) == "#(int64! [])"
		--assert (mold make vector! [u8! ]) == "#(uint8! [])"
		--assert (mold make vector! [u16!]) == "#(uint16! [])"
		--assert (mold make vector! [u32!]) == "#(uint32! [])"
		--assert (mold make vector! [u64!]) == "#(uint64! [])"
		--assert (mold make vector! [f32!]) == "#(float32! [])"
		--assert (mold make vector! [f64!]) == "#(float64! [])"

	--test-- "Compact construction syntax (empty, long names)"
		--assert (mold make vector! [int8! ])  == "#(int8! [])"
		--assert (mold make vector! [int16!])  == "#(int16! [])"
		--assert (mold make vector! [int32!])  == "#(int32! [])"
		--assert (mold make vector! [int64!])  == "#(int64! [])"
		--assert (mold make vector! [uint8! ]) == "#(uint8! [])"
		--assert (mold make vector! [byte!  ]) == "#(uint8! [])"
		--assert (mold make vector! [uint16!]) == "#(uint16! [])"
		--assert (mold make vector! [uint32!]) == "#(uint32! [])"
		--assert (mold make vector! [uint64!]) == "#(uint64! [])"
		--assert (mold make vector! [float!])  == "#(float32! [])"
		--assert (mold make vector! [double!]) == "#(float64! [])"

	--test-- "Compact construction syntax (size)"
		--assert (mold make vector! [i8!  3]) == "#(int8! [0 0 0])"
		--assert (mold make vector! [i16! 3]) == "#(int16! [0 0 0])"
		--assert (mold make vector! [i32! 3]) == "#(int32! [0 0 0])"
		--assert (mold make vector! [i64! 3]) == "#(int64! [0 0 0])"
		--assert (mold make vector! [u8!  3]) == "#(uint8! [0 0 0])"
		--assert (mold make vector! [u16! 3]) == "#(uint16! [0 0 0])"
		--assert (mold make vector! [u32! 3]) == "#(uint32! [0 0 0])"
		--assert (mold make vector! [u64! 3]) == "#(uint64! [0 0 0])"
		--assert (mold make vector! [f32! 3]) == "#(float32! [0.0 0.0 0.0])"
		--assert (mold make vector! [f64! 3]) == "#(float64! [0.0 0.0 0.0])"

	--test-- "Compact construction syntax (data)"
		--assert (mold make vector! [i8!  [1 2]]) == "#(int8! [1 2])"
		--assert (mold make vector! [i16! [1 2]]) == "#(int16! [1 2])"
		--assert (mold make vector! [i32! [1 2]]) == "#(int32! [1 2])"
		--assert (mold make vector! [i64! [1 2]]) == "#(int64! [1 2])"
		--assert (mold make vector! [u8!  [1 2]]) == "#(uint8! [1 2])"
		--assert (mold make vector! [u16! [1 2]]) == "#(uint16! [1 2])"
		--assert (mold make vector! [u32! [1 2]]) == "#(uint32! [1 2])"
		--assert (mold make vector! [u64! [1 2]]) == "#(uint64! [1 2])"
		--assert (mold make vector! [f32! [1 2]]) == "#(float32! [1.0 2.0])"
		--assert (mold make vector! [f64! [1 2]]) == "#(float64! [1.0 2.0])"

	--test-- "Compact construction syntax (data with index)"
		--assert (mold v: make vector! [i8!  [1 2] 2]) = "#(int8! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [i16! [1 2] 2]) = "#(int16! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [i32! [1 2] 2]) = "#(int32! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [i64! [1 2] 2]) = "#(int64! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [u8!  [1 2] 2]) = "#(uint8! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [u16! [1 2] 2]) = "#(uint16! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [u32! [1 2] 2]) = "#(uint32! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [u64! [1 2] 2]) = "#(uint64! [2])"
		--assert 2 = index? v
		--assert (mold v: make vector! [f32! [1 2] 2]) = "#(float32! [2.0])"
		--assert 2 = index? v
		--assert (mold v: make vector! [f64! [1 2] 2]) = "#(float64! [2.0])"
		--assert 2 = index? v

	--test-- "Construction syntax"
		--assert (mold v: #(i8!  [1 2] 2)) = "#(int8! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i16! [1 2] 2)) = "#(int16! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i32! [1 2] 2)) = "#(int32! [2])"
		--assert 2 = index? v
		--assert (mold v: #(i64! [1 2] 2)) = "#(int64! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u8!  [1 2] 2)) = "#(uint8! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u16! [1 2] 2)) = "#(uint16! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u32! [1 2] 2)) = "#(uint32! [2])"
		--assert 2 = index? v
		--assert (mold v: #(u64! [1 2] 2)) = "#(uint64! [2])"
		--assert 2 = index? v
		--assert (mold v: #(f32! [1 2] 2)) = "#(float32! [2.0])"
		--assert 2 = index? v
		--assert (mold v: #(f64! [1 2] 2)) = "#(float64! [2.0])"
		--assert 2 = index? v
===end-group===

===start-group=== "VECTOR math"

--test-- "VECTOR 8bit integer add/subtract"
	v: #(u8![1 2 3 4])
	--assert (v: v + 200) = #(u8![201 202 203 204])
	; the values are truncated on overflow:
	--assert (v: v + 200) = #(u8![145 146 147 148])
	--assert (v: v - 400) = #(u8![1 2 3 4])
	v: subtract (add v 10) 10
	--assert v = #(u8![1 2 3 4])
	v: 1 + v
	--assert v = #(u8![2 3 4 5])
	v: -1.0 + v
	--assert v = #(u8![1 2 3 4])

	v: #(i8![1 2 3 4])
	--assert (v: v + 125) = #(i8![126 127 -128 -127])
	--assert (v: v - 125) = #(i8![1 2 3 4])

--test-- "VECTOR 8bit integer multiply"
	v: #(u8![1 2 3 4])
	--assert (v: v * 4) = #(u8![4 8 12 16])
	; the values are truncated on overflow:
	--assert (v: v * 20) = #(u8![80 160 240 64]) ;64 = (16 * 20) - 256

	v: #(i8![1 2 3 4])
	--assert (v: v * 2.0) = #(i8![2 4 6 8])
	; the decimal is first converted to integer (2):
	--assert (v: v * 2.4) = #(i8![4 8 12 16])
	v: divide (multiply v 2) 2
	--assert v = #(i8![4 8 12 16])

--test-- "VECTOR 16bit integer multiply"
	v: #(u16![1 2 3 4])
	--assert (v: v * 4)  = #(u16![4 8 12 16])
	--assert (v: v * 20) = #(u16![80 160 240 320])
	v: multiply v 2
	--assert v = #(u16![160 320 480 640])

	v: #(u16![1 2 3 4])
	--assert (10   * v) = #(u16![10 20 30 40])
	--assert (10.0 * v) = #(u16![10 20 30 40])

	; the values are truncated on overflow:
	v: #(u16![1 2 3 4])
	--assert (v: v * 10000) = #(u16![10000 20000 30000 40000])
	--assert (v: v * 10.0)  = #(u16![34464 3392 37856 6784])

--test-- "VECTOR 16bit integer divide"
	v: #(u16![80 160 240 320])
	v: v / 20 / 2
	v: divide v 2
	--assert v = #(u16![1 2 3 4])
	--assert error? try [10 / v]
	--assert error? try [ v / 0] 

--test-- "VECTOR 32bit decimal add/subtract"
	v: #(f32![1 2 3 4])
	--assert (v: v + 200) = #(f32![201 202 203 204])
	--assert (v: v + 0.5) = #(f32![201.5 202.5 203.5 204.5])
	; notice the precision lost with 32bit decimal value:
	v: v - 0.1
	--assert 2013 = to integer! 10 * v/1 ; result is not 201.4 as would be with 64bit

--test-- "VECTOR 64bit decimal add/subtract"
	v: #(f64![1 2 3 4])
	--assert (v: v + 200) = #(f64![201 202 203 204])
	--assert (v: v + 0.5) = #(f64![201.5 202.5 203.5 204.5])
	--assert (v: v - 0.1) = #(f64![201.4 202.4 203.4 204.4])

--test-- "VECTOR 64bit decimal multiply/divide"
	v: #(f64![1 2 3 4])
	--assert (v: v * 20.5) = #(f64![20.5 41.0 61.5 82.0])
	--assert (v: v / 20.5) = #(f64![1.0 2.0 3.0 4.0])

--test-- "VECTOR math operation with vector not at head"
	v: #(i8![1 2 3 4])
	--assert (2 + skip v 2) = #(i8![5 6])
	--assert v = #(i8![1 2 3 4])

--test-- "VECTOR + vector"
	--assert (#(i8! [1 2]) + #(i8! [3 4])) = #(i8! [4 6])
	--assert (#(i16! [1 2]) + #(i16! [3 4 5])) = #(i16! [4 6])
	--assert (#(u32! [1 2]) + #(u32! [1 3 4] 2)) = #(u32! [4 6])
	--assert (#(f64! [1 1 2] 2) + #(f64! [1 3 4] 2)) = #(f64! [4 6])

--test-- "VECTOR - vector"
	--assert (#(i8! [4 6]) - #(i8! [3 4])) = #(i8! [1 2])
	--assert (#(i16! [4 6]) - #(i16! [3 4 5])) = #(i16! [1 2])
	--assert (#(u32! [4 6]) - #(u32! [1 3 4] 2)) = #(u32! [1 2])
	--assert (#(f64! [1 4 6] 2) - #(f64! [1 3 4] 2)) = #(f64! [1 2])

--test-- "VECTOR * vector"
	--assert (#(i8! [1 2]) * #(i8! [3 4])) = #(i8! [3 8])
	--assert (#(i16! [1 2]) * #(i16! [3 4 5])) = #(i16! [3 8])
	--assert (#(u32! [1 2]) * #(u32! [1 3 4] 2)) = #(u32! [3 8])
	--assert (#(f64! [1 1 2] 2) * #(f64! [1 3 4] 2)) = #(f64! [3 8])

--test-- "VECTOR / vector"
	--assert (#(i8! [10 20]) / #(i8! [2 4])) = #(i8! [5 5])
	--assert (#(i16! [10 20]) / #(i16! [2 4 5])) = #(i16! [5 5])
	--assert (#(u32! [10 20]) / #(u32! [1 2 4] 2)) = #(u32! [5 5])
	--assert (#(f64! [1 10 20] 2) / #(f64! [1 2 4] 2)) = #(f64! [5 5])

;@@ https://github.com/Oldes/Rebol-issues/issues/2524
;@@ https://github.com/Oldes/Rebol-issues/issues/2617
--test-- "VECTOR or"
	--assert (#(int8!  [1 2 3 4]) or 2) == #(int8!  [3 2 3 6])
	--assert (#(int16! [1 2 3 4]) or 2) == #(int16! [3 2 3 6])
	--assert (#(int32! [1 2 3 4]) or 2) == #(int32! [3 2 3 6])
	--assert (#(int64! [1 2 3 4]) or 2) == #(int64! [3 2 3 6])
	--assert (#(uint8!  [1 2 3 4]) or 2) == #(uint8!  [3 2 3 6])
	--assert (#(uint16! [1 2 3 4]) or 2) == #(uint16! [3 2 3 6])
	--assert (#(uint32! [1 2 3 4]) or 2) == #(uint32! [3 2 3 6])
	--assert (#(uint64! [1 2 3 4]) or 2) == #(uint64! [3 2 3 6])
	--assert all [error? e: try [#(float32! [1 2]) or 1]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) or 1]  e/id = 'not-related]

--test-- "VECTOR and"
	--assert (#(int8!  [1 2 3 4]) and 10) == #(int8!  [0 2 2 0])
	--assert (#(int16! [1 2 3 4]) and 10) == #(int16! [0 2 2 0])
	--assert (#(int32! [1 2 3 4]) and 10) == #(int32! [0 2 2 0])
	--assert (#(int64! [1 2 3 4]) and 10) == #(int64! [0 2 2 0])
	--assert (#(uint8!  [1 2 3 4]) and 10) == #(uint8!  [0 2 2 0])
	--assert (#(uint16! [1 2 3 4]) and 10) == #(uint16! [0 2 2 0])
	--assert (#(uint32! [1 2 3 4]) and 10) == #(uint32! [0 2 2 0])
	--assert (#(uint64! [1 2 3 4]) and 10) == #(uint64! [0 2 2 0])
	--assert all [error? e: try [#(float32! [1 2]) and 1]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) and 1]  e/id = 'not-related]

--test-- "VECTOR xor"
	--assert (#(int8!  [1 2 3 4]) xor 2) == #(int8!  [3 0 1 6])
	--assert (#(int16! [1 2 3 4]) xor 2) == #(int16! [3 0 1 6])
	--assert (#(int32! [1 2 3 4]) xor 2) == #(int32! [3 0 1 6])
	--assert (#(int64! [1 2 3 4]) xor 2) == #(int64! [3 0 1 6])
	--assert (#(uint8!  [1 2 3 4]) xor 2) == #(uint8!  [3 0 1 6])
	--assert (#(uint16! [1 2 3 4]) xor 2) == #(uint16! [3 0 1 6])
	--assert (#(uint32! [1 2 3 4]) xor 2) == #(uint32! [3 0 1 6])
	--assert (#(uint64! [1 2 3 4]) xor 2) == #(uint64! [3 0 1 6])
	--assert all [error? e: try [#(float32! [1 2]) xor 2]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) xor 2]  e/id = 'not-related]

--test-- "VECTOR remainder"
	--assert (#(int8!  [1 2 3 4]) % 2) == #(int8!  [1 0 1 0])
	--assert (#(int16! [1 2 3 4]) % 2) == #(int16! [1 0 1 0])
	--assert (#(int32! [1 2 3 4]) % 2) == #(int32! [1 0 1 0])
	--assert (#(int64! [1 2 3 4]) % 2) == #(int64! [1 0 1 0])
	--assert (#(uint8!  [1 2 3 4]) % 2) == #(uint8!  [1 0 1 0])
	--assert (#(uint16! [1 2 3 4]) % 2) == #(uint16! [1 0 1 0])
	--assert (#(uint32! [1 2 3 4]) % 2) == #(uint32! [1 0 1 0])
	--assert (#(uint64! [1 2 3 4]) % 2) == #(uint64! [1 0 1 0])
	--assert (#(float32! [1 2 3 4]) % 2) == #(float32! [1 0 1 0])
	--assert (#(float64! [1 2 3 4]) % 2) == #(float64! [1 0 1 0])
--test-- "VECTOR remainder with zero"
	--assert all [error? e: try [#(int8! [1 2]) % 0]  e/id = 'zero-divide]
	--assert all [error? e: try [#(float32! [1 2]) % 0]  e/id = 'zero-divide]
	--assert all [error? e: try [#(float64! [1 2]) % 0]  e/id = 'zero-divide]

--test-- "VECTOR or vector"
	--assert (#(int8!  [1 2 3 4]) or #(i8! [5 6 7 8])) == #(int8! [5 6 7 12])
	--assert (#(int16! [1 2 3 4]) or #(i16! [5 6 7 8])) == #(int16! [5 6 7 12])
	--assert (#(int32! [1 2 3 4]) or #(i32! [5 6 7 8])) == #(int32! [5 6 7 12])
	--assert (#(int64! [1 2 3 4]) or #(i64! [5 6 7 8])) == #(int64! [5 6 7 12])
	--assert (#(uint8!  [1 2 3 4]) or #(u8! [5 6 7 8])) == #(uint8!  [5 6 7 12])
	--assert (#(uint16! [1 2 3 4]) or #(u16! [5 6 7 8])) == #(uint16! [5 6 7 12])
	--assert (#(uint32! [1 2 3 4]) or #(u32! [5 6 7 8])) == #(uint32! [5 6 7 12])
	--assert (#(uint64! [1 2 3 4]) or #(u64! [5 6 7 8])) == #(uint64! [5 6 7 12])
	--assert all [error? e: try [#(float32! [1 2]) or #(float32! [1 2])]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) or #(float64! [1 2])]  e/id = 'not-related]

--test-- "VECTOR and vector"
	--assert (#(int8!  [1 2 3 4]) and #(i8! [5 6 7 8])) == #(int8!  [1 2 3 0])
	--assert (#(int16! [1 2 3 4]) and #(i16! [5 6 7 8])) == #(int16! [1 2 3 0])
	--assert (#(int32! [1 2 3 4]) and #(i32! [5 6 7 8])) == #(int32! [1 2 3 0])
	--assert (#(int64! [1 2 3 4]) and #(i64! [5 6 7 8])) == #(int64! [1 2 3 0])
	--assert (#(uint8!  [1 2 3 4]) and #(u8! [5 6 7 8])) == #(uint8!  [1 2 3 0])
	--assert (#(uint16! [1 2 3 4]) and #(u16! [5 6 7 8])) == #(uint16! [1 2 3 0])
	--assert (#(uint32! [1 2 3 4]) and #(u32! [5 6 7 8])) == #(uint32! [1 2 3 0])
	--assert (#(uint64! [1 2 3 4]) and #(u64! [5 6 7 8])) == #(uint64! [1 2 3 0])
	--assert all [error? e: try [#(float32! [1 2]) and #(float32! [1 2])]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) and #(float64! [1 2])]  e/id = 'not-related]

--test-- "VECTOR xor vector"
	--assert (#(int8!  [1 2 3 4]) xor #(i8! [5 6 7 8])) == #(int8! [4 4 4 12])
	--assert (#(int16! [1 2 3 4]) xor #(i16! [5 6 7 8])) == #(int16! [4 4 4 12])
	--assert (#(int32! [1 2 3 4]) xor #(i32! [5 6 7 8])) == #(int32! [4 4 4 12])
	--assert (#(int64! [1 2 3 4]) xor #(i64! [5 6 7 8])) == #(int64! [4 4 4 12])
	--assert (#(uint8!  [1 2 3 4]) xor #(u8! [5 6 7 8])) == #(uint8!  [4 4 4 12])
	--assert (#(uint16! [1 2 3 4]) xor #(u16! [5 6 7 8])) == #(uint16! [4 4 4 12])
	--assert (#(uint32! [1 2 3 4]) xor #(u32! [5 6 7 8])) == #(uint32! [4 4 4 12])
	--assert (#(uint64! [1 2 3 4]) xor #(u64! [5 6 7 8])) == #(uint64! [4 4 4 12])
	--assert all [error? e: try [#(float32! [1 2]) xor #(float32! [1 2])]  e/id = 'not-related]
	--assert all [error? e: try [#(float64! [1 2]) xor #(float64! [1 2])]  e/id = 'not-related]

--test-- "VECTOR remainder vector"
	--assert (#(int8!  [1 2 3 4]) % #(i8! [2 2 2 2])) == #(int8!  [1 0 1 0])
	--assert (#(int16! [1 2 3 4]) % #(i16! [2 2 2 2])) == #(int16! [1 0 1 0])
	--assert (#(int32! [1 2 3 4]) % #(i32! [2 2 2 2])) == #(int32! [1 0 1 0])
	--assert (#(int64! [1 2 3 4]) % #(i64! [2 2 2 2])) == #(int64! [1 0 1 0])
	--assert (#(uint8!  [1 2 3 4]) % #(u8! [2 2 2 2])) == #(uint8!  [1 0 1 0])
	--assert (#(uint16! [1 2 3 4]) % #(u16! [2 2 2 2])) == #(uint16! [1 0 1 0])
	--assert (#(uint32! [1 2 3 4]) % #(u32! [2 2 2 2])) == #(uint32! [1 0 1 0])
	--assert (#(uint64! [1 2 3 4]) % #(u64! [2 2 2 2])) == #(uint64! [1 0 1 0])
	--assert (#(float32! [1 2 3 4]) % #(float32! [2 2 2 2])) == #(float32! [1 0 1 0])
	--assert (#(float64! [1 2 3 4]) % #(float64! [2 2 2 2])) == #(float64! [1 0 1 0])

--test-- "operations on empty vectors"
	--assert (copy #(u32! [])) == #(u32! [])
	--assert (copy #(f64! [])) == #(f64! [])
	--assert 0 = length? copy #(u8! [])
	--assert (copy/part #(u32! [1 2 3]) 0) == #(u32! [])
	--assert (#(u32! []) + 1) == #(u32! [])
	--assert (#(u32! []) * 2) == #(u32! [])
	--assert (#(u32! []) + #(u32! [])) == #(u32! [])
	--assert 0 = length? take/part #(u32! []) 5

===end-group===


===start-group=== "VECTOR ´minimum/maximum"
	vi08: #(i8!  [1 -2 0])
	vi16: #(i16! [1 -2 0])
	vi32: #(i32! [1 -2 0])
	vi64: #(i64! [1 -2 0])
	vu08: #(u8!  [1 2 0])
	vu16: #(u16! [1 2 0])
	vu32: #(u32! [1 2 0])
	vu64: #(u64! [1 2 0])
	vf32: #(f32! [1 -2 0])
	vf64: #(f64! [1 -2 0])
	--test-- "Find minimum of the vector"
		--assert vi08/min == -2
		--assert vi16/min == -2
		--assert vi32/min == -2
		--assert vi64/min == -2
		--assert vu08/min ==  0
		--assert vu16/min ==  0
		--assert vu32/min ==  0
		--assert vu64/min ==  0
		--assert vf32/min == -2.0
		--assert vf64/min == -2.0
		;; it can be used also full word
		--assert vi08/minimum == -2
	--test-- "Find maximum of the vector"
		--assert vi08/max == 1
		--assert vi16/max == 1
		--assert vi32/max == 1
		--assert vi64/max == 1
		--assert vu08/max == 2
		--assert vu16/max == 2
		--assert vu32/max == 2
		--assert vu64/max == 2
		--assert vf32/max == 1.0
		--assert vf64/max == 1.0
		--assert vi08/maximum == 1

	--test-- "Find min/max using query v1"
		--assert [minimum: -2   maximum: 1]   == query vi08 [minimum maximum]
		--assert [minimum: -2   maximum: 1]   == query vi16 [minimum maximum]
		--assert [minimum: -2   maximum: 1]   == query vi32 [minimum maximum]
		--assert [minimum: -2   maximum: 1]   == query vi64 [minimum maximum]
		--assert [minimum:  0   maximum: 2]   == query vu08 [minimum maximum]
		--assert [minimum:  0   maximum: 2]   == query vu16 [minimum maximum]
		--assert [minimum:  0   maximum: 2]   == query vu32 [minimum maximum]
		--assert [minimum:  0   maximum: 2]   == query vu64 [minimum maximum]
		--assert [minimum: -2.0 maximum: 1.0] == query vf32 [minimum maximum]
		--assert [minimum: -2.0 maximum: 1.0] == query vf64 [minimum maximum]
	--test-- "Find min/max using query v2"
		--assert [-2   1]   == query vi08 [:minimum :maximum]
		--assert [-2   1]   == query vi16 [:minimum :maximum]
		--assert [-2   1]   == query vi32 [:minimum :maximum]
		--assert [-2   1]   == query vi64 [:minimum :maximum]
		--assert [ 0   2]   == query vu08 [:minimum :maximum]
		--assert [ 0   2]   == query vu16 [:minimum :maximum]
		--assert [ 0   2]   == query vu32 [:minimum :maximum]
		--assert [ 0   2]   == query vu64 [:minimum :maximum]
		--assert [-2.0 1.0] == query vf32 [:minimum :maximum]
		--assert [-2.0 1.0] == query vf64 [:minimum :maximum]

	vi08: #(i8!  [])
	vi16: #(i16! [])
	vi32: #(i32! [])
	vi64: #(i64! [])
	vu08: #(u8!  [])
	vu16: #(u16! [])
	vu32: #(u32! [])
	vu64: #(u64! [])
	vf32: #(f32! [])
	vf64: #(f64! [])
	--test-- "Find minimum of the empty vector"
		--assert none? vi08/min
		--assert none? vi16/min
		--assert none? vi32/min
		--assert none? vi64/min
		--assert none? vu08/min
		--assert none? vu16/min
		--assert none? vu32/min
		--assert none? vu64/min
		--assert none? vf32/min
		--assert none? vf64/min
	--test-- "Find maximum of the empty vector"
		--assert none? vi08/max
		--assert none? vi16/max
		--assert none? vi32/max
		--assert none? vi64/max
		--assert none? vu08/max
		--assert none? vu16/max
		--assert none? vu32/max
		--assert none? vu64/max
		--assert none? vf32/max
		--assert none? vf64/max
===end-group===


===start-group=== "VECTOR statictics"
;@@ https://github.com/Oldes/Rebol-issues/issues/2648
	--test-- "Query modes"
		all-modes: query #(u8![]) none
		--assert all-modes
		== [element-type signed type size length shape shaped minimum maximum range sum mean median variance sample-variance population-deviation sample-deviation]
		all-get-modes: collect [foreach m all-modes [keep to get-word! m]]
		--assert all-get-modes
		== [:element-type :signed :type :size :length :shape :shaped :minimum :maximum :range :sum :mean :median :variance :sample-variance :population-deviation :sample-deviation]
	
	--test-- "int8! vector statictics"
	v: #(int8! [-2 -1 1 2 4])
	--assert (query v all-modes) == [
		element-type: int8!
	    signed: #(true)
	    type: integer!
	    size: 8
	    length: 5
	    shape: 5x1
	    shaped: #(false)
	    minimum: -2
	    maximum: 4
	    range: 6
	    sum: 4
	    mean: 0.8
	    median: 1.0
	    variance: 4.56
	    sample-variance: 5.7
	    population-deviation: 2.13541565040626
	    sample-deviation: 2.38746727726266
	]

	--assert (query v all-get-modes) 
	== [int8! #(true) integer! 8 5 5x1 #(false) -2 4 6 4 0.8 1.0 4.56 5.7 2.13541565040626 2.38746727726266]

	--test-- "uint64! vector statictics"
	v: #(uint64! [4 9 11 12 17])
	--assert (query v all-modes) == [
		element-type: uint64!
	    signed: #(false)
	    type: integer!
	    size: 64
	    length: 5
	    shape: 5x1
	    shaped: #(false)
	    minimum: 4
	    maximum: 17
	    range: 13
	    sum: 53
	    mean: 10.6
	    median: 11.0
	    variance: 17.84
	    sample-variance: 22.3
	    population-deviation: 4.22374241638857
	    sample-deviation: 4.72228758124704
	]

	--assert (query v all-get-modes) 
	== [uint64! #(false) integer! 64 5 5x1 #(false) 4 17 13 53 10.6 11.0 17.84 22.3 4.22374241638857 4.72228758124704]

	--test-- "float64! vector statictics"
	v: #(float64! [1.62 1.72 1.64 1.7 1.78 1.64 1.65 1.64 1.66 1.74])
	--assert (query v all-modes) == [
		element-type: float64!
	    signed: #(true)
	    type: decimal!
	    size: 64
	    length: 10
	    shape: 10x1
	    shaped: #(false)
	    minimum: 1.62
	    maximum: 1.78
	    range: 0.16
	    sum: 16.79
	    mean: 1.679
	    median: 1.655
	    variance: 0.002529
	    sample-variance: 0.00281
	    population-deviation: 0.0502891638427207
	    sample-deviation: 0.0530094331227943
	]

	--assert (query v all-get-modes)
	== [float64! #(true) decimal! 64 10 10x1 #(false) 1.62 1.78 0.16 16.79 1.679 1.655 0.002529 0.00281 0.0502891638427207 0.0530094331227943]

	--test-- "QUERY on empty vector"
	--assert (query #(u8! []) all-get-modes) == [uint8! #(false) integer! 8 0 0x1 #(false) _ _ _ _ _ _ _ _ _ _]

	--test-- "QUERY on single value vector"
	--assert (query #(u8! [1])  all-modes) == [
		element-type: uint8!
	    signed: #(false)
	    type: integer!
	    size: 8
	    length: 1
	    shape: 1x1
	    shaped: #(false)
	    minimum: 1
	    maximum: 1
	    range: 0
	    sum: 1
	    mean: 1.0
	    median: 1.0
	    variance: 0.0
	    sample-variance: _
	    population-deviation: 0.0
	    sample-deviation: _
	]
	--assert (query #(u8! [1]) all-get-modes)
	== [uint8! #(false) integer! 8 1 1x1 #(false) 1 1 0 1 1.0 1.0 0.0 _ 0.0 _]

	--test-- "median covers the same range as the other statistics"
		v: skip #(i8! [100 1 2 3]) 1     ;; visible = [1 2 3]
		--assert 2.0 = v/mean
		--assert 2.0 = v/median          ;; over the whole series it'd be 2.5
		--assert 1   = v/minimum
		--assert 3   = v/maximum

	--test-- "QUERY on vector not at head"
		v: next #(int8! [100 1 2 3])
		--assert (query v all-modes) == [
			element-type: int8!
		    signed: #(true)
		    type: integer!
		    size: 8
		    length: 3
		    shape: 3x1
		    shaped: #(false)
		    minimum: 1
		    maximum: 3
		    range: 2
		    sum: 6
		    mean: 2.0
		    median: 2.0
		    variance: 0.666666666666667
		    sample-variance: 1.0
		    population-deviation: 0.816496580927726
		    sample-deviation: 1.0
		]
		v: tail v
		--assert (query v all-modes) == [
			element-type: int8!
		    signed: #(true)
		    type: integer!
		    size: 8
		    length: 0
		    shape: 0x1
		    shaped: #(false)
		    minimum: _
		    maximum: _
		    range: _
		    sum: _
		    mean: _
		    median: _
		    variance: _
		    sample-variance: _
		    population-deviation: _
		    sample-deviation: _
		]

===end-group===


===start-group=== "VECTOR Compare"
	--test-- "compare vectors"
	;@@  https://github.com/Oldes/Rebol-issues/issues/458
	--assert equal? (make vector! 3)(make vector! 3)
	--assert not equal? #(u16! [1 2]) #(u16! [1 2 3])
	--assert #(u16! [1 2]) = #(u16! [1 2])
	--assert #(u16! [1 2]) < #(u16! [1 2 0])
	--assert #(u16! [1 2]) < #(u16! [1 2 1])
	--assert #(u16! [1 2]) < #(u16! [2 2])
	--assert #(u16! [2 2]) > #(u16! [1 2])

	--test-- "compare vectors - i64"
		vi1: #(int64! [-1 2]) vi2: #(int64! [1 2])
		--assert vi1 < vi2
		--assert not (vi1 > vi2)
		--assert vi1 = vi1
		--assert #(i64! [9223372036854775807]) > #(i64! [9223372036854775806]) 

	--test-- "compare vectors - u64"
		vi1: #(uint64! [1 2]) vi2: #(uint64! [1 3])
		--assert vi1 < vi2
		--assert not (vi1 > vi2)
		--assert #(u64! [0#FFFFFFFFFFFFFFFF]) > #(u64! [0#FEFFFFFFFFFFFFFF])

	--test-- "compare vectors - f32"
		vf1: #(f32! [-1 2]) vf2: #(f32! [1 2])
		--assert vf1 < vf2
		--assert not (vf1 > vf2)
		--assert vf1 = vf1

	--test-- "compare vectors - f64 - negative zero"
		vz1: #(float64! [-0.0])
		vz2: #(float64! [ 0.0])
		--assert vz1 = vz2
		--assert not vz1 < vz2
		--assert not vz1 > vz2

	--test-- "compare vectors - cross-signedness at 64-bit width"
		vs: #(i64! [-1])
		vu: #(u64! [0#FFFFFFFFFFFFFFFF])  ;; UINT64_MAX — same bit pattern as -1 in two's complement
		--assert not vs = vu   ;; must NOT silently treat as equal
		--assert     vs < vu   ;; -1 is numerically far less than UINT64_MAX

		--assert #(u64! [1 2])  = #(u32! [1 2])
		--assert #(i64! [1 2])  = #(i32! [1 2])
		--assert #(i64! [1 2]) != #(i32! [1 3])
		--assert #(i64! [-1])   = #(i32! [-1])
		--assert #(i64! [-1])   < #(i32! [0])
		--assert #(i64! [-1])  != #(u32! [-1])

	--test-- "cross-category loose equality now works, no trap"
		--assert #(i32! [1 2]) = #(f32! [1.0 2.0])
		--assert #(f64! [1.0 2.0]) = #(i64! [1 2])
		--assert #(u16! [1 2]) = #(f64! [1.0 2.0])

	--test-- "...but strict equality still separates them"
		--assert      #(i32! [1 2]) !== #(f32! [1.0 2.0])
		--assert not (#(i32! [1 2])  == #(f32! [1.0 2.0]))

	--test-- "ordering across categories"
		--assert #(i32! [1]) < #(f32! [1.5])
		--assert #(f32! [0.5]) < #(i32! [1])
		--assert #(i32! [2]) > #(f64! [1.999])

	--test-- "PRECISION: must not collapse via double-widening"
		--assert not (#(i64! [9007199254740993]) = #(f64! [9007199254740992.0]))
		--assert #(i64! [9007199254740993]) > #(f64! [9007199254740992.0])

	--test-- "out-of-range floats resolve by magnitude, not by overflowing the cast"
		--assert #(i64! [ 9223372036854775807]) < #(f64! [ 1e30])
		--assert #(i64! [-9223372036854775808]) > #(f64! [-1e30])

	--test-- "-0.0 against integer zero"
		--assert #(i32! [0]) = #(f64! [-0.0])


===end-group===


===start-group=== "VECTOR copy"

--test-- "COPY"
	;@@ https://github.com/Oldes/Rebol-issues/issues/463
	;@@ https://github.com/Oldes/Rebol-issues/issues/2400
	v1: #(u16! [1 2])
	v2: v1
	v3: copy v2
	--assert     same? v1 v2
	--assert not same? v1 v3
	v2/1: 3
	--assert v1/1 = 3
	--assert v3/1 = 1
	

--test-- "COPY/PART"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2399
	v: #(u16! [1 2 3 4])
	--assert           2 = length? copy/part v 2
	--assert #{01000200} = to-binary copy/part v 2
	--assert #{03000400} = to-binary copy/part skip v 2 2

===end-group===


===start-group=== "TAKE"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2714
	--test-- "take of vector!"
		v: #(i32! [10 20 30 40 50])
		--assert (take v) == 10
		--assert (take/last v) == 50
		--assert (take/part v 2) == #(i32! [20 30])
		--assert (take/part/last v 1) == #(i32! [40])
		--assert empty? v
		--assert none? take v
		--assert none? take/last v

	--test-- "take/part with count exceeding remaining length should clamp, not error"
		v: #(i32! [1 2 3])
		--assert (take/part v 100) == #(i32! [1 2 3])

	--test-- "take of vector! not at head"
		v: #(i32! [10 20 30 40 50])
		v2: skip v 2                    ; v2 view starts at "30" (index 2)
		--assert (take v2) == 30        ; default take is relative to current position, not absolute head
		--assert v2 == #(i32! [40 50])
		--assert  v == #(i32! [10 20 40 50]) ; same underlying series -- shrinks for both refs

	--test-- "take/last of vector! not at head"
		v: #(i32! [10 20 30 40 50])
		v2: skip v 3                    ; view = [40 50]
		--assert (take/last v2) == 50
		--assert v2 == #(i32! [40])
		--assert v == #(i32! [10 20 30 40])

	--test-- "take/part of vector! not at head - v1"
		v: skip #(i32! [10 20 30 40 50]) 2
		--assert (take/part v 2) == #(i32! [30 40])
		--assert v == #(i32! [50])
		--assert (head v) == #(i32! [10 20 50])

	--test-- "take/part of vector! not at head - v2"
		v: #(i32! [10 20 30 40 50])
		v2: skip v 1                    ; view = [20 30 40 50]
		--assert (take/part v2 2) == #(i32! [20 30])
		--assert v2 == #(i32! [40 50])
		--assert v == #(i32! [10 40 50])

	--test-- "take/part with negative length"
		;; negative /part takes backwards from the current position
		v: #(u8! [1 2 3 4 5 6])
		--assert (take/part tail v -2) == #(u8! [5 6])
		--assert v == #(u8! [1 2 3 4])

		v: #(u8! [1 2 3 4 5 6])
		--assert (take/part skip v 3 -2) == #(u8! [2 3])
		--assert v == #(u8! [1 4 5 6])

		;; clamped at the head, like blocks
		v: #(u8! [1 2 3])
		--assert (take/part skip v 1 -5) == #(u8! [1])
		--assert v == #(u8! [2 3])

		;; zero and head-position cases
		v: #(u8! [1 2 3])
		--assert (take/part v -2) == #(u8! [])
		--assert v == #(u8! [1 2 3])

	--test-- "take/part/last of vector! not at head"
		v: skip #(i32! [10 20 30 40 50]) 2
		--assert (take/part/last v 4) == #(i32! [30 40 50])
		--assert empty? v
		--assert (head v) == #(i32! [10 20])

	--test-- "take/part/last with negative length"
		;; matches block behaviour -- /last already counts back from the tail
		v: #(u8! [1 2 3 4 5 6])
		--assert (take/part/last v -2) == #(u8! [])
		--assert v == #(u8! [1 2 3 4 5 6])
		--assert (take/part/last [1 2 3 4 5 6] -2) == []

	--test-- "take/part/last must not reach before the current index"
		v: #(i32! [10 20 30 40 50])
		v2: skip v 3                    ; view = [40 50], only 2 elements visible
		--assert (take/part/last v2 5) == #(i32! [40 50])   ; clamp to what's visible, not the full tail
		--assert empty? v2
		--assert v == #(i32! [10 20 30])   ; the hidden prefix [10 20 30] must survive untouched

	--test-- "take/last exactly at the visible boundary"
		v: #(i32! [10 20 30 40 50])
		v2: skip v 2                    ; view = [30 40 50], visible = 3
		--assert (take/part/last v2 3) == #(i32! [30 40 50])   ; exactly all visible elements
		--assert empty? v2
		--assert v == #(i32! [10 20])

===end-group===


===start-group=== "PICK"
	--test-- "PICK of vector!"
	;@@  https://github.com/Oldes/Rebol-issues/issues/748
	v: #(u32! [1 2 3])
	--assert all [
		1   = pick v 1
		2   = pick v 2
		none? pick v -1
		none? pick v 0
		none? pick v 10
	]
===end-group===


===start-group=== "POKE"
	--test-- "POKE into vector!"
	v: #(u32! [1 2 3])
	--assert all [
		10 = poke v 1 10
		10 = pick v 1
	]
	;@@  https://github.com/Oldes/Rebol-issues/issues/2427
	--assert all [
		error? err: try [poke v 10 1]
		err/id = 'out-of-range
	]
	--assert all [
		error? err: try [poke v 0 1]
		err/id = 'out-of-range
	]

	--test-- "POKE into decimal vector"
	;@@ https://github.com/metaeducation/rebol-issues/issues/2508
	--assert all [
		vector? a: make vector! [decimal! 32 3]
		1.0 = poke a 1 1.0
		1.0 = a/1
		1.0 = pick a 1
	]
===end-group===


===start-group=== "FIND-MAX / FIND-MIN"
	;@@ https://github.com/Oldes/Rebol-issues/issues/460
	v: #(i32! [1 2 3 -1])
	--test-- "FIND-MAX vector!" --assert  3 = first find-max v
	--test-- "FIND-MIN vector!" --assert -1 = first find-min v
===end-group===


===start-group=== "SORT"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1101
	--test-- "SORT vector!"
		--assert  #(i8!  [1 2 3 4]) == sort #(i8!  [2 4 1 3])
		--assert  #(i16! [1 2 3 4]) == sort #(i16! [2 4 1 3])
		--assert  #(i32! [1 2 3 4]) == sort #(i32! [2 4 1 3])
		--assert  #(i64! [1 2 3 4]) == sort #(i64! [2 4 1 3])
		--assert  #(f32! [1 2 3 4]) == sort #(f32! [2 4 1 3])
		--assert  #(f64! [1 2 3 4]) == sort #(f64! [2 4 1 3])
	--test-- "SORT/reverse vector!"
		--assert  #(i8!  [4 3 2 1]) == sort/reverse #(i8!  [2 4 1 3])
		--assert  #(i16! [4 3 2 1]) == sort/reverse #(i16! [2 4 1 3])
		--assert  #(i32! [4 3 2 1]) == sort/reverse #(i32! [2 4 1 3])
		--assert  #(i64! [4 3 2 1]) == sort/reverse #(i64! [2 4 1 3])
		--assert  #(f32! [4 3 2 1]) == sort/reverse #(f32! [2 4 1 3])
		--assert  #(f64! [4 3 2 1]) == sort/reverse #(f64! [2 4 1 3])
	--test-- "SORT/part vector!"
		--assert  #(i8!  [1 2 4 3]) == sort/part #(i8!  [2 4 1 3]) 3
		--assert  #(i16! [1 2 4 3]) == sort/part #(i16! [2 4 1 3]) 3
		--assert  #(i32! [1 2 4 3]) == sort/part #(i32! [2 4 1 3]) 3
		--assert  #(i64! [1 2 4 3]) == sort/part #(i64! [2 4 1 3]) 3
		--assert  #(f32! [1 2 4 3]) == sort/part #(f32! [2 4 1 3]) 3
		--assert  #(f64! [1 2 4 3]) == sort/part #(f64! [2 4 1 3]) 3
	--test-- "SORT/part/reverse vector!"
		--assert  #(i8!  [4 2 1 3]) == sort/part/reverse #(i8!  [2 4 1 3]) 3
		--assert  #(i16! [4 2 1 3]) == sort/part/reverse #(i16! [2 4 1 3]) 3
		--assert  #(i32! [4 2 1 3]) == sort/part/reverse #(i32! [2 4 1 3]) 3
		--assert  #(i64! [4 2 1 3]) == sort/part/reverse #(i64! [2 4 1 3]) 3
		--assert  #(f32! [4 2 1 3]) == sort/part/reverse #(f32! [2 4 1 3]) 3
		--assert  #(f64! [4 2 1 3]) == sort/part/reverse #(f64! [2 4 1 3]) 3
	--test-- "SORT next vector!"
		--assert  #(i8!  [2 1 3 4]) == head sort next #(i8!  [2 4 1 3])
		--assert  #(i16! [2 1 3 4]) == head sort next #(i16! [2 4 1 3])
		--assert  #(i32! [2 1 3 4]) == head sort next #(i32! [2 4 1 3])
		--assert  #(i64! [2 1 3 4]) == head sort next #(i64! [2 4 1 3])
		--assert  #(f32! [2 1 3 4]) == head sort next #(f32! [2 4 1 3])
		--assert  #(f64! [2 1 3 4]) == head sort next #(f64! [2 4 1 3])
	--test-- "SORT/reverse next vector!"
		--assert  #(i8!  [2 4 3 1]) == head sort/reverse next #(i8!  [2 4 1 3])
		--assert  #(i16! [2 4 3 1]) == head sort/reverse next #(i16! [2 4 1 3])
		--assert  #(i32! [2 4 3 1]) == head sort/reverse next #(i32! [2 4 1 3])
		--assert  #(i64! [2 4 3 1]) == head sort/reverse next #(i64! [2 4 1 3])
		--assert  #(f32! [2 4 3 1]) == head sort/reverse next #(f32! [2 4 1 3])
		--assert  #(f64! [2 4 3 1]) == head sort/reverse next #(f64! [2 4 1 3])
	--test-- "SORT/part next vector!"
		--assert  #(i8!  [2 1 4 3]) == head sort/part next #(i8!  [2 4 1 3]) 2
		--assert  #(i16! [2 1 4 3]) == head sort/part next #(i16! [2 4 1 3]) 2
		--assert  #(i32! [2 1 4 3]) == head sort/part next #(i32! [2 4 1 3]) 2
		--assert  #(i64! [2 1 4 3]) == head sort/part next #(i64! [2 4 1 3]) 2
		--assert  #(f32! [2 1 4 3]) == head sort/part next #(f32! [2 4 1 3]) 2
		--assert  #(f64! [2 1 4 3]) == head sort/part next #(f64! [2 4 1 3]) 2
	--test-- "SORT/part/reverse next vector!"
		--assert  #(i8!  [2 4 1 3]) == head sort/part/reverse next #(i8!  [2 4 1 3]) 2
		--assert  #(i16! [2 4 1 3]) == head sort/part/reverse next #(i16! [2 4 1 3]) 2
		--assert  #(i32! [2 4 1 3]) == head sort/part/reverse next #(i32! [2 4 1 3]) 2
		--assert  #(i64! [2 4 1 3]) == head sort/part/reverse next #(i64! [2 4 1 3]) 2
		--assert  #(f32! [2 4 1 3]) == head sort/part/reverse next #(f32! [2 4 1 3]) 2
		--assert  #(f64! [2 4 1 3]) == head sort/part/reverse next #(f64! [2 4 1 3]) 2

	--test-- "SORT/compare with a field offset"
		;; sort records of 2 by their second field
		--assert (sort/skip/compare #(i32! [1 30  2 10  3 20]) 2 2)
		      == #(i32! [2 10  3 20  1 30])
		;; offset 1 is the default ordering
		--assert (sort/skip/compare #(i32! [3 30  1 10  2 20]) 2 1)
		      == #(i32! [1 10  2 20  3 30])
		--assert (sort/skip/compare/reverse #(i32! [1 30  2 10  3 20]) 2 2)
		      == #(i32! [1 30  3 20  2 10])

	--test-- "SORT/compare with a block of offsets"
		;; primary field 2, tie-broken by field 1
		--assert (sort/skip/compare #(i32! [2 10  1 10  3 5]) 2 [2 1])
		      == #(i32! [3 5  1 10  2 10])

	--test-- "SORT/compare validation matches blocks"
		--assert all [error? e: try [sort/compare #(i32! [1 2 3 4]) 1]        e/id = 'invalid-arg]
		--assert all [error? e: try [sort/skip/compare #(i32! [1 2 3 4]) 2 0] e/id = 'invalid-arg]
		--assert all [error? e: try [sort/skip/compare #(i32! [1 2 3 4]) 2 3] e/id = 'invalid-arg]
		--assert all [error? e: try [sort/skip/compare/all #(i32! [1 2 3 4]) 2 1] e/id = 'bad-refines]
		--assert all [error? e: try [sort/skip/compare #(i32! [1 2 3 4]) 2 [1 9]] e/id = 'invalid-arg]

	--test-- "SORT/compare with a function is still unsupported"
		--assert all [
			error? e: try [sort/compare #(i8! [2 4 1 3]) func [a b][a < b]]
			e/id = 'feature-na
		]

	--test-- "SORT/skip vector!"
		--assert (sort/skip #(i32! [3 30 1 10 2 20]) 2) == #(i32! [1 10 2 20 3 30])
		--assert (sort/skip #(u8!  [3 30 1 10 2 20]) 2) == #(u8!  [1 10 2 20 3 30])
		--assert (sort/skip #(f64! [3 30 1 10 2 20]) 2) == #(f64! [1 10 2 20 3 30])
		;; skip 1 is plain sort
		--assert (sort/skip #(i32! [2 4 1 3]) 1) == #(i32! [1 2 3 4])

	--test-- "SORT/skip/reverse vector!"
		--assert (sort/skip/reverse #(i32! [3 30 1 10 2 20]) 2) == #(i32! [3 30 2 20 1 10])

	--test-- "SORT/skip sorts whole rows of a shaped vector"
		m: make vector! [u8! 3x2 [4 5 6 1 2 3]]
		sort/skip m 3
		--assert m == #(u8! 3x2 [1 2 3 4 5 6])
		--assert m/shape = 3x2

		m: make vector! [u8! 3x3 [7 8 9 1 2 3 4 5 6]]
		sort/skip m to integer! first m/shape
		--assert m == #(u8! 3x3 [1 2 3 4 5 6 7 8 9])

	--test-- "SORT/skip on a vector not at head"
		v: #(i32! [99 3 30 1 10])
		sort/skip next v 2
		--assert v == #(i32! [99 1 10 3 30])

	--test-- "SORT/skip compares only the leading field"
		--assert (sort/skip #(i32! [3 3 30  1 1 20  1 2 10]) 3)
		      == #(i32! [1 2 10  1 1 20  3 3 30])

	--test-- "SORT/skip/all compares whole records"
		--assert (sort/skip/all #(i32! [3 3 30  1 1 20  1 2 10]) 3)
		      == #(i32! [1 1 20  1 2 10  3 3 30])
		;; ties on the first two fields fall through to the third
		--assert (sort/skip/all #(i32! [1 1 30  1 1 10  1 1 20]) 3)
		      == #(i32! [1 1 10  1 1 20  1 1 30])

	--test-- "SORT/skip/all/reverse is the exact reverse ordering"
		--assert (sort/skip/all/reverse #(i32! [1 1 20  3 3 30  1 2 10]) 3)
		      == #(i32! [3 3 30  1 2 10  1 1 20])

	--test-- "SORT/all without /skip is a no-op"
		--assert (sort/all #(i32! [2 4 1 3])) == #(i32! [1 2 3 4])

	--test-- "SORT/skip argument validation"
		--assert all [error? e: try [sort/skip #(i8! [1 2 3 4]) 0]   e/id = 'out-of-range]
		--assert all [error? e: try [sort/skip #(i8! [1 2 3 4]) -2]  e/id = 'out-of-range]
		--assert all [error? e: try [sort/skip #(i8! [1 2]) 5]       e/id = 'out-of-range]
		;; length must be a whole number of records -- matches block behaviour
		--assert all [error? e: try [sort/skip #(i8! [1 2 3 4 5]) 2] e/id = 'out-of-range]
		--assert           error? try [sort/skip [1 2 3 4 5] 2]
		;; ...but a 1-element series short-circuits before validation
		--assert (sort/skip #(i8! [1]) 5) == #(i8! [1])
		--assert (sort/skip [1] 5) == [1]

===end-group===


===start-group=== "sort/case with vectors"

	--test-- "element type as primary /case key, deterministic across permutations"
		--assert (sort/case [#(f32! [1.0]) #(i32! [1])]) == [#(i32! [1]) #(f32! [1.0])]
		--assert (sort/case [#(i32! [1]) #(f32! [1.0])]) == [#(i32! [1]) #(f32! [1.0])]

	--test-- "VECT_TYPE enum order: signed ints, then unsigned, then floats"
		--assert (sort/case [#(f64! [0]) #(u8! [0]) #(i64! [0]) #(i8! [0]) #(f32! [0]) #(u64! [0])])
		                 == [#(i8! [0]) #(i64! [0]) #(u8! [0]) #(u64! [0]) #(f32! [0]) #(f64! [0])]

	--test-- "type outranks value under /case"
		--assert (sort/case [#(i32! [99]) #(f32! [-99.0])]) == [#(i32! [99]) #(f32! [-99.0])]

	--test-- "plain sort ignores element type, orders purely by value"
		--assert (sort [#(i32! [99]) #(f32! [-99.0])]) == [#(f32! [-99.0]) #(i32! [99])]
		--assert (sort [#(f32! [1.0]) #(i32! [0]) #(i64! [2])])
		            == [#(i32! [0]) #(f32! [1.0]) #(i64! [2])]

	--test-- "same element type: /case and plain sort agree"
		--assert (sort/case [#(i32! [3]) #(i32! [1]) #(i32! [2])]) == [#(i32! [1]) #(i32! [2]) #(i32! [3])]
		--assert (sort      [#(i32! [3]) #(i32! [1]) #(i32! [2])]) == [#(i32! [1]) #(i32! [2]) #(i32! [3])]

	--test-- "type vs shape precedence under /case"
		--assert (sort/case [#(f32! 6x1 [0 0 0 0 0 0]) #(i32! 3x2 [0 0 0 0 0 0])])
		                 == [#(i32! 3x2 [0 0 0 0 0 0]) #(f32! 6x1 [0 0 0 0 0 0])]

	--test-- "element type outranks shape under /case"
		;; i32! has rows=3, f32! has rows=1 -- type wins, so i32! sorts first
		--assert (sort/case [#(f32! 6x1 [0 0 0 0 0 0]) #(i32! 2x3 [0 0 0 0 0 0])])
		                 == [#(i32! 2x3 [0 0 0 0 0 0]) #(f32! 6x1 [0 0 0 0 0 0])]

	--test-- "shape still decides within a same-element-type group"
		--assert (sort/case [#(i32! 2x3 [0 0 0 0 0 0]) #(i32! 6x1 [0 0 0 0 0 0])])
		                 == [#(i32! 6x1 [0 0 0 0 0 0]) #(i32! 2x3 [0 0 0 0 0 0])]

	--test-- "plain sort ignores element type, shape still primary"
		--assert (sort [#(f32! 6x1 [0 0 0 0 0 0]) #(i32! 2x3 [0 0 0 0 0 0])])
		            == [#(f32! 6x1 [0 0 0 0 0 0]) #(i32! 2x3 [0 0 0 0 0 0])]

===end-group===


===start-group=== "Vector modification actions"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1326
	;@@ https://github.com/Oldes/Rebol-issues/issues/2527
	--test-- "APPEND vector number"
		--assert (append #(i8! [1 2]) 3) == #(i8! [1 2 3])
		--assert (append next #(i16! [1 2]) 3) == #(i16! [1 2 3])
		--assert (append #(i32! [1 2]) 3.5) == #(i32! [1 2 3])
		--assert (append/part #(i64! [1 2]) 3 2) == #(i64! [1 2 3])
		--assert (append/dup #(f32! [1 2]) 3 2) == #(f32! [1 2 3 3])
	
	--test-- "APPEND vector block"
		--assert (append #(i8! [1 2]) [3 4]) == #(i8! [1 2 3 4])
		--assert (append #(i16! [1 2]) [3.5 4.1]) == #(i16! [1 2 3 4])
		--assert (append next #(i32! [1 2]) [3 4]) == #(i32! [1 2 3 4])
		--assert (append/part #(i64! [1 2]) [3 4] 1) == #(i64! [1 2 3])
		--assert (append/part #(f32! [1 2]) [3 4] 3) == #(f32! [1 2 3 4])
		--assert (append/dup  #(f64! [1 2]) [3 4] 2) == #(f64! [1 2 3 4 3 4])

	--test-- "APPEND vector vector"
		--assert (append #(i8! [1 2]) #(i8! [3 4])) == #(i8! [1 2 3 4])
		--assert (append #(i16! [1 2]) #(f32! [3.5 4.1])) == #(i16! [1 2 3 4])
		--assert (append next #(i32! [1 2]) #(i8! [3 4])) == #(i32! [1 2 3 4])
		--assert (append/part #(i64! [1 2]) #(i8! [3 4]) 1) == #(i64! [1 2 3])
		--assert (append/part #(f32! [1 2]) #(i8! [3 4]) 3) == #(f32! [1 2 3 4])
		--assert (append/dup  #(f64! [1 2]) #(i8! [3 4]) 2) == #(f64! [1 2 3 4 3 4])

	--test-- "APPEND vector binary"
		--assert (append #(i8! [1 2]) #{0304}) == #(i8! [1 2 3 4])
		--assert (append #(i16! [1 2]) #{03000400})   == #(i16! [1 2 3 4])
		--assert (append next #(i8! [1 2]) #{0304})   == #(i8! [1 2 3 4])
		--assert (append/part #(i8! [1 2]) #{0304} 1) == #(i8! [1 2 3])
		--assert (append/part #(i8! [1 2]) #{0304} 3) == #(i8! [1 2 3 4])
		--assert (append/dup  #(i8! [1 2]) #{0304} 2) == #(i8! [1 2 3 4 3 4])
	--test-- "APPEND vector binary (invalid)"
		--assert all [
			error? e: try [append #(i16! [1 2]) #{03}]
			e/id = 'invalid-data
			e/arg1 = #{03}
		]
		--assert all [
			error? e: try [append/part #(i16! [1 2]) #{0304} 1]
			e/id = 'invalid-data
			e/arg1 = #{0304}
		]

	--test-- "INSERT/APPEND from a same-type vector not at head"
		--assert (append #(i32! [1 2]) next #(i32! [3 4])) == #(i32! [1 2 4])
		--assert (append #(i64! [1 2]) skip #(i64! [3 4 5]) 2) == #(i64! [1 2 5])
		--assert all [
			(insert v: #(i32! [1 2]) next #(i32! [3 4])) == #(i32! [1 2])
			v == #(i32! [4 1 2])
		]
		;; width 1 was already correct -- regression guard
		--assert (append #(i8! [1 2]) next #(i8! [3 4])) == #(i8! [1 2 4])

	--test-- "INSERT vector number"
		--assert all [
			(insert v: #(i8! [1 2]) 3) == #(i8! [1 2])
			v == #(i8! [3 1 2])
		]
		--assert all [
			(insert next v: #(i8! [1 2]) 3) == #(i8! [2])
			v == #(i8! [1 3 2])
		]
		--assert all [
			(insert v: #(i8! [1 2]) 3.5) == #(i8! [1 2])
			v == #(i8! [3 1 2])
		]
		--assert all [
			(insert/part v: #(i8! [1 2]) 3 2) == #(i8! [1 2])
			v == #(i8! [3 1 2])
		]
		--assert all [
			(insert/dup v: #(i8! [1 2]) 3 2) == #(i8! [1 2])
			v == #(i8! [3 3 1 2])
		]

	--test-- "INSERT vector block"
		--assert all [
			(insert v: #(i8! [1 2]) [3 4]) == #(i8! [1 2])
			v == #(i8! [3 4 1 2])
		]
		--assert all [
			(insert v: #(i8! [1 2]) [3.5 4.1]) == #(i8! [1 2])
			v == #(i8! [3 4 1 2])
		]
		--assert all [
			(insert next v: #(i8! [1 2]) [3 4]) == #(i8! [2])
			v == #(i8! [1 3 4 2])
		]
		--assert all [
			(insert/part v: #(i8! [1 2]) [3 4] 1) == #(i8! [1 2])
			v == #(i8! [3 1 2])
		]
		--assert all [
			(insert/part v: #(i8! [1 2]) [3 4] 3) == #(i8! [1 2])
			v == #(i8! [3 4 1 2])
		]
		--assert all [
			(insert/dup v: #(i8! [1 2]) [3 4] 2) == #(i8! [1 2])
			v == #(i8! [3 4 3 4 1 2])
		]

	--test-- "INSERT vector vector"
		--assert all [
			(insert v: #(i8! [1 2]) #(i8! [3 4])) == #(i8! [1 2])
			v == #(i8! [3 4 1 2])
		]
		--assert all [
			(insert v: #(i16! [1 2]) #(f32! [3.5 4.1])) == #(i16! [1 2])
			v == #(i16! [3 4 1 2])
		]
		--assert all [
			(insert next v: #(i32! [1 2]) #(i8! [3 4])) == #(i32! [2])
			v == #(i32! [1 3 4 2])
		]
		--assert all [
			(insert/part v: #(i64! [1 2]) #(i8! [3 4]) 1) == #(i64! [1 2])
			v == #(i64! [3 1 2])
		]
		--assert all [
			(insert/part v: #(f32! [1 2]) #(i8! [3 4]) 3) == #(f32! [1 2])
			v == #(f32! [3 4 1 2])
		]
		--assert all [
			(insert/dup v: #(f64! [1 2]) #(i8! [3 4]) 2) == #(f64! [1 2])
			v == #(f64! [3 4 3 4 1 2])
		]

	--test-- "CHANGE vector number"
		--assert all [
			(change v: #(i8! [1 2]) 3) == #(i8! [2])
			v == #(i8! [3 2])
		]
		--assert all [
			(change next v: #(i8! [1 2 3]) 4) == #(i8! [3])
			v == #(i8! [1 4 3])
		]
		--assert all [
			(change/part v: #(i8! [1 2]) 3 1) == #(i8! [2])
			v == #(i8! [3 2])
		]
		--assert all [
			(change/part v: #(i8! [1 2]) 3 3) == #(i8! [])
			v == #(i8! [3])
		]
		--assert all [
			(change/dup v: #(i8! [1 2]) 3 2) == #(i8! [])
			v == #(i8! [3 3])
		]

	--test-- "CHANGE vector block"
		--assert all [
			(change v: #(i8! [1 2]) [3 4]) == #(i8! [])
			v == #(i8! [3 4])
		]
		--assert all [
			(change v: #(i8! [1 2]) [3.5 4.1]) == #(i8! [])
			v == #(i8! [3 4])
		]
		--assert all [
			(change v: #(i8! [1 2 3]) [3 4]) == #(i8! [3])
			v == #(i8! [3 4 3])
		]
		--assert all [
			(change next v: #(i8! [1 2 3]) [3 4]) == #(i8! [])
			v == #(i8! [1 3 4])
		]
		--assert all [
			(change/part v: #(i8! [1 2]) [3 4] 1) == #(i8! [2])
			v == #(i8! [3 4 2])
		]
		--assert all [
			(change/part v: #(i8! [1 2]) [3 4] 3) == #(i8! [])
			v == #(i8! [3 4])
		]
		--assert all [
			(change/dup v: #(i8! [1 2]) [3 4] 2) == #(i8! [])
			v == #(i8! [3 4 3 4])
		]
		--assert all [
			(change/dup v: #(i8! [1 2 3 4 5]) [6 7] 2) == #(i8! [5])
			v == #(i8! [6 7 6 7 5])
		]

	--test-- "CHANGE vector vector"
		--assert all [
			(change v: #(i8! [1 2]) #(i8! [3 4])) == #(i8! [])
			v == #(i8! [3 4])
		]
		--assert all [
			(change v: #(i16! [1 2]) #(f32! [3.5 4.1])) == #(i16! [])
			v == #(i16! [3 4])
		]
		--assert all [
			(change v: #(i32! [1 2 3]) #(i8! [3 4])) == #(i32! [3])
			v == #(i32! [3 4 3])
		]
		--assert all [
			(change next v: #(i64! [1 2 3]) #(i8! [3 4])) == #(i64! [])
			v == #(i64! [1 3 4])
		]
		--assert all [
			(change/part v: #(f32! [1 2]) #(i8! [3 4]) 1) == #(f32! [2])
			v == #(f32! [3 4 2])
		]
		--assert all [
			(change/part v: #(f64! [1 2]) #(i8! [3 4]) 3) == #(f64! [])
			v == #(f64! [3 4])
		]
		--assert all [
			(change/dup v: #(i8! [1 2]) #(u16! [3 4]) 2) == #(i8! [])
			v == #(i8! [3 4 3 4])
		]
		--assert all [
			(change/dup v: #(i16! [1 2 3 4 5]) #(u32! [6 7]) 2) == #(i16! [5])
			v == #(i16! [6 7 6 7 5])
		]

	--test-- "CLEAR vector"
		--assert all [
			v: #(i8! [1 2])
			(clear v) == #(i8! [])
			empty? v
		]
		--assert all [
			v: #(u32! [4294967295 1])
			(clear next v) == #(u32! [])
			v == #(u32! [4294967295])
		]
===end-group===

===start-group=== "MATRIX (shaped vectors)"
	--test-- "Compact construction: shape without full data zero-fills"
		--assert (transcode/one {#(u8! 2x2))}      ) == #(uint8! 2x2 [0 0 0 0])
		--assert (transcode/one {#(u8! 2x2 []))}   ) == #(uint8! 2x2 [0 0 0 0])
		--assert (transcode/one {#(u8! 2x2 [1 2]))}) == #(uint8! 2x2 [1 2 0 0])
		;; matches the make spelling
		--assert #(u8! 2x2) == make vector! [u8! 2x2]
		;; rows 1 -- no annotation, but still allocated
		--assert (mold #(i32! 3x1)) == "#(int32! [0 0 0])"
		;; the plain-size form stays rejected (integer slot is the index)
		--assert error? transcode/one/error {#(u8! 2)}
	--test-- "Make shaped vector"
		--assert all [
			vector? try [m: make vector! [u8! 3x2 [1 2 3 4 5 6]]]
			m/shape = 3x2
			m/3 == 3
			m/(3x1) == 3
			(pick m 2x2) == 5
			m/(3x1): 33
			poke m 2x2 55
			m/(3x1) == 33
			(pick m 2x2) == 55
		]
		--assert all [
			vector? try [m: make vector! [u8! 2x3 [1 2 3 4 5 6]]]   ; 2 cols, 3 rows (X=cols, Y=rows)
			(pick m 1x1) == 1    
			(pick m 2x1) == 2    ;; second column, first row
			(pick m 1x2) == 3    ;; first column, second row
			(pick m 2x3) == 6    ;; last col, last row
			(pick m 3x1) == none ;; col 3 doesn't exist (only 2 cols)
			(pick m 1x4) == none ;; row 4 doesn't exist (only 3 rows)
		]
		;; degenerate case for a plain vector
		--assert all [
			v: #(f64! [10 20 30])
			(pick v 3x1) == 30.0 ;; matches pick v 3
			(pick v 1x2) == none ;; only 1 row exists
		]

	--test-- "pair indexing follows the view"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		v: next m
		--assert v/shape = 5x1
		--assert not v/shaped
		--assert (pick v 1x1) = 2        ;; first visible element
		--assert none? pick v 1x2        ;; only one row now
		--assert (pick m 1x2) = 4        ;; whole view still addresses the grid

	--test-- "reshape requires a whole view"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		v: next m
		--assert error? try [v/shape: 5x1]
		--assert error? try [v/shape: 1x5]
		m/shape: 2x3
		--assert m/shape = 2x3

	--test-- "pair and scalar indexing both follow the cursor"
		--assert all [
			vector? try [m: make vector! [u8! 3x2 [1 2 3 10 20 30]]] ;; 3 cols, 2 rows
			m2: skip m 3          ;; cursor at the start of the second row
			m2/shape = 3x1        ;; a partial view is one row of what it can see
			not m2/shaped
			(pick m2 1)   == 10   ;; scalar pick is cursor-relative
			(pick m2 1x1) == 10   ;; ...and so is pair pick
			(pick m2 2x1) == 20
			none? pick m2 1x2     ;; only one row in this view
			(pick m 1x2)  == 10   ;; the whole view still addresses the grid
		]

	--test-- "Math ops with shaped vectors"
		;; both shaped, matching shape -- elementwise, result inherits shape
		a: #(u16! 2x3 [1 2 3 4 5 6])
		b: #(u16! 2x3 [10 20 30 40 50 60])
		--assert (a + b) == #(u16! 2x3 [11 22 33 44 55 66])
		--assert (b + a) == #(u16! 2x3 [11 22 33 44 55 66])
		;; both shaped, same total length but different shape -- must trap, not silently truncate
		c: #(u16! 3x2 [1 2 3 4 5 6])
		--assert error? try [a + c]  ; 2x3 vs 3x2 -- same 6 elements, incompatible shape
		--assert error? try [c + a]
		;; both shaped, genuinely different length -- must trap
		d: #(u16! 2x2 [1 2 3 4])
		--assert error? try [a + d]
		--assert error? try [d + a]
		;; one shaped, one plain, matching total length -- allowed, inherits shaped side's shape
		e: #(u16! [1 1 1 1 1 1])
		--assert (a + e) == #(u16! 2x3 [2 3 4 5 6 7])
		--assert (e + a) == #(u16! 2x3 [2 3 4 5 6 7])
		;; mismatched-length plain-vs-shaped
		f: #(u16! [1 1 1 1])   ; 4 elements, vs a's 6
		--assert error? try [a + f]
		--assert error? try [f + a]
		;; Scalar broadcast still carries shape through
		--assert (a + 1) == #(u16! 2x3 [2 3 4 5 6 7])
		--assert (1 + a) == #(u16! 2x3 [2 3 4 5 6 7])
		;; * elementwise multiplication
		--assert (a * b) == #(u16! 2x3 [10 40 90 160 250 360])

	--test-- "Math with a skipped shaped operand must not inherit the shape"
		a: #(u16! 2x3 [1 2 3 4 5 6])
		e: #(u16! [1 1 1 1 1])
		--assert all [
			r: (skip a 1) + e
			r = #(uint16! [3 4 5 6 7])
			r/shape == 5x1
		]
	
	--test-- "Shaped vectors compare"
		g: #(u16! 3x2 [1 2 3 4 5 6])
		h: #(u16! 2x3 [1 2 3 4 5 6])
		--assert not (g = h)
		--assert not (g == h)
		--assert     (g != h)
		;; same shape, same content -- still equal, no regression
		i: #(u16! 2x3 [1 2 3 4 5 6])
		--assert (h = i)
		--assert (h == i)
		;; plain vs. shaped, same flat content
		p: #(u16! [1 2 3 4 5 6])
		--assert not (p = h)

	--test-- "reshape"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		--assert (m/shape: 2x3) = 2x3
		--assert m/shape = 2x3
		--assert error? try [m/shape: 3x20]
		m/shape: 6x1
		--assert m/shape == 6x1

	--test-- "reshape is per-value"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		a: m
		a/shape: 2x3
		--assert m/shape = 3x2

	--test-- "mold of shaped vector"
		--assert (mold #(u8! 3x2 [1 2 3 4 5 6])) == {#(uint8! 3x2 [
    1 2 3
    4 5 6
])}
	--test-- "mold round-trip of shaped vector"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		--assert equal? m load mold m
		--assert not find mold next m "3x2"
		--assert equal? (next m) (load mold/all next m)
		--assert not find mold/flat m "^/"

	--test-- "CHANGE is allowed when the length is unchanged"
		--assert all [
			r: change v: #(u8! 2x2 [1 2 3 4]) 10
			v == #(u8! 2x2 [10 2 3 4])
			2 = index? r
			r == skip v 1          ;; same series, same index, same shape
			r/shape = 3x1          ;; value is treated as unshaped
		]
		--assert all [
			r: change v: #(u8! 2x2 [1 2 3 4]) [10 20]
			v == #(u8! 2x2 [10 20 3 4])
			3 = index? r
		]
		--assert all [
			r: skip #(u8! 2x2 [1 2 3 4]) 1
			r/shape = 3x1
		]

	--test-- "CHANGE that would alter the length still traps"
		v: #(u8! 2x2 [1 2 3 4])
		--assert all [error? e: try [change/part v 9 2]     e/id = 'fixed-sized-series]
		--assert all [error? e: try [change/dup  v 9 8]     e/id = 'fixed-sized-series]
		--assert all [error? e: try [change skip v 3 [1 2]] e/id = 'fixed-sized-series]
		;; and the vector is untouched after the trap
		--assert v == #(u8! 2x2 [1 2 3 4])

	--test-- "Shaped vectors are zeroed with change/dup, not clear"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		--assert all [error? e: try [clear m]  e/id = 'fixed-sized-series]
		change/dup m 0 length? m
		--assert m == #(u8! 3x2 [0 0 0 0 0 0])
		--assert m/shape = 3x2

	--test-- "COPY of a shaped vector"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		--assert all [r: copy m  r/shape = 3x2  r == m  not same? r m]
		;; the copy is length-locked too
		--assert all [r: copy m  error? try [append r 7]]

	--test-- "partial COPY drops the shape"
		m: make vector! [u8! 3x2 [1 2 3 4 5 6]]
		--assert all [r: copy/part m 4     r/shape == 4x1  r == #(u8! [1 2 3 4])]
		--assert all [r: copy skip m 1     r/shape == 5x1]
		;; ...and is not length-locked
		--assert all [r: copy/part m 4     vector? append r 7]

===end-group===


mx: try [import 'matrix]   ;; module exports nothing - reach the words through it
if module? mx [
;; float comparison helper (elementwise, with tolerance)
near?: func [a [vector!] b [vector!] /local i][
	all [
		(length? a) = (length? b)
		none? repeat i length? a [
			if 1E-9 < abs (to decimal! a/:i) - (to decimal! b/:i) [break/return true]
		]
	]
]

===start-group=== "TRANSPOSE"
	--test-- "transpose swaps rows and columns"
		m: #(i32! 3x2 [1 2 3  4 5 6])         ;; 3 cols, 2 rows
		--assert all [
			t: mx/transpose m
			t == #(i32! 2x3 [1 4  2 5  3 6])
			t/shape = 2x3
			m == #(i32! 3x2 [1 2 3  4 5 6])   ;; source untouched
			not same? m t
		]

	--test-- "transpose is its own inverse"
		--assert all [m: #(u8! 3x2 [1 2 3 4 5 6])  m == mx/transpose mx/transpose m]
		--assert all [s: #(f64! 2x2 [1 2 3 4])     s == mx/transpose mx/transpose s]

	--test-- "transpose of a plain vector gives a column"
		--assert all [
			c: mx/transpose #(u8! [1 2 3])
			c/shape = 1x3
			c/shaped
			(pick c 1x2) = 2
		]
===end-group===

===start-group=== "IDENTITY"
	--test-- "identity modifies in place"
		--assert all [
			m: make vector! [u8! 4x4]
			same? m mx/identity m
			m == #(u8! 4x4 [1 0 0 0  0 1 0 0  0 0 1 0  0 0 0 1])
			m/shape = 4x4
		]

	--test-- "identity clears existing contents"
		--assert all [
			m: make vector! [i16! 2x2 [9 9 9 9]]
			mx/identity m
			m == #(i16! 2x2 [1 0 0 1])
		]

	--test-- "identity on float types"
		--assert all [
			m: make vector! [f64! 2x2 [5 5 5 5]]
			mx/identity m
			m == #(f64! 2x2 [1.0 0.0 0.0 1.0])
		]

	--test-- "identity copy leaves the original alone"
		--assert all [
			m: make vector! [u8! 2x2 [9 9 9 9]]
			i: mx/identity copy m
			m == #(u8! 2x2 [9 9 9 9])
			i == #(u8! 2x2 [1 0 0 1])
		]

	--test-- "identity requires a square matrix"
		--assert error? try [mx/identity make vector! [u8! 3x2]]
		--assert error? try [mx/identity make vector! [u8! 3]]
===end-group===

===start-group=== "TRACE"
	--test-- "trace sums the diagonal"
		--assert 15 = mx/trace #(i32! 3x3 [1 2 3  4 5 6  7 8 9])
		--assert  5 = mx/trace #(u8!  2x2 [1 2  3 4])
		--assert integer? mx/trace #(i32! 2x2 [1 2 3 4])

	--test-- "trace of a float matrix returns a decimal"
		--assert all [
			d: mx/trace #(f64! 2x2 [1.5 2.0  3.0 2.5])
			decimal? d
			d = 4.0
		]

	--test-- "trace requires a square matrix"
		--assert error? try [mx/trace #(i32! 3x2 [1 2 3 4 5 6])]
===end-group===

===start-group=== "DIAGONAL"
	--test-- "diagonal of a square matrix"
		--assert all [
			d: mx/diagonal #(i32! 3x3 [1 2 3  4 5 6  7 8 9])
			d == #(i32! [1 5 9])
			not d/shaped                  ;; returned unshaped
		]

	--test-- "diagonal of a non-square matrix stops at the shorter side"
		--assert (mx/diagonal #(i32! 3x2 [1 2 3  4 5 6])) == #(i32! [1 5])
		--assert (mx/diagonal #(i32! 2x3 [1 2  3 4  5 6])) == #(i32! [1 4])
===end-group===

===start-group=== "SWAP-ROWS"
	--test-- "swap-rows modifies in place"
		--assert all [
			m: make vector! [u8! 3x2 [1 2 3  4 5 6]]
			same? m mx/swap-rows m 1 2
			m == #(u8! 3x2 [4 5 6  1 2 3])
			m/shape = 3x2
		]

	--test-- "swapping a row with itself is a no-op"
		--assert all [
			m: make vector! [u8! 3x2 [1 2 3  4 5 6]]
			mx/swap-rows m 2 2
			m == #(u8! 3x2 [1 2 3  4 5 6])
		]

	--test-- "swap-rows range checks"
		--assert error? try [mx/swap-rows make vector! [u8! 3x2] 0 1]
		--assert error? try [mx/swap-rows make vector! [u8! 3x2] 1 3]
		--assert error? try [mx/swap-rows make vector! [u8! 3x2] -1 1]
===end-group===

===start-group=== "ROTATE"
	;;  1 2 3
	;;  4 5 6
	--test-- "rotate clockwise"
		--assert all [
			r: mx/rotate #(i32! 3x2 [1 2 3  4 5 6])
			r == #(i32! 2x3 [4 1  5 2  6 3])
			r/shape = 2x3
		]

	--test-- "rotate counter-clockwise"
		--assert all [
			r: mx/rotate/left #(i32! 3x2 [1 2 3  4 5 6])
			r == #(i32! 2x3 [3 6  2 5  1 4])
		]

	--test-- "rotate twice keeps the shape"
		--assert all [
			r: mx/rotate/twice #(i32! 3x2 [1 2 3  4 5 6])
			r == #(i32! 3x2 [6 5 4  3 2 1])
			r/shape = 3x2
		]

	--test-- "four clockwise rotations return the original"
		m: #(u8! 3x2 [1 2 3  4 5 6])
		--assert m == mx/rotate mx/rotate mx/rotate mx/rotate m

	--test-- "/twice wins over /left"
		;; pins current behaviour: ref_twice is tested first
		--assert (mx/rotate/left/twice #(i32! 3x2 [1 2 3  4 5 6]))
		      == (mx/rotate/twice     #(i32! 3x2 [1 2 3  4 5 6]))
===end-group===

===start-group=== "MATMUL"
	--test-- "matrix product"
		;; A = 2x3, B = 3x2  ->  2x2
		--assert all [
			a: #(i32! 3x2 [1 2 3  4 5 6])
			b: #(i32! 2x3 [7 8  9 10  11 12])
			r: mx/matmul a b
			r == #(i32! 2x2 [58 64  139 154])
			r/shape = 2x2
			a == #(i32! 3x2 [1 2 3  4 5 6])     ;; operands untouched
		]

	--test-- "matmul is not commutative"
		--assert all [
			a: #(i32! 3x2 [1 2 3  4 5 6])
			b: #(i32! 2x3 [7 8  9 10  11 12])
			r: mx/matmul b a
			r/shape = 3x3
			not equal? (mx/matmul a b) r
		]

	--test-- "multiplying by the identity"
		--assert all [
			m: #(f64! 3x3 [1 2 3  4 5 6  7 8 9])
			i: mx/identity copy m
			m == mx/matmul m i
			m == mx/matmul i m
		]

	--test-- "row vector times matrix"
		--assert (mx/matmul #(i32! [1 2 3]) #(i32! 2x3 [7 8  9 10  11 12]))
		      == #(i32! [58 64])

	--test-- "matrix times column vector"
		--assert all [
			col: mx/transpose #(i32! [7 9 11])
			col/shape = 1x3
			r: mx/matmul #(i32! 3x2 [1 2 3  4 5 6]) col
			r == #(i32! 1x2 [58 139])
		]

	--test-- "matmul dimension and type checks"
		--assert error? try [mx/matmul #(i32! 3x2 [1 2 3 4 5 6]) #(i32! 3x2 [1 2 3 4 5 6])]
		--assert error? try [mx/matmul #(i32! 2x2 [1 2 3 4])     #(f64! 2x2 [1 2 3 4])]
		--assert error? try [mx/matmul #(i32! [1 2 3])           #(i32! [4 5 6])]

	--test-- "matmul truncates on store"
		;; 100*100 + 100*100 = 20000 -> 32 as u8
		--assert (mx/matmul #(u8! 2x1 [100 100]) #(u8! 1x2 [100 100])) == #(u8! [32])
===end-group===

===start-group=== "KRONECKER"
	--test-- "kronecker product"
		;; [1 2; 3 4] (x) [0 5; 6 7]
		--assert all [
			k: mx/kronecker #(i32! 2x2 [1 2  3 4]) #(i32! 2x2 [0 5  6 7])
			k == #(i32! 4x4 [
				 0  5   0 10
				 6  7  12 14
				 0 15   0 20
				18 21  24 28
			])
			k/shape = 4x4
		]

	--test-- "kronecker with the 1x1 identity is a copy"
		--assert (mx/kronecker #(i32! 2x2 [1 2 3 4]) #(i32! [1])) == #(i32! 2x2 [1 2 3 4])

	--test-- "kronecker of non-square operands"
		;; (1x2) (x) (2x1) -> shape follows cols*cols x rows*rows
		--assert all [
			k: mx/kronecker #(u8! 1x2 [1 2]) #(u8! 2x1 [3 4])
			k/shape = 2x2
			k == #(u8! 2x2 [3 4  6 8])
		]

	--test-- "kronecker type check"
		--assert error? try [mx/kronecker #(i32! 2x2 [1 2 3 4]) #(u8! 2x2 [1 2 3 4])]
===end-group===

===start-group=== "empty vectors"
	--test-- "empty vectors pass through the shape-preserving commands"
		--assert (mx/transpose #(u8! [])) == #(u8! [])
		--assert (mx/diagonal  #(u8! [])) == #(u8! [])
		--assert (mx/rotate    #(u8! [])) == #(u8! [])
		;; ...but the square-only ones reject them
		--assert error? try [mx/identity #(u8! [])]
		--assert error? try [mx/trace    #(u8! [])]
		--assert error? try [mx/matmul   #(u8! []) #(u8! [])]
===end-group===

===start-group=== "commands follow the view"
	--test-- "a partial view is one row"
		m: #(i32! 3x2 [1 2 3  4 5 6])
		--assert all [v: next m  v/shape = 5x1]
		--assert all [t: mx/transpose next m  t/shape = 1x5  t == #(i32! 1x5 [2 3 4 5 6])]
		--assert all [d: mx/diagonal next m   d == #(i32! [2])]
		--assert error? try [mx/identity next #(u8! 2x2 [1 2 3 4])]

	--test-- "whole views still see the grid"
		--assert all [t: mx/transpose #(i32! 3x2 [1 2 3 4 5 6])  t/shape = 2x3]
===end-group===

===start-group=== "mezzanine helpers"
	--test-- "square?"
		--assert     mx/square? #(u8! 2x2 [1 2 3 4])
		--assert not mx/square? #(u8! 3x2 [1 2 3 4 5 6])
		--assert     mx/square? #(u8! [5])            ;; 1x1

	--test-- "symmetric?"
		--assert     mx/symmetric? #(i32! 2x2 [1 2  2 1])
		--assert     mx/symmetric? #(i32! 3x3 [1 2 3  2 4 5  3 5 6])
		--assert not mx/symmetric? #(i32! 2x2 [1 2  3 4])
		--assert not mx/symmetric? #(i32! 3x2 [1 2 3 4 5 6])

	--test-- "as-float"
		--assert all [
			f: mx/as-float #(i32! 2x2 [1 2 3 4])
			f/element-type = 'float64!
			f/shape = 2x2
			f == #(f64! 2x2 [1.0 2.0 3.0 4.0])
		]
		;; already float -- plain copy, original untouched
		--assert all [
			m: #(f32! 2x2 [1 2 3 4])
			f: mx/as-float m
			f/element-type = 'float32!
			not same? m f
		]

	--test-- "row and col"
		m: #(i32! 3x2 [1 2 3  4 5 6])
		--assert all [r: mx/row m 1  r == #(i32! [1 2 3])  not r/shaped]
		--assert all [r: mx/row m 2  r == #(i32! [4 5 6])]
		--assert all [c: mx/col m 2  c == #(i32! 1x2 [2 5])  c/shaped]
		--assert error? try [mx/row m 3]
		--assert error? try [mx/col m 4]
		--assert error? try [mx/row m 0]

	--test-- "augment"
		--assert all [
			a: mx/augment #(i32! 2x2 [1 2  3 4]) #(i32! 1x2 [5 6])
			a == #(i32! 3x2 [1 2 5  3 4 6])
			a/shape = 3x2
		]
		--assert error? try [mx/augment #(i32! 2x2 [1 2 3 4]) #(i32! 1x3 [5 6 7])]
		--assert error? try [mx/augment #(i32! 2x2 [1 2 3 4]) #(u8!  1x2 [5 6])]
===end-group===

===start-group=== "linear algebra"
	--test-- "rref"
		--assert all [
			m: mx/rref mx/as-float #(i32! 3x2 [1 2 3  4 5 6])
			near? m #(f64! 3x2 [1.0 0.0 -1.0  0.0 1.0 2.0])
		]

	--test-- "determinant"
		--assert -2.0 = mx/determinant #(i32! 2x2 [1 2  3 4])
		--assert  1.0 = mx/determinant #(f64! 3x3 [1 0 0  0 1 0  0 0 1])
		--assert (abs -3.0 - mx/determinant #(i32! 3x3 [1 2 3  4 5 6  7 8 10])) < 1E-9
		--assert 1E-9 > abs mx/determinant #(i32! 3x3 [1 2 3  4 5 6  7 8 9])   ;; singular
		--assert error? try [mx/determinant #(i32! 3x2 [1 2 3 4 5 6])]

	--test-- "determinant does not modify its argument"
		m: #(i32! 2x2 [1 2 3 4])
		mx/determinant m
		--assert m == #(i32! 2x2 [1 2 3 4])

	--test-- "invert"
		--assert all [
			i: mx/invert #(f64! 2x2 [4 7  2 6])
			near? i #(f64! 2x2 [0.6 -0.7  -0.2 0.4])
			i/shape = 2x2
		]
		;; A * inv(A) = I
		--assert all [
			m: #(f64! 3x3 [2 1 1  1 3 2  1 0 0])
			near? (mx/matmul m mx/invert m) (mx/identity copy m)
		]
		--assert error? try [mx/invert #(i32! 3x2 [1 2 3 4 5 6])]

	--test-- "invert rejects a singular matrix"
		--assert error? try [mx/invert #(f64! 3x3 [1 2 3  4 5 6  7 8 9])]
		--assert error? try [mx/invert #(f64! 2x2 [1 2  2 4])]
		--assert error? try [mx/invert #(f64! 2x2 [0 0  0 0])]

	--test-- "solve"
		;;  2x +  y =  5
		;;   x + 3y = 10
		--assert all [
			x: mx/solve #(f64! 2x2 [2 1  1 3]) #(f64! 1x2 [5 10])
			near? x #(f64! 1x2 [1.0 3.0])
			x/shape = 1x2
		]
		--assert error? try [mx/solve #(f64! 3x2 [1 2 3 4 5 6]) #(f64! 1x2 [1 2])]
		--assert error? try [mx/solve #(f64! 2x2 [1 2 3 4])     #(f64! 1x3 [1 2 3])]
===end-group===
] ;end of matrix module text

~~~end-file~~~
