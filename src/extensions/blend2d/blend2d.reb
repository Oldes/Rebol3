REBOL [
	Title:   "Rebol Blend2D Extension"
	Name:    blend2d
	Version: 0.21.3
	Needs:   3.22.5
	Author:  @Oldes
	License: Apache-2.0
	Url:     https://github.com/Siskin-framework/Rebol-Blend2D
	Options: [delay]
	Exports: [draw]
	Purpose: {
		2D vector graphics using the Blend2D rendering engine - a `draw`
		dialect writing directly into Rebol images, plus reusable path,
		font and image handles.

		Data only - never evaluated. The C header (gen-blend2d.h) and the
		command table (gen-blend2d.c) are generated from this file by
		make-extension.r3, which also refreshes the reference sections of
		%README.md.
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
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; NOTE: the Blend2D header is carried by `c-header:` rather than by
;; `c-include:`. The template emits `c-include:` inside the `#ifdef REB_EXT`
;; branch only, while the declarations below need Blend2D's types in BOTH
;; build modes - `$c-header` is emitted after that #ifdef/#else block.
;c-include: []

c-header: {
#include <math.h>   // floor() - used by ROUND_TO_INT
#include <stdio.h>  // snprintf() - used by the `info` command and the molds
#include "blend2d/blend2d.h"

// Symbols of the registered handle types; defined in blend2d.c
extern REBCNT Handle_BLPath;
extern REBCNT Handle_BLFontFace;
extern REBCNT Handle_BLImage;

// Number of threads a rendering context is created with; see `set-threads`.
// Prefixed, because an embedded build links every extension into one binary.
extern uint32_t Blend2D_thread_count;
}

;; ---------------------------------------------------------------------------
;; Words resolved at init time through RL_MAP_WORDS.
;;
;; `cmd` holds the words of the `draw` dialect, `arg` the words used as their
;; arguments. The ORDER OF BOTH LISTS IS SIGNIFICANT: the generated
;; W_BLEND2D_ARG_* values are used as offsets into Blend2D's own enumerations
;; (extend modes, gradient types, composition operators), so words may only be
;; appended at the end of a group, never reordered.
;;
;; The path-accessor words of the `handles:` block below are collected into
;; `arg` by the generator - they are appended after the words listed here.
words: [
	cmd: [
		move
		line
		curve
		curv
		cubic
		quad
		hline
		vline
		qcurve
		qcurv

		polygon
		shape
		box
		circle
		ellipse
		arc
		image
		text
		fill-all ; fills all context area
		clear
		clear-all
		point
		point-size
		triangle

		pen
		fill
		line-width
		line-cap
		line-join
		alpha
		blend     ; blend-mode?
		composite ; composite-mode?

		reset-matrix
		rotate
		scale
		translate
		close

		clip

		font
		;- aliases (for compatibility)
		fill-pen ; alias for `fill`
	]
	arg: [
		;- pattern/gradient modes
		pad              ; BL_EXTEND_MODE_PAD = 0,
		tile             ; BL_EXTEND_MODE_REPEAT = 1,
		flip             ; BL_EXTEND_MODE_REFLECT = 2,
		tile-y           ; BL_EXTEND_MODE_PAD_X_REPEAT_Y = 3,
		flip-y           ; BL_EXTEND_MODE_PAD_X_REFLECT_Y = 4,
		tile-x           ; BL_EXTEND_MODE_REPEAT_X_PAD_Y = 5,
		tile-x-flip-y    ; BL_EXTEND_MODE_REPEAT_X_REFLECT_Y = 6,
		flip-x           ; BL_EXTEND_MODE_REFLECT_X_PAD_Y = 7,
		flip-x-tile-y    ; BL_EXTEND_MODE_REFLECT_X_REPEAT_Y = 8,
		;- gradient types:
		linear
		radial
		conical
		;- blend modes:
		source-over      ; BL_COMP_OP_SRC_OVER = 0,
		source-copy      ; BL_COMP_OP_SRC_COPY = 1,
		source-in        ; BL_COMP_OP_SRC_IN = 2,
		source-out       ; BL_COMP_OP_SRC_OUT = 3,
		source-atop      ; BL_COMP_OP_SRC_ATOP = 4,
		destination-over ; BL_COMP_OP_DST_OVER = 5,
		destination-copy ; BL_COMP_OP_DST_COPY = 6,
		destination-in   ; BL_COMP_OP_DST_IN = 7,
		destination-out  ; BL_COMP_OP_DST_OUT = 8,
		destination-atop ; BL_COMP_OP_DST_ATOP = 9,
		xor              ; BL_COMP_OP_XOR = 10,
		clear            ; BL_COMP_OP_CLEAR = 11,
		plus             ; BL_COMP_OP_PLUS = 12,
		minus            ; BL_COMP_OP_MINUS = 13,
		modulate         ; BL_COMP_OP_MODULATE = 14,
		multiply         ; BL_COMP_OP_MULTIPLY = 15,
		screen           ; BL_COMP_OP_SCREEN = 16,
		overlay          ; BL_COMP_OP_OVERLAY = 17,
		darken           ; BL_COMP_OP_DARKEN = 18,
		lighten          ; BL_COMP_OP_LIGHTEN = 19,
		color-dodge      ; BL_COMP_OP_COLOR_DODGE = 20,
		color-burn       ; BL_COMP_OP_COLOR_BURN = 21,
		linear-burn      ; BL_COMP_OP_LINEAR_BURN = 22,
		linear-light     ; BL_COMP_OP_LINEAR_LIGHT = 23,
		pin-light        ; BL_COMP_OP_PIN_LIGHT = 24,
		hard-light       ; BL_COMP_OP_HARD_LIGHT = 25,
		soft-light       ; BL_COMP_OP_SOFT_LIGHT = 26,
		difference       ; BL_COMP_OP_DIFFERENCE = 27,
		exclusion        ; BL_COMP_OP_EXCLUSION = 28,
		;- line-join:
		miter
		bevel
		round
		;- arc types:
		pie
		closed ; same as pie
		chord
		sweep
		large
		;------------------------------
		;	stroke-alignment
		;	center
		;	inner
		;	outer
	]
]

;; ---------------------------------------------------------------------------
;; Handle types registered by this extension.
;;
;; The keys below are documentation names; on the C side the types keep the
;; names they were always registered under - BLImage, BLPath and BLFontFace
;; (RL_REGISTER_HANDLE_SPEC in blend2d.c), which is what MOLD and
;; `system/catalog/handles` show.
;;
;; Each field row is:
;;
;;     NAME  GET-types  SET-types  "description"
;;
;; where `none` in the SET column marks a read-only field. All fields are
;; read-only here, so the handles register a get_path accessor only.
;;
;; The generator collects these field names into `words/arg`, so the
;; W_BLEND2D_ARG_* values used by the accessors are generated from this block.
handles: [
	image: [
		"Blend2D image (BLImage)"
		;NAME     GET       SET   DESCRIPTION
		size      pair!     none  "Image size in pixels"
		width     integer!  none  "Image width in pixels"
		height    integer!  none  "Image height in pixels"
		format    integer!  none  "Pixel format (BLFormat value; 1 is PRGB32)"
		stride    integer!  none  "Number of bytes between two consecutive rows"
	]
	path: [
		"Blend2D path (BLPath)"
		;NAME     GET       SET   DESCRIPTION
		size      integer!  none  "Number of vertices"
		capacity  integer!  none  "Number of vertices which fit without reallocating"
	]
	font-face: [
		"Blend2D font face (BLFontFace)"
		;NAME         GET       SET   DESCRIPTION
		glyphs        integer!  none  "Number of glyphs the face provides"
		face-type     integer!  none  "Face type (BLFontFaceType value)"
		outline-type  integer!  none  "Outline type (BLFontOutlineType value)"
		revision      integer!  none  "Face revision"
		face-index    integer!  none  "Index of the face in a font collection"
		face-flags    integer!  none  "Face flags (BLFontFaceFlags bit set)"
		diag-flags    integer!  none  "Diagnostic flags (BLFontFaceDiagFlags bit set)"
	]
]

;; ---------------------------------------------------------------------------
;; Commands.
;; The `_init` command is injected as the first one by the generator.
commands: [
	draw: [
		{Draws scalable vector graphics to an image}
		image    [image! pair!] "Target image, or a size of a new one"
		commands [block!]       "The draw dialect"
	]
	path: [
		{Prepares path object}
		commands [block!] "Path dialect: move, line, curve, curv, qcurve, qcurv, arc, hline, vline, close"
	]
	font: [
		{Prepares font handle}
		file [file! string!] "Font location or name"
	]
	image: [
		{Prepares Blend2D's native image}
		from [pair! image! file!] "Size of a new image, an image to wrap, or a file to load"
	]
	info: [
		{Returns info about Blend2D library}
		/of handle [handle!] "Blend2D object"
	]
	set-threads: [
		{Sets number of threads to be used}
		count [integer!] {0 means synchronous rendering; 1 is for async on main thread only else thread pool is used}
	]

	;; A scratch pad for quick native experiments - not part of the module's
	;; interface. Enable it here AND in the nest file (which leaves
	;; %blend2d-command-draw-test.c out of the build) at the same time; the
	;; command must stay LAST, so that enabling it cannot move the indices of
	;; the commands above.
	;draw-test: [
	;	{Draws a hardcoded gradient into an image (used for quick native experiments)}
	;	image [image! pair!] "Target image, or a size of a new one"
	;]
]