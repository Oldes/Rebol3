//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `set-threads` command.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

// Blend2D declares this as an enumeration constant (BLRuntimeLimits), which
// the preprocessor cannot see - the fallback keeps older headers building.
// Safe to define here: blend2d.h has already been included above.
#ifndef BL_RUNTIME_MAX_THREAD_COUNT
#define BL_RUNTIME_MAX_THREAD_COUNT 32
#endif

// Number of threads every following rendering context is created with:
// 0 renders synchronously, 1 asynchronously on the main thread only, more
// than that uses Blend2D's thread pool. Returns the value which was set.
//
// NOTE: the count is read from argument 1 - the previous version read frame
// slot 0, which is not an argument at all but the frame's own type header.
COMMAND cmd_blend2d_set_threads(RXIFRM *frm, void *ctx) {
	REBI64 count = RXA_INT64(frm, 1);

	if (count < 0) count = 0;
	if (count > BL_RUNTIME_MAX_THREAD_COUNT) count = BL_RUNTIME_MAX_THREAD_COUNT;

	Blend2D_thread_count = (uint32_t)count;

	RXA_INT64(frm, 1) = count;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}