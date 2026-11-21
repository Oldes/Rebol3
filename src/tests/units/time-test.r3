Rebol [
	Title:   "Rebol3 time test script"
	Author:  "Oldes, Peter W A Wood"
	File: 	 %time-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]



~~~start-file~~~ "TIME"

===start-group=== "construction syntax - time!"
	--test-- "#(time! time)"
		--assert 1:2:3 = load "#(time! 1:2:3)"
	--test-- "#(time! string)"
		--assert 1:2:3 = load {#(time! "1:2:3")}
		--assert error? try [load {#(time! "abc")}]          ;; Scan_Time fails
	--test-- "#(time! integer/decimal)"
		--assert 0:0:10  = load "#(time! 10)"
		--assert 0:0:1.5 = load "#(time! 1.5)"
	--test-- "#(time! block)"
		--assert 1:0:0    = load "#(time! [1])"
		--assert 1:2:0    = load "#(time! [1 2])"
		--assert 1:2:3    = load "#(time! [1 2 3])"
		--assert 1:2:3.5  = load "#(time! [1 2 3.5])"
		--assert -1:2:3   = load "#(time! [-1 2 3])"
	--test-- "#(time! invalid) - NO_TIME branch"
		--assert error? try [load "#(time! foo)"]            ;; unsupported type
		--assert error? try [load "#(time! [])"]             ;; empty block
		--assert error? try [load "#(time! [a])"]            ;; non-integer hours
		--assert error? try [load "#(time! [1 a])"]          ;; non-integer minutes
		--assert error? try [load "#(time! [1 -2])"]         ;; negative minutes
		--assert error? try [load "#(time! [1 2 -3])"]       ;; negative seconds
		--assert error? try [load "#(time! [1 2 a])"]        ;; bad seconds type
		--assert error? try [load "#(time! [1 2 3 4])"]      ;; block too long
===end-group===

===start-group=== "time"
	--test-- "round time"
	;@@ https://github.com/Oldes/Rebol-issues/issues/162
		--assert 1:30 = round/to 1:32:0 0:10
		--assert 2:00 = round/to 1:32:0 1:00
		--assert 0:00:01 = round/to 0:0:1.4 0:0:1
		--assert 12:35:23 = round/to 12:34:56 0:1:1
		--red-- --assert 12:35:23 = round/to 12:34:56 to integer! 0:1:1
		--red-- --assert 12:35:23 = round/to 12:34:56 to decimal! 0:1:1
		;Red also supports this (don't know why):
		; --assert 12:34:56.1 = round/to 12:34:56 0.3
		; --assert 12:34:56.1 = round/to 12:34:56 30%

	--test-- "time limits"
	;; Red language is storing time as 64bit decimal value, while in Rebol3 64bit integer is used,
	;; so Red has larger range, but is not so precise.
	;@@ https://github.com/Oldes/Rebol-issues/issues/277
		--assert -596523:14:08.999999999 = try [-1.0 + -596523:14:07.999999999]
	;@@ https://github.com/Oldes/Rebol-issues/issues/958
		--assert 1193046:28:15           = try [make time! (2 ** 32) - 1]
		--assert 1193046:28:15.010000384 = try [make time! (2 ** 32) - 0.99]
		--assert 1193046:28:14.989999616 = try [make time! (2 ** 32) - 1.01]
	;@@ https://github.com/Oldes/Rebol-issues/issues/1383
		--assert 2562047:47:14 = to time! 9223372034
		--assert 2562047:47:15 = to time! 9223372035
		--assert 2562047:47:16 = to time! 9223372036
		--assert  9223372036   = to integer! to time!  9223372036
		--assert -9223372036   = to integer! to time! -9223372036
		--assert  9223372036.0 = to decimal! to time!  9223372036.0
		--assert -9223372036.0 = to decimal! to time! -9223372036.0
		--assert all [error? e: try [to time!  9223372036.5] e/id = 'out-of-range]
		--assert all [error? e: try [to time! -9223372036.5] e/id = 'out-of-range]
		--assert all [error? e: try [0:0:0.1 + to-time 9223372036] e/id = 'type-limit]
	;@@ https://github.com/Oldes/Rebol-issues/issues/2464
		--assert 1.0e-9 = to decimal! 0:0:0.000000001
		--assert 0.0    = to decimal! 0:0:0.0000000001
		
	--test-- "compare with 0"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2463
		--assert 0 < 0:0:0.1
		--assert 0:0:0.1 > 0
		--assert 0.0 < 0:0:0.1
		--assert 0:0:0.1 > 0.0
		--assert 0% < 0:0:0.1
		--assert 0:0:0.1 > 0%

	--test-- "issue-90"
	;@@ https://github.com/Oldes/Rebol-issues/issues/90
		--assert -23:00 = (1:0:0 - 24:00)
		--assert 0:0 = subtract 1:0:0  1:0:0
		--assert -2:01:02 = (3:0:0 - 5:1:2)

	--test-- "issue-108"
	;@@ https://github.com/Oldes/Rebol-issues/issues/108
		--assert "0:00:00.0001"   = mold 0:0:0.0001
		--assert "0:00:00.001"    = mold 0:0:0.0001 * 10
		--assert "0:00:00.01"     = mold 0:0:0.0001 * 100
		--assert "0:00:00.1"      = mold 0:0:0.0001 * 1000
		--assert "0:00:01"        = mold 0:0:0.0001 * 10000
		--assert "0:00:00.01"     = mold 0:0:0.1 / 10
		--assert "0:00:00.001"    = mold 0:0:0.1 / 100
		--assert "0:00:00.0001"   = mold 0:0:0.1 / 1000
		--assert "0:00:00.00001"  = mold 0:0:0.1 / 10000
		--assert "0:00:00.000001" = mold 0:0:0.1 / 100000
		--assert "0:00:01.0001"   = mold 0:0:1.0001
		--assert "0:00:01.001"    = mold 0:0:1.001
		--assert "0:00:01.01"     = mold 0:0:1.01
		--assert "0:00:01.1"      = mold 0:0:1.1

	--test-- "issue-149"
	;@@ https://github.com/Oldes/Rebol-issues/issues/149
		--assert same?  5:01:40  100 + 5:00
		--assert same? -4:58:20  100 - 5:00
		--assert same? -4:58:20  100 + -5:00

	--test-- "issue-150"
	;@@ https://github.com/Oldes/Rebol-issues/issues/150
		--assert error? try [0:0:0 / 0] ;- no crash
		--assert error? try [0:0:0 % 0] ;- no crash

	--test-- "issue-263"
	;@@ https://github.com/Oldes/Rebol-issues/issues/263
		--assert time? t: try [load "596523:00"]
		--assert t = load "596522:60"

	--test-- "issue-289"
	;@@ https://github.com/Oldes/Rebol-issues/issues/289
		t: 225:00 --assert 224:59:59 = (t - 1)
		t: 226:00 --assert 225:59:59 = (t - 1)

	--test-- "issue-1032"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1032
		--assert -3723.0 = to-decimal make time! [-1 2 3]
		--assert -3723.4 = to-decimal make time! [-1 2 3.4]

	--test-- "issue-1033"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1033
		--assert "-1:02:03.45" = mold make time! [-1 2 3.45]

	--test-- "issue-1391"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1391
		--assert 5:0 = (50% * 10:0:0)
		--assert error? try [50% + 10:0]
		--assert error? try [50% - 10:0]
		--assert error? try [50% / 10:0]

	--test-- "issue-972"
	;@@ https://github.com/Oldes/Rebol-issues/issues/972
		--assert 0:00:00.123456789 = 0:00:00.1234567892
		--assert 0:00:00.12345679  = 0:00:00.1234567895
		--assert 0:00:00.123456789 = 0:00:00.123456789456

	--test-- "issue-1055"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1055
		--assert "[0:00:00.12345679 1:00:00.12345679]" = remold [make time! 0.1234567896 make time! [1 0 0.1234567896]]

	--test-- "divide by tuple"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1196
		--assert all [error? e: try [0:0:01 / 1.1.1] e/id = 'not-related]
		--assert all [error? e: try [1.1.1 / 0:0:01] e/id = 'not-related]

	--test-- "issue-1054"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1054
		--assert 596523:14:07 = to-time (to-integer #{8000 0000}) - 1
		--assert -596523:14:07 = to-time negate (to-integer #{8000 0000}) - 1
		--assert all [error? e: try [to-time 2 ** 63 / (10 ** 9) - 1 + 0.9999999] e/id = 'out-of-range]

	--test-- "time! money! math"
	;@@ https://github.com/Oldes/Rebol-issues/issues/2497
		--assert $7.5 = (1:30:0 * $5)
		--assert $25  = (4:0:0 / $100) ; an hourly rate
		--assert error? try [1:30:0 + $5]
		--assert error? try [1:30:0 - $5]

===end-group===

===start-group=== "random"
	--test-- "random time!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/911
		t: random 0:0:0.10
		--assert all [t > 0:0 t < 0:0:0.10]
		t: random 1:0:0
		--assert all [t > 0:0 t < 1:0:0]


===end-group===


===start-group=== "time! path access"
	--test-- "time/bad-select"
		t: 1:2:3
		--assert error? try [t/foo]                  ;; unknown word
		--assert error? try [t/("x")]                ;; unsupported select type
	--test-- "time/hour: minute: second:"
		t: 1:2:3
		t/hour: 5     --assert t = 5:02:03
		t/minute: 30  --assert t = 5:30:03
		t/second: 10  --assert t = 5:30:10
	--test-- "time/second: decimal"
		t: 1:2:3
		t/second: 1.5 --assert t = 1:02:01.5
	--test-- "time/minute: decimal (truncated by Int32s)"
		t: 1:2:3
		t/minute: 4.9 --assert t = 1:04:03
	--test-- "time/x: none"
		t: 1:2:3
		t/hour: none   --assert t = 0:02:03
		t/minute: none --assert t = 0:00:03
		t/second: none --assert t = 0:00:00
	--test-- "time/integer: value"
		t: 1:2:3
		t/1: 7  --assert t = 7:02:03
		t/2: 8  --assert t = 7:08:03
		t/3: 9  --assert t = 7:08:09
		--assert error? try [t/4: 1]               ;; PE_BAD_SELECT in set branch
	--test-- "time/x: invalid"
		t: 1:2:3
		--assert error? try [t/hour: "x"]          ;; PE_BAD_SET
		--assert error? try [t/hour: -1]           ;; Int32s min 0
		--assert error? try [t/second: -1.5]       ;; Trap_Range
		--assert t = 1:2:3
===end-group===


===start-group=== "time! actions"
	--test-- "time + time"
		--assert 3:00 = (1:00 + 2:00)
	--test-- "time / time"
		--assert 2.0 = (3:00 / 1:30)
		--assert error? try [1:00 / 0:00]
	--test-- "remainder time time"
		--assert 1:00 = remainder 3:00 2:00
		--assert error? try [remainder 1:00 0:00]
	--test-- "remainder time integer"
		--assert 0:00:01   = remainder 1:00:05 2
		--assert 0:00:00.5 = remainder 0:00:02.5 1
	--test-- "time * integer overflow"
		t: 1:00
		--assert error? try [loop 100 [t: t * 2]]
	--test-- "time - decimal"
		--assert 0:59:59.5 = (1:00 - 0.5)
	--test-- "time * decimal"
		--assert 1:30 = (1:00 * 1.5)
	--test-- "time / decimal"
		--assert 1:00 = (1:30 / 1.5)
		--assert error? try [1:00 / 0.0]
	--test-- "time + date"
		--assert 1-Jan-2000/1:00 = (1:00 + 1-Jan-2000)
	--test-- "odd? / even?"
		--assert odd?  1:00:01
		--assert even? 1:00:02
		--assert not even? 1:00:01
	--test-- "negate / absolute"
		--assert -1:00 = negate 1:00
		--assert  1:00 = absolute -1:00
		--assert  1:00 = absolute 1:00
	--test-- "round (no /to)"
		--assert 1:00:02 = round 1:00:01.5
	--test-- "round/to invalid scale"
		--assert error? try [round/to 1:00 $1]
	--test-- "random/seed time"
		--assert unset? random/seed 1:00
	--test-- "pick time"
		--assert 1 = pick 1:2:3 1
		--assert 3 = pick 1:2:3 3
	--test-- "unsupported unary action"
		--assert error? try [reflect 1:2:3 'spec]
===end-group===

;===start-group=== "time protect"
;	;@@ https://github.com/Oldes/Rebol-issues/issues/2416
;	--test-- "time protect 1"
;		t: now/time protect 't
;		--assert error? e: try [t/hour: 0]
;		--assert e/id = 'locked-word
;	--test-- "time protect 2"
;		protect/words o: object [t: now/time]
;		--assert error? e: try [o/t/hour: 0]
;		--assert e/id = 'locked-word
;	--test-- "time protect 3"
;		o: object [t: 1 protect/words o: object [t: now/time]]
;		--assert protected? 'o/o/t
;		--assert protected? 'o/o/t/hour        ;@@ <--- fails!
;		--assert error? e: try [o/o/t/hour: 0] ;@@ <--- fails!
;===end-group===

~~~end-file~~~
