Rebol [
	purpose: "Generate GitHub action workflows for Rebol project"
]

make-workflow: function/with [spec] [
	ctx: make values spec
	out: make string! 5000
	
	;; Prepare `triggers` section if any branches are specified.
	if block? ctx/branches [
		;; `out` buffer is reused for each part.
		emit LF
		foreach branch ctx/branches [
			emit ajoin ["      - " branch LF]
		]
		if block? ctx/paths [
			emit ajoin ["    paths:" LF]
			foreach path ctx/paths [
				emit ajoin ["      - " path LF]
			]
		]
		ctx/branches: take/all out
		emit-form template-triggers
		ctx/triggers: take/all out
	]

	emit-form template-header
	emit "^/jobs:"
	foreach [host host-spec] ctx/build [
		ctx: make ctx host-spec
		ctx/host: host
		append clear ctx/steps LF
		apps: clear []
		if block? ctx/libs [
			foreach lib ctx/libs [
				foreach arch ctx/arch [
					lib-name: lowercase form lib
					emit-step [
						"    - name: ^"[BUILD] " lib { static library } arch {"^/}
						"      run: ./siskin make/" lib-name ".nest cmake-" lib-name "-" arch LF
					]
				]
			]
			append ctx/steps LF
		]
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
		branches: _
		paths:    _
		action-checkout: "actions/checkout@v5"
		action-upload:   "actions/upload-artifact@v5"
		action-siskin:   "oldes/install-siskin@v0.22.5"
		steps: ""
		flags: "--gzip"
		artifact-suffix: "gz"
		build:   []
		tests:   []
		no-test: []
		libs:    []
		triggers: ""
	]

	template-triggers: %%{
  # Triggers the workflow on push or pull request events for specified branches
  push:
    branches: @BRANCHES@
  pull_request:
    branches: @BRANCHES@
}%%
	template-header: %%{#########################################################
# This file is generated using make-workflow.r3 script! #
#########################################################

name: '@NAME@'
on: @TRIGGERS@
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
    - name: Set env var using workspace path
      run: |
        echo "SISKIN_INSTALL=$GITHUB_WORKSPACE/install" >> $GITHUB_ENV
        echo "SISKIN_TEMP=$GITHUB_WORKSPACE/temp" >> $GITHUB_ENV
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

if all [
	name: first system/script/args
	spec: try [load ajoin [%./ name %.reb]]
][
	try/with [
		write ajoin [%workflows/ name %.yml] make-workflow spec
	] :print
]
