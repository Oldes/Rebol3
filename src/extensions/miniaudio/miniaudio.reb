REBOL [
	Title:   "Rebol MiniAudio Extension"
	Name:    miniaudio
	Version: 0.11.23
	Needs:   3.22.5
	Author:  @Oldes
	License: MIT
	Url:     https://github.com/Oldes/Rebol-MiniAudio
	Purpose: {
		Audio playback using the MiniAudio library.

		Data only - never evaluated. The C header (gen-miniaudio.h) and
		the command table (gen-miniaudio.c) are generated from this file
		by tools/make-extension.r3.
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
// Project: Rebol/MiniAudio extension
// SPDX-License-Identifier: MIT
// =============================================================================
// NOTE: auto-generated file, do not modify!}

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; NOTE: `miniaudio.h` is included from `c-header:` and not from `c-include:`
;; on purpose - the generator emits `c-include:` inside the `#ifdef REB_EXT`
;; branch, while `c-header:` lands after it and so serves both build modes.
c-header: {
#include "miniaudio.h"

#ifndef SERIES_TEXT
#define SERIES_TEXT(s)   ((char*)SERIES_DATA(s))
#endif

typedef struct MAContext {
	ma_engine* engine;
	ma_device* device;
	RXICBI     callback;
} MAContext;

extern REBCNT Handle_MAEngine;
extern REBCNT Handle_MASound;
extern REBCNT Handle_MANoise;
extern REBCNT Handle_MAWaveform;
extern REBCNT Handle_MADelay;
extern REBCNT Handle_MAGroup;
}

;; ---------------------------------------------------------------------------
;; Words resolved at init time. The `init-words` command, the
;; W_MINIAUDIO_ARG_* / W_MINIAUDIO_TYPE_* enums (each with its _0 sentinel)
;; and the handler filling `Miniaudio_arg_words` / `Miniaudio_type_words`
;; are all generated.
words: [
	;; path accessors of all handle types below
	arg: [
		volume pan pitch position cursor time duration frames sample-rate
		spatialize is-looping is-playing at-end source start stop
		x y z outputs output resources channels gain-db
		amplitude frequency format type delay decay dry wet
	]
	;; @@ Order is important - the C side indexes this list by the
	;; matching miniaudio enum value (ma_noise_type, ma_waveform_type,
	;; ma_format), see W_MINIAUDIO_TYPE_WHITE / _SINE / _F32.
	type: [
		;- noise types
		white
		pink
		brownian
		;- waveform types
		sine
		square
		triangle
		sawtooth
		;- format types
		;; @@ must follow the ma_format enum, `unknown` included, because
		;; the C side indexes this from W_MINIAUDIO_TYPE_UNKNOWN
		unknown
		u8
		s16
		s24
		s32
		f32
	]
]

;; ---------------------------------------------------------------------------
;; Handle types registered by this extension (used for the README).
handles: [
	ma-sound: [
		"MiniAudio sound object"
		;NAME          GET       SET                 DESCRIPTION
		volume         decimal!  [integer! decimal! percent!] "Sound volume"
		pan            decimal!   decimal!           "Stereo panning (from -1.0 to 1.0)"
		pitch          decimal!   decimal!           "Sound pitch"
		position       pair!      pair!              "Sound position (x and y for now) relative to the listener"
		cursor         integer!  [integer! time!]    "Sound playback position in PCM frames"
		duration       time!      none               "Sound duration in time"
		frames         integer!   none               "Sound length in PCM frames"
		sample-rate    integer!   none               "Number of samples per second"
		spatialize     logic!     logic!             "3D spatialization state"
		is-looping     logic!     logic!             "Whether sound is looping"
		is-playing     logic!     logic!             "Whether sound is playing"
		at-end         logic!     none               "Whether sound is at end"
		start          integer!  [integer! time!]    "Absolute timer when the sound should be started (frames or time)"
		stop           integer!  [integer! time!]    "Absolute timer when the sound should be stopped (frames or time)"
		x              decimal!  [integer! decimal!] "Sound X position"
		y              decimal!  [integer! decimal!] "Sound Y position"
		z              decimal!  [integer! decimal!] "Sound Z position"
		source        [file! handle!] none           "Sound source as a loaded file or data source node"
		outputs        integer!   none               "Number of output buses"
		output         none      [handle! none!]     "Output bus node (write only)"
	]
	ma-group: [
		"MiniAudio sound group"
		;NAME          GET       SET                 DESCRIPTION
		volume         decimal!  [integer! decimal! percent!] "Sound volume"
		pan            decimal!   decimal!           "Stereo panning (from -1.0 to 1.0)"
		pitch          decimal!   decimal!           "Sound group pitch"
		position       pair!      pair!              "Sound group position (x and y for now) relative to the listener"
		sample-rate    integer!   none               "Number of samples per second"
		spatialize     logic!     logic!             "3D spatialization state"
		is-playing     logic!     logic!             "Whether sound is playing"
		start          integer!  [integer! time!]    "Absolute timer when the sound should be started (frames or time)"
		stop           integer!  [integer! time!]    "Absolute timer when the sound should be stopped (frames or time)"
		x              decimal!  [integer! decimal!] "Sound group X position"
		y              decimal!  [integer! decimal!] "Sound group Y position"
		z              decimal!  [integer! decimal!] "Sound group Z position"
		outputs        integer!   none               "Number of output buses"
		output         none      [handle! none!]     "Output bus node (write only)"
		resources      block!     none               "Used group resources (sounds, nodes..)"
	]
	ma-engine: [
		"MiniAudio device engine"
		volume         decimal!  [integer! decimal! percent!] "Global volume"
		frames         integer!  [integer! time!]    "Engine playback position in PCM frames"
		time           time!      time!              "Engine playback position as time"
		resources      block!     none               "Used engine resources (sounds, nodes..)"
		channels       integer!   none               "Number of output channels"
		sample-rate    integer!   none               "Ouput device sample rate per second"
		gain-db        decimal!  [integer! decimal!] "The amplification factor in decibels"
	]
	ma-noise: [
		"MiniAudio noise generator"
		amplitude      decimal!   decimal!           "Maximum value of the noise signal"
		format         word!      none               "u8, s16, s24, s32 or f32"
		type           word!      none               "white, pink or brownian"
	]
	ma-waveform: [
		"MiniAudio sine, square, triangle and sawtooth waveforms generator"
		amplitude      decimal!   decimal!           "Signal amplitude"
		frequency      decimal!   decimal!           "Signal frequency in hertzs"
		format         word!      none               "u8, s16, s24, s32 or f32"
		type           word!      none               "sine, square, triangle or sawtooth"
	]
	ma-delay: [
		"MiniAudio delay node"
		delay          integer!  none                "PCM frames"
		decay          decimal!  [decimal! percent!] "Value between 0.0 and 1.0"
		dry            decimal!  [decimal! percent!] "The mix level of the dry (original) sound"
		wet            decimal!  [decimal! percent!] "The mix level of the wet (delayed) sound"
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;; The `init-words` command is injected as the first one by the generator.
commands: [
	version: ["Native MiniAudio version"]

	get-devices:   ["Retrive playback/capture device names"]
	init-playback: [
		"Initialize a playback device"
		index [integer!]
		/pause    "Don't start it automatically"
		/channels "The number of channels to use for playback"
		 number [integer!] "When set to 0 the device's native channel count will be used"
		/period            "Hint for making up the device's entire buffer"
		 size   [integer!] "The desired size of a period in milliseconds"
		/callback "On-data callback (two args.. buffer frames, and engine total frames)"
		 context [object!] "The function's context"
		 word    [word!]   "The function's name"
	]

	load:  [
		"Loads a file and returns sound's handle"
		sound [file!]
		/group "Group of sounds which have their own effect processing and volume control"
		 node [handle!] "ma-group handle"
	]

	play:  [
		"Loads a file (if not already loaded) and starts playing it. Returns a sound handle."
		sound [file! handle!] "Source file or a ma-sound handle"
		/stream "Do not load the entire sound into memory"
		/loop   "Turn looping on"
		/volume vol [percent! decimal!]
		/fade   in  [integer! time!] "PCM frames or time"
		/group  "Group of sounds which have their own effect processing and volume control"
		 node [handle!] "ma-group handle"
	]
	pause: [
		"Pause sound playback"
		sound [handle!]
	]

	start: [
		{Start sound or device playback. A sound is restarted from its beginning (use /seek to start elsewhere) and its looping state is taken from /loop, so a plain start turns looping off.}
		handle  [handle!] "ma-sound or ma-engine handle"
		/loop   "Turn looping on (only for sounds)"
		/seek   "Starting position"
		 frames [integer! time!] "PCM frames or time"
		/fade   "Fade in the sound"
		 in     [integer! time!] "PCM frames or time"
		/at     "Absolute engine time when the sound should be started"
		 time   [integer! time!] "PCM frames or time"
	]
	stop:  [
		"Stop sound or device playback"
		handle [handle!] "ma-sound or ma-engine handle"
		/fade out [integer! time!] "PCM frames or time (only for sounds)"
	]
	fade:  [
		"Fade sound volume"
		sound [handle!]
		frames [integer! time!] start [percent! decimal!] end [percent! decimal!]
	]
	seek:  [
		"Seek to specified position"
		sound [handle!]
		frames [integer! time!]
		/relative "Relative to the current sound position"
	]

	make-noise-node:  [
		"Creates a noise node for generating random noise"
		type      [integer!] "The type of noise to generate (0 - 2)"
		amplitude [decimal!] "The peak amplitude of the noise"
		/seed                "Optional random seed"
		 val [integer!]
		/format              "The sample format (default is 2 = signed 16bit integer)"
		 frm [integer!]      "Value betweem 1 - 5"
	]
	make-waveform-node: [
		"Creates a sound waveform node"
		type      [integer!] "The type of waveform to generate (0 - 3)"
		amplitude [decimal!] "The peak amplitude of the waveform"
		frequency [decimal!] "The frequency of the waveform in Hertz (Hz)"
		/format "The sample format (default is 2 = signed 16bit integer)"
		 frm [integer!] "Value betweem 1 - 5"
	]
	make-delay-node: [
		"Creates a delay (echo) sound node"
		delay [decimal! integer! time!] "The time before the echo is heard. Seconds, PCM frames or time."
		decay [decimal! percent!] "Feedback decay (0.0 - 1.0). Affects how quickly or gradually the echoes fade away. 0 means no feedback."
		/dry "The mix level of the dry (original) sound"
		 d [decimal! percent!]
		/wet "The mix level of the wet (delayed) sound"
		 w [decimal! percent!]
	]
	make-group-node: [
		"Creates a sound group node"
	]

	;; Keep these (s|g)etters?
	volume:   ["Set the volume"  sound [handle!] volume [percent! decimal!]]
	volume?:  ["Get the volume"  sound [handle!]]
	pan:      ["Set the pan"     sound [handle!] pan [decimal!]]
	pan?:     ["Get the pan"     sound [handle!]]
	pitch:    ["Set the pitch"   sound [handle!] pitch [decimal!]]
	pitch?:   ["Get the pitch"   sound [handle!]]
	looping:  ["Set the looping" sound [handle!] value [logic!]]
	looping?: ["Get the looping" sound [handle!]]
	end?:     ["Return true if sound ended" sound [handle!]]
]

;; ---------------------------------------------------------------------------
;; Module body. Previously a C string literal with escaped newlines - as a
;; block it is ordinary Rebol code that an editor can indent and check.
mezzanine: [
	;; Waveform types
	type_sine:     0
	type_square:   1
	type_triangle: 2
	type_sawtooth: 3

	;; Sample data formats
	format_u8:     1
	format_s16:    2  ;; Seems to be the most widely supported format.
	format_s24:    3  ;; Tightly packed. 3 bytes per sample.
	format_s32:    4
	format_f32:    5
]
