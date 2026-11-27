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
**  Module:  p-timer.c
**  Summary: timer port interface
**  Section: ports
**  Author:  Carl Sassenrath, Oldes
**  Notes:
**    Timers are not a device. Armed timers are kept in a list which is
**    checked by WAIT (see Wait_Ports), and due timers post a `time`
**    event to the system queue, so the port's awake function is called
**    like for any other port event.
**
**    The list holds only WEAK references: it is not marked by the GC.
**    A timer port which is not referenced anymore stays active until it
**    is released by the GC (Sweep_Timers is called from Recycle before
**    any series is freed).
**
***********************************************************************/
/*
	General idea of usage:

	t: open [scheme: 'timer timeout: repeat: 0.5] ;; will wake up every half a second
	t/awake: func [event] [print "timer!"]        ;; will be evaluated on time
	wait 1                                        ;; process any events
	print 'close
	close t                                       ;; close the timer
	wait 1                                        ;; no timer events there
	print 'restart
	open t                                        ;; restarted the timer
	wait 1                                        ;; there should be 2 timer events again
	wait 1                                        ;; and again...
	t: none                                       ;; port is not referenced anymore!
	wait 1                                        ;; but still active untill it is released by GC!
	print 'recycle
	loop 2 [recycle recycle]                      ;; the timer port should be released!
	wait 1                                        ;; there should not be processed any timer events
	print 'done

	Spec fields:
		timeout: delay before the first event
		repeat:  interval of the following events (none or zero = one-shot)
	Both accept integer! or decimal! (seconds) and time! values.
	When only `repeat` is used, it is also used as the first delay.
*/

#include "sys-core.h"
#include "reb-evtypes.h"

#define MAX_TIMER_SECS 9223372036LL   // keeps microseconds within REBI64
#define MAX_TIMER_LAG  4               // missed ticks to catch up before resync

typedef struct rebol_timer_state TIMER_STATE;
struct rebol_timer_state {
	REBSER      *port;      // weak link back to the port (not marked by GC)
	REBI64       due;       // time of the next event (OS_Delta_Time microseconds)
	REBI64       interval;  // repeat interval in microseconds (0 = one-shot)
	TIMER_STATE *prev;
	TIMER_STATE *next;
	REBFLG       active;    // TRUE when linked in the Timers list
};

static TIMER_STATE *Timers; // armed timers (weak references)


/***********************************************************************
**
*/	static REBI64 Timer_Now(void)
/*
**		Returns microseconds since boot (same clock as `stats/timer`).
**		NOTE: OS_Delta_Time(0, 0) is not usable as a time value - it
**		returns a raw OS counter (on Windows in performance counter
**		ticks), only a delta from a base is in microseconds.
**
***********************************************************************/
{
	return OS_Delta_Time(PG_Boot_Time, 0);
}


/***********************************************************************
**
*/	static void Link_Timer(TIMER_STATE *tmr)
/*
***********************************************************************/
{
	if (tmr->active) return;
	tmr->prev = NULL;
	tmr->next = Timers;
	if (Timers) Timers->prev = tmr;
	Timers = tmr;
	tmr->active = TRUE;
}


/***********************************************************************
**
*/	static void Unlink_Timer(TIMER_STATE *tmr)
/*
***********************************************************************/
{
	if (!tmr->active) return;
	if (tmr->prev) tmr->prev->next = tmr->next;
	else Timers = tmr->next;
	if (tmr->next) tmr->next->prev = tmr->prev;
	tmr->prev = tmr->next = NULL;
	tmr->active = FALSE;
}


/***********************************************************************
**
*/	static void Free_Timer_State(void *data)
/*
**		Handle's free callback. Called from GC sweep (or on shutdown),
**		so it must not touch the port (it may be already released).
**
***********************************************************************/
{
	Unlink_Timer((TIMER_STATE *)data);
}


/***********************************************************************
**
*/	void Sweep_Timers(void)
/*
**		Called by GC after marking, before anything is released.
**		Disarms timers whose ports were not marked (they are going
**		to be released), so no event is posted to a released port.
**
***********************************************************************/
{
	TIMER_STATE *tmr, *next;

	for (tmr = Timers; tmr; tmr = next) {
		next = tmr->next;
		if (!IS_MARK_SERIES(tmr->port)) {
			Unlink_Timer(tmr);
			tmr->port = NULL;
		}
	}
}


/***********************************************************************
**
*/	REBCNT Check_Timers(void)
/*
**		Posts a `time` event for each timer which is due.
**		Returns number of milliseconds until the next timer is due
**		(zero if already due) or ALL_BITS when no timer is armed.
**
***********************************************************************/
{
	TIMER_STATE *tmr, *next;
	REBVAL *evt;
	REBI64 now, delta;
	REBI64 min = -1;
	REBFLG posted;

	if (!Timers) return ALL_BITS;

	now = Timer_Now();

	for (tmr = Timers; tmr; tmr = next) {
		next = tmr->next;
		if (now >= tmr->due) {
			// Post the event, unless the previous one was not processed yet
			// (so a slow awake function cannot flood the event queue):
			posted = FALSE;
			if (!Find_Event(EVM_PORT, EVT_TIME, tmr->port)) {
				evt = Append_Event();
				if (evt) {
					posted = TRUE;
					VAL_SET(evt, REB_EVENT);
					VAL_EVENT_TYPE(evt)  = EVT_TIME;
					VAL_EVENT_FLAGS(evt) = 0;
					VAL_EVENT_WIN(evt)   = 0;
					VAL_EVENT_MODEL(evt) = EVM_PORT;
					VAL_EVENT_DATA(evt)  = 0;
					VAL_EVENT_SER(evt)   = tmr->port;
				}
			}
			if (tmr->interval <= 0) {
				Unlink_Timer(tmr); // one-shot timer is done
				continue;
			}
			// When the event was not posted, the tick stays due, so it is
			// posted once the queued event is processed.
			if (posted) {
				// Late ticks are caught up one by one (so the rate is kept
				// even when the OS wakes us late), but when too far behind
				// (slow awake function or long blocking code), the backlog
				// is dropped and the timer is resynchronized.
				tmr->due += tmr->interval;
				if (now - tmr->due >= MAX_TIMER_LAG * tmr->interval)
					tmr->due += ((now - tmr->due) / tmr->interval + 1) * tmr->interval;
			}
		}
		delta = tmr->due - now;
		if (delta < 0) delta = 0; // already due
		if (min < 0 || delta < min) min = delta;
	}

	if (min < 0) return ALL_BITS;
	min = (min + 999) / 1000; // microseconds to milliseconds (rounded up)
	return (min >= (REBI64)MAX_U32) ? (MAX_U32 - 1) : (REBCNT)min;
}


/***********************************************************************
**
*/	static REBI64 Timer_Spec_Time(REBVAL *spec, REBCNT sym)
/*
**		Returns the spec field value converted to microseconds.
**		Missing field or none is zero.
**
***********************************************************************/
{
	REBCNT n = Find_Word_Index(VAL_OBJ_FRAME(spec), sym, FALSE);
	REBVAL *val;
	REBDEC dec;

	if (!n) return 0;
	val = Obj_Value(spec, n);
	if (!val) return 0;

	switch (VAL_TYPE(val)) {
	case REB_NONE:
		return 0;
	case REB_INTEGER:
		if (VAL_INT64(val) < 0 || VAL_INT64(val) > MAX_TIMER_SECS) Trap_Range(val);
		return VAL_INT64(val) * 1000000;
	case REB_DECIMAL:
		dec = VAL_DECIMAL(val);
		if (!(dec >= 0.0 && dec <= (REBDEC)MAX_TIMER_SECS)) Trap_Range(val); // also NaN
		return (REBI64)(dec * 1000000.0);
	case REB_TIME:
		if (VAL_TIME(val) < 0) Trap_Range(val);
		return VAL_TIME(val) / 1000; // nanoseconds to microseconds
	default:
		Trap1(RE_INVALID_SPEC, val);
	}
	return 0; // for compiler only
}


/***********************************************************************
**
*/	static TIMER_STATE *Get_Timer_State(REBSER *port)
/*
**		Returns the port's timer state or NULL if not made yet.
**
***********************************************************************/
{
	REBVAL *state = BLK_SKIP(port, STD_PORT_STATE);

	if (IS_HANDLE(state) && IS_CONTEXT_HANDLE(state)
		&& IS_VALID_CONTEXT_HANDLE(state, SYM_TIMER))
		return (TIMER_STATE *)VAL_HANDLE_CONTEXT_DATA(state);
	return NULL;
}


/***********************************************************************
**
*/	static TIMER_STATE *Use_Timer_State(REBSER *port)
/*
**		Returns the port's timer state, making it when needed.
**
***********************************************************************/
{
	REBVAL *state;
	TIMER_STATE *tmr = Get_Timer_State(port);

	if (tmr) return tmr;

	state = BLK_SKIP(port, STD_PORT_STATE);
	VAL_HANDLE_FLAGS(state) = 0; // SET_HANDLE ORs into this
	MAKE_HANDLE(state, SYM_TIMER);
	if (!VAL_HANDLE_CTX(state)) {
		SET_NONE(state); // never leave a handle without its context
		Trap0(RE_NO_MEMORY);
	}
	tmr = (TIMER_STATE *)VAL_HANDLE_CONTEXT_DATA(state);
	tmr->port = port;
	return tmr;
}


/***********************************************************************
**
*/	static void Arm_Timer(TIMER_STATE *tmr, REBVAL *spec)
/*
***********************************************************************/
{
	REBI64 timeout  = Timer_Spec_Time(spec, SYM_TIMEOUT);
	REBI64 interval = Timer_Spec_Time(spec, SYM_REPEAT);

	if (timeout == 0) {
		// Without any time, the timer would never stop firing.
		if (interval == 0) Trap1(RE_INVALID_SPEC, spec);
		timeout = interval;
	}
	tmr->interval = interval;
	tmr->due = Timer_Now() + timeout;
	Link_Timer(tmr); // no-op when already armed (only restarted)
}


/***********************************************************************
**
*/	static int Timer_Actor(REBVAL *ds, REBVAL *port_value, REBCNT action)
/*
***********************************************************************/
{
	REBSER *port;
	REBVAL *spec;
	TIMER_STATE *tmr;

	port = Validate_Port_Value(port_value);
	spec = BLK_SKIP(port, STD_PORT_SPEC);

	*D_RET = *D_ARG(1);

	switch (action) {

	case A_OPEN:
		// Opening an already open timer restarts it.
		Arm_Timer(Use_Timer_State(port), spec);
		break;

	case A_CLOSE:
		tmr = Get_Timer_State(port);
		if (tmr) Unlink_Timer(tmr); // keeps the state, so it may be reopened
		break;

	case A_OPENQ:
		tmr = Get_Timer_State(port);
		return (tmr && tmr->active) ? R_TRUE : R_FALSE;

	case A_UPDATE:
		// Called by WAKE-UP before the awake function. Nothing to update.
		return R_NONE;

	default:
		Trap1(RE_NO_PORT_ACTION, Get_Action_Word(action));
	}

	return R_RET;
}


/***********************************************************************
**
*/	void Init_Timer_Scheme(void)
/*
***********************************************************************/
{
	Timers = NULL;
	Register_Handle(SYM_TIMER, sizeof(TIMER_STATE), (REB_HANDLE_FREE_FUNC)Free_Timer_State);
	Register_Scheme(SYM_TIMER, 0, Timer_Actor);
}
