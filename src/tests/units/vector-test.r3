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
	--assert all [error? e: try [to vector! []] e/id = 'bad-make-arg]
	
--test-- "VECTOR can be initialized using a block with CHARs"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2348
	--assert vector? v: make vector! [integer! 8 [#"^(00)" #"^(01)" #"^(02)" #"a" #"b"]]
	--assert  0 = v/1
	--assert 98 = v/5

	--assert vector? v: make vector! [integer! 16 [#"^(00)" #"^(01)" #"^(02)" #"a" #"b"]]
	--assert  0 = v/1
	--assert 98 = v/5

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
	--assert 1 = length? v: #(i16! 1 #{01000200})
	--assert none? v/2

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
	--assert "#(vector! integer! 32 2 [0 0])" = mold/all make vector! [signed integer! 32 2]
	--assert "#(vector! unsigned integer! 32 2 [0 0])" = mold/all make vector! [unsigned integer! 32 2]

--test-- "MOLD/flat on vector"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2349
	--assert (mold/flat make vector! [integer! 8 12]) = {#(int8! [0 0 0 0 0 0 0 0 0 0 0 0])}
	--assert (mold/all/flat make vector! [integer! 8 12]) = "#(vector! integer! 8 12 [0 0 0 0 0 0 0 0 0 0 0 0])"
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
	--assert [signed type size length minimum maximum range sum mean median variance population-deviation sample-deviation] = query v none
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
	--assert [unsigned integer! 16 2] = reflect v 'spec
	--assert [unsigned integer! 16 2] = spec-of v
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

===start-group=== "VECTOR compact construction"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2396
	--test-- "Compact construction syntax (empty)"
		--assert (mold #(i8! ))  == "#(int8! [])"
		--assert (mold #(i16!))  == "#(int16! [])"
		--assert (mold #(i32!))  == "#(int32! [])"
		--assert (mold #(i64!))  == "#(int64! [])"
		--assert (mold #(u8! ))  == "#(uint8! [])"
		--assert (mold #(u16!))  == "#(uint16! [])"
		--assert (mold #(u32!))  == "#(uint32! [])"
		--assert (mold #(u64!))  == "#(uint64! [])"
		--assert (mold #(f32! )) == "#(float32! [])"
		--assert (mold #(f64! )) == "#(float64! [])"

	--test-- "Compact construction syntax (size)"
		--assert (mold #(i8!  3)) == "#(int8! [0 0 0])"
		--assert (mold #(i16! 3)) == "#(int16! [0 0 0])"
		--assert (mold #(i32! 3)) == "#(int32! [0 0 0])"
		--assert (mold #(i64! 3)) == "#(int64! [0 0 0])"
		--assert (mold #(u8!  3)) == "#(uint8! [0 0 0])"
		--assert (mold #(u16! 3)) == "#(uint16! [0 0 0])"
		--assert (mold #(u32! 3)) == "#(uint32! [0 0 0])"
		--assert (mold #(u64! 3)) == "#(uint64! [0 0 0])"
		--assert (mold #(f32! 3)) == "#(float32! [0.0 0.0 0.0])"
		--assert (mold #(f64! 3)) == "#(float64! [0.0 0.0 0.0])"

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
	--assert (v + 200) = #(u8![201 202 203 204])
	; the values are truncated on overflow:
	--assert (v + 200) = #(u8![145 146 147 148])
	--assert (v - 400) = #(u8![1 2 3 4])
	subtract (add v 10) 10
	--assert v = #(u8![1 2 3 4])
	1 + v
	--assert v = #(u8![2 3 4 5])
	-1.0 + v
	--assert v = #(u8![1 2 3 4])

	v: #(i8![1 2 3 4])
	--assert (v + 125) = #(i8![126 127 -128 -127])
	--assert (v - 125) = #(i8![1 2 3 4])

--test-- "VECTOR 8bit integer multiply"
	v: #(u8![1 2 3 4])
	--assert (v * 4) = #(u8![4 8 12 16])
	; the values are truncated on overflow:
	--assert (v * 20) = #(u8![80 160 240 64]) ;64 = (16 * 20) - 256

	v: #(i8![1 2 3 4])
	--assert (v * 2.0) = #(i8![2 4 6 8])
	; the decimal is first converted to integer (2):
	--assert (v * 2.4) = #(i8![4 8 12 16])
	subtract (add v 10) 10
	--assert v = #(i8![4 8 12 16])

--test-- "VECTOR 16bit integer multiply"
	v: #(u16![1 2 3 4])
	--assert (v * 4)  = #(u16![4 8 12 16])
	--assert (v * 20) = #(u16![80 160 240 320])
	multiply v 2
	--assert v = #(u16![160 320 480 640])

	v: #(u16![1 2 3 4])
	--assert (10   * copy v) = #(u16![10 20 30 40])
	--assert (10.0 * copy v) = #(u16![10 20 30 40])

	; the values are truncated on overflow:
	v: #(u16![1 2 3 4])
	--assert (v * 10000) = #(u16![10000 20000 30000 40000])
	--assert (v * 10.0)  = #(u16![34464 3392 37856 6784])

--test-- "VECTOR 16bit integer divide"
	v: #(u16![80 160 240 320])
	v / 20 / 2
	divide v 2
	--assert v = #(u16![1 2 3 4])
	--assert error? try [10 / v]
	--assert error? try [ v / 0] 

--test-- "VECTOR 32bit decimal add/subtract"
	v: #(f32![1 2 3 4])
	--assert (v + 200) = #(f32![201 202 203 204])
	--assert (v + 0.5) = #(f32![201.5 202.5 203.5 204.5])
	; notice the precision lost with 32bit decimal value:
	v - 0.1
	--assert 2013 = to integer! 10 * v/1 ; result is not 201.4 as would be with 64bit

--test-- "VECTOR 64bit decimal add/subtract"
	v: #(f64![1 2 3 4])
	--assert (v + 200) = #(f64![201 202 203 204])
	--assert (v + 0.5) = #(f64![201.5 202.5 203.5 204.5])
	--assert (v - 0.1) = #(f64![201.4 202.4 203.4 204.4])

--test-- "VECTOR 64bit decimal multiply/divide"
	v: #(f64![1 2 3 4])
	--assert (v * 20.5) = #(f64![20.5 41.0 61.5 82.0])
	--assert (v / 20.5) = #(f64![1.0 2.0 3.0 4.0])

--test-- "VECTOR math operation with vector not at head"
	v: #(i8![1 2 3 4])
	--assert (2 + skip v 2) = #(i8![5 6])
	--assert v = #(i8![1 2 5 6])

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
	all-modes: [minimum maximum range sum mean median variance population-deviation sample-deviation]
	all-get-modes: [:minimum :maximum :range :sum :mean :median :variance :population-deviation :sample-deviation]
	--test-- "int8! vector statictics"
	v: #(int8! [-2 -1 1 2 4])
	--assert (query v all-modes) == [
	    minimum: -2
	    maximum: 4
	    range: 6
	    sum: 4
	    mean: 0.8
	    median: 1.0
	    variance: 22.8
	    population-deviation: 2.13541565040626
	    sample-deviation: 2.38746727726266
	]

	--assert (query v all-get-modes) == [
	    -2
	    4
	    6
	    4
	    0.8
	    1.0
	    22.8
	    2.13541565040626
	    2.38746727726266
	]

	--test-- "uint64! vector statictics"
	v: #(uint64! [4 9 11 12 17])
	--assert (query v all-modes) == [
	    minimum: 4
	    maximum: 17
	    range: 13
	    sum: 53
	    mean: 10.6
	    median: 11.0
	    variance: 89.2
	    population-deviation: 4.22374241638857
	    sample-deviation: 4.72228758124704
	]

	--assert (query v all-get-modes) == [
	    4
	    17
	    13
	    53
	    10.6
	    11.0
	    89.2
	    4.22374241638857
	    4.72228758124704
	]

	--test-- "float64! vector statictics"
	v: #(float64! [1.62 1.72 1.64 1.7 1.78 1.64 1.65 1.64 1.66 1.74])
	--assert (query v all-modes) == [
	    minimum: 1.62
	    maximum: 1.78
	    range: 0.16
	    sum: 16.79
	    mean: 1.679
	    median: 1.655
	    variance: 0.02529
	    population-deviation: 0.0502891638427207
	    sample-deviation: 0.0530094331227943
	]

	--assert (query v all-get-modes) == [
	    1.62
	    1.78
	    0.16
	    16.79
	    1.679
	    1.655
	    0.02529
	    0.0502891638427207
	    0.0530094331227943
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
	--test-- "SORT/skip vector!"
		--assert  all [
			error? e: try [sort/skip #(i8!  [2 4 1 3]) 2]
			e/id = 'feature-na
		]
	--test-- "SORT/compare vector!"
		--assert  all [
			error? e: try [sort/compare #(i8!  [2 4 1 3]) func[a b][a < b]]
			e/id = 'feature-na
		]
===end-group===

~~~end-file~~~
