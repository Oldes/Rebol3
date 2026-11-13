REBOL [
	Title:   "Rebol Easing Extension"
	Name:    easing
	Version: 1.3.2.0
	Needs:   3.5.4
	Author:  @Oldes
	License: Unlicense
	Url:     https://github.com/Oldes/Rebol-Easing
	Exports: [tween]
	Purpose: {
		Robert Penner's easing functions, backed by the public domain
		AHEasing sources (%easing.c / %easing.h).

		Data only - never evaluated. The C header (gen-easing.h) and the
		command table (gen-easing.c) are generated from this file by
		make-extension.r3.

		Builds either embedded into the host or as a standalone
		easing-x64.rebx library.
	}
]

;; ---------------------------------------------------------------------------
;; C-side configuration
;;
;; NOTE: `easing.h` is carried by `c-header:` and not by `c-include:` on
;; purpose - the template emits `c-include:` inside the `#ifdef REB_EXT`
;; branch only, while `$c-header` lands after that #ifdef/#else block and
;; so serves both build modes.
c-header: {
#include "easing.h"
// No ABI 1 related features used in this extension
#undef RL_ABI_VERSION
#define RL_ABI_VERSION 0
}

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices.
;;
;; Every command takes a single decimal in the range 0.0 - 1.0 and returns the
;; eased value. The input is NOT validated or clamped; feeding a value from
;; outside that range is meaningful for some of the curves (`in-back` and
;; `in-elastic` overshoot by design) and nonsense for others.
;;
;; This extension registers no handles and needs no `words:` block, so the
;; generated table starts directly at CMD_EASING_LINEAR.
commands: [
	linear:         ["Modeled after the line     y = x"    x [decimal!]]

	in-quad:        ["Modeled after the parabola y = x^^2" x [decimal!]]
	in-cubic:       ["Modeled after the cubic    y = x^^3" x [decimal!]]
	in-quart:       ["Modeled after the quartic  y = x^^4" x [decimal!]]
	in-quint:       ["Modeled after the quintic  y = x^^5" x [decimal!]]
	in-sine:        ["Modeled after quarter-cycle of sine wave" x [decimal!]]
	in-circ:        ["Modeled after shifted quadrant IV of unit circle" x [decimal!]]
	in-expo:        ["Modeled after the exponential function y = 2^^(10(x - 1))" x [decimal!]]
	in-elastic:     ["Modeled after the damped sine wave y = sin(13pi/2*x)*pow(2, 10 * (x - 1))" x [decimal!]]
	in-back:        ["Modeled after the overshooting cubic y = x^^3-x*sin(x*pi)" x [decimal!]]
	in-bounce:      ["Modeled after the reversed bounce y = 1 - out-bounce(1 - x)" x [decimal!]]

	out-quad:       ["Modeled after the parabola y = -x^^2 + 2x"     x [decimal!]]
	out-cubic:      ["Modeled after the cubic    y = (x - 1)^^3 + 1" x [decimal!]]
	out-quart:      ["Modeled after the quartic  y = 1 - (x - 1)^^4" x [decimal!]]
	out-quint:      ["Modeled after the quintic  y = (x - 1)^^5 + 1" x [decimal!]]
	out-sine:       ["Modeled after quarter-cycle of sine wave (different phase)" x [decimal!]]
	out-circ:       ["Modeled after shifted quadrant II of unit circle" x [decimal!]]
	out-expo:       ["Modeled after the exponential function y = -2^^(-10x) + 1" x [decimal!]]
	out-elastic:    ["Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2, -10x) + 1" x [decimal!]]
	out-back:       ["Modeled after the overshooting cubic y = 1-((1-x)^^3-(1-x)*sin((1-x)*pi))" x [decimal!]]
	out-bounce:     ["Modeled after the piecewise quadratic decaying bounce" x [decimal!]]

	in-out-quad:    [{Modeled after the piecewise parabola  y = (1/2)((2x)^^2) for x [0, 0.5) and y = -(1/2)((2x-1)*(2x-3) - 1) for x [0.5, 1]} x [decimal!]]
	in-out-cubic:   [{Modeled after the piecewise cubic     y = (1/2)((2x)^^3) for x [0, 0.5) and y = (1/2)((2x-2)^^3 + 2) for x [0.5, 1]} x [decimal!]]
	in-out-quart:   [{Modeled after the piecewise quartic   y = (1/2)((2x)^^4) for x [0, 0.5) and y = (1/2)((2x-2)^^4 + 2) for x [0.5, 1]} x [decimal!]]
	in-out-quint:   [{Modeled after the piecewise quintic   y = (1/2)((2x)^^5) for x [0, 0.5) and y = (1/2)((2x-2)^^5 + 2) for x [0.5, 1]} x [decimal!]]
	in-out-sine:    [{Modeled after half sine wave} x [decimal!]]
	in-out-circ:    [{Modeled after the piecewise circular function y = (1/2)(1 - sqrt(1 - 4x^^2)) for x [0, 0.5) and y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) for x [0.5, 1]} x [decimal!]]
	in-out-expo:    [{Modeled after the piecewise exponential y = (1/2)2^^(10(2x - 1)) for x [0, 0.5) and y = -(1/2)*2^^(-10(2x - 1)) + 1 for x [0.5, 1]} x [decimal!]]
	in-out-elastic: [{Modeled after the piecewise exponentially-damped sine wave y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1)) for x [0, 0.5) and y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) for x [0.5, 1]} x [decimal!]]
	in-out-back:    [{Modeled after the piecewise overshooting cubic function y = (1/2)*((2x)^^3-(2x)*sin(2*x*pi)) for x [0, 0.5) and y = (1/2)*(1-((1-x)^^3-(1-x)*sin((1-x)*pi))+1) for x [0.5, 1]} x [decimal!]]
	in-out-bounce:  [{Modeled after the bounce applied to each half of the range} x [decimal!]]
]

;; ---------------------------------------------------------------------------
;; Module body. Previously a C string literal with escaped newlines - as a
;; block it is ordinary Rebol code that an editor can indent and check.
mezzanine: [
	tween: func [
		{Interpolates a value at t using easing function}
		val1     [number! pair! tuple!] "Value to interpolate from"
		val2     [number! pair! tuple!] "Value to interpolate to"
		t        [decimal!]             "Value from 0.0 to 1.0"
		ease     [any-word!]            "Easing function"
	][
		t: self/:ease t
		to val1 add val1 * (1 - t) val2 * t
	]
]
