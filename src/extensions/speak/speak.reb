REBOL [
	Title:   "Rebol Speak Extension"
	Name:    speak
	Version: 0.0.1
	Needs:   3.14.1
	Author:  @Oldes
	License: MIT
	Options: [delay]
	Url:     https://github.com/Oldes/Rebol-Speak
	Exports: [say]
	Purpose: {
		Text to speech using the system speech synthesizer
		(SAPI on Windows, NSSpeechSynthesizer on macOS).

		Data only - never evaluated. The C header (gen-speak.h) and the
		command table (gen-speak.c) are generated from this file by
		tools/make-extension.r3.
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
// Project: Rebol/Speak extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; The struct lives here (and not in speak.h) because the generated header is
;; what every source of the extension includes first - the command sources as
;; well as the platform backends.
c-header: {
// No ABI 1 related features used in this extension
#undef RL_ABI_VERSION
#define RL_ABI_VERSION 0

extern REBCNT Handle_VoiceHandle;

// One synthesizer plus the text it is speaking. `text` is owned by the
// handle - a NUL terminated UTF-8 string, or a wide string on Windows,
// reallocated on each `say` and released with the handle.
typedef struct voice_t {
	void *synth;   // NSSpeechSynthesizer* (macOS) or ISpVoice* (Windows)
	void *text;
	int   number;  // 1-based system voice index, 0 = the default voice
#ifdef TO_MACOS
	// Deliberately `int` and not `BOOL`: reb-c.h makes BOOL an int in the
	// plain C sources but leaves it as Objective-C's signed char in
	// speak-mac.m, which would give this struct two different sizes.
	int   isSpeaking;
#endif
} voice_t;
}

;; ---------------------------------------------------------------------------
;; Handle types and their path accessors.
;;
;; Each row is: NAME, the type read from the field, the type accepted when
;; writing it (`none` for read-only), and a description. The names become the
;; `arg` word list and the W_SPEAK_ARG_* enum used by VoiceHandle_get_path
;; and VoiceHandle_set_path.
handles: [
	voice-handle: [
		"System speech synthesizer voice"
		;NAME   GET       SET       DESCRIPTION
		number  integer!  integer!  "1-based index of the system voice (0 is the default one)"
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;; The `_init` command is injected as the first one by the generator.
commands: [
	list-voices: [
		"Print a list of available system voices"
	]
	say: [
		"Converts text to speech and plays it"
		 text [string!] "The text to be spoken"
		 /as voice [integer! handle!] "Specify the voice or handle to use for speech synthesis"
		 /no-wait "Do not block execution while the speech is playing"
	]
]
