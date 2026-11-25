Rebol [
	Title:   "Runs all available unit tests"
	Author:  ["Peter W A Wood" "Oldes"]
	File:    %run-tests.r3
	Needs:   [%quick-test-module.r3 %fuzz-module.r3]
]
? system/build

;- check speed of available checksum methods
;do %test-checksums.r3

change-dir %units/

;- run unit tests..
dt [ ;- delta time
	***start-run*** "All tests"
	;recycle/torture
	failed-units: copy []
	foreach file [
		%bbcode-test.r3
		%bincode-test.r3
		%bitset-test.r3
		%call-test.r3
		%char-test.r3
		%codecs-test.r3
		%codecs-test-pdf.r3
		%compare-test.r3
		%compress-test.r3
		%conditional-test.r3
		%copy-test.r3
		%crash-test.r3
		%csv-test.r3
		%datatype-test.r3
		%date-test.r3
		%decimal-test.r3
		%delect-test.r3
		%enbase-test.r3
		%enum-test.r3
		%error-test.r3
		%evaluation-test.r3
		%event-test.r3
		%handle-test.r3
		%hash-test.r3
		%file-test.r3
		%format-test.r3
		%func-test.r3
		%image-test.r3
		%integer-test.r3
		%issue-test.r3
		%lexer-test.r3
		%load-test.r3
		%make-test.r3
		%map-test.r3
		%module-test.r3
		%mold-test.r3
		%money-test.r3
		%native-test.r3
		%object-test.r3
		%os-test.r3
		%pair-test.r3
		%parse-test.r3
		%percent-test.r3
		%port-test.r3
		%port-clipboard-test.r3
		%port-console-test.r3
		%port-net-test.r3
		%port-serial-test.r3
	;@@	%port-http-test.r3 ;; temporary disabled, because httpbin.org server has serious connection issues!
		%power-test.r3
		%protect-test.r3
		%rebin-test.r3
		%repl-test.r3
		%series-test.r3
		%slice-test.r3
		%stack-test.r3
		%struct-test.r3
		%task-test.r3
		%time-test.r3
		%tuple-test.r3
		%typeset-test.r3
		%unicode-test.r3
		%url-test.r3
		%vector-test.r3
		%word-test.r3
		%thru-cache-test.r3
		;- cryptography tests:
		%aes-test.r3
		%chacha20-test.r3
		%checksum-test.r3
		%dh-test.r3
		%crypt-test.r3
		%crypt-port-test.r3
		%crypt-port-camelia-test.r3
		%crypt-port-ccm-test.r3
		%crypt-port-gcm-test.r3
		%poly1305-test.r3
		%rc4-test.r3
		%rsa-test.r3
		;- known issues - waiting for fix
		;%_known-issues_.r3
		;%xx-test.r3
		;%_test.r3
	][
		try/with [do file] func[error][
			repend failed-units [file error]
		]
	]


	recycle/torture
	recycle
	stats/show

	print "System profiler:"
	print dump-obj stats/profile

	print "------------------------------------------------------------"
	prin "Checking memory...   " evoke 1 print "OK"
	prin "Checking bindings... " evoke 2 print "OK"
	print "------------------------------------------------------------"
	
	***end-run***
	recycle/on

	unless empty? failed-units [
		print "------------------------------------------------------------"
		print as-red "FAILED units files:"
		foreach [file error] failed-units [
			print [as-yellow mold file mold/part error 500] 
		]
		quit/return -2 ;; Test should not pass when there is any error in the test files.
	]


]
