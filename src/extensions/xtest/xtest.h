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

// Handle callbacks, named <Type>_<op> to match the other extensions.
// Registered when the module initializes; implemented in xtest-commands.c.
int XTest_free(void *hndl);
int XTest_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int XTest_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg);
int XTest_mold(REBHOB *hob, REBSER *str);

#ifndef REB_EXT
// Embedded build only - called from host-main.c under INCLUDE_EXT_XTEST.
void Init_Ext_XTest(void);
#endif

#endif // XTEST_EXT_H
