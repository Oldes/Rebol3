//
// Project: Rebol/XTest extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Shared between the entry points (xtest.c) and the command sources.
//
// The XTEST struct and the `Handle_XTest` extern come from the generated
// header, via the specification's `c-header:` field.
//

#ifndef XTEST_EXT_H
#define XTEST_EXT_H

// Callbacks of the XTEST context handle, named after the handle type - not
// after the extension, which owns Xtest_Init() and the generated symbols.
// Registered by Xtest_Init(); implemented in xtest-commands.c.
int XTestContext_free(void *hndl);
int XTestContext_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int XTestContext_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int XTestContext_mold(REBHOB *hob, REBSER *str);

// Xtest_Init() is declared in gen-xtest.h - the generated `_init` handler
// calls it, and every extension is required to define one.

#ifndef REB_EXT
// Embedded build only - called from host-main.c under INCLUDE_EXT_XTEST.
RL_API void OS_Init_Ext_XTest(void);
#endif

#endif // XTEST_EXT_H