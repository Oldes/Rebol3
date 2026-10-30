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
**  Module:  p-dns.c
**  Summary: DNS port interface
**  Section: ports
**  Author:  Carl Sassenrath
**  Notes:
**
***********************************************************************/

#include "sys-core.h"
#include "reb-net.h"


#ifdef INCLUDE_NET_INTERFACES
/***********************************************************************
**
*/	static REBVAL *Append_Map_Key(REBSER *map, REBCNT sym)
/*
**		Append a key (as a set-word, like in a map literal) to a map
**		block under construction. Returns the value cell for the key.
**		(The cell is valid only until the next append.)
**
***********************************************************************/
{
	REBVAL *val = Append_Value(map);
	Init_Word(val, sym);
	VAL_TYPE(val) = REB_SET_WORD;
	return Append_Value(map);
}


/***********************************************************************
**
*/	static void Set_Net_Interface(REBVAL *out, REBNIF *nif)
/*
**		Set out to a map describing a local network interface:
**		#[name: "eth0" local-ip: 192.168.1.10 netmask: 255.255.255.0 flags: [up]]
**
**		The out value must be GC safe (it holds the new map).
**
***********************************************************************/
{
	REBSER *map = Make_Block(8);
	REBSER *ser;
	REBVAL *val;
	REBLEN len = (REBLEN)LEN_BYTES(cb_cast(nif->name));

	Set_Series(REB_MAP, out, map); // GC safe (hashed when complete)

	ser = Copy_Bytes(cb_cast(nif->name), len);
	if (!Is_ASCII(BIN_HEAD(ser), len)) UTF8_SERIES(ser);
	Set_String(Append_Map_Key(map, SYM_NAME), ser);

	Set_Tuple(Append_Map_Key(map, SYM_LOCAL_IP), (REBYTE *)&nif->ip, 4);
	Set_Tuple(Append_Map_Key(map, SYM_NETMASK), (REBYTE *)&nif->netmask, 4);

	ser = Make_Block(2);
	Set_Block(Append_Map_Key(map, SYM_FLAGS), ser);
	if (GET_FLAG(nif->flags, RNIF_UP))       Init_Word(Append_Value(ser), SYM_UP);
	if (GET_FLAG(nif->flags, RNIF_LOOPBACK)) Init_Word(Append_Value(ser), SYM_LOOPBACK);

	Block_As_Map(map);
}
#endif // INCLUDE_NET_INTERFACES


/***********************************************************************
**
*/	static int DNS_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	REBSER *port;
	REBVAL *spec;
	REBREQ *sock;
	REBINT result;
	REBVAL *arg;
	REBCNT len;
	REBOOL sync = FALSE; // act synchronously
	REBVAL tmp;

	port = Validate_Port_With_Request(port_value, RDI_DNS, &sock);

	arg = D_ARG(2);
	*D_RET = *D_ARG(1);

	spec = OFV(port, STD_PORT_SPEC);

	sock->timeout = 4000; // where does this go? !!!

	switch (action) {

	case A_READ:
		if (!IS_OPEN(sock)) {
			if (OS_Do_Device(sock, RDC_OPEN)) Trap_Port(RE_CANNOT_OPEN, port, sock->error);
			sync = TRUE;
		}

		arg = Obj_Value(spec, STD_PORT_SPEC_NET_HOST);
		if (IS_NONE(arg)) {
			// case: read dns://
			sock->data = NULL;
		}
		else if (IS_TUPLE(arg)) {
			SET_FLAG(sock->modes, RST_REVERSE);
			memcpy(&sock->net.remote_ip, VAL_TUPLE(arg), 4);
		}
		else if (IS_STRING(arg)) {
			if (Scan_Tuple(VAL_BIN(arg), (REBCNT)LEN_BYTES(VAL_BIN(arg)), &tmp)) {
				SET_FLAG(sock->modes, RST_REVERSE);
				memcpy(&sock->net.remote_ip, VAL_TUPLE(&tmp), 4);
			}
			else {
				sock->data = VAL_BIN(arg);
			}
		}
		else Trap_Port(RE_INVALID_SPEC, port, -10);

		result = OS_Do_Device(sock, RDC_READ);
		if (result < 0) Trap_Port(RE_READ_ERROR, port, sock->error);

		// Wait for it...
		if (sync && result == DR_PEND) {
			for (len = 0; GET_FLAG(sock->flags, RRF_PENDING) && len < 10; len++) {
				OS_Wait(2000, 0);
			}
			len = 1;
			goto pick;
		}
		if (result == DR_DONE) {
			len = 1;
			goto pick;
		}
		break;

	case A_PICK:  // FIRST - return result
		if (!IS_OPEN(sock)) Trap_Port(RE_NOT_OPEN, port, -12);
		len = Get_Num_Arg(arg); // Position
pick:
		if (len == 1) {
			if (!sock->net.host_info || !GET_FLAG(sock->flags, RRF_DONE)) return R_NONE;
			if (sock->error) {
				OS_Do_Device(sock, RDC_CLOSE);
				return R_NONE;
				//Trap_Port(RE_READ_ERROR, port, sock->error);
			}
			if (GET_FLAG(sock->modes, RST_REVERSE)) {
				Set_String(D_RET, Copy_Bytes(sock->data, LEN_BYTES(sock->data)));
			} else {
				Set_Tuple(D_RET, (REBYTE*)&sock->net.remote_ip, 4);
			}
			OS_Do_Device(sock, RDC_CLOSE);
		} else Trap_Range(arg);
		break;

	case A_OPEN:
		if (OS_Do_Device(sock, RDC_OPEN)) Trap_Port(RE_CANNOT_OPEN, port, -12);
		break;

	case A_CLOSE:
		OS_Do_Device(sock, RDC_CLOSE);
		break;

	case A_OPENQ:
		if (IS_OPEN(sock)) return R_TRUE;
		return R_FALSE;

	case A_UPDATE:
		return R_NONE;

#ifdef INCLUDE_NET_INTERFACES
	case A_QUERY: {
		// Host level information (the port does not have to be open).
		// Using a temporary request, so a pending lookup is not disturbed.
		REBREQ req;
		REBSER *ser;
		REBNIF *nif;
		REBCNT n, sym;
		arg = D_ARG(ARG_QUERY_FIELD);
		if (IS_NONE(arg)) {
			// List of the supported fields:
			ser = Make_Block(2);
			Init_Word(Append_Value(ser), SYM_LOCAL_IP);
			Init_Word(Append_Value(ser), SYM_INTERFACES);
			Set_Block(D_RET, ser);
			break;
		}
		if (!IS_WORD(arg)) Trap_Arg(arg);
		sym = VAL_WORD_CANON(arg);
		if (sym != SYM_LOCAL_IP && sym != SYM_INTERFACES) Trap_Arg(arg);

		CLEARS(&req);
		req.device = RDI_DNS;
		if (OS_Do_Device(&req, RDC_QUERY) < 0)
			Trap_Port(RE_READ_ERROR, port, req.error);

		ser = Make_Block(req.length);
		Set_Block(D_RET, ser); // GC safe
		nif = (REBNIF *)req.data;
		for (n = 0; n < req.length; n++, nif++) {
			if (sym == SYM_LOCAL_IP) {
				// Only usable addresses (up, not loopback):
				if (!GET_FLAG(nif->flags, RNIF_UP) || GET_FLAG(nif->flags, RNIF_LOOPBACK))
					continue;
				Set_Tuple(Append_Value(ser), (REBYTE *)&nif->ip, 4);
			}
			else Set_Net_Interface(Append_Value(ser), nif);
		}
		OS_Free(req.data);
		break;
	}
#endif

	default:
		Trap1(RE_NO_PORT_ACTION, Get_Action_Word(action));
	}

	return R_RET;
}


/***********************************************************************
**
*/	void Init_DNS_Scheme(void)
/*
***********************************************************************/
{
	Register_Scheme(SYM_DNS, 0, DNS_Actor);
}