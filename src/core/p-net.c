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
**  Module:  p-net.c
**  Summary: network port interface
**  Section: ports
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"

#include "reb-net.h"
#include "reb-evtypes.h"

#define NET_BUF_SIZE 32*1024

// The port state holds two requests (see PORT_STATE_REQS in c-port.c).
// The first one is used for everything but WRITE, which uses the second
// one, so a READ and a WRITE may be pending at the same time.
#define NET_WRITE_REQ(sock) ((sock) + 1)

/***********************************************************************
**
*/	static REBHOB *Net_State_Hob(REBSER *port)
/*
**		Returns the state handle context of an open net port.
**		Its series keeps the data of the pending WRITE (GC safe).
**
***********************************************************************/
{
	return VAL_HANDLE_CTX(OFV(port, STD_PORT_STATE));
}

enum Transport_Types {
	TRANSPORT_TCP,
	TRANSPORT_UDP
};

/***********************************************************************
**
*/	static REBOOL Set_Net_Mode_Value(REBREQ *sock, REBCNT mode, REBVAL *ret)
/*
**		Set a value with net data according specified mode
**
***********************************************************************/
{
	switch (mode) {
	case SYM_REMOTE_IP:
		Set_Tuple(ret, (REBYTE *)&sock->net.remote_ip, 4);
		break;
	case SYM_REMOTE_PORT:
		SET_INTEGER(ret, sock->net.remote_port);
		break;
	case SYM_LOCAL_IP:
		Set_Tuple(ret, (REBYTE *)&sock->net.local_ip, 4);
		break;
	case SYM_LOCAL_PORT:
		SET_INTEGER(ret, sock->net.local_port);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


static REBOOL Query_Net_Field(REBVAL *port, REBVAL *word, REBVAL *ret, void *ctx)
{
	return Set_Net_Mode_Value((REBREQ *)ctx, VAL_WORD_CANON(word), ret);
}


/***********************************************************************
**
*/	void Ret_Net_Modes(REBSER *port, REBVAL *ret)
/*
**		Sets value with block of possible net mode names
**
***********************************************************************/
{
	Set_Block(ret, Get_Object_Words(In_Object(port, STD_PORT_SCHEME, STD_SCHEME_INFO, 0)));
}


/***********************************************************************
**
*/	static void Accept_New_Port(REBVAL *ds, REBSER *port, REBREQ *sock)
/*
**		Clone a listening port as a new accept port.
**
***********************************************************************/
{
	REBREQ *nsock;
	REBVAL *state;

	// Get temp sock struct created by the device:
	nsock = sock->sock;
	if (!nsock) return;  // false alarm
	sock->sock = nsock->next;
	nsock->data = 0;
	nsock->next = 0;

	// Create a new port using ACCEPT request passed by sock->sock:
	port = Copy_Block(port, 0);
	SET_PORT(DS_RETURN, port);	// Also for GC protect
	SET_NONE(OFV(port, STD_PORT_DATA)); // just to be sure.
	SET_NONE(OFV(port, STD_PORT_STATE)); // just to be sure.

	// Copy over the new sock data:
	state = Use_Port_State_Handle(port, RDI_NET);
	sock = (REBREQ *)VAL_HANDLE_CONTEXT_DATA(state);
	*sock = *nsock;
	//sock->clen = sizeof(*sock);
	sock->port = port;
	OS_Free(nsock); // allocated by dev_net.c (MT issues?)
}

/***********************************************************************
**
*/	static int Transport_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action, enum Transport_Types proto)
/*
***********************************************************************/
{
	REBSER *port;
	REBREQ *sock;	// IO request
	REBREQ *wreq;	// IO request used for WRITE
	REBHOB *hob;	// state handle context (keeps the WRITE data)
	REBVAL *spec;	// port spec
	REBVAL *arg;	// action argument value
	REBVAL *val;	// e.g. port number value
	REBINT result;	// IO result
	REBCNT refs;	// refinement argument flags
	REBCNT len;		// generic length
	REBSER *ser;	// simplifier

	port = Validate_Port_With_Request(port_value, RDI_NET, &sock);

	*D_RET = *D_ARG(1);
	arg = D_ARG(2);

	if (proto == TRANSPORT_UDP) {
		SET_FLAG(sock->modes, RST_UDP);
	}
	//Debug_Fmt("Sock: %x", sock);
	spec = OFV(port, STD_PORT_SPEC);
	if (!IS_OBJECT(spec)) Trap0(RE_INVALID_PORT);

	// sock->timeout = 4000; // where does this go? !!!

	// HOW TO PREVENT OVERWRITE DURING BUSY OPERATION!!!
	// Should it just ignore it or cause an error?

	// Actions for an unopened socket:
	if (!IS_OPEN(sock)) {

		switch (action) {	// Ordered by frequency

		case A_OPEN:

			arg = Obj_Value(spec, STD_PORT_SPEC_NET_HOST);
			val = Obj_Value(spec, STD_PORT_SPEC_NET_PORT);

			Check_Security(SYM_NET, POL_EXEC, Obj_Value(spec, STD_PORT_SPEC_NET_REF));

			if (OS_Do_Device(sock, RDC_OPEN)) Trap_Port(RE_CANNOT_OPEN, port, -12);
			SET_OPEN(sock);

			// Lookup host name (an extra TCP device step):
			if (IS_STRING(arg)) {
				sock->data = VAL_BIN(arg);
				sock->net.remote_port = IS_INTEGER(val) ? VAL_INT32(val) : 80;
				result = OS_Do_Device(sock, RDC_LOOKUP);  // sets remote_ip field
				if (result < 0) Trap_Port(RE_NO_CONNECT, port, sock->error);
				return R_RET;
			}

			// Host IP specified:
			else if (IS_TUPLE(arg)) {
				sock->net.remote_port = IS_INTEGER(val) ? VAL_INT32(val) : 80;
				memcpy(&sock->net.remote_ip, VAL_TUPLE(arg), 4);
				break;
			}

			// No host, must be a LISTEN socket:
			else if (IS_NONE(arg)) {
				SET_FLAG(sock->modes, RST_LISTEN);
				sock->data = 0; // where ACCEPT requests are queued
				sock->net.local_port = IS_INTEGER(val) ? VAL_INT32(val) : 8000;
				break;
			}
			else Trap_Port(RE_INVALID_SPEC, port, -10);

		case A_CLOSE:
			return R_RET;

		case A_OPENQ:
			return R_FALSE;

		case A_UPDATE:	// allowed after a close
			break;

		default:
			Trap_Port(RE_NOT_OPEN, port, -12);
		}
	}

	// Actions for an open socket:
	switch (action) {	// Ordered by frequency

	case A_UPDATE:
		// Update the port object after a READ or WRITE operation.
		// This is normally called by the WAKE-UP function.
		arg = OFV(port, STD_PORT_DATA);
		if (sock->command == RDC_READ) {
			// UPDATE is called for every event of the port, so the received
			// length must be consumed, else it would be appended again.
			if (ANY_BINSTR(arg)) VAL_TAIL(arg) += sock->actual;
			sock->actual = 0;
		}
		// Release the data of a finished (or failed) WRITE:
		if (!GET_FLAG(NET_WRITE_REQ(sock)->flags, RRF_PENDING))
			Net_State_Hob(port)->series = NULL;
		return R_NONE;

	case A_READ:
		// Read data into a buffer, expanding the buffer if needed.
		// If no length is given, program must stop it at some point.
		//refs = Find_Refines(ds, ALL_READ_REFS);
		if (!GET_FLAG(sock->modes, RST_UDP)
				&& !GET_FLAG(sock->state, RSM_CONNECT))
			Trap_Port(RE_NOT_CONNECTED, port, -15);

		// Setup the read buffer (allocate a buffer if needed):
		arg = OFV(port, STD_PORT_DATA);
		if (!IS_STRING(arg) && !IS_BINARY(arg)) {
			Set_Binary(arg, Make_Binary(NET_BUF_SIZE));
		}
		ser = VAL_SERIES(arg);
		sock->length = SERIES_AVAIL(ser); // space available
		if (sock->length < NET_BUF_SIZE/2) Extend_Series(ser, NET_BUF_SIZE);
		sock->length = SERIES_AVAIL(ser);
		sock->data = STR_TAIL(ser); // write at tail
		//if (SERIES_TAIL(ser) == 0)
		sock->actual = 0;  // Actual for THIS read, not for total.

		//Print("(max read length %d)", sock->length);
		result = OS_Do_Device(sock, RDC_READ); // recv can happen immediately
		if (result < 0)
			Trap_Port(RE_READ_ERROR, port, sock->error);
		if (result == DR_DONE && GET_FLAG(sock->modes, RST_UDP)) {
			// Make the received datagram visible right away. The device has
			// already queued the READ event, so mark the data as consumed,
			// else the UPDATE action would append the same length again.
			VAL_TAIL(arg) += sock->actual;
			sock->actual = 0;
		}
		break;

	case A_WRITE:
		// Write the entire argument string to the network.
		// The lower level write code continues until done.

		refs = Find_Refines(ds, ALL_WRITE_REFS);
		if (!GET_FLAG(sock->modes, RST_UDP)
				&& !GET_FLAG(sock->state, RSM_CONNECT))
			Trap_Port(RE_NOT_CONNECTED, port, -15);

		// Determine length. Clip /PART to size of string if needed.
		spec = D_ARG(2);
		len = VAL_LEN(spec);
		if (refs & AM_WRITE_PART) {
			REBCNT n = Int32s(D_ARG(ARG_WRITE_LENGTH), 0);
			if (n <= len) len = n;
		}

		// WRITE has its own request, so it does not disturb a pending READ.
		wreq = NET_WRITE_REQ(sock);
		hob = Net_State_Hob(port);

		// TCP: append to the data of a still pending WRITE (keeps the order).
		// (UDP: a pending datagram is replaced, as it was before.)
		if (GET_FLAG(wreq->flags, RRF_PENDING) && hob->series
				&& !GET_FLAG(sock->modes, RST_UDP)) {
			Append_Bytes_Len(hob->series, VAL_BIN_DATA(spec), len);
			wreq->data = BIN_HEAD(hob->series) + wreq->actual; // may be reallocated
			wreq->length += len;
			break;
		}

		// Compatibility: a WRITE used to drop the read buffer (it was used
		// for the written data). Keep it only when a READ is in progress
		// or when its received data were not yet processed by UPDATE.
		if (!(GET_FLAG(sock->flags, RRF_PENDING) && sock->command == RDC_READ)
				&& sock->actual == 0)
			SET_NONE(OFV(port, STD_PORT_DATA));

		// Setup the write (with a copy of the data, so it is GC safe
		// and it cannot be modified while it is being sent):
		hob->series = Copy_Bytes(VAL_BIN_DATA(spec), len);
		wreq->socket = sock->socket;
		wreq->modes  = sock->modes;
		wreq->state  = sock->state;  // RSM_CONNECT
		wreq->net    = sock->net;    // remote address used by UDP
		wreq->length = len;
		wreq->data   = BIN_HEAD(hob->series);
		wreq->actual = 0;

		//Print("(write length %d)", len);
		result = OS_Do_Device(wreq, RDC_WRITE); // send can happen immediately
		if (result <= 0) hob->series = NULL; // done or failed
		if (result < 0) Trap_Port(RE_WRITE_ERROR, port, wreq->error);
		break;

	case A_PICK:
		// FIRST server-port returns new port connection.
		len = Get_Num_Arg(arg); // Position
		// Only a TCP listen port has an accept queue in sock->data.
		// A UDP listen port uses sock->data for its read buffer!
		if (len == 1 && GET_FLAG(sock->modes, RST_LISTEN)
				&& !GET_FLAG(sock->modes, RST_UDP) && sock->data)
			Accept_New_Port(ds, port, sock); // sets D_RET
		else
			Trap_Range(arg);
		break;

	case A_QUERY: {
		// Get specific information - the scheme's info object.
		// Special notation allows just getting part of the info.
		REBVAL *info = In_Object(port, STD_PORT_SCHEME, STD_SCHEME_INFO, 0);
		if (!info || !IS_OBJECT(info)) Trap_Port(RE_INVALID_SPEC, port, -10);
		Query_Fields(D_ARG(1), D_ARG(ARG_QUERY_FIELD), info, Query_Net_Field, sock, D_RET);
		break;
	}
	case A_OPENQ:
		// Connect for clients, bind for servers:
		if (sock->state & ((1<<RSM_CONNECT) | (1<<RSM_BIND))) return R_TRUE;
		return R_FALSE;

	case A_CLOSE:
		if (IS_OPEN(sock)) {
			// A pending WRITE must not be polled with the closed socket:
			OS_Abort_Device(NET_WRITE_REQ(sock));
			Net_State_Hob(port)->series = NULL;
            if (OS_Do_Device(sock, RDC_CLOSE) < 0) {
                Trap_Port(RE_CANNOT_CLOSE, port, sock->error);
            }
			SET_CLOSED(sock);
			// Don't release the state as there may be pending events (sock being signaled)!
			//Release_Port_State(port); // Crashes on POSIX with the unit test for issue-2445!
		}
		break;

	case A_LENGTHQ:
		arg = OFV(port, STD_PORT_DATA);
		len = ANY_SERIES(arg) ? VAL_TAIL(arg) : 0;
		SET_INTEGER(D_RET, len);
		break;

	case A_OPEN:
		result = OS_Do_Device(sock, RDC_CONNECT);
		if (result < 0) Trap_Port(RE_NO_CONNECT, port, sock->error);
		break;
		//Trap_Port(RE_ALREADY_OPEN, port);

	case A_DELETE: // Temporary to TEST error handler!
		{
			REBVAL *event = Append_Event();		// sets signal
			VAL_SET(event, REB_EVENT);		// (has more space, if we need it)
			VAL_EVENT_TYPE(event) = EVT_ERROR;
			VAL_EVENT_DATA(event) = 101;
			VAL_EVENT_REQ(event) = sock;
		}
		break;

	default:
		Trap1(RE_NO_PORT_ACTION, Get_Action_Word(action));
	}

	return R_RET;
}

/***********************************************************************
**
*/	static int TCP_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	return Transport_Actor(ds, port_value, action, TRANSPORT_TCP);
}

/***********************************************************************
**
*/	static int UDP_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	return Transport_Actor(ds, port_value, action, TRANSPORT_UDP);
}

/***********************************************************************
**
*/	void Init_TCP_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_TCP, 0, TCP_Actor);
}
/***********************************************************************
**
*/	void Init_UDP_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_UDP, 0, UDP_Actor);
}