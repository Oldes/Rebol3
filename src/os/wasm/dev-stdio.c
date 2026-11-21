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
**  Title: Device: Standard I/O for WebAssembly (Emscripten)
**  Author: Oldes
**  Purpose:
**      Output is passed to JavaScript (Module.onRebolOutput), input
**      line is requested synchronously using window.prompt.
**
**  Notes:
**      - There is no terminal: no termios, no signals, no key reading.
**      - Reading a single key (read-key mode) is not supported in the
**        synchronous build and returns an error.
**      - When the JS side has no `prompt` (e.g. in Node.js tests),
**        reading returns end of stream.
**
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <emscripten.h>

#include "reb-host.h"

#define SF_DEV_NULL 31		// local flag to mark NULL device

static FILE *Std_Echo = NULL;

extern REBDEV *Devices[];


/***********************************************************************
**
**	JavaScript side
**
***********************************************************************/

// Passes UTF-8 bytes to the page. `is_error` is 1 for stderr output.
EM_JS(void, JS_Write_Output, (const char *data, int len, int is_error), {
	var text = UTF8ToString(data, len);
	if (Module.onRebolOutput) Module.onRebolOutput(text, !!is_error);
	else if (is_error) err(text);
	else out(text);
});

// Asks for one line of input. Returns a malloced UTF-8 string
// (without line terminator), or 0 when cancelled or unavailable.
EM_JS(char*, JS_Read_Line, (void), {
	var line = null;
	if (Module.onRebolInput) line = Module.onRebolInput();
	else if (typeof prompt === 'function') line = prompt("");
	if (line === null || line === undefined) return 0;
	return stringToNewUTF8(String(line));
});

// Console width used by Rebol for formatting (help, probe...).
EM_JS(int, JS_Console_Cols, (void), {
	return Module.consoleCols || 80;
});

EM_JS(int, JS_Console_Rows, (void), {
	return Module.consoleRows || 24;
});


/***********************************************************************
**
*/	DEVICE_CMD Quit_IO(REBREQ *dr)
/*
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr;

	if (Std_Echo) {
		fclose(Std_Echo);
		Std_Echo = 0;
	}
	OS_Close_StdIO(); // frees host's input buffer

	CLR_FLAG(dev->flags, RDF_OPEN);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Open_IO(REBREQ *req)
/*
***********************************************************************/
{
	REBDEV *dev = Devices[req->device];

	if (GET_FLAG(dev->flags, RDF_OPEN)) {
		if (GET_FLAG(dev->flags, SF_DEV_NULL))
			SET_FLAG(req->modes, RDM_NULL);
		SET_FLAG(req->flags, RRF_OPEN);
		return DR_DONE;
	}

	if (GET_FLAG(req->modes, RDM_NULL))
		SET_FLAG(dev->flags, SF_DEV_NULL);

	SET_FLAG(req->flags, RRF_OPEN);
	SET_FLAG(dev->flags, RDF_OPEN);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Write_IO(REBREQ *req)
/*
***********************************************************************/
{
	if (GET_FLAG(req->modes, RDM_NULL)) {
		req->actual = req->length;
		return DR_DONE;
	}

	JS_Write_Output((const char*)req->data, (int)req->length, GET_FLAG(req->flags, RRF_ERROR) ? 1 : 0);
	CLR_FLAG(req->flags, RRF_FLUSH); // nothing is buffered here
	req->actual = req->length;

	if (Std_Echo) fwrite(req->data, req->length, 1, Std_Echo);

	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Read_IO(REBREQ *req)
/*
**		Reads one line (line mode only). The result is LF terminated
**		like on other platforms, but NOT null terminated.
**		Zero actual means end of stream.
**
***********************************************************************/
{
	char *line;
	REBCNT len;

	req->actual = 0;

	if (GET_FLAG(req->modes, RDM_NULL)) {
		if (req->length > 0) req->data[0] = 0;
		return DR_DONE;
	}

	if (!GET_FLAG(req->modes, RDM_READ_LINE)) {
		// read-key is not possible without waiting for the browser
		req->error = -1;
		return DR_ERROR;
	}

	line = JS_Read_Line();
	if (!line) return DR_DONE; // end of stream

	len = (REBCNT)strlen(line);
	if (req->length == 0) {
		free(line);
		return DR_DONE;
	}
	if (len > req->length - 1) len = req->length - 1; // keep space for LF
	memcpy(req->data, line, len);
	req->data[len++] = LF;
	req->actual = len;
	free(line);

	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Query_IO(REBREQ *req)
/*
***********************************************************************/
{
	req->console.window_rows =
	req->console.buffer_rows = JS_Console_Rows();
	req->console.window_cols =
	req->console.buffer_cols = JS_Console_Cols();
	req->console.length = 0; // no buffered input
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Modify_IO(REBREQ *req)
/*
**		Only the line mode flag is tracked. Other modes are accepted
**		and ignored (there is no terminal to modify).
**
***********************************************************************/
{
	REBOOL value = (REBOOL)req->modify.value;

	switch (req->modify.mode) {
		case MODE_CONSOLE_LINE:
			ASSIGN_FLAG(req->modes, RDM_READ_LINE, value);
			break;
		case MODE_CONSOLE_ECHO:
		case MODE_CONSOLE_ERROR:
		default:
			break;
	}
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Open_Echo(REBREQ *req)
/*
**		Open a file (in the virtual file system) for console echo.
**
***********************************************************************/
{
	if (Std_Echo) {
		fclose(Std_Echo);
		Std_Echo = 0;
	}
	if (req->file.path) {
		Std_Echo = fopen(req->file.path, "w");
		if (!Std_Echo) {
			req->error = errno;
			return DR_ERROR;
		}
	}
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Flush_IO(REBREQ *req)
/*
***********************************************************************/
{
	if (Std_Echo) fflush(Std_Echo);
	return DR_DONE;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
	0,	// init
	Quit_IO,
	Open_IO,
	0,	// close
	Read_IO,
	Write_IO,
	0,	// poll
	0,	// connect
	Query_IO,
	Modify_IO,
	Open_Echo,	// CREATE used for opening echo file
	0,	// delete
	0,	// rename
	0,	// lookup
	Flush_IO
};

DEFINE_DEV(Dev_StdIO, "Standard IO", 1, Dev_Cmds, RDC_MAX, 0);