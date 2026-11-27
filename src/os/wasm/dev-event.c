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
**  Title: Device: Event handler for WebAssembly (Emscripten)
**  Author: Oldes
**  Purpose:
**      Minimal event device for the synchronous browser build.
**
**  Notes:
**      Query_Events is used by WAIT as its timer. The synchronous
**      build cannot give control back to the browser, so it returns
**      at once and OS_Wait's caller keeps checking the elapsed time.
**      `wait 1` therefore still takes one second, but the page does
**      not repaint meanwhile.
**
**      For the async build, replace the body of Query_Events with
**      emscripten_sleep(req->length) (Asyncify) or its JSPI variant.
**
***********************************************************************/

#include "reb-host.h"
#include "host-lib.h"


/***********************************************************************
**
*/	DEVICE_CMD Init_Events(REBREQ *dr)
/*
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr;
	SET_FLAG(dev->flags, RDF_INIT);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Poll_Events(REBREQ *req)
/*
***********************************************************************/
{
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Query_Events(REBREQ *req)
/*
**		Wait for an event, or a timeout (in milliseconds) specified
**		by req->length. See the note at the top of this file.
**
***********************************************************************/
{
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Connect_Events(REBREQ *req)
/*
**		Simply keeps the request pending for polling purposes.
**		Use Abort_Device to remove it.
**
***********************************************************************/
{
	return DR_PEND;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] = {
	Init_Events,	// init device driver resources
	0,	// RDC_QUIT
	0,	// RDC_OPEN
	0,	// RDC_CLOSE
	0,	// RDC_READ
	0,	// RDC_WRITE
	Poll_Events,
	Connect_Events,
	Query_Events,
};

DEFINE_DEV(Dev_Event, "OS Events", 1, Dev_Cmds, RDC_MAX, 0);