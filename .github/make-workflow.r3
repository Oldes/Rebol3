Rebol [
	purpose: "Generate GitHub action workflow"
]

spec: [
	name: "Rebol CI"
	branches: "[master]"
	build: [
		windows: [
			runs-on: 'windows-latest
			flags: "--msvc"
			artifact-suffix: "exe"
			targets: [
				"Rebol/Base x86"   %rebol3-base-windows-x86
				"Rebol/Bulk x86"   %rebol3-bulk-windows-x86
				"Rebol/Base x64"   %rebol3-base-windows-x64
				"Rebol/Bulk x64"   %rebol3-bulk-windows-x64
			]
		]
		linux: [
			runs-on: 'ubuntu-latest
			flags: "--gzip"
			artifact-suffix: "gz"
			targets: [
				"Rebol/Bulk x64"   %rebol3-bulk-linux-x64
			]
		]
		macos: [
			runs-on: 'macos-latest
			flags: "--gzip"
			artifact-suffix: "gz"
			targets: [
				"Rebol/Bulk x64"   %rebol3-bulk-macos-x64
				"Rebol/Bulk arm64" %rebol3-bulk-macos-arm64
			]
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
	foreach [host host-spec] ctx/build [
		ctx: make ctx host-spec
		ctx/host: host
		append clear ctx/steps LF
		apps: clear []
		foreach [title target] ctx/targets [
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
		name: "ci"
		host: "linux"
		runs-on: "ubuntu-latest"
		branches: ""
		action-checkout: "actions/checkout@v5"
		action-siskin:   "oldes/install-siskin@v0.21.15"
		action-upload:   "actions/upload-artifact@v5"
		steps: ""
		flags: "--gzip"
		artifact-suffix: "gz"
		build:   []
		tests:   []
		no-test: []
	]

	template-header: %%{#########################################################
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
    runs-on: @RUNS-ON@
    steps:
    - name: Checkout repository
      uses: @ACTION-CHECKOUT@
    - name: Install Siskin Builder
      uses: @ACTION-SISKIN@
    @STEPS@
    - uses: @ACTION-UPLOAD@
      with:
        name: @NAME@-${{github.run_id}}-@HOST@
        path: ./rebol3-*.@ARTIFACT-SUFFIX@
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