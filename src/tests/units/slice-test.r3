Rebol [
	Title:   "Rebol slice series test script"
	Author:  "Oldes"
	File: 	 %slice-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "SLICE Series"

===start-group=== "Slice series"
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
		#{FF03} == swap slc next s2
		bin/1 == 255
	]
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

===end-group===




~~~end-file~~~