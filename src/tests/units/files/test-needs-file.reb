Rebol [
	title: "Needs module test 2"
	name:  test-needs-file
	needs: %test-needs-file-value.reb ;; relative to this file
]

export test-needs-file-result: (42 = test-needs-file-value)
