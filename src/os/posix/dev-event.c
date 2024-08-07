/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
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
**  Title: Device: Event handler for Posix
**  Author: Carl Sassenrath
**  Purpose:
**      Processes events to pass to REBOL. Note that events are
**      used for more than just windowing.
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>

#include "reb-host.h"
#include "host-lib.h"

#ifdef REB_VIEW
#include <gtk/gtk.h>
extern GMainContext *GTKCtx;
extern REBINT exit_loop;
#else
REBINT exit_loop = 0;
#endif

extern struct pollfd poller; // currently in dev-stdio.c

void Done_Device(int handle, int error);

/***********************************************************************
**
*/	DEVICE_CMD Init_Events(REBREQ *dr)
/*
**		Initialize the event device.
**
**		Create a hidden window to handle special events,
**		such as timers and async DNS.
**
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr; // just to keep compiler happy
	SET_FLAG(dev->flags, RDF_INIT);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Poll_Events(REBREQ *req)
/*
**		Poll for events and process them.
**		Returns 1 if event found, else 0.
**
***********************************************************************/
{
	int flag = DR_DONE;
#ifdef REB_VIEW
	//X_Event_Loop(-1);
	//puts("Poll_Events");
	if (exit_loop > 0) {
		while(g_main_context_pending(GTKCtx)) {
			flag = DR_PEND;
			//printf(".");
			g_main_context_iteration(GTKCtx, FALSE);
		}
	}
#endif
	return flag;	// different meaning compared to most commands
}


/***********************************************************************
**
*/	DEVICE_CMD Query_Events(REBREQ *req)
/*
**		Wait for an event, or a timeout (in milliseconds) specified by
**		req->length. The latter is used by WAIT as the main timing
**		method.
**
***********************************************************************/
{
	struct timeval tv = {0,0};

#ifdef REB_VIEW
	//TODO: process GUI events!!!
#endif

	tv.tv_usec = req->length * 1000; // converts ms to us
	if (select(0, 0, 0, 0, &tv) < 0) {
		if (errno == EINTR) return DR_DONE; // Ctrl-C interrupts a timer on a WAIT
		req->error = errno; // report the error code
		#ifdef _DEBUG
			printf("select() returned -1 in dev-event.c (I/O error!)\n");
		#endif
		return DR_ERROR;
	}
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
	return DR_PEND;	// keep pending
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] = {
	Init_Events,			// init device driver resources
	0,	// RDC_QUIT,		// cleanup device driver resources
	0,	// RDC_OPEN,		// open device unit (port)
	0,	// RDC_CLOSE,		// close device unit
	0,	// RDC_READ,		// read from unit
	0,	// RDC_WRITE,		// write to unit
	Poll_Events,
	Connect_Events,
	Query_Events,
};

DEFINE_DEV(Dev_Event, "OS Events", 1, Dev_Cmds, RDC_MAX, 0);
