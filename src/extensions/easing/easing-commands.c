//
// Project: Rebol/Easing extension
// SPDX-License-Identifier: Unlicense
// ===========================================================================
// Command implementations. One function per command; the enum, the
// declarations, the dispatch table and the module init code are all
// generated from easing.reb into gen-easing.c / gen-easing.h.
//
// COMMAND and ARG_Double come from the shared reb-ext-common.h part of the
// extension API header - they used to be #defined locally here.
//
// Every command overwrites its own first argument and returns RXR_VALUE,
// so the eased value is returned in place. The input is not validated;
// see the note in the specification.
//

#include "gen-easing.h"


//== no easing ================================================================

// LinearInterpolation() is the identity, so the argument is returned as it
// came in - there is nothing to compute.
COMMAND cmd_easing_linear(RXIFRM *frm, void *ctx) {
	return RXR_VALUE;
}


//== quadratic ================================================================

COMMAND cmd_easing_in_quad(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuadraticEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_quad(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuadraticEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_quad(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuadraticEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== cubic ====================================================================

COMMAND cmd_easing_in_cubic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CubicEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_cubic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CubicEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_cubic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CubicEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== quartic ==================================================================

COMMAND cmd_easing_in_quart(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuarticEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_quart(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuarticEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_quart(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuarticEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== quintic ==================================================================

COMMAND cmd_easing_in_quint(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuinticEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_quint(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuinticEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_quint(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = QuinticEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== sine =====================================================================

COMMAND cmd_easing_in_sine(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = SineEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_sine(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = SineEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_sine(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = SineEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== circular =================================================================

COMMAND cmd_easing_in_circ(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CircularEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_circ(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CircularEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_circ(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = CircularEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== exponential ==============================================================

COMMAND cmd_easing_in_expo(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ExponentialEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_expo(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ExponentialEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_expo(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ExponentialEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== elastic ==================================================================

COMMAND cmd_easing_in_elastic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ElasticEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_elastic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ElasticEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_elastic(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = ElasticEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== back =====================================================================

COMMAND cmd_easing_in_back(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BackEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_back(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BackEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_back(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BackEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}


//== bounce ===================================================================

COMMAND cmd_easing_in_bounce(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BounceEaseIn(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_out_bounce(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BounceEaseOut(ARG_Double(1));
	return RXR_VALUE;
}
COMMAND cmd_easing_in_out_bounce(RXIFRM *frm, void *ctx) {
	RXA_DEC64(frm, 1) = BounceEaseInOut(ARG_Double(1));
	return RXR_VALUE;
}
