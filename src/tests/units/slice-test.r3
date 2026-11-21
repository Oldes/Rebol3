Rebol [
	Title:   "Rebol slice series test script"
	Author:  "Oldes"
	File: 	 %slice-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "SLICE Series"

===start-group=== "Slice binary"
--test-- "slice binary"
	--assert all [
		bin: #{010203040506}
		slc: slice bin 3
		3 == length? slc
		slc == #{010203}
		slc/2: 255
		bin/2 == 255
		#{03FF01} == reverse slc
		bin == #{03FF01040506}
		1 == pick slc 3
		none? pick slc 4
		s2: slice slc 2
		2 == length? s2
		s2 == #{03FF}
		#{FF0301} == swap slc next s2
		bin/1 == 255
	]
--test-- "traverse binary slice"
	--assert all [
		bin: #{010203040506FF}
		slc: slice at bin 4 3
		slc == #{040506}
		sum: 0
		foreach v slc [sum: sum + v]
		sum == 15
	]
	--assert all [
		sum: 0
		foreach [a b] slc [sum: sum + a + any [b 0]]
		sum == 15
	]
	--assert all [
		sum: 0
		forall slc [sum: sum + slc/1]
		sum == 15
	]
	--assert all [
		sum: 0
		forskip slc 2 [sum: sum + slc/1]
		sum == 10
	]
	--assert all [
		sum: 0
		while [not tail? slc][sum: sum + first ++ slc]
		sum == 15
		tail? slc
		head? slc: head slc
	]
===end-group===


===start-group=== "Slice string"
--test-- "slice string"
	--assert all [
		str: "123456"
		slc: slice str 3
		3 == length? slc
		slc == "123"
		slc/2: #"x"
		str/2 ==  #"x"
		"3x1" == reverse slc
		str == "3x1456"
		#"1" == pick slc 3
		none? pick slc 4
		s2: slice slc 2
		2 == length? s2
		s2 == "3x"
		"x31" == swap slc next s2
		str/1 == #"x"
	]
--test-- "traverse string slice"
	--assert all [
		str: "123456x"
		slc: slice at str 4 3
		slc == "456"
		sum: 0
		foreach v slc [sum: sum + v]
		sum == 159
	]
	--assert all [
		sum: 0
		foreach [a b] slc [sum: sum + a + any [b 0]]
		sum == 159
	]
	--assert all [
		sum: 0
		forall slc [sum: sum + slc/1]
		sum == 159
	]
	--assert all [
		sum: 0
		forskip slc 2 [sum: sum + slc/1]
		sum == 106
	]
	--assert all [
		sum: 0
		while [not tail? slc][sum: sum + first ++ slc]
		sum == 159
		tail? slc
		head? slc: head slc
	]
--test-- "parse string capture"
	str: "123456"
	--assert all [
		parse str ["12" capture slc 3 skip "6"]
		slc == "345"
	]
--test-- "slice string protection"
	str: "abcde"
	slc: slice at str 3 2
	--assert all [error? e: try [append str 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [insert str 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [ clear str]    e/id = 'fixed-sized-series]

	--assert all [error? e: try [append slc 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [insert slc 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [ clear slc]    e/id = 'fixed-sized-series]

	;; change is allowed when it doesn't change size
	--assert all [change slc 'y    slc = "yd"  str = "abyde"]
	--assert all [change slc "xy"  slc = "xy"  str = "abxye"]

	--assert all [change str "12"   str = "12xye" slc = "xy"]
	--assert all [change str "123"  str = "123ye" slc = "3y"]
	--assert all [change str "1234" str = "1234e" slc = "34"]

	--assert all [error? e: try [change/part slc 'x 2] e/id = 'fixed-sized-series]
	--assert all [error? e: try [change str "123456"   e/id = 'fixed-sized-series]]



===end-group===

===start-group=== "Slice vector"
--test-- "slice vector"
	--assert all [
		vec: #(uint8! #{010203040506})
		slc: slice vec 3
		3 == length? slc
		slc == #(uint8! [1 2 3])
		slc/2: 255
		vec/2 == 255
		#{01FF03} == to binary! slc
		reverse slc
		#{03FF01} == to binary! slc 
		1 == pick slc 3
		none? pick slc 4
		s2: slice slc 2
		s2 == #(uint8! [3 255])
		2 == length? s2
		s2/1: 33
		vec/1 == 33
	]
--test-- "traverse vector slice"
	--assert all [
		vec: #(uint8! #{010203040506FF})
		slc: slice at vec 4 3
		slc == #(uint8! [4 5 6])
		slc/sum == 15
		sum: 0
		foreach v slc [sum: sum + v]
		sum == 15
	]
	--assert all [
		sum: 0
		foreach [a b] slc [sum: sum + a + any [b 0]]
		sum == 15
	]
	--assert all [
		sum: 0
		forall slc [sum: sum + slc/1]
		sum == 15
	]
	--assert all [
		sum: 0
		forskip slc 2 [sum: sum + slc/1]
		sum == 10
	]
	--assert all [
		sum: 0
		while [not tail? slc][sum: sum + first ++ slc]
		sum == 15
		tail? slc
		head? slc: head slc
	]
--test-- "map-each vector slice"
	--assert [40 50 60] == map-each w slc [w * 10]

===end-group===

===start-group=== "Slice block"
--test-- "slice block"
	--assert all [
		blk: [1 2 3 4 5 6]
		slc: slice blk 3
		3 == length? slc
		slc == [1 2 3]
		slc/2: 0
		blk/2 == 0
		slc == [1 0 3]
		reverse slc
		slc == [3 0 1]
		1 == pick slc 3
		none? pick slc 4
		s2: slice slc 2
		s2 == [3 0]
		2 == length? s2
		s2/1: 9
		blk/1 == 9
	]
--test-- "traverse block slice"
	--assert all [
		blk: [1 2 3 4 5 6 255]
		slc: slice at blk 4 3
		slc == [4 5 6]
		sum: 0
		foreach v slc [sum: sum + v]
		sum == 15
	]
	--assert all [
		sum: 0
		foreach [a b] slc [sum: sum + a + any [b 0]]
		sum == 15
	]
	--assert all [
		sum: 0
		forall slc [sum: sum + slc/1]
		sum == 15
	]
	--assert all [
		sum: 0
		forskip slc 2 [sum: sum + slc/1]
		sum == 10
	]
	--assert all [
		sum: 0
		while [not tail? slc][sum: sum + first ++ slc]
		sum == 15
		tail? slc
		head? slc: head slc
	]
--test-- "map-each block slice"
	--assert [40 50 60] == map-each w slc [w * 10]

--test-- "slice block protection"
	blk: [a b c d e]
	slc: slice at blk 3 2
	--assert all [error? e: try [append blk 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [insert blk 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [ clear blk]    e/id = 'fixed-sized-series]

	--assert all [error? e: try [append slc 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [insert slc 'x] e/id = 'fixed-sized-series]
	--assert all [error? e: try [ clear slc]    e/id = 'fixed-sized-series]

	;; change is allowed when it doesn't change size
	--assert all [change slc 'y     slc = [y d]  blk = [a b y d e]]
	--assert all [change slc [x y]  slc = [x y]  blk = [a b x y e]]

	--assert all [change blk [1 2]     blk = [1 2 x y e] slc = [x y]]
	--assert all [change blk [1 2 3]   blk = [1 2 3 y e] slc = [3 y]]
	--assert all [change blk [1 2 3 4] blk = [1 2 3 4 e] slc = [3 4]]

	--assert all [error? e: try [change/part slc 'x 2] e/id = 'fixed-sized-series]
	--assert all [error? e: try [change blk [1 2 3 4 5 6] e/id = 'fixed-sized-series]]

===end-group===




~~~end-file~~~