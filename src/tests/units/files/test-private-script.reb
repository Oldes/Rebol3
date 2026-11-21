Rebol [
	title: "Private script test"
	type:  private
	needs: [%test-needs-file-value.reb]
]

private-script-counter: 0
private-script-value: test-needs-file-value ;; imported only into this script

reduce [
	system/script/header/type
	private-script-value
	func [] [private-script-counter: private-script-counter + 1]
	true? find system/script/path %/units/files/
]
