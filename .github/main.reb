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
libs: [Blend2D]
build: [
	windows: [
		runs-on: 'windows-latest
		flags: "--msvc"
		artifact-suffix: "exe"
		arch: [x86 x64]
	]
	linux: [
		runs-on: 'ubuntu-latest
		flags: "--gzip"
		artifact-suffix: "gz"
		arch: [x64]
	]
	macos: [
		runs-on: 'macos-latest
		flags: "--gzip"
		artifact-suffix: "gz"
		arch: [x64 arm64]
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