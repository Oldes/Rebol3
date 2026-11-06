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
**  Module:  p-console.c
**  Summary: console port interface
**  Section: ports
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"


#define OUT_BUF_SIZE 32*1024

#ifndef DEFAULT_WINDOW_COLS
#define DEFAULT_WINDOW_COLS 80 // used when reported console width is zero (not available)
#endif

// Does OS use wide chars or byte chars (UTF-8):
#ifdef OS_WIDE_CHAR
#define MAKE_OS_BUFFER Make_Unicode
#else
#define MAKE_OS_BUFFER Make_Binary
#endif


/***********************************************************************
**
*/	static REBOOL Set_Console_Mode_Value(REBREQ *req, REBCNT mode, REBVAL *ret)
/*
**		Set a value with file data according specified mode
**
***********************************************************************/
{
	switch (mode) {
	case SYM_BUFFER_COLS:
		SET_INTEGER(ret, req->console.buffer_cols);
		break;
	case SYM_BUFFER_ROWS:
		SET_INTEGER(ret, req->console.buffer_rows);
		break;
	case SYM_WINDOW_COLS:
		if (req->console.window_cols == 0)
			req->console.window_cols = DEFAULT_WINDOW_COLS;
		SET_INTEGER(ret, req->console.window_cols);
		break;
	case SYM_WINDOW_ROWS:
		SET_INTEGER(ret, req->console.window_rows);
		break;
	case SYM_LENGTH:
		SET_INTEGER(ret, req->console.length);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


static REBOOL Query_Console_Field(REBVAL *port, REBVAL *word, REBVAL *ret, void *ctx)
{
	return Set_Console_Mode_Value((REBREQ *)ctx, VAL_WORD_CANON(word), ret);
}

/***********************************************************************
**
*/	static int Console_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	REBSER *port;
	REBREQ *req;
	REBINT result;
    REBVAL *arg;
	REBSER *ser;
	REBCNT args = 0;
	REBVAL *spec;

	port = Validate_Port_Value(port_value);

	arg = D_ARG(2);

	//O: known limitation: works only with default system's imput port (not for custom console ports) 
	req = Std_IO;
	req->port = port;

	switch (action) {

	case A_READ:
		// If not open, open it:
		if (!IS_OPEN(req)) {
			if (OS_Do_Device(req, RDC_OPEN)) Trap_Port(RE_CANNOT_OPEN, port, req->error);
		}

		// If no buffer, create a buffer:
		arg = OFV(port, STD_PORT_DATA);
		if (!IS_STRING(arg) && !IS_BINARY(arg)) {
			Set_Binary(arg, MAKE_OS_BUFFER(OUT_BUF_SIZE));
		}
		ser = VAL_SERIES(arg);
		RESET_SERIES(ser);

		req->data = BIN_HEAD(ser);
		req->length = SERIES_AVAIL(ser);

		result = OS_Do_Device(req, RDC_READ);
		if (result < 0) Trap_Port(RE_READ_ERROR, port, req->error);

		if (req->actual == 1 && req->data[0] == '\x1B') return R_NONE; // CTRL-C

		if (GET_FLAG(req->modes, RDM_READ_LINE) && req->actual > 0 && req->data[req->actual - 1] == '\n') {
#ifdef TO_WINDOWS
			if (req->actual > 1 && req->data[req->actual-2]== '\r') req->actual -= 2; // remove CRLF from tail
#else
			req->actual -= 1; // remove LF from tail
#endif
		}

		// Convert to string or block of strings.
		args = Find_Refines(ds, ALL_READ_REFS);
		if (args & (AM_READ_STRING | AM_READ_LINES)) {
			ser = Decode_UTF_String(req->data, req->actual, -1, TRUE, &req->error);
			if (!ser) return R_NONE;
			Set_String(ds, ser);
			if (args & AM_READ_LINES) Set_Block(ds, Split_Lines(ds));
		} else {
			Set_Binary(ds, Copy_Bytes(req->data, req->actual));
		}
		return R_RET;

	case A_WRITE:
		Prin_Value(arg, 0, FALSE, FALSE);
		break;

	case A_UPDATE:
		// do nothing here, no wake-up, events should be handled by user defined port's awake function
		// ==>> SYSTEM/PORTS/INPUT/SCHEME/AWAKE
		return R_NONE;

	case A_OPEN:
		// ?? why???
		if (OS_Do_Device(req, RDC_OPEN)) Trap_Port(RE_CANNOT_OPEN, port, req->error);
		SET_OPEN(req);
		break;

	case A_CLOSE:
		SET_CLOSED(req);
		//OS_Do_Device(req, RDC_CLOSE);
		break;

	case A_OPENQ:
		if (IS_OPEN(req)) return R_TRUE;
		return R_FALSE;

	case A_MODIFY:
		if (IS_WORD(arg)) {
			switch (VAL_WORD_CANON(arg)) {
				case SYM_ECHO:  req->modify.mode = MODE_CONSOLE_ECHO; break;
				case SYM_LINE:  req->modify.mode = MODE_CONSOLE_LINE; break;
				case SYM_ERROR: req->modify.mode = MODE_CONSOLE_ERROR; break;
				default: Trap1(RE_BAD_FILE_MODE, arg);
			}
			spec = D_ARG(3);
			if (!IS_LOGIC(spec)) Trap2(RE_INVALID_VALUE_FOR, spec, arg);
			req->modify.value = VAL_LOGIC(spec);
			OS_Do_Device(req, RDC_MODIFY);
		} else Trap1(RE_BAD_FILE_MODE, arg);
		return R_ARG3;

	case A_QUERY:
		spec = Get_System(SYS_STANDARD, STD_CONSOLE_INFO);
		arg  = D_ARG(ARG_QUERY_FIELD);
		if (!IS_NONE(arg) && OS_Do_Device(req, RDC_QUERY) < 0) {
			if (req->error == 25) return R_NONE; //Inappropriate ioctl for device (not running in terminal)
			SET_INTEGER(D_ARG(2), req->error);
			Trap1(RE_PROTOCOL, D_ARG(2));
		}
		Query_Fields(port_value, arg, spec, Query_Console_Field, req, D_RET);
		return R_RET;

	case A_FLUSH:
		OS_Do_Device(req, RDC_FLUSH);
		break;

	default:
		Trap1(RE_NO_PORT_ACTION, Get_Action_Word(action));
	}

	return R_ARG1; //= port
}


/***********************************************************************
**
*/	void Init_Console_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_CONSOLE, 0, Console_Actor);
}
