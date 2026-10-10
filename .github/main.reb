Rebol [
	purpose: "Specification for the basic Rebol CI workflow"
]

name: "Rebol CI"
branches: [master]
paths: [
	%make/**
	%src/**
	%'!src/tests/**'
	%.github/workflows/main.yml
]
build: [
	windows: [
		runs-on: 'windows-latest
		flags: "--msvc"
		artifact-suffix: "exe"
		arch: [x86 x64]
		libs: [Blend2D]
		targets: [
			"Rebol/Bulk x86"   %rebol3-bulk-windows-x86
			"Rebol/Bulk x64"   %rebol3-bulk-windows-x64
		]
	]
	linux: [
		runs-on: 'ubuntu-latest
		flags: "--gzip"
		artifact-suffix: "gz"
		arch: [x64]
		libs: [Blend2D]
		targets: [
			"Rebol/Bulk x64"   %rebol3-bulk-linux-x64
		]
	]
	macos: [
		runs-on: 'macos-latest
		flags: "--gzip"
		artifact-suffix: "gz"
		arch: [x64 arm64]
		libs: [Blend2D]
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
	%./src/tests/test-extension-blend2d.r3
]
no-test: [
	%rebol3-base-windows-x86
	%rebol3-bulk-windows-x86
	%rebol3-base-windows-x64
	%rebol3-bulk-macos-arm64
]