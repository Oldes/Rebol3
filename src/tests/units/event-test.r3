Rebol [
	Title:   "Rebol3 event test script"
	Author:  "Oldes"
	File: 	 %event-test.r3
	Tabs:	 4
	Needs:   [%../quick-test-module.r3]
]

~~~start-file~~~ "EVENT"

===start-group=== "event"

--test-- "make event!"
	;@@ https://github.com/Oldes/Rebol-issues/issues/132
	--assert  event? e: make event! [type: 'connect]
	--assert  'connect = e/type
	;@@ https://github.com/Oldes/Rebol-issues/issues/2484
	--assert all [event? e2: make e [offset: 0x0] e2/offset = 0x0]

--test-- "event! offset out of range"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1635
	--assert all [
		error? e: try [make event! [offset: 32768x0]]
		e/id = 'out-of-range
	]
	--assert all [
		error? e: try [make event! [offset: 65536x1]]
		e/id = 'out-of-range
	]
	--assert all [
		event? e: try [make event! [offset: 1x2]]
		e/offset = 1x2
	]
--test-- "key event"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1526
	--assert all [
		event? e: try [make event! [type: 'key key: #"A"]]
		e/key == #"A"
		none? e/offset
	]
	--assert all [
		event? e: try [make event! [key: #"B"]]
		e/key == #"B"
		e/type = 'key ; added automaticaly if no type is specified
	]
	--assert all [
		event? e: try [make event! [type: 'custom key: #"C"]]
		none? e/key ; only key and key-up types will provide it
		e/type = 'custom
		e/code = 67
	]
	--assert all [
		event? e: try [make event! [type: 'key-up key: #"C"]]
		e/key == #"C"
		e/type = 'key-up
		e/code = 67
	]

--test-- "key event with a codepoint above the BMP"
	;; A character key uses the whole 32 bit data field - MAX_CHAR needs 21
	;; bits, so the code must not be packed into 16.
	--assert all [
		event? e: try [make event! [key: #"^(1F600)"]]
		e/key == #"^(1F600)"
		e/code = to integer! #"^(1F600)"
	]

--test-- "named key event"
	;; A named key is stored as a 1-based index into system/catalog/event-keys.
	;; It used to be shifted into the high 16 bits while the getter read the
	;; data field raw, so reading it back gave none.
	--assert all [
		event? e: try [make event! [type: 'named-key key: 'home]]
		e/key = 'home
		e/type = 'named-key
	]
	;; The type is added automatically like it is for a character key, but it
	;; must be one which decodes a NAME, not a character.
	--assert all [
		event? e: try [make event! [key: 'home]]
		e/key = 'home
		e/type = 'named-key
	]
	;; A lit-word is accepted the same way.
	--assert all [
		event? e: try [make event! [key: 'end]]
		e/key = 'end
	]
	;; Both ends of the catalog - an off-by-one shows up only here.
	--assert all [
		event? e: try [make event! [type: 'named-key key: 'page-up]]
		e/key = 'page-up
	]
	--assert all [
		event? e: try [make event! [type: 'named-key key: 'begin]]
		e/key = 'begin
	]
	;; The stored code is the key's 1-based position in the catalog.
	--assert all [
		event? e: try [make event! [type: 'named-key key: 'home]]
		e/code = index? find system/catalog/event-keys 'home
	]
	;; With no key set the code is 0, which is not a valid position - it must
	;; read as none instead of the value stored before the block's head.
	--assert all [
		event? e: try [make event! [type: 'named-key]]
		none? e/key
	]
	--assert all [
		event? e: try [make event! [type: 'named-key-up]]
		none? e/key
	]
	;; Only the named key types decode it, like with a character key.
	--assert all [
		event? e: try [make event! [type: 'custom key: 'home]]
		none? e/key
		e/type = 'custom
		e/code = index? find system/catalog/event-keys 'home
	]
	;; An unknown key word is refused, not stored as some other key.
	--assert all [
		error? e: try [make event! [key: 'not-a-key]]
		e/id = 'no-event-key
	]

--test-- "key event does not leave a stale offset"
	;; Both branches write the same data field, so setting a key must clear
	;; the flag which says that field holds a packed XY.
	--assert all [
		event? e: try [make event! [offset: 1x1 key: 'home]]
		none? e/offset
		e/key = 'home
	]
	--assert all [
		event? e: try [make event! [offset: 1x1 key: #"A"]]
		none? e/offset
		e/key == #"A"
	]

--test-- "key event type coercion"
	;; Setting a key makes the event a key event of the matching kind, so the
	;; stored data is decodable - otherwise e/key would read as none.
	--assert all [
		event? e: try [make event! [type: 'down key: 'home]]
		e/type = 'named-key
		e/key = 'home
	]
	--assert all [
		event? e: try [make event! [type: 'down key: #"A"]]
		e/type = 'key
		e/key == #"A"
	]
	;; ...unless the type already decodes that kind of key...
	--assert all [
		event? e: try [make event! [type: 'key-up key: #"A"]]
		e/type = 'key-up
		e/key == #"A"
	]
	--assert all [
		event? e: try [make event! [type: 'named-key-up key: 'home]]
		e/type = 'named-key-up
		e/key = 'home
	]
	;; ...or the type says these fields are the caller's own. `custom` and
	;; the extension range (192 and up) both mean that.
	--assert all [
		event? e: try [make event! [type: 'custom key: 'home]]
		e/type = 'custom
		none? e/key
		e/code = index? find system/catalog/event-keys 'home
	]
	--assert all [
		event? e: try [make event! [type: 200 key: 'home]]
		e/type = 200
		none? e/key
		e/code = index? find system/catalog/event-keys 'home
	]

--test-- "event type must not contradict the stored key"
	;; The data field is decoded by the type, so relabelling a named key as a
	;; character key (or the other way round) would read the catalog position
	;; as a codepoint. Refused rather than silently misread.
	--assert error? try [make event! [key: 'home type: 'key]]
	--assert error? try [make event! [key: 'home type: 'key-up]]
	--assert error? try [make event! [key: #"A" type: 'named-key]]
	--assert error? try [make event! [key: #"A" type: 'named-key-up]]
	;; Switching within the same kind is fine.
	--assert all [
		event? e: try [make event! [key: 'home type: 'named-key-up]]
		e/type = 'named-key-up
		e/key = 'home
	]
	--assert all [
		event? e: try [make event! [key: #"A" type: 'key-up]]
		e/type = 'key-up
		e/key == #"A"
	]
	;; And relabelling to a type which claims the fields is allowed.
	--assert all [
		event? e: try [make event! [key: #"A" type: 'custom]]
		e/type = 'custom
		e/code = 65
	]

--test-- "custom event"
	;@@ https://github.com/Oldes/Rebol-issues/issues/1821
	--assert all [
		event? e: try [make event! [type: 'custom code: 1]]
		e/type = 'custom
		e/code = 1
	]
	if system/ports/event [
		; using port in the custom event
		--assert all [
			event? e: try [make event! [type: 'custom code: 2 port: system/ports/event]]
			e/type = 'custom
			e/code = 2
			e/port = system/ports/event
		]
	]

===end-group===


===start-group=== "event types"

--test-- "event type groups"
	;; The type code indexes system/catalog/event-types, so a miscounted
	;; reserved run would silently shift every type after it. These are the
	;; group bases the catalog is laid out around.
	--assert 0   = indexz? find system/catalog/event-types 'ignore
	--assert 32  = indexz? find system/catalog/event-types 'open
	--assert 64  = indexz? find system/catalog/event-types 'show
	--assert 96  = indexz? find system/catalog/event-types 'move
	--assert 128 = indexz? find system/catalog/event-types 'key
	--assert 160 = indexz? find system/catalog/event-types 'change
	--assert 192 = length? system/catalog/event-types

--test-- "named event types"
	--assert all [event? e: try [make event! [type: 'shutdown]]    'shutdown    = e/type]
	--assert all [event? e: try [make event! [type: 'pending]]     'pending     = e/type]
	--assert all [event? e: try [make event! [type: 'close-request]] 'close-request = e/type]
	--assert all [event? e: try [make event! [type: 'down]]        'down        = e/type]
	--assert all [event? e: try [make event! [type: 'touch-down]]  'touch-down  = e/type]
	--assert all [event? e: try [make event! [type: 'touch-cancel]] 'touch-cancel = e/type]
	--assert all [event? e: try [make event! [type: 'drop-text]]   'drop-text   = e/type]
	;; A menu selection carries the item id in `code`.
	--assert all [
		event? e: try [make event! [type: 'menu-select code: 42]]
		e/type = 'menu-select
		e/code = 42
	]
	;; Type 0 is the "no type" sentinel and reads as none, not as 'ignore.
	--assert all [
		event? e: try [make event! [type: 'ignore]]
		none? e/type
	]

--test-- "unnamed event types"
	;; A type past the named range - where an extension defines its own - has
	;; no word in the catalog and is reported as a plain integer.
	--assert all [event? e: try [make event! [type: 200]]  200 = e/type]
	--assert all [event? e: try [make event! [type: 255]]  255 = e/type]
	;; The same for a reserved slot inside the named range. (30 is reserved in
	;; the system group - pick another code here if it ever gets a name.)
	--assert not word? pick system/catalog/event-types 31
	--assert all [event? e: try [make event! [type: 30]]  30 = e/type]
	;; A code is accepted from Rebol too, so an extension's own types can be
	;; constructed and compared without a word for them.
	--assert all [
		event? e: try [make event! [type: 200 code: 7]]
		e/type = 200
		e/code = 7
	]
	;; Out of range is refused rather than truncated into the type byte.
	--assert error? try [make event! [type: 256]]
	--assert error? try [make event! [type: 300]]
	--assert error? try [make event! [type: -1]]

===end-group===

~~~end-file~~~