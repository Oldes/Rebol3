Rebol [
	title: "Private script binding test"
	type:  private
]

result: copy []
append result type? :checksum                 ;; lib value before its local assignment
foreach h [a b] [nested: h]                   ;; set-word in a nested block
append result nested
set [x y] [1 2]                               ;; words set without set-words
append result x + y
parse "abc" [copy chars to end]
append result chars
checksum: "local"
append result checksum
append result type? :lib/checksum             ;; lib is not modified
result
