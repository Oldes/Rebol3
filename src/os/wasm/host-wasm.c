/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2026 Rebol Open Source Contributors
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Title: Host environment entry points for WebAssembly (Emscripten)
**  Author: Oldes
**  Purpose:
**      Replaces host-main.c for the browser. There is no main() and
**      no console loop. JavaScript calls:
**
**          r3_init()      - initialize and start the interpreter (once)
**          r3_do(source)  - evaluate UTF-8 source and print the result
**                           (the same as one console line)
**
**      Evaluation uses the same context as the console, so values
**      set by one r3_do call are visible in the next one.
**
**      Optional command line arguments may be passed from the page as
**      an array of strings in Module.rebolArgs (for example: ["-q"]).
**      They are processed the same way as on other platforms.
**
**      Optional boot code may be passed from the page as a string in
**      Module.rebolBoot (for example: "secure none"). It is evaluated
**      once by r3_init, after the startup. Only errors are printed.
**
**      All output goes through the stdio device (os/wasm/dev-stdio.c),
**      which forwards it to Module.onRebolOutput in JavaScript.
**
**  Notes:
**      This build is synchronous: nothing here may wait for the
**      browser event loop. An async build (Asyncify or JSPI) may come
**      later; keep r3_do as the only evaluation entry, so it is the
**      single place that needs wrapping then.
**
**      The functions below intentionally do not use the OS_API/RL_API
**      header markers, so the make tools do not collect them into
**      the host-lib table.
**
***********************************************************************/

#if !defined(REBOL_OPTIONS_FILE)
#include "opt-config.h"
#else
#include REBOL_OPTIONS_FILE
#endif

#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#define OS_LIB_TABLE		// include the host-lib dispatch table

#include "reb-host.h"		// standard host include files

#include "gen-ext-init.h"	// entry points of the embedded extensions

// The page is not a terminal, but the JS side converts basic ANSI
// colors, so the same result prefix as in the console is used.
#ifdef COLOR_CONSOLE
#define RESULT_STR (REBYTE*)"\x1B[32m==\x1B[1;32m "
#define RESET_COLOR OS_Put_Str(b_cast("\x1B[0m"))
#else
#define RESULT_STR (REBYTE*)"== "
#define RESET_COLOR
#endif

static int R3_Ready = 0;

// Returns a malloced copy of Module.rebolBoot followed by an empty paren,
// so the evaluation ends with UNSET and only errors get printed.
// Returns 0 when there is no boot code.
EM_JS(char*, JS_Boot_Code, (void), {
	var code = Module.rebolBoot;
	if (typeof code !== 'string' || !code.trim()) return 0;
	return stringToNewUTF8(code + '\n()');
});

// Number of strings in Module.rebolArgs (0 when not provided).
EM_JS(int, JS_Arg_Count, (void), {
	var args = Module.rebolArgs;
	return Array.isArray(args) ? args.length : 0;
});

// Returns a malloced UTF-8 copy of Module.rebolArgs[index].
EM_JS(char*, JS_Arg, (int index), {
	return stringToNewUTF8(String(Module.rebolArgs[index]));
});

// Builds a null terminated argv: "rebol3" followed by Module.rebolArgs.
// The result is never released, because the parsed options keep
// pointers into it (see OS_Parse_Args in host-args.c).
static char **Make_Argv(int *argc)
{
	int n = JS_Arg_Count();
	int i;
	char **argv = malloc((n + 2) * sizeof(char *));

	if (!argv) Host_Crash("Failed to allocate arguments");
	argv[0] = "rebol3";
	for (i = 0; i < n; i++) argv[i + 1] = JS_Arg(i);
	argv[n + 1] = NULL;
	*argc = n + 1;
	return argv;
}


//
// Initialize the Rebol library with arguments from Module.rebolArgs,
// run the startup code and the optional boot code from Module.rebolBoot.
// Safe to call more than once (only the first call does the work).
// Returns the RL_Start result (0: ok, -1: error, 1: bad data).
//
EMSCRIPTEN_KEEPALIVE int r3_init(void)
{
	char **argv;
	int argc;
	REBYTE vers[8];
	REBARGS *args;
	REBINT n;
	char *boot;

	if (R3_Ready) return 0;

	vers[0] = 5; // len
	RL_Version(&vers[0]);
	if (vers[1] != RL_VER || vers[2] != RL_REV) Host_Crash("Incompatible reb-lib");

	argv = Make_Argv(&argc);
	args = RL_Init(argc, argv);
	if (!args) Host_Crash("Failed to initialize Rebol library");

	INIT_EMBEDDED_EXTENSIONS();

	n = RL_Start(0, 0, 0);
	RESET_COLOR;
	R3_Ready = 1;

	boot = JS_Boot_Code();
	if (boot) {
		RL_Do_String((REBYTE *)boot, 0, 0);
		RL_Print_TOS(TRUE, RESULT_STR); // prints nothing unless it failed
		RESET_COLOR;
		free(boot);
	}
	return n;
}


//
// Evaluate UTF-8 source and print the result, like one console line.
// Returns the RL_Do_String result, or -1 when not initialized.
//
EMSCRIPTEN_KEEPALIVE int r3_do(const char *source)
{
	REBINT result;

	if (!R3_Ready || !source) return -1;

	result = RL_Do_String((REBYTE *)source, 0, 0);
	RL_Print_TOS(TRUE, RESULT_STR);
	RESET_COLOR;
	return result;
}