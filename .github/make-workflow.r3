Rebol [
	purpose: "Generate GitHub action workflow"
]

spec: [
	name: "Rebol CI"
	branches: "[master]"
	build: [
		windows: [
			"Rebol/Base x86"   %rebol3-base-windows-x86
			"Rebol/Bulk x86"   %rebol3-bulk-windows-x86
			"Rebol/Base x64"   %rebol3-base-windows-x64
			"Rebol/Bulk x64"   %rebol3-bulk-windows-x64
		]
		linux: [
			"Rebol/Bulk x64"   %rebol3-bulk-linux-x64
		]
		macos: [
			"Rebol/Bulk x64"   %rebol3-bulk-macos-x64
			"Rebol/Bulk arm64" %rebol3-bulk-macos-arm64
		]
	]

	tests: [
		%./src/tests/run-tests.r3
		%./src/tests/test-extension-sqlite.r3
		%./src/tests/test-extension-qoi.r3
		%./src/tests/test-extension-triangulate.r3
	]
	no-test: [
		%rebol3-base-windows-x86
		%rebol3-bulk-windows-x86
		%rebol3-base-windows-x64
		%rebol3-bulk-macos-arm64
	]
]

make-workflow: function/with [spec] [
	ctx: make values spec
	out: make string! 5000
	emit-form template-header
	emit "^/jobs:"
	foreach [host targets] ctx/build [
		ctx/HOST: host
		either host = 'windows [
			ctx/FLAGS: "--msvc"
			ctx/ARTIFACT_SUFFIX: "exe"
		][
			ctx/FLAGS: "--gzip"
			ctx/ARTIFACT_SUFFIX: "gz"
		]
		append clear ctx/steps LF
		apps: clear []
		foreach [title target] targets [
			unless find ctx/no-test target [ append apps target ]
			emit-step [
				"    - name: ^"[BUILD] " title {"^/}
				"      run: ./siskin make/rebol3.nest -o ./ " ctx/FLAGS SP target LF
			]
		]
		foreach test-app apps [
			append ctx/steps LF
			foreach test ctx/tests [
				emit-step [
					"    - name: ^"[TEST] " test-app SP test {"^/}
					"      run: ./" test-app " -s " test LF 
				]
			]
		]
		emit-form template-job
	]
	probe 
	deline out
][
	out: ctx: _
	values: object [
		NAME: "CI"
		HOST: "linux"
		BRANCHES: ""
		ACTION_CHECKOUT: "actions/checkout@v5"
		ACTION_SISKIN:   "oldes/install-siskin@v0.21.15"
		ACTION_UPLOAD:   "actions/upload-artifact@v5"
		STEPS: ""
		FLAGS: "--gzip"
		ARTIFACT_SUFFIX: "gz"
		build:   []
		tests:   []
		no-test: []
	]

	template-header: next %%{
#########################################################
# This file is generated using make-workflow.r3 script! #
#########################################################

name: '@NAME@'
on:
  # Triggers the workflow on push or pull request events but only for the master branch
  push:
    branches: @BRANCHES@
    paths:
      - make/**
      - src/**
      - '!src/tests/**'
      - .github/workflows/main.yml

  pull_request:
    branches: @BRANCHES@

  # Allows you to run this workflow manually from the Actions tab
  workflow_dispatch:
}%%

 	template-job: %%{
  @HOST@:
    strategy:
      fail-fast: true
    runs-on: @HOST@-latest
    steps:
    - name: Checkout repository
      uses: @ACTION_CHECKOUT@
    - name: Install Siskin Builder
      uses: @ACTION_SISKIN@
    @STEPS@
    - uses: @ACTION_UPLOAD@
      with:
        name: @NAME@-${{github.run_id}}-@HOST@
        path: ./rebol3-*.@ARTIFACT_SUFFIX@
}%%
	emit: func[value][
		append out value
	]
	emit-step: func[value][
		append ctx/steps ajoin value
	]
	emit-form: func [template][
		emit reword/escape template ctx [#"@" #"@"]
	]
]

write %workflows/main.yml make-workflow spec