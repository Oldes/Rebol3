Rebol [title: "Map benchmark"]

bench: function [label keys misses][
	recycle
	recycle/off
	mem: stats
	t-put: dt [m: make map! [] foreach k keys [put m k 1]]
	mem: stats - mem
	t-hit:  dt [loop 10 [foreach k keys   [select m k]]]
	t-miss: dt [loop 10 [foreach k misses [select m k]]]
	recycle/on
	print [label length? keys "put:" t-put "hit:" t-hit "miss:" t-miss "mem:" mem]
]

foreach n [1000 100000 1000000][
	ints: make block! n  imiss: make block! n
	strs: make block! n  smiss: make block! n
	repeat i n [
		append ints  i
		append imiss i + n
		append strs  ajoin ["key" i]
		append smiss ajoin ["nokey" i]
	]
	bench "integer" ints imiss
	bench "string " strs smiss
]