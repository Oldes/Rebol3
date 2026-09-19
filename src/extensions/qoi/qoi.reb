REBOL [
	Title:   "Rebol QOI Codec Extension"
	Name:    qoi
	Version: 1.0.0
	Needs:   3.22.6
	Author:  @Oldes
	License: MIT
	Url:     https://github.com/Siskin-framework/Rebol-QOI
	Exports: []
	Purpose: {
		The "Quite OK Image" codec as a loadable extension instead of an
		INCLUDE_QOI_CODEC build option of the interpreter.

		Data only - never evaluated. The C header (gen-qoi.h) and the command
		table (gen-qoi.c) are generated from this file by make-extension.r3.

		NOTE on `Needs:` - the commands call RL_ALLOC, which was appended to
		the RL_API table without bumping RL_ABI_VERSION. Tail additions are
		only safe when the HOST is at least as new as the extension, so the
		version check in RX_Init is what keeps this loadable: an older host
		would have a shorter table and RL->alloc would read past its end.

		NOTE on `Options:` - `delay` is deliberately NOT used. A delayed
		module registers its codec only when something imports it, and until
		then `encoding?` does not recognise the format at all.
	}
]

;; ---------------------------------------------------------------------------
;; Banner put on top of the generated files.
logo: {//
// Project: Rebol/QOI extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;; The `_init` command is injected as the first one by the generator.
commands: [
	codec-handle: [
		{Returns the QOI dispatcher as a codec handle, for use as `entry`}
	]
]

;; ---------------------------------------------------------------------------
;; Registration.
;;
;; `register-codec` builds the same object shape that base-defs.reb builds for
;; the internal codecs at boot, and appends the suffix to
;; system/catalog/file-types. That boot loop only ever runs once, so an
;; extension which called Register_Codec on the C side would leave a bare
;; handle in system/codecs and DECODE would not recognise it - the object has
;; to be made here.
;;
;; The handle is fetched once and the command is hidden afterwards: it exists
;; only to hand the dispatcher over, and a stray `codec-handle` call from user
;; code has no meaning.
mezzanine: [
	register-codec [
		name:     'qoi
		type:     'image
		title:    "Quite OK Image"
		suffixes: [%.qoi]
		entry:    codec-handle
	]
	protect/hide 'codec-handle
]
