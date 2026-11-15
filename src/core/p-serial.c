/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2013 REBOL Technologies
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
**  Module:  p-serial.c
**  Summary: serial port interface
**  Section: ports
**  Author:  Carl Sassenrath, Joshua Shireman, Shixin Zeng, Oldes
**  Notes:
**
***********************************************************************/

#include "sys-core.h"
#include "reb-net.h"
#include "reb-evtypes.h"

#define MAX_SERIAL_DEV_PATH 128
#define SERIAL_BUF_SIZE     32000

// The second request of the port's state is used for WRITE, so it may
// be pending while a READ is pending too (the same way as in net ports).
#define SERIAL_WRITE_REQ(req) ((req) + 1)

/***********************************************************************
**
*/	static REBHOB *Serial_State_Hob(REBSER *port)
/*
**		Returns the state handle context of an open serial port.
**		Its series keeps the data of the pending WRITE (GC safe).
**
***********************************************************************/
{
	return VAL_HANDLE_CTX(OFV(port, STD_PORT_STATE));
}

/***********************************************************************
**
*/	static void Accept_Read_Data(REBVAL *data, REBREQ *req)
/*
**		Adds the bytes received by a finished READ to the port's buffer.
**
**		The device stores the bytes at the tail which the buffer had when
**		the READ was started. A pending READ may be finished much later,
**		and the buffer may be cleared (or shortened) in the meantime, so
**		the bytes are moved to the current tail when needed.
**
***********************************************************************/
{
	REBSER *ser;
	REBYTE *tail;
	REBCNT len = req->actual;

	req->actual = 0; // consumed (UPDATE is called for every event of the port)
	if (len == 0 || !IS_BINARY(data)) return;

	ser = VAL_SERIES(data);
	tail = BIN_TAIL(ser);
	if (req->data != tail) {
		// The bytes may be used only when they are still inside the buffer
		// and behind its tail. (The buffer must not be extended while
		// a READ is pending!)
		if (req->data < tail || req->data + len > BIN_HEAD(ser) + SERIES_REST(ser)) return;
		memmove(tail, req->data, len);
	}
	SERIES_TAIL(ser) += len;
	TERM_SERIES(ser);
}


/***********************************************************************
**
*/	static int Serial_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	REBREQ *req;	// IO request
	REBREQ *wreq;	// IO request used for writing
	REBHOB *hob;	// port's state handle context
	REBVAL *spec;	// port spec
	REBVAL *arg;	// action argument value
	REBINT result;	// IO result
	REBCNT refs;	// refinement argument flags
	REBCNT len;		// generic length
	REBSER *ser;	// simplifier
	REBSER *port;
	REBCHR path[MAX_SERIAL_DEV_PATH];	// device path (used only while opening)

	port = Validate_Port_With_Request(port_value, RDI_SERIAL, &req);

	*D_RET = *D_ARG(1);

	// Validate PORT fields:
	spec = OFV(port, STD_PORT_SPEC);
	if (!IS_OBJECT(spec)) Trap0(RE_INVALID_PORT);

	// Actions for an unopened serial port:
	if (!IS_OPEN(req)) {

		switch (action) {

		case A_OPEN:
			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_PATH);  //Should Obj_Value really return a char* ?
			if (! (IS_FILE(arg) || IS_STRING(arg) || IS_BINARY(arg))) {
				Trap1(RE_INVALID_PORT_ARG, arg);
			}
			if (VAL_LEN(arg) >= MAX_SERIAL_DEV_PATH) {
				Trap1(RE_INVALID_PORT_ARG, arg); // would be truncated
			}
			// The buffer is cleared, so the path is always terminated.
			CLEAR(path, sizeof(path));
			TO_OS_STR(path, (char *) VAL_DATA(arg), MAX_SERIAL_DEV_PATH - 1);
			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_SPEED);
			if (! IS_INTEGER(arg)) {
				Trap1(RE_INVALID_PORT_ARG, arg);
			}
			req->serial.baud = VAL_INT32(arg);
			//Secure_Port(SYM_SERIAL, ???, path, ser);
			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_DATA_SIZE);
			if (!IS_INTEGER(arg)
				|| VAL_INT64(arg) < 5
				|| VAL_INT64(arg) > 8) {
				Trap1(RE_INVALID_PORT_ARG, arg);
			}
			req->serial.data_bits = VAL_INT32(arg);

			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_STOP_BITS);
			if (!IS_INTEGER(arg)
				|| VAL_INT64(arg) < 1
				|| VAL_INT64(arg) > 2) {
				Trap1(RE_INVALID_PORT_ARG, arg);
			}
			req->serial.stop_bits = VAL_INT32(arg);

			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_PARITY);
			if (IS_NONE(arg)) {
				req->serial.parity = SERIAL_PARITY_NONE;
			} else {
				if (!IS_WORD(arg)) {
					Trap1(RE_INVALID_PORT_ARG, arg);
				}
				switch (VAL_WORD_CANON(arg)) {
					case SYM_ODD:
						req->serial.parity = SERIAL_PARITY_ODD;
						break;
					case SYM_EVEN:
						req->serial.parity = SERIAL_PARITY_EVEN;
						break;
					default:
						Trap1(RE_INVALID_PORT_ARG, arg);
				}
			}

			arg = Obj_Value(spec, STD_PORT_SPEC_SERIAL_FLOW_CONTROL);
			if (IS_NONE(arg)) {
				req->serial.flow_control = SERIAL_FLOW_CONTROL_NONE;
			} else {
				if (!IS_WORD(arg)) {
					Trap1(RE_INVALID_PORT_ARG, arg);
				}
				switch (VAL_WORD_CANON(arg)) {
					case SYM_HARDWARE:
						req->serial.flow_control = SERIAL_FLOW_CONTROL_HARDWARE;
						break;
					case SYM_SOFTWARE:
						req->serial.flow_control = SERIAL_FLOW_CONTROL_SOFTWARE;
						break;
					default:
						Trap1(RE_INVALID_PORT_ARG, arg);
				}
			}

			// The device needs the path only to open it, so there
			// is no need to allocate it (and to free it later).
			req->serial.path = path;
			result = OS_Do_Device(req, RDC_OPEN);
			req->serial.path = NULL;
			if (result) Trap_Port(RE_CANNOT_OPEN, port, -12);
			SET_OPEN(req);
			return R_RET;

		case A_CLOSE:
			return R_RET;

		case A_OPENQ:
			return R_FALSE;

		default:
			Trap_Port(RE_NOT_OPEN, port, -12);
		}
	}

	// Actions for an open socket:
	switch (action) {

	case A_READ:
		refs = Find_Refines(ds, ALL_READ_REFS);

		// Setup the read buffer (allocate a buffer if needed):
		arg = OFV(port, STD_PORT_DATA);
		if (!IS_BINARY(arg)) {
			Set_Binary(arg, Make_Binary(SERIAL_BUF_SIZE));
		}
		else if (req->command == RDC_READ) {
			// The previous READ may be done, but its event was not processed yet.
			// Apply its data now, else the new READ would overwrite it.
			// (The later UPDATE is fine with it, as the length is consumed.)
			Accept_Read_Data(arg, req);
		}
		ser = VAL_SERIES(arg);
		req->length = SERIES_AVAIL(ser); // space available
		if (req->length < SERIAL_BUF_SIZE/2) Extend_Series(ser, SERIAL_BUF_SIZE);
		req->length = SERIES_AVAIL(ser);
		req->data = STR_TAIL(ser); // write at tail
		req->actual = 0;  // Actual for THIS read, not for total.
#ifdef DEBUG_SERIAL
		printf("(max read length %d)", req->length);
#endif
		result = OS_Do_Device(req, RDC_READ); // recv can happen immediately
		if (result < 0) Trap_Port(RE_READ_ERROR, port, req->error);
#ifdef DEBUG_SERIAL
		for (len = 0; len < req->actual; len++) {
			if (len % 16 == 0) printf("\n");
			printf("%02x ", req->data[len]);
		}
		printf("\n");
#endif
		if (result == DR_DONE) {
			// Make the received data visible right away. The device has
			// already queued the READ event, so mark the data as consumed,
			// else the UPDATE action would append the same length again.
			Accept_Read_Data(arg, req);
		}
		*D_RET = *arg;
		return R_RET;

	case A_WRITE:
		refs = Find_Refines(ds, ALL_WRITE_REFS);

		// Only bytes may be sent (strings are internally UTF-8 encoded).
		// Any other value has no series data or it holds Rebol values!
		spec = D_ARG(2);
		if (!ANY_BINSTR(spec)) Trap1(RE_INVALID_ARG, spec);

		// Determine length. Clip /PART to size of string if needed.
		len = VAL_LEN(spec);
		if (refs & AM_WRITE_PART) {
			REBCNT n = Int32s(D_ARG(ARG_WRITE_LENGTH), 0);
			if (n <= len) len = n;
		}

		// WRITE has its own request, so it does not disturb a pending READ
		// and it does not use the port's data, where the received bytes are.
		wreq = SERIAL_WRITE_REQ(req);
		hob = Serial_State_Hob(port);

		// Append to the data of a still pending WRITE (keeps the order).
		if (GET_FLAG(wreq->flags, RRF_PENDING) && hob->series) {
			Append_Bytes_Len(hob->series, VAL_BIN_DATA(spec), len);
			wreq->data = BIN_HEAD(hob->series) + wreq->actual; // may be reallocated
			wreq->length += len;
			break;
		}

		// Setup the write (with a copy of the data, so it is GC safe
		// and it cannot be modified while it is being sent):
		hob->series = Copy_Bytes(VAL_BIN_DATA(spec), len);
		wreq->handle = req->handle; // the opened device (handle or id)
		wreq->length = len;
		wreq->data   = BIN_HEAD(hob->series);
		wreq->actual = 0;

		//Print("(write length %d)", len);
		result = OS_Do_Device(wreq, RDC_WRITE); // send can happen immediately
		if (result <= 0) hob->series = NULL; // done or failed
		if (result < 0) Trap_Port(RE_WRITE_ERROR, port, wreq->error);
		break;
	case A_UPDATE:
		// Update the port object after a READ or WRITE operation.
		// This is normally called by the WAKE-UP function.
		arg = OFV(port, STD_PORT_DATA);
		if (req->command == RDC_READ) Accept_Read_Data(arg, req);
		// Release the data of a finished (or failed) WRITE:
		if (!GET_FLAG(SERIAL_WRITE_REQ(req)->flags, RRF_PENDING))
			Serial_State_Hob(port)->series = NULL;
		return R_NONE;
	case A_OPENQ:
		return R_TRUE;

	case A_CLOSE:
		if (IS_OPEN(req)) {
			// A pending WRITE must not be polled with the closed device:
			OS_Abort_Device(SERIAL_WRITE_REQ(req));
			Serial_State_Hob(port)->series = NULL;
			OS_Do_Device(req, RDC_CLOSE);
			Release_Port_State(port);
		}
		break;
	case A_MODIFY:
		arg = D_ARG(2);
		if (IS_WORD(arg)) {
			switch (VAL_WORD_CANON(arg)) {
			case SYM_BRK:  req->modify.mode = 1; break;
			case SYM_RTS:  req->modify.mode = 2; break;
			case SYM_DTR:  req->modify.mode = 3; break;
			default: Trap1(RE_BAD_FILE_MODE, arg);
			}
			spec = D_ARG(3);
			if (!IS_LOGIC(spec)) Trap2(RE_INVALID_VALUE_FOR, spec, arg);
			req->modify.value = VAL_LOGIC(spec);
			OS_Do_Device(req, RDC_MODIFY);
		}
		else Trap1(RE_BAD_FILE_MODE, arg);
		return R_ARG3;

	default:
		Trap_Action(REB_PORT, action);
	}

	return R_RET;
}


/***********************************************************************
**
*/	void Init_Serial_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_SERIAL, 0, Serial_Actor);
}