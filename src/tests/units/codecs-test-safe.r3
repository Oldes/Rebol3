Rebol [
	Title:   "Rebol codecs SAFE test script"
	Author:  "Oldes"
	File: 	 %codecs-test-safe.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
	type:    private
]

~~~start-file~~~ "Codecs SAFE"

if find codecs 'safe [
;- using environmental variable to avoid interactive password input using `ask`
temp: get-env "REBOL_SAFE_PASS"
set-env "REBOL_SAFE_PASS" "my-pass"
user: system/user/name ;; store existing user
===start-group=== "SAFE codec"		
	--test-- "Save/Load SAFE file"
		foreach data [
			#[key: "Hello"]
			43
			#{DEADBEEF}
			[key: "aaa" value: 12]
		][
			--assert equal? data load save %temp.safe data
			delete %temp.safe
		]
	--test-- "Set-user which does not exists"
		;@@ https://github.com/Oldes/Rebol-issues/issues/2547
		--assert not error? try [set-user not-existing-user]
	--test-- "Initialise new user"
		--assert not error? try [set-user/n/p temp-user "passw"]
		--assert system/user/name = @temp-user
		--assert 'file = try [exists? system/user/data/spec/ref]
		--assert "hello" = put system/user/data 'key "hello"  ;; store some data...
		--assert "hello" = user's key               ;; resolve the data
		--assert not error? try [su]                ;; release user
		--assert none? system/user/name
		--assert not error? try [set-user/p temp-user "passw"]
		--assert "hello" = user's key               ;; resolve the data
		--assert not error? try [su #(none)]        ;; release user using none value
		--assert none? system/user/name

		try [delete system/user/data/spec/ref]

===end-group===

===start-group=== "SAFE codec key cache"
	safe: system/codecs/safe
	--test-- "encode/cache fills the cache"
		cache: copy []
		bin1: safe/encode/key/iterations/cache #{0102} "pass" 1000 cache
		--assert 4 == length? cache
		--assert 1000 == cache/2
		--assert (copy/part skip bin1 14 16) == cache/1  ;; salt in header
	--test-- "encode/cache reuses salt and keys, but not IV"
		bin2: safe/encode/key/iterations/cache #{0102} "pass" 1000 cache
		--assert (copy/part skip bin1 14 16) == (copy/part skip bin2 14 16)
		--assert (copy/part skip bin1 30 16) <> (copy/part skip bin2 30 16)
	--test-- "cached keys take precedence over the password"
		bin3: safe/encode/key/iterations/cache #{0102} "other" 1000 cache
		--assert #{0102} == safe/decode/key bin3 "pass"
	--test-- "decode/cache fills cache and reuses it"
		dcache: copy []
		--assert #{0102} == safe/decode/key/cache bin1 "pass" dcache
		--assert equal? cache dcache
		--assert #{0102} == safe/decode/key/cache bin2 "wrong" dcache
	--test-- "wrong password does not poison the cache"
		c: copy []
		--assert none? safe/decode/key/cache bin1 "wrong" c
		--assert empty? c
	--test-- "lower cached iteration count is upgraded on encode"
		c: copy cache
		bin4: safe/encode/key/cache #{0102} "pass" c
		--assert c/2 == safe/default-iterations
		--assert c/1 <> cache/1
		--assert #{0102} == safe/decode/key bin4 "pass"
===end-group===

===start-group=== "SAFE scheme file lock"
	dir:  %tmp-safe-lock/
	make-dir dir
	file: join dir %lock-test.safe
	delete file
	spec: [scheme: 'safe pass: "x" path: :dir target: %lock-test]
	wait-old: system/schemes/safe/lock-wait
	system/schemes/safe/lock-wait: 0:00:00.2
	--test-- "save and reload"
		p: open spec
		put p 'a 1
		update p
		close p
		p: open spec
		--assert 1 == select p 'a
		close p
	--test-- "no helper files are created"
		--assert [%lock-test.safe] = read dir
	--test-- "open fails while the file is exclusively locked"
		lck: open/read/write/lock file
		--assert error? try [open spec]
		close lck
	--test-- "open works while another reader holds a shared lock"
		lck: open/read/lock file
		--assert not error? try [p: open spec]
		--assert 1 == select p 'a
	--test-- "save fails while the file is locked, works after release"
		put p 'b 2
		--assert error? try [update p]
		close lck
		--assert not error? try [update p]
		close p
		p: open spec
		--assert 2 == select p 'b
		close p
	--test-- "shorter save truncates the file"
		p: open spec
		put p 'big append/dup copy "" "x" 10000
		update p
		size1: size? file
		remove/key p 'big
		update p
		--assert size1 > size? file
		close p
		p: open spec
		--assert none? select p 'big
		close p
	system/schemes/safe/lock-wait: wait-old
	delete file
	delete dir
===end-group===

===start-group=== "SAFE scheme conflict detection"
	dir:  %tmp-safe-conflict/
	make-dir dir
	file: join dir %conflict.safe
	delete file
	spec: [scheme: 'safe pass: "x" path: :dir target: %conflict]
	--test-- "unchanged port reloads data saved by another port"
		p1: open spec
		p2: open spec
		put p1 'a 1
		update p1
		wait 0.05
		update p2
		--assert 1 == select p2 'a
	--test-- "conflicting local changes are not written"
		put p1 'b 2
		update p1
		wait 0.05
		put p2 'c 3
		update p2                  ;; prints the conflict warning
		--assert 3 == select p2 'c ;; local change is kept in the port
		p3: open spec
		--assert 2 == select p3 'b
		--assert none? select p3 'c
		close p3
	--test-- "port without conflict still saves"
		wait 0.05
		put p1 'd 4
		update p1
		p3: open spec
		--assert 4 == select p3 'd
		close p3
		close p2                   ;; prints the warning again, changes dropped
		close p1
	delete file
	delete dir
===end-group===

===start-group=== "SAFE scheme wrong password"
	dir:  %tmp-safe-pass/
	make-dir dir
	file: join dir %pass-test.safe
	delete file
	spec: [scheme: 'safe pass: "x" path: :dir target: %pass-test]
	--test-- "open with a wrong password fails"
		p: open spec
		put p 'a 1
		close p
		--assert all [
			error? e: try [open [scheme: 'safe pass: "wrong" path: :dir target: %pass-test]]
			e/id = 'cannot-open
		]
	--test-- "the correct password still works"
		p: open spec
		--assert 1 == select p 'a
	--test-- "failed reload keeps the loaded data"
		wait 0.05
		;; another writer re-encrypts the file with a different password
		write file system/codecs/safe/encode/key/iterations #[a: 2] "other" 1000
		update p                  ;; prints the decrypt warning
		--assert open? p
		--assert 1 == select p 'a
		close p
	delete file
	delete dir
===end-group===


===start-group=== "SAFE scheme modify"
	dir:  %tmp-safe-modify/
	make-dir dir
	file: join dir %modify-test.safe
	delete file
	spec-x: [scheme: 'safe pass: "x" path: :dir target: %modify-test]
	spec-y: [scheme: 'safe pass: "y" path: :dir target: %modify-test]
	--test-- "modify 'password with [old new]"
		p: open spec-x
		put p 'a 1
		--assert modify p 'password ["x" "y"]
		close p
		--assert error? try [open spec-x]
		p: open spec-y
		--assert 1 == select p 'a
	--test-- "modify 'password with a wrong old password"
		--assert not modify p 'password ["wrong" "z"]
		--assert error? try [modify p 'password ["y"]]
		close p
	--test-- "modify 'reload drops local changes"
		p1: open spec-y
		p2: open spec-y
		put p1 'b 2
		update p1
		wait 0.05
		put p2 'c 3
		update p2                    ;; prints the conflict warning
		--assert modify p2 'reload true
		--assert 2 == select p2 'b
		--assert none? select p2 'c
	--test-- "modify 'overwrite forces local changes"
		wait 0.05
		put p1 'd 4
		update p1
		wait 0.05
		put p2 'e 5
		update p2                    ;; prints the conflict warning
		--assert modify p2 'overwrite true
		p3: open spec-y
		--assert 5 == select p3 'e
		--assert none? select p3 'd
		close p3
		close p2
		close p1
	delete file
	delete dir
===end-group===

===start-group=== "SAFE serializable values"
	dir:  %tmp-safe-values/
	make-dir dir
	file: join dir %values-test.safe
	delete file
	spec: [scheme: 'safe pass: "x" path: :dir target: %values-test]
	--test-- "storing an unserializable value fails"
		p: open spec
		put p 'a 1
		--assert all [
			error? e: try [put p 'f :print]
			e/id = 'invalid-arg
		]
		--assert none? select p 'f
	--test-- "storing a circular value fails"
		b: [1]
		append/only b b
		--assert error? try [put p 'c b]
		--assert none? select p 'c
	--test-- "serializable values are stored and survive reopening"
		--assert not error? try [put p 'b [1 "x" #[k: 2.5] 1-Jan-2026]]
		close p
		p: open spec
		--assert 1 == select p 'a
		--assert [1 "x" #[k: 2.5] 1-Jan-2026] = select p 'b
		close p
	--test-- "codec refuses unserializable data"
		--assert error? try [system/codecs/safe/encode/key/iterations reduce [1 :print] "x" 1000]
		--assert binary? system/codecs/safe/encode/key/iterations [1 "x"] "x" 1000
	delete file
	delete dir
===end-group===


set-env "REBOL_SAFE_PASS" :temp
set-user :user
]

~~~end-file~~~