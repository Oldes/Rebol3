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
**  Module:  t-event.c
**  Summary: event datatype
**  Section: datatypes
**  Author:  Carl Sassenrath
**  Notes:
**    Events are kept compact in order to fit into normal 128 bit
**    values cells. This provides high performance for high frequency
**    events and also good memory efficiency using standard series.
**
***********************************************************************/

#include "sys-core.h"
#include "reb-evtypes.h"
#include "reb-net.h"

#define IS_CHAR_KEY_TYPE(t)   ((t) == EVT_KEY       || (t) == EVT_KEY_UP)
#define IS_NAMED_KEY_TYPE(t)  ((t) == EVT_NAMED_KEY || (t) == EVT_NAMED_KEY_UP)

/***********************************************************************
**
*/	REBINT CT_Event(REBVAL *a, REBVAL *b, REBINT mode)
/*
***********************************************************************/
{
	REBINT diff = Cmp_Event(a, b);
	if (mode >=0) return diff == 0;
	return -1;
}


/***********************************************************************
**
*/	REBINT Cmp_Event(REBVAL *t1, REBVAL *t2)
/*
**	Given two events, compare them.
**
***********************************************************************/
{
	REBINT	diff;

	if (
		   (diff = VAL_EVENT_MODEL(t1) - VAL_EVENT_MODEL(t2))
		|| (diff = VAL_EVENT_TYPE(t1) - VAL_EVENT_TYPE(t2))
		|| (diff = VAL_EVENT_XY(t1) - VAL_EVENT_XY(t2))
	) return diff;

	return 0;
}


/***********************************************************************
**
*/	static REBFLG Set_Event_Var(REBVAL *value, REBVAL *word, REBVAL *val)
/*
***********************************************************************/
{
	REBVAL *arg;
	REBINT n;
	REBCNT w;

	switch (VAL_WORD_CANON(word)) {

	case SYM_TYPE:
		// An extension defines its own types above the named range, so a
		// plain code is accepted too - and is what event/type hands back.
		if (IS_INTEGER(val)) {
			if (VAL_INT64(val) < 0 || VAL_INT64(val) > 255) return FALSE;
			n = VAL_INT32(val);
		}
		else if (IS_WORD(val) || IS_LIT_WORD(val)) {
			arg = Get_System(SYS_CATALOG, CAT_EVENT_TYPES);
			if (!IS_BLOCK(arg)) return FALSE;
			w = VAL_WORD_CANON(val);
			for (n = 0, arg = VAL_BLK(arg); NOT_END(arg); arg++, n++) {
				if (IS_WORD(arg) && VAL_WORD_CANON(arg) == w) break;
			}
			if (IS_END(arg)) Trap_Arg(val);
		}
		else return FALSE;

		// The data field is decoded by the TYPE, so a key event must not be
		// relabelled as the other kind of key - the stored catalog position
		// would be read as a codepoint, or the codepoint as a position.
		if ((IS_CHAR_KEY_TYPE(VAL_EVENT_TYPE(value)) && IS_NAMED_KEY_TYPE(n))
		|| (IS_NAMED_KEY_TYPE(VAL_EVENT_TYPE(value)) && IS_CHAR_KEY_TYPE(n))) {
			// Name the type it already is - the conflict is the whole point
			// of the refusal, so the message has to show both sides.
			REBVAL current;
			arg = Get_System(SYS_CATALOG, CAT_EVENT_TYPES);
			if (IS_BLOCK(arg) && (REBCNT)VAL_EVENT_TYPE(value) < VAL_TAIL(arg))
				current = *VAL_BLK_SKIP(arg, VAL_EVENT_TYPE(value));
			else
				SET_INTEGER(&current, VAL_EVENT_TYPE(value));
			Trap2(RE_BAD_EVENT_TYPE, val, &current);
		}

		VAL_EVENT_TYPE(value) = (u8)n;
		return TRUE;

	case SYM_PORT:
		if (IS_PORT(val)) {
			VAL_EVENT_MODEL(value) = EVM_PORT;
			VAL_EVENT_SER(value) = VAL_PORT(val);
		}
		else if (IS_OBJECT(val)) {
			VAL_EVENT_MODEL(value) = EVM_OBJECT;
			VAL_EVENT_SER(value) = VAL_OBJ_FRAME(val);
		}
		else if (IS_NONE(val)) {
			VAL_EVENT_MODEL(value) = EVM_DEVICE;
			VAL_EVENT_SER(value) = 0;
		} else return FALSE;
		break;

	case SYM_HANDLE:
		// Only a CONTEXT handle: it is the only kind with a REBHOB the
		// GC can mark and whose lifetime outlives the value.
		if (IS_HANDLE(val) && IS_CONTEXT_HANDLE(val)) {
			VAL_EVENT_MODEL(value) = EVM_HANDLE;
			VAL_EVENT_HOB(value) = VAL_HANDLE_CTX(val);
			break;
		}
		else if (IS_NONE(val)) {
			VAL_EVENT_MODEL(value) = EVM_DEVICE;
			VAL_EVENT_SER(value) = 0;
			break;
		}
		return FALSE;

	case SYM_OFFSET:
		if (IS_PAIR(val)) {
			SET_EVENT_XY(value, Float_Int16(VAL_PAIR_X(val)), Float_Int16(VAL_PAIR_Y(val)));
			CLR_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE);
			SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY);
			break;
		}
		//O: should it be possible to remove offset value from event?
		//else if (IS_NONE(val)) {
		//	CLR_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY);
		//}
		return FALSE;

	case SYM_KEY:
		if (IS_CHAR(val)) {
			// Only EVT_KEY/EVT_KEY_UP decode the data as a character, so
			// the default type has to follow the kind of key given.
			if (!IS_CHAR_KEY_TYPE(VAL_EVENT_TYPE(value))
				&& VAL_EVENT_TYPE(value) != EVT_CUSTOM
				&& VAL_EVENT_TYPE(value) < EVT_MAX) // extension types are their own
				VAL_EVENT_TYPE(value) = EVT_KEY;
			VAL_EVENT_DATA(value) = VAL_CHAR(val);
			CLR_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY);
			SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE);
			break;
		}
		else if (IS_LIT_WORD(val) || IS_WORD(val)) {
			arg = Get_System(SYS_CATALOG, CAT_EVENT_KEYS);
			if (IS_BLOCK(arg)) {
				// Count from the HEAD: Get_Event_Var indexes the catalog
				// with VAL_BLK_SKIP from the head, so the two must agree.
				// (The old init read VAL_INDEX of the first ELEMENT, which
				// aliases a word's frame field - zero only by luck.)
				arg = VAL_BLK(arg);
				for (n = 0; NOT_END(arg); n++, arg++) {
					if (IS_WORD(arg) && VAL_WORD_CANON(arg) == VAL_WORD_CANON(val)) {
						if (!IS_NAMED_KEY_TYPE(VAL_EVENT_TYPE(value))
							&& VAL_EVENT_TYPE(value) != EVT_CUSTOM
							&& VAL_EVENT_TYPE(value) < EVT_MAX) // extension types are their own
							VAL_EVENT_TYPE(value) = EVT_NAMED_KEY;
						// 1-based, unshifted: a character key needs all 32
						// bits (MAX_CHAR is 21), so the two uses of this
						// field take turns by event type rather than
						// splitting it 16/16 as SET_EVENT_KEY assumed.
						VAL_EVENT_DATA(value) = n + 1;
						CLR_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY);
						SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE);
						break;
					}
				}
				if (IS_END(arg)) Trap1(RE_NO_EVENT_KEY, val);
				break;
			}
		}
		return FALSE;

	case SYM_CODE:
		if (IS_INTEGER(val)) {
			VAL_EVENT_DATA(value) = VAL_INT64(val);
			CLR_FLAGS(VAL_EVENT_FLAGS(value), EVF_HAS_XY, EVF_HAS_SYM);
			SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE);
			break;
		}
		if (IS_WORD(val) || IS_LIT_WORD(val)) {
			VAL_EVENT_DATA(value) = VAL_WORD_CANON(val);
			CLR_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY);
			SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_SYM);
			SET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE);
			break;
		}
		return FALSE;

	default:
		return FALSE;
	}

	return TRUE;
}


/***********************************************************************
**
*/	static void Set_Event_Vars(REBVAL *evt, REBVAL *blk)
/*
***********************************************************************/
{
	REBVAL *var;
	REBVAL *val;

	while (NOT_END(blk)) {
		var = blk++;
		val = blk++;
		if (IS_END(val)) val = NONE_VALUE;
		else val = Get_Simple_Value(val);
		if (!Set_Event_Var(evt, var, val)) Trap2(RE_BAD_FIELD_SET, var, Of_Type(val));
	}
}


/***********************************************************************
**
*/	static REBFLG Get_Event_Var(REBVAL *value, REBCNT sym, REBVAL *val)
/*
***********************************************************************/
{
	REBVAL *arg;
	REBREQ *req;
	REBINT n;
	REBSER *ser;

	switch (sym) {

	case SYM_TYPE:
		if (VAL_EVENT_TYPE(value) == 0) goto is_none;
		arg = Get_System(SYS_CATALOG, CAT_EVENT_TYPES);
		if (IS_BLOCK(arg)) {
			n = VAL_EVENT_TYPE(value);
			// A reserved slot holds no word, and an extension-defined type
			// is past the end of the catalog entirely. Report the raw code
			// instead of reading whatever happens to sit there - the old
			// EVT_MAX check did not bound the type itself.
			if ((REBCNT)n < VAL_TAIL(arg) && IS_WORD(VAL_BLK_SKIP(arg, n)))
				*val = *VAL_BLK_SKIP(arg, n);
			else
				SET_INTEGER(val, n);
			break;
		}
		return FALSE;

	case SYM_PORT:
		// Event holds a port:
		if (IS_EVENT_MODEL(value, EVM_PORT) || IS_EVENT_MODEL(value, EVM_MIDI)) {
			SET_PORT(val, VAL_EVENT_SER(value));
		}
		// Event holds an object:
		else if (IS_EVENT_MODEL(value, EVM_OBJECT)) {
			SET_OBJECT(val, VAL_EVENT_SER(value));
		}
		// Event holds a handle - it belongs to no port at all. This case
		// must be explicit: without it the HOB falls through to the
		// EVM_DEVICE branch below and is read as a REBREQ.
		else if (IS_EVENT_MODEL(value, EVM_HANDLE)) {
			goto is_none;
		}
		else if (IS_EVENT_MODEL(value, EVM_CALLBACK)) {
			*val = *Get_System(SYS_PORTS, PORTS_CALLBACK);
		}
		else if (IS_EVENT_MODEL(value, EVM_CONSOLE)) {
			*val = *Get_System(SYS_PORTS, PORTS_INPUT);
		}
		else {
			// assumes EVM_DEVICE
			// Event holds the IO-Request, which has the PORT:
			req = VAL_EVENT_REQ(value);
			if (!req || !req->port) goto is_none;
			SET_PORT(val, (REBSER*)(req->port));
		}
		break;

	case SYM_HANDLE:
		if (IS_EVENT_MODEL(value, EVM_HANDLE) && VAL_EVENT_HOB(value)) {
			REBHOB *hob = VAL_EVENT_HOB(value);
			// A handle released since the event was made reads as none
			// rather than handing back a recycled context.
			if (!IS_USED_HOB(hob)) goto is_none;
			VAL_HANDLE_FLAGS(val) = 0; // SET_HANDLE ORs into this
			SET_HANDLE(val, hob, hob->sym, HANDLE_CONTEXT);
			break;
		}
		goto is_none;

	case SYM_OFFSET:
		if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_XY)) {
			VAL_SET(val, REB_PAIR);
			VAL_PAIR_X(val) = (REBD32)VAL_EVENT_X(value);
			VAL_PAIR_Y(val) = (REBD32)VAL_EVENT_Y(value);
			break;
		}
		goto is_none;

	case SYM_KEY:
		n = VAL_EVENT_DATA(value);
		if (VAL_EVENT_TYPE(value) == EVT_KEY || VAL_EVENT_TYPE(value) == EVT_KEY_UP) {
			// The data may come from an extension - do not build a char!
			// out of a codepoint the rest of the system cannot encode.
			if ((REBCNT)n > MAX_CHAR) goto is_none;
			SET_CHAR(val, n);
			break;
		}
		else if (VAL_EVENT_TYPE(value) == EVT_NAMED_KEY || VAL_EVENT_TYPE(value) == EVT_NAMED_KEY_UP) {
			arg = Get_System(SYS_CATALOG, CAT_EVENT_KEYS);
			// n is 1-based; n == 0 means no key, and without the lower
			// bound VAL_BLK_SKIP(arg, -1) reads before the block's data.
			if (IS_BLOCK(arg) && n > 0 && n <= (REBINT)VAL_TAIL(arg)) {
				*val = *VAL_BLK_SKIP(arg, n-1);
				break;
			}
		}
		goto is_none;

	case SYM_FLAGS:
		if (VAL_EVENT_FLAGS(value) & (1<<EVF_DOUBLE | 1<<EVF_CONTROL | 1<<EVF_SHIFT)) {
			ser = Make_Block(3);
			if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_DOUBLE)) {
				arg = Append_Value(ser);
				Init_Word(arg, SYM_DOUBLE);
			}
			if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_CONTROL)) {
				arg = Append_Value(ser);
				Init_Word(arg, SYM_CONTROL);
			}
			if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_SHIFT)) {
				arg = Append_Value(ser);
				Init_Word(arg, SYM_SHIFT);
			}
			Set_Block(val, ser);
		} else goto is_none;
		break;

	case SYM_CODE:
		// A symbol id reads back as the word it names. This is what lets an
		// extension report WHICH item, menu entry or command an event is
		// about, in the one payload slot an event has.
		if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_SYM)) {
			Init_Word(val, VAL_EVENT_DATA(value));
			break;
		}
		if (GET_FLAG(VAL_EVENT_FLAGS(value), EVF_HAS_CODE)) {
			SET_INTEGER(val, VAL_EVENT_DATA(value));
			break;
		}
		goto is_none;

	default:
		return FALSE;
	}

	return TRUE;

is_none:
	SET_NONE(val);
	return TRUE;
}


/***********************************************************************
**
*/	REBFLG MT_Event(REBVAL *out, REBVAL *data, REBCNT type)
/*
***********************************************************************/
{
	if (IS_BLOCK(data)) {
		CLEARS(out);
		Set_Event_Vars(out, VAL_BLK_DATA(data));
		VAL_SET(out, REB_EVENT);
		return TRUE;
	}

	return FALSE;
}


/***********************************************************************
**
*/	REBINT PD_Event(REBPVS *pvs)
/*
***********************************************************************/
{
	if (IS_WORD(pvs->select)) {
		if (pvs->setval == 0 || NOT_END(pvs->path+1)) {
			if (!Get_Event_Var(pvs->value, VAL_WORD_CANON(pvs->select), pvs->store)) return PE_BAD_SELECT;
			return PE_USE;
		} else {
			if (!Set_Event_Var(pvs->value, pvs->select, pvs->setval)) return PE_BAD_SET;
			return PE_OK;
		}
	}
	return PE_BAD_SELECT;
}


/***********************************************************************
**
*/	REBTYPE(Event)
/*
***********************************************************************/
{
	REBVAL *value;
	REBVAL *arg;

	value = D_ARG(1);
	arg = D_ARG(2);

	if (action == A_MAKE || action == A_TO) {
		if (IS_EVENT(value) || IS_DATATYPE(value)) {
			if (IS_EVENT(arg)) return R_ARG2;
			//Trap_Make(REB_EVENT, value);
			VAL_SET(D_RET, REB_EVENT);
			CLEARS(&(D_RET->data.event));
		}
		else
is_arg_error:
			Trap_Types(RE_EXPECT_VAL, REB_EVENT, VAL_TYPE(arg));

		// Initialize event from block:
		if (IS_BLOCK(arg)) Set_Event_Vars(D_RET, VAL_BLK_DATA(arg));
		else goto is_arg_error;
	}
	else Trap_Action(REB_EVENT, action);

	return R_RET;
}

/***********************************************************************
**
*/	 void Mold_Event(REBVAL *value, REB_MOLD *mold)
/*
***********************************************************************/
{
	REBVAL val;
	REBCNT field;
	REBCNT fields[] = {
		SYM_TYPE, SYM_PORT, SYM_HANDLE, SYM_OFFSET, SYM_KEY,
		SYM_FLAGS, SYM_CODE, 0
	};
	REBOOL indented = !GET_MOPT(mold, MOPT_INDENT);

	Pre_Mold(value, mold);
	Append_Byte(mold->series, '[');
	mold->indent++;

	for (field = 0; fields[field]; field++) {
		if (Get_Event_Var(value, fields[field], &val) && !IS_NONE(&val)) {
			if(indented)
				New_Indented_Line(mold);
			else if (field > 0)
				Append_Byte(mold->series, ' ');
			Append_UTF8(mold->series, Get_Sym_Name(fields[field]), -1);
			Append_Bytes(mold->series, ": ");
			if (IS_WORD(&val)) Append_Byte(mold->series, '\'');
			Mold_Value(mold, &val, TRUE);
		}
	}

	mold->indent--;
	if (indented) New_Indented_Line(mold);
	Append_Byte(mold->series, ']');

	End_Mold(mold);
}

