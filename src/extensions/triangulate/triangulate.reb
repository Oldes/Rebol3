REBOL [
	Title:   "Rebol Triangulate Extension"
	Name:    triangulate
	Version: 1.7.0
	Needs:   3.22.5
	Author:  @Oldes
	License: Apache-2.0
	Options: [delay]
	Url:     https://github.com/Siskin-framework/Rebol-Triangulate
	Exports: [triangulate]
	Purpose: {
		Two-dimensional quality mesh generation and Delaunay triangulation
		using Jonathan Shewchuk's Triangle library.

		Data only - never evaluated. The C header (gen-triangulate.h) and the
		command table (gen-triangulate.c) are generated from this file by
		make-extension.r3.
	}
]

;; ---------------------------------------------------------------------------
;; Banner put on top of the generated files.
logo: {//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Triangulate extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; `REAL` lives here (and not in triangulate.h) because the generated header is
;; what every source of the extension includes first, and `triangle.h` must see
;; the same definition in all of them - Triangle itself is built with the
;; default double precision, so this must not be changed alone.
c-header: {
#define REAL double
}

;; ---------------------------------------------------------------------------
;; Words resolved at init time through RL_MAP_WORDS.
;;
;; These are the field names of the `in` and `out` objects. Previously each one
;; was mapped with a separate RL_MAP_WORD call on every single triangulation;
;; now the whole list is resolved once, when the module body evaluates, and
;; W_TRIANGULATE_IO_* indexes into `Triangulate_io_words` to get the word id.
words: [
	io: [
		;; input only
		report
		regions
		;; input and output
		points
		attributes
		markers
		;; output only
		segments
		segment-markers
		edges
		triangles
		;; voronoi output
		v-points
		v-attributes
		v-edges
		v-norms
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;; The `_init` command is injected as the first one by the generator.
commands: [
	triangulate: [
		"Triangulates a set of points; the results are stored in the output object"
		 in  [object!] "Must have `points` as a 64bit decimal vector; see the README for the optional fields"
		 out [object!] "Only the fields which this object already has are filled in"
	]
]