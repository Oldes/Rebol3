//
// Project: Rebol/GUI extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Command implementations and the event queue.
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from gui.reb.
//
// Nothing here is platform specific - every OS call goes through the small
// Gui_* backend declared in gui.h.
//

#include "gen-gui.h"
#include "gui.h"
#include <stdio.h>
#include <string.h>
// MAKE_MEM / FREE_MEM are malloc and free behind a macro, and nothing in
// rebol-extension.h declares either - a window's default font name is the
// one thing this file allocates for itself.
#include <stdlib.h>

static const REBYTE* ERR_INVALID_HANDLE = (const REBYTE*)"Invalid GUI window handle!";
static const REBYTE* ERR_NO_HANDLE      = (const REBYTE*)"Failed to create the window handle!";
static const REBYTE* ERR_NO_WINDOW      = (const REBYTE*)"Failed to create the window!";
static const REBYTE* ERR_BAD_SIZE       = (const REBYTE*)"Size must be positive!";
static const REBYTE* ERR_NO_WIDGET      = (const REBYTE*)"Failed to create the widget!";
static const REBYTE* ERR_BAD_IMAGE      = (const REBYTE*)"Empty or invalid image!";
static const REBYTE* ERR_NO_PORT_STATE  = (const REBYTE*)"Not a usable port!";
static const REBYTE* ERR_DEVICE_FAIL    = (const REBYTE*)"GUI device command failed!";

// The device request behind a port. A port's state field is where the host
// keeps it, and this is the only way an extension can reach the device it
// registered: there is no OS_Do_Device in the RL_ API.
#define GUI_DEV_REQ(n) RL_PORT_STATE(RXA_PORT(frm, n), (REBCNT)Gui_Dev_Id)

// The menu dialect's separator. Not in a `words:` list because to-c-name
// would spell it W_GUI_MENU____; it is one symbol, so it is mapped by name
// in Gui_Init() instead. See Block_To_Menu().
REBCNT Word_Separator = 0;


//== event queue ==============================================================
//
// A fixed ring buffer, written by the window procedure and drained by
// `poll-events`. Deliberately allocation free: the producer may run inside a
// modal OS loop (moving or resizing a window), where calling back into the
// interpreter to grow a series would be a very bad idea.
//
// The size must stay a power of two - the free running counters are masked,
// not wrapped, so that `Head - Tail` is the count even across overflow.

#define GUI_QUEUE_SIZE 512
#define GUI_QUEUE_MASK (GUI_QUEUE_SIZE - 1)

static GUIEVT Event_Queue[GUI_QUEUE_SIZE];
static REBCNT Event_Head = 0;    // next slot to be written
static REBCNT Event_Tail = 0;    // next slot to be read
static REBCNT Event_Dropped = 0; // reported once, by the next poll

#define QUEUE_COUNT() ((REBCNT)(Event_Head - Event_Tail))
#define QUEUE_AT(n)   (&Event_Queue[((Event_Tail) + (n)) & GUI_QUEUE_MASK])


/***********************************************************************
**  Appends an event, unless the queue is full.
**
**  Consecutive `move` events of the same window are collapsed onto the
**  last one: a fast mouse produces hundreds of them per second and only
**  the newest position is of any use.
***********************************************************************/
void Gui_Queue_Event(REBHOB *source, REBCNT type, REBINT x, REBINT y, REBINT value)
{
	GUIEVT *evt;

	if (type == EVT_MOVE && QUEUE_COUNT() > 0) {
		evt = QUEUE_AT(QUEUE_COUNT() - 1);
		if (evt->type == EVT_MOVE && evt->source == source) {
			evt->x = x;
			evt->y = y;
			evt->value = value;
			return;
		}
	}

	if (QUEUE_COUNT() >= GUI_QUEUE_SIZE) {
		Event_Dropped++;
		return;
	}

	evt = &Event_Queue[Event_Head & GUI_QUEUE_MASK];
	CLEARS(evt);           // `drop` is NULL for everything but a drop event
	evt->source = source;
	evt->type   = type;
	evt->x      = x;
	evt->y      = y;
	evt->value  = value;
	Event_Head++;
}


/***********************************************************************
**  A drop payload: plain C memory, grown one string at a time.
**
**  Neither OS hands its strings over in one block - Win32 asks for them
**  by index and AppKit has an array of NSStrings - so this appends, and
**  keeps them NUL separated in one buffer rather than as an array of
**  pointers. One allocation to free, and `poll-events` walks it once.
***********************************************************************/
GUIDROPDATA *Gui_Drop_Payload(REBCNT kind, REBCNT size)
{
	GUIDROPDATA *data = (GUIDROPDATA*)MAKE_CLEAR_MEM(sizeof(GUIDROPDATA));
	if (!data) return NULL;

	if (size < 256) size = 256;
	data->text = (REBYTE*)MAKE_MEM(size);
	if (!data->text) {
		FREE_MEM(data);
		return NULL;
	}
	data->kind     = kind;
	data->capacity = size;
	data->text[0]  = 0;    // size and count are already zero
	return data;
}


REBOOL Gui_Drop_Append(GUIDROPDATA *data, const REBYTE *utf8, REBCNT len)
{
	if (!data || !utf8) return FALSE;

	if (data->size + len + 1 > data->capacity) {
		REBCNT want = (data->size + len + 1) * 2;
		REBYTE *grown = (REBYTE*)MAKE_MEM(want);
		if (!grown) return FALSE;
		COPY_MEM(grown, data->text, data->size);
		FREE_MEM(data->text);
		data->text = grown;
		data->capacity = want;
	}

	COPY_MEM(data->text + data->size, utf8, len);
	data->size += len;
	data->text[data->size++] = 0;
	data->count++;
	return TRUE;
}


void Gui_Drop_Free(GUIDROPDATA *data)
{
	if (!data) return;
	if (data->text) FREE_MEM(data->text);
	FREE_MEM(data);
}


/***********************************************************************
**  Queues a drop.
**
**  The queue takes the payload whether the event fits or not, which is
**  the only way a full queue cannot leak it - the backend has already
**  told the OS the drop was accepted by the time it gets here.
***********************************************************************/
void Gui_Queue_Drop(REBHOB *target, GUIDROPDATA *data, REBINT x, REBINT y)
{
	GUIEVT *evt;

	if (!data) return;
	if (!target || QUEUE_COUNT() >= GUI_QUEUE_SIZE) {
		Event_Dropped++;
		Gui_Drop_Free(data);
		return;
	}

	evt = &Event_Queue[Event_Head & GUI_QUEUE_MASK];
	CLEARS(evt);
	evt->source = target;
	evt->type   = (data->kind == GUI_DROP_TEXT) ? EVT_DROP_TEXT : EVT_DROP_FILE;
	evt->x      = x;
	evt->y      = y;
	evt->drop   = data;
	Event_Head++;
}


/***********************************************************************
**  Removes every queued event of one source.
**
**  Queued events hold the source's handle context, and closing a window or
**  a widget drops the lock which kept that context alive - so anything
**  still referring to it has to go at the same moment.
***********************************************************************/
static void Purge_Events(REBHOB *source)
{
	REBCNT count = QUEUE_COUNT();
	REBCNT kept = 0;
	REBCNT n;

	for (n = 0; n < count; n++) {
		GUIEVT *evt = QUEUE_AT(n);
		if (evt->source == source) {
			// The payload is the queue's to free - nothing else holds it
			// until `poll-events` turns it into Rebol values.
			Gui_Drop_Free(evt->drop);
			continue;
		}
		if (kept != n) *QUEUE_AT(kept) = *evt;
		kept++;
	}
	Event_Head = Event_Tail + kept;
}


/***********************************************************************
**  What the device poll asks before doing anything.
**
**  The window count is kept rather than derived: there is no global
**  window list, and the poll runs on every WAIT the program makes, so
**  the answer has to be a load rather than a walk.
***********************************************************************/
static REBCNT Open_Windows = 0;

REBOOL Gui_Windows_Open(void)
{
	return Open_Windows > 0 ? TRUE : FALSE;
}

// What RDC_READ reports for `read gui/event-port`.
REBCNT Gui_Event_Count(void)
{
	return QUEUE_COUNT();
}


/***********************************************************************
**  Answers TRUE exactly once per batch: the device rings its doorbell on
**  the first poll which finds the queue non-empty, and not again until
**  `poll-events` has drained it.
**
**  Why "anything waiting" rather than "anything new since the pump": not
**  every event arrives during a pump. A programmatic resize dispatches
**  WM_SIZE inside SetWindowPos, so the event is queued from the middle of
**  `win/size:` - and the user dragging a window runs a modal OS loop
**  which dispatches for itself and never comes through Gui_Pump at all.
**
**  And why it is asked HERE rather than done in Gui_Queue_Event, which
**  would be the obvious place: that function is the one thing in this
**  file which may run inside such a modal loop, where calling RL_Event
**  would grow a Rebol series - see the note above it. The poll is on the
**  interpreter's own thread of control and is safe.
***********************************************************************/
static REBOOL Event_Rung = FALSE;

REBOOL Gui_Ring_Doorbell(void)
{
	if (QUEUE_COUNT() == 0 || Event_Rung) return FALSE;
	Event_Rung = TRUE;
	return TRUE;
}


// Undoes what keeps a handle context alive while its native object exists.
static void Release_Handle(REBHOB *hob)
{
	if (!hob) return;
	Purge_Events(hob);
	hob->flags &= ~HANDLE_CONTEXT_LOCKED;
}


/***********************************************************************
**  Radio groups.
**
**  Neither platform groups radios the way a caller means it. Win32 goes
**  by sibling order bounded by WS_GROUP flags; AppKit goes by superview
**  and action selector - and since every control here is a direct child
**  of the window with the same action, both would put every radio of a
**  window into one group.
**
**  So the grouping is done here instead, over the window's own widget
**  list, and `wid->state` - not the native control - is the truth.
**
**  Every radio in the window is then written, not just this group's: a
**  click on one radio makes AppKit clear the others behind our back, so
**  the ones it touched have to be put back. That only works because
**  Gui_Widget_Set_State() writes a state without AppKit reading it as a
**  group operation - see the note on it in gui-mac.m. Writing them in
**  two passes, with the one being switched on written LAST, keeps the
**  right control selected even if some platform still insists on
**  clearing siblings when a radio goes on.
**
**  NOTE: the widget list is in reverse creation order, because widgets
**  are prepended to it. Nothing here may depend on that order.
***********************************************************************/
static void Sync_Radio_Group(GUIWIDGET *wid, REBOOL on)
{
	GUIWIDGET *other;

	if (!wid || !wid->owner) return;

	wid->state = on ? 1 : 0;

	if (on) {
		for (other = (GUIWIDGET*)wid->owner->widgets; other;
		     other = (GUIWIDGET*)other->next)
		{
			if (other != wid
			 && other->kind  == W_GUI_WIDGET_RADIO
			 && other->group == wid->group) other->state = 0;
		}
	}

	// pass 1: everything which should be off
	for (other = (GUIWIDGET*)wid->owner->widgets; other;
	     other = (GUIWIDGET*)other->next)
	{
		if (other->kind == W_GUI_WIDGET_RADIO && other->handle && !other->state)
			Gui_Widget_Set_State(other, FALSE);
	}

	// pass 2: everything which should be on, except this one
	for (other = (GUIWIDGET*)wid->owner->widgets; other;
	     other = (GUIWIDGET*)other->next)
	{
		if (other != wid
		 && other->kind == W_GUI_WIDGET_RADIO && other->handle && other->state)
			Gui_Widget_Set_State(other, TRUE);
	}

	// pass 3: and this one last of all
	if (wid->handle && wid->state) Gui_Widget_Set_State(wid, TRUE);
}


/***********************************************************************
**  A control was activated - a button pressed, a box ticked.
**
**  A checkbox has already toggled itself by the time this runs, so its
**  state is only read back; a radio is switched on and its group
**  settled. Everything else just reports the click.
***********************************************************************/
void Gui_Widget_Activated(GUIWIDGET *widget, REBINT x, REBINT y, REBINT flags)
{
	if (!widget || !widget->hob) return;

	switch (widget->kind) {
	case W_GUI_WIDGET_CHECK:
		widget->state = Gui_Widget_Get_State(widget) ? 1 : 0;
		break;
	case W_GUI_WIDGET_RADIO:
		// Clicking a radio always turns it on - there is no untick.
		Sync_Radio_Group(widget, TRUE);
		break;
	}

	Gui_Queue_Event(widget->hob, EVT_CLICK, x, y, flags);
}


// Fills an RXIARG with a handle context; defined further down, where the
// rest of the argument helpers are.
static void Set_Handle_Arg(RXIARG *arg, REBHOB *hob);

/***********************************************************************
**  The one GC-marked slot, shared.
**
**  A handle context has exactly ONE series the collector marks, and
**  three kinds need two things kept alive in it: whatever the kind
**  itself holds, and the children once it has any. A window holds the
**  menu block it was given; an image widget holds its image!.
**
**  So the slot is a BLOCK of exactly two values, with fixed meanings:
**
**      [0] the payload  - an image! for an image widget, the menu block
**                         for a window, none for anything else
**      [1] the children - a block of handles, or none
**
**  Marking the outer block marks both, and nothing outside these four
**  functions knows the layout. A widget which is neither a container
**  nor an image never allocates one.
***********************************************************************/
#define SLOT_PAYLOAD  0
#define SLOT_CHILDREN 1

static REBSER *Hob_Slots(REBHOB *hob)
{
	REBSER *blk;
	RXIARG  none;

	if (!hob) return NULL;
	if (hob->series) return hob->series;

	blk = (REBSER*)RL_MAKE_BLOCK(2);
	if (!blk) return NULL;

	// Stored BEFORE anything else can allocate: until it is in the slot
	// the GC marks, nothing references this block and a collection in
	// the middle of filling it would take it away.
	hob->series = blk;

	// Both slots exist from the start, so the layout never depends on
	// which of the two was assigned first.
	CLEARS(&none);
	RL_SET_VALUE(blk, SLOT_PAYLOAD,  none, RXT_NONE);
	RL_SET_VALUE(blk, SLOT_CHILDREN, none, RXT_NONE);

	return blk;
}


// The payload as a plain series - an image's pixels, a menu's block - or
// NULL when there is none.
static REBSER *Hob_Payload(REBHOB *hob)
{
	RXIARG val;
	REBCNT type;

	if (!hob || !hob->series) return NULL;
	type = RL_GET_VALUE(hob->series, SLOT_PAYLOAD, &val);
	if (type != RXT_IMAGE && type != RXT_BLOCK) return NULL;

	// An image! and a block! carry their series in the same place. The
	// index is deliberately not read: for an image it overlaps the
	// dimensions, which is the trap this extension has been caught by
	// before.
	return (REBSER*)val.series;
}


// Replaces the payload. A type of RXT_NONE clears it.
static REBOOL Hob_Set_Payload(REBHOB *hob, RXIARG *val, REBCNT type)
{
	REBSER *blk = Hob_Slots(hob);
	RXIARG  none;

	if (!blk) return FALSE;
	if (!val) {
		CLEARS(&none);
		val  = &none;
		type = RXT_NONE;
	}
	RL_SET_VALUE(blk, SLOT_PAYLOAD, *val, (int)type);
	return TRUE;
}


// The children block. `make` decides whether an absent one is created,
// so a read of `children` on a childless container allocates nothing.
static REBSER *Hob_Children(REBHOB *hob, REBOOL make)
{
	RXIARG  val;
	REBSER *blk, *kids;

	if (!hob) return NULL;

	if (hob->series
	    && RL_GET_VALUE(hob->series, SLOT_CHILDREN, &val) == RXT_BLOCK)
		return (REBSER*)val.series;

	if (!make) return NULL;

	blk = Hob_Slots(hob);
	if (!blk) return NULL;

	kids = (REBSER*)RL_MAKE_BLOCK(4);
	if (!kids) return NULL;

	CLEARS(&val);
	val.series = kids;
	val.index  = 0;
	// Protected across the store: the block is referenced by nothing
	// until it is in the slot.
	RL_PROTECT_GC(kids, 1);
	RL_SET_VALUE(blk, SLOT_CHILDREN, val, RXT_BLOCK);
	RL_PROTECT_GC(kids, 0);

	return kids;
}


// Appends a child's handle to its container's list.
static void Add_Child(REBHOB *parent, REBHOB *child)
{
	REBSER *kids;
	RXIARG  val;

	if (!parent || !child) return;
	kids = Hob_Children(parent, TRUE);
	if (!kids) return;

	Set_Handle_Arg(&val, child);
	RL_SET_VALUE(kids, (u32)RL_SERIES(kids, RXI_SER_TAIL), val, RXT_HANDLE);
}


// Takes it out again, keeping the order of the rest.
static void Drop_Child(REBHOB *parent, REBHOB *child)
{
	REBSER *kids;
	RXIARG  val;
	REBCNT  n, tail, kept = 0;

	if (!parent || !child) return;
	kids = Hob_Children(parent, FALSE);
	if (!kids) return;

	tail = (REBCNT)RL_SERIES(kids, RXI_SER_TAIL);
	for (n = 0; n < tail; n++) {
		if (RL_GET_VALUE(kids, n, &val) != RXT_HANDLE) continue;
		if (val.handle.hob == child) continue; // the one going away
		if (kept != n) RL_SET_VALUE(kids, kept, val, RXT_HANDLE);
		kept++;
	}

	// Nothing in the RL_ API shortens a block, and leaving the tail full
	// of stale handles is not an option - so the length is set directly
	// and the block re-terminated, which is what SET_VALUE would have
	// done on the way past.
	SERIES_TAIL(kids) = kept;
	SET_END(BLK_TAIL(kids));
}



/***********************************************************************
**  Called when a widget's native control is gone.
**
**  Unlinks it from its window, so that the window does not later try to
**  close a widget which has already been dealt with.
***********************************************************************/
void Gui_Widget_Closed(GUIWIDGET *widget)
{
	if (!widget) return;

	// Out of whatever held it - a container's own children block, or the
	// window's. Done before `parent` and `owner` are cleared below,
	// because they are what says where it was.
	Drop_Child(widget->parent
		? ((GUIWIDGET*)widget->parent)->hob
		: (widget->owner ? widget->owner->hob : NULL),
		widget->hob);

	if (widget->owner) {
		GUIWIDGET **link = (GUIWIDGET**)&widget->owner->widgets;
		while (*link) {
			if (*link == widget) { *link = (GUIWIDGET*)widget->next; break; }
			link = (GUIWIDGET**)&(*link)->next;
		}
		widget->owner = NULL;
	}
	widget->handle = NULL;
	widget->parent = NULL;
	widget->next   = NULL;
	Release_Handle(widget->hob);
}


/***********************************************************************
**  Empties a panel before it is itself destroyed.
**
**  Each child is destroyed properly rather than left to the panel: on
**  Windows the OS would take them anyway, but on macOS this file holds a
**  reference to every control, so letting the container drop them would
**  leak one object each. Destroying them first is right on both.
**
**  Immediate children only, and a nested panel is emptied before it goes
**  - so nothing is ever destroyed after the thing containing it, and a
**  native handle is always still valid when it is used.
**
**  The list is re-walked from the start after every removal, because
**  closing a widget unlinks it and any pointer into the list is stale
**  from that moment. These lists hold a handful of entries; correctness
**  is worth more here than a single pass.
***********************************************************************/
static void Close_Contents_Of(GUIWIDGET *panel)
{
	REBOOL again = TRUE;

	if (!panel || !panel->owner) return;

	while (again) {
		GUIWIDGET *wid;
		again = FALSE;
		for (wid = (GUIWIDGET*)panel->owner->widgets; wid;
		     wid = (GUIWIDGET*)wid->next)
		{
			if ((GUIWIDGET*)wid->parent != panel) continue;

			if (Kind_Is_Container(wid->kind)) Close_Contents_Of(wid);
			Gui_Destroy_Widget(wid); // the native control
			Gui_Widget_Closed(wid);  // the Rebol side of it
			again = TRUE;
			break;
		}
	}
}


/***********************************************************************
**  Called by the backend when a native window has really gone away.
**
**  Everything which kept the handle context alive is undone here, in one
**  place, so that it does not matter whether the window was closed by
**  `close-window`, by releasing the handle, or by the system.
**
**  Child controls go with their parent on both platforms, so their
**  handles are released here too - without destroying anything, which
**  the OS has already done. A backend which owns references to its
**  native child objects must have released them before calling this.
***********************************************************************/
void Gui_Window_Closed(REBHOB *window)
{
	GUIWIN *win;

	if (!window) return;

	win = (GUIWIN*)window->data;
	if (win) {
		GUIWIDGET *widget = (GUIWIDGET*)win->widgets;
		while (widget) {
			GUIWIDGET *next = (GUIWIDGET*)widget->next;
			widget->handle = NULL;
			widget->owner  = NULL;
			widget->next   = NULL;
			Release_Handle(widget->hob);
			widget = next;
		}
		win->widgets = NULL;
	}
	if (Open_Windows > 0) Open_Windows--;
	Release_Handle(window);
}


//== helpers ==================================================================

// Validates argument `n` as a live window handle.
static GUIWIN* Frm_Window(RXIFRM *frm, REBCNT n)
{
	REBHOB *hob;
	if (!FRM_IS_HANDLE(n, Handle_GuiWindow)) return NULL;
	hob = RXA_HANDLE_CONTEXT(frm, n);
	if (!hob || !IS_USED_HOB(hob) || !hob->data) return NULL;
	return (GUIWIN*)hob->data;
}

// Which kinds carry a string: everything except the image widget. `text`
// means the label on a button or a static, and the contents of an entry.
static REBOOL Kind_Has_Text(REBCNT kind)
{
	return (kind == W_GUI_WIDGET_BUTTON
	     || kind == W_GUI_WIDGET_TEXT
	     || kind == W_GUI_WIDGET_FIELD
	     || kind == W_GUI_WIDGET_AREA
	     || kind == W_GUI_WIDGET_CHECK
	     || kind == W_GUI_WIDGET_RADIO
	     // the caption of a framed panel; harmless on an unframed one,
	     // which simply keeps a string nothing draws
	     || kind == W_GUI_WIDGET_PANEL
	     // readable, but not writable - see the set path
	     || kind == W_GUI_WIDGET_DROP_DOWN) ? TRUE : FALSE;
}

// Which kinds are on or off.
static REBOOL Kind_Has_State(REBCNT kind)
{
	return (kind == W_GUI_WIDGET_CHECK
	     || kind == W_GUI_WIDGET_RADIO) ? TRUE : FALSE;
}

// ... and which sit somewhere between 0% and 100%.
static REBOOL Kind_Has_Value(REBCNT kind)
{
	return (kind == W_GUI_WIDGET_SLIDER
	     || kind == W_GUI_WIDGET_PROGRESS) ? TRUE : FALSE;
}

/***********************************************************************
**  Block <-> list marshalling for a drop-down.
**
**  Done here rather than in the backends: it is identical work on every
**  platform, and a backend only has to know how to hold strings.
***********************************************************************/
static REBSER* Items_To_Block(GUIWIDGET *wid)
{
	REBCNT count = Gui_Widget_Count_Items(wid);
	REBCNT n;
	REBSER *blk;
	RXIARG item;

	// Sized up front, so filling it cannot expand - and so cannot collect
	// the strings put in on the way.
	blk = (REBSER*)RL_MAKE_BLOCK(count);
	if (!blk) return NULL;

	RL_PROTECT_GC(blk, 1);
	for (n = 0; n < count; n++) {
		REBSER *str = Gui_Widget_Get_Item(wid, n);
		if (!str) continue;
		CLEARS(&item);
		item.series = str;
		item.index  = 0;
		RL_SET_VALUE(blk, n, item, RXT_STRING);
	}
	RL_PROTECT_GC(blk, 0);
	return blk;
}


// Replaces the list. Anything in the block which is not a string is
// skipped rather than refused - a block of words or files is a reasonable
// thing to hand over, and FORM-ing it is the caller's business.
static void Block_To_Items(GUIWIDGET *wid, REBSER *blk)
{
	REBCNT n;
	REBCNT type;
	RXIARG val;

	Gui_Widget_Clear_Items(wid);
	if (!blk) return;

	for (n = 0; (type = RL_GET_VALUE(blk, n, &val)) != 0; n++) {
		REBYTE *utf8 = NULL;
		int len;

		if (type == RXT_END) break;
		if (type != RXT_STRING) continue;

		len = RL_GET_UTF8_STRING((REBSER*)val.series, val.index, (void**)&utf8);
		if (len < 0) continue;
		Gui_Widget_Add_Item(wid, utf8, (REBCNT)len);
	}
}


//== the menu dialect =========================================================
//
//   win/menu: [
//       "File" [
//           "New"     new  #"N"          ;; Ctrl+N / Cmd+N
//           "Save As" save [shift #"S"]  ;; ... with extra modifiers
//           ---                          ;; a dividing line
//           "Recent" ["Nothing yet" nil] ;; a block after a label: a submenu
//       ]
//   ]
//
// One item is a LABEL followed by a WORD, which is the id that comes back
// in the event - not the label, so that renaming "Save" does not break a
// handler. A label followed by a BLOCK is a submenu instead.
//
// The whole thing is parsed here and pushed at the backend through the
// Gui_Menu_* calls, so neither backend ever sees a Rebol value, and the
// dialect is defined exactly once.

// Item ids are 1-based indices into these two arrays, which grow together.
static REBOOL Menu_Add_Id(GUIWIN *win, REBCNT word)
{
	REBCNT  n = win->menu_count;
	REBCNT *ids;
	REBYTE *on;

	// Powers of two from 16: a menu bar is small, and this is built once.
	if ((n & (n - 1)) == 0 && n >= 16) {
		// n is a power of two and the arrays are exactly full
		ids = (REBCNT*)MAKE_MEM(sizeof(REBCNT) * n * 2);
		on  = (REBYTE*)MAKE_MEM(n * 2);
		if (!ids || !on) {
			if (ids) FREE_MEM(ids);
			if (on)  FREE_MEM(on);
			return FALSE;
		}
		COPY_MEM(ids, win->menu_ids, sizeof(REBCNT) * n);
		COPY_MEM(on,  win->menu_on,  n);
		FREE_MEM(win->menu_ids);
		FREE_MEM(win->menu_on);
		win->menu_ids = ids;
		win->menu_on  = on;
	} else if (n == 0) {
		win->menu_ids = (REBCNT*)MAKE_MEM(sizeof(REBCNT) * 16);
		win->menu_on  = (REBYTE*)MAKE_MEM(16);
		if (!win->menu_ids || !win->menu_on) return FALSE;
	}

	win->menu_ids[n] = word;
	win->menu_on[n]  = 1; // every item starts selectable
	win->menu_count  = n + 1;
	return TRUE;
}

static void Menu_Free_Ids(GUIWIN *win)
{
	if (win->menu_ids) FREE_MEM(win->menu_ids);
	if (win->menu_on)  FREE_MEM(win->menu_on);
	win->menu_ids   = NULL;
	win->menu_on    = NULL;
	win->menu_count = 0;
}

// A shortcut is either a bare char! - the platform's own menu modifier plus
// that key - or a block of modifier words ending in one.
static void Menu_Shortcut(REBCNT type, RXIARG *val, REBCNT *key, REBCNT *mods)
{
	*key  = 0;
	*mods = 0;

	if (type == RXT_CHAR) {
		*key = (REBCNT)val->int32a;
		return;
	}
	if (type != RXT_BLOCK) return;

	{	REBSER *blk = (REBSER*)val->series;
		REBCNT  n;
		RXIARG  item;
		REBCNT  t;

		for (n = val->index; (t = RL_GET_VALUE(blk, n, &item)) != 0; n++) {
			if (t == RXT_END) break;
			if (t == RXT_CHAR) { *key = (REBCNT)item.int32a; continue; }
			if (t != RXT_WORD) continue;
			switch (RL_FIND_WORD(Gui_menu_words, (REBCNT)item.int32a)) {
			case W_GUI_MENU_SHIFT:   *mods |= GUI_FLAG_SHIFT;   break;
			case W_GUI_MENU_CONTROL: *mods |= GUI_FLAG_CONTROL; break;
			case W_GUI_MENU_ALT:     *mods |= GUI_FLAG_ALT;     break;
			}
		}
	}
}

/***********************************************************************
**  Walks one level of the dialect, building into `parent` - which is
**  NULL for the menu bar itself and a backend's popup handle below it.
**
**  Recursive, because a submenu is the same grammar again. The depth is
**  whatever the caller wrote, and a block cannot contain itself, so
**  there is nothing to guard against.
***********************************************************************/
static void Block_To_Menu(GUIWIN *win, REBSER *blk, REBCNT index, void *parent)
{
	REBCNT n;
	REBCNT type;
	RXIARG val;

	if (!blk) return;

	for (n = index; (type = RL_GET_VALUE(blk, n, &val)) != 0; n++) {
		REBYTE *label = NULL;
		int     label_len;
		REBCNT  next_type;
		RXIARG  next;

		if (type == RXT_END) break;

		// `---` on its own
		if (type == RXT_WORD && (REBCNT)val.int32a == Word_Separator) {
			Gui_Menu_Add_Separator(win, parent);
			continue;
		}

		// Everything else starts with a label, and anything which is not
		// one is skipped rather than refused: a menu is a description, and
		// a stray value in it should not cost the caller the whole bar.
		if (type != RXT_STRING) continue;
		label_len = RL_GET_UTF8_STRING((REBSER*)val.series, val.index,
		                               (void**)&label);
		if (label_len < 0) continue;

		next_type = RL_GET_VALUE(blk, n + 1, &next);

		// label + block -> a submenu
		if (next_type == RXT_BLOCK) {
			void *popup = Gui_Menu_Add_Popup(win, parent, label,
			                                 (REBCNT)label_len);
			if (popup) {
				Block_To_Menu(win, (REBSER*)next.series, next.index, popup);
			}
			n++;
			continue;
		}

		// label + word -> an item, optionally followed by a shortcut
		if (next_type == RXT_WORD) {
			REBCNT key = 0, mods = 0;
			RXIARG after;
			REBCNT after_type;

			if (!Menu_Add_Id(win, (REBCNT)next.int32a)) return;

			after_type = RL_GET_VALUE(blk, n + 2, &after);
			if (after_type == RXT_CHAR || after_type == RXT_BLOCK) {
				Menu_Shortcut(after_type, &after, &key, &mods);
				n++;
			}
			Gui_Menu_Add_Item(win, parent, label, (REBCNT)label_len,
			                  win->menu_count, key, mods);
			n++;
			continue;
		}

		// A label with nothing after it is a dead item: it is shown, and
		// it does nothing, which is more informative than dropping it.
		if (Menu_Add_Id(win, 0)) {
			Gui_Menu_Add_Item(win, parent, label, (REBCNT)label_len,
			                  win->menu_count, 0, 0);
		}
	}
}


// Rebuilds the whole bar from a block, or takes it away when blk is NULL.
static REBOOL Set_Menu(GUIWIN *win, REBSER *blk, REBCNT index)
{
	Gui_Menu_Free(win);
	Menu_Free_Ids(win);
	// The PAYLOAD only: the same slot block holds the window's children,
	// which a menu going away has nothing to do with.
	Hob_Set_Payload(win->hob, NULL, RXT_NONE);

	if (!blk) return TRUE;

	if (!Gui_Menu_Begin(win)) return FALSE;
	Block_To_Menu(win, blk, index, NULL);
	if (!Gui_Menu_End(win)) {
		Gui_Menu_Free(win);
		Menu_Free_Ids(win);
		return FALSE;
	}

	// Kept so that `win/menu` can answer with the very block it was given,
	// in the payload half of the one slot the GC marks.
	{	RXIARG val;
		CLEARS(&val);
		val.series = blk;
		val.index  = index;
		Hob_Set_Payload(win->hob, &val, RXT_BLOCK);
	}
	return TRUE;
}


/***********************************************************************
**  Called by a backend when an item is picked.
**
**  The native id is only ever an index into this window's table; the
**  WORD is what reaches Rebol, which is why renaming a label cannot
**  break a handler.
***********************************************************************/
void Gui_Menu_Picked(GUIWIN *win, REBCNT id)
{
	if (!win || !win->hob) return;
	if (id == 0 || id > win->menu_count) return;
	if (win->menu_ids[id - 1] == 0) return; // a label with no id of its own

	Gui_Queue_Event(win->hob, EVT_MENU_SELECT, 0, 0,
	                (REBINT)win->menu_ids[id - 1]);
}


// An image is painted, not operated, and a progress bar takes no input at
// all - neither has an enabled state worth reporting.
static REBOOL Kind_Has_Enabled(REBCNT kind)
{
	// A panel is left out because the two platforms disagree: disabling a
	// child window on Windows greys everything inside it, while an NSView
	// has no enabled state at all.
	return (kind != W_GUI_WIDGET_IMAGE
	     && kind != W_GUI_WIDGET_PROGRESS
	     && kind != W_GUI_WIDGET_PANEL) ? TRUE : FALSE;
}

// Which kinds can be read-only: the two the user types into. A label
// cannot be edited to begin with, and a drop-down's text is already
// read-only in a sense of its own - you pick an item rather than write one.
#define Kind_Has_Read_Only(kind) \
	((kind) == W_GUI_WIDGET_FIELD || (kind) == W_GUI_WIDGET_AREA)

// Which kinds scroll, and can be asked where they are. Only an area for
// now: a drop-down's list scrolls but is not addressable, and nothing
// else here has a scrollbar at all.
#define Kind_Scrolls(kind) ((kind) == W_GUI_WIDGET_AREA)

static const char* Kind_Name(REBCNT kind)
{
	switch (kind) {
	case W_GUI_WIDGET_IMAGE: return "image";
	case W_GUI_WIDGET_TEXT:  return "text";
	case W_GUI_WIDGET_FIELD: return "field";
	case W_GUI_WIDGET_AREA:  return "area";
	case W_GUI_WIDGET_CHECK: return "check";
	case W_GUI_WIDGET_RADIO: return "radio";
	case W_GUI_WIDGET_SLIDER:   return "slider";
	case W_GUI_WIDGET_PROGRESS: return "progress";
	case W_GUI_WIDGET_DROP_DOWN: return "drop-down";
	case W_GUI_WIDGET_PANEL:     return "panel";
	default:                 return "button";
	}
}

// ... and as a live widget handle.
static GUIWIDGET* Frm_Widget(RXIFRM *frm, REBCNT n)
{
	REBHOB *hob;
	if (!FRM_IS_HANDLE(n, Handle_GuiWidget)) return NULL;
	hob = RXA_HANDLE_CONTEXT(frm, n);
	if (!hob || !IS_USED_HOB(hob) || !hob->data) return NULL;
	return (GUIWIDGET*)hob->data;
}

/***********************************************************************
**  Resolves argument `n` as the thing a new widget goes into: either a
**  window handle, or a panel handle.
**
**  Returns the owning WINDOW, and sets *panel to the containing panel or
**  NULL. Widgets always belong to a window - the panel only says where
**  they sit inside it - which is what keeps the window's widget list flat
**  and teardown a single walk.
***********************************************************************/
static GUIWIN* Frm_Parent(RXIFRM *frm, REBCNT n, GUIWIDGET **container)
{
	GUIWIDGET *wid;
	GUIWIN *win;

	*container = NULL;

	win = Frm_Window(frm, n);
	if (win) return win;

	wid = Frm_Widget(frm, n);
	if (!wid || !wid->handle) return NULL;

	// Which kinds may hold other widgets. A PANEL is the obvious one; an
	// IMAGE is here so that a label or a check can sit ON rendered pixels
	// rather than beside them - which only works if it is a CHILD of the
	// image, because two overlapping siblings have no defined order on
	// Win32 and would fight over the same pixels.
	//
	// Nothing else: a button with children inside it is not a thing, and
	// the native control would not clip or move them.
	if (wid->kind != W_GUI_WIDGET_PANEL && wid->kind != W_GUI_WIDGET_IMAGE)
		return NULL;

	*container = wid;
	return wid->owner;
}


/***********************************************************************
**  The last step of every add-* command.
**
**  Links the widget onto its window's list - done here rather than by a
**  backend, so the list has exactly one owner and both platforms behave
**  the same - locks the handle against the GC, and paints the control.
**
**  The redraw request matters: a control created after the window was
**  shown has nothing on screen until something paints it.
**
**  What that request DOES is the backend's business, and the two differ.
**  Windows paints there and then. macOS only marks the control and lets
**  the next Gui_Pump() paint it, because AppKit draws between events and
**  forcing it at any other moment is unreliable - the first widgets of a
**  batch would silently never appear.
***********************************************************************/
// Which kinds have text to set a font and a colour on. It is exactly the
// kinds which have `text` - if there is nothing to read, there is nothing
// to style - so the two questions share one answer rather than drifting
// apart as kinds are added.
#define Kind_Has_Font(kind) Kind_Has_Text(kind)

/***********************************************************************
**  Gives a widget the size its own content asks for, on the axes named.
**
**  ONLY WHEN ASKED. Nothing here happens on its own: a widget which was
**  told how big to be stays that size, whatever happens to its font or
**  its text afterwards, because the box a script laid out is the box it
**  meant. Re-fitting on every change would move widgets around under a
**  layout that had already been settled.
**
**  Asking is a zero axis - the convention `add-*` already uses, both at
**  creation and now through `widget/size:`.
**
**  Returns FALSE for a kind with nothing to measure: an image widget is
**  whatever size it was given, and a panel is a container whose contents
**  this layer knows nothing about.
***********************************************************************/
static REBOOL Fit_To_Content(GUIWIDGET *wid, REBOOL fit_w, REBOOL fit_h)
{
	REBINT nw = 0, nh = 0;
	REBINT x, y, w, h;

	if (!wid || (!fit_w && !fit_h)) return TRUE; // nothing asked for
	if (!Gui_Widget_Natural_Size(wid, &nw, &nh)) return FALSE;
	if (!Gui_Widget_Get_Box(wid, &x, &y, &w, &h)) return FALSE;

	if (fit_w && nw > 0) w = nw;
	if (fit_h && nh > 0) h = nh;

	Gui_Widget_Set_Box(wid, x, y, w, h);
	return TRUE;
}

/***********************************************************************
**  Links a new widget to its window, and finishes it off.
**
**  `w` and `h` are what the caller ASKED for; a zero in either means
**  "work it out", and this is where that is worked out - after the
**  font, because the answer depends on it, and before the first draw,
**  so nothing is ever seen at the placeholder size.
***********************************************************************/
static void Attach_Widget(GUIWIDGET *wid, GUIWIN *win, REBINT w, REBINT h)
{
	wid->next = win->widgets;
	win->widgets = wid;

	if (wid->hob) wid->hob->flags |= HANDLE_CONTEXT_LOCKED;

	// And onto whatever holds it, which is what `children` reads back.
	// The intrusive list above is the extension's own and window-wide;
	// this is the per-container one Rebol sees.
	Add_Child(wid->parent ? ((GUIWIDGET*)wid->parent)->hob : win->hob,
	          wid->hob);

	// The window's default is read HERE, once, at creation - which is the
	// whole of the inheritance. Restyling a window afterwards changes what
	// the next widget starts with and leaves everything already on screen
	// alone, so a widget's font is only ever its own business.
	if (Kind_Has_Font(wid->kind)
	    && (win->font.name || win->font.size || win->font.style)) {
		Gui_Widget_Set_Font(wid,
			(const REBYTE*)win->font.name,
			win->font.name ? (REBCNT)strlen(win->font.name) : 0,
			win->font.size, win->font.style);
	}

	// A zero axis asks the widget, once, here. A kind with nothing to
	// measure keeps the placeholder box it was created at, which is why
	// the answer is not checked: `add-panel win 20x20 0x0` is the caller
	// asking a container how big its contents are, and there is no answer
	// to that at this level.
	Fit_To_Content(wid, w <= 0 ? TRUE : FALSE, h <= 0 ? TRUE : FALSE);

	// Invalidate, do not paint. A script builds its whole layout with
	// nothing pumping in between, so painting here would make the window
	// assemble itself visibly, one widget per `add-*`. Left to the pump,
	// every widget added since the last one appears together.
	Gui_Widget_Invalidate(wid);
}


/***********************************************************************
**  A window's default font, which is the only GUIFONT anything keeps.
**
**  The name is a copy: the Rebol string it came from belongs to the
**  caller and may be modified or collected the moment the accessor
**  returns.
***********************************************************************/
static REBOOL Font_Set_Name(GUIFONT *font, const REBYTE *utf8, REBCNT len)
{
	char *copy = NULL;

	if (utf8 && len > 0) {
		copy = (char*)MAKE_MEM(len + 1);
		if (!copy) return FALSE;
		COPY_MEM(copy, utf8, len);
		copy[len] = 0;
	}
	if (font->name) FREE_MEM(font->name);
	font->name = copy;
	return TRUE;
}

static void Font_Free(GUIFONT *font)
{
	if (font->name) FREE_MEM(font->name);
	font->name = NULL;
	font->size = 0;
	font->style = 0;
}


/***********************************************************************
**  Changing one part of a widget's font.
**
**  A native font is one object, not four settings, so every one of the
**  four accessors is the same read-change-write. `part` says which of
**  them is being written; the rest are carried over from what the
**  control already has.
***********************************************************************/
enum gui_font_part { FONT_PART_NAME, FONT_PART_SIZE, FONT_PART_STYLE };

static REBOOL Set_Font_Part(GUIWIDGET *wid, REBCNT part,
                            const REBYTE *name, REBCNT name_len,
                            REBINT size, REBCNT style, REBCNT style_mask)
{
	REBSER *current_name = NULL;
	REBINT  current_size = 0;
	REBCNT  current_style = 0;
	REBYTE *utf8 = NULL;
	int     len;

	if (!Gui_Widget_Get_Font(wid, &current_name, &current_size, &current_style))
		return FALSE;

	// Whatever the caller is not writing comes back from the control.
	if (part != FONT_PART_NAME && current_name) {
		len = RL_GET_UTF8_STRING(current_name, 0, (void**)&utf8);
		if (len > 0) {
			name     = utf8;
			name_len = (REBCNT)len;
		}
	}
	if (part != FONT_PART_SIZE) size = current_size;
	if (part != FONT_PART_STYLE)
		style = current_style;
	else
		style = (current_style & ~style_mask) | (style & style_mask);

	// Deliberately no re-fit here. A bigger font in a box which was laid
	// out for a smaller one clips, and the remedy is `widget/size:` with a
	// zero axis - a decision for the script, which knows what else is
	// around the widget, rather than for this function.
	return Gui_Widget_Set_Font(wid, name, name_len, size, style);
}


// Fills an RXIARG with a handle context, as `poll-events` and the `parent`
// accessor both need to hand one back to Rebol.
static void Set_Handle_Arg(RXIARG *arg, REBHOB *hob)
{
	CLEARS(arg);
	arg->handle.hob   = hob;
	arg->handle.type  = hob->sym;
	arg->handle.flags = hob->flags;
	arg->handle.index = hob->index;
}


/***********************************************************************
**  The image behind an image widget, as the backends want it.
**
**  Read fresh on every paint rather than cached: the series can move
**  when it is expanded, and its dimensions can change under the widget,
**  so a stored pointer would eventually be a stale one.
**
**  The pixel order is the image! datatype's own - BGRA on both supported
**  platforms - so neither backend converts anything.
***********************************************************************/
REBOOL Gui_Widget_Pixels(GUIWIDGET *wid, REBYTE **data, REBINT *w, REBINT *h)
{
	REBSER *img;

	if (!wid || !wid->hob) return FALSE;
	img = Hob_Payload(wid->hob);
	if (!img) return FALSE;

	*w    = (REBINT)IMG_WIDE(img);
	*h    = (REBINT)IMG_HIGH(img);
	*data = IMG_DATA(img);

	return (*w > 0 && *h > 0 && *data) ? TRUE : FALSE;
}


//== commands =================================================================

/***********************************************************************
**  open-window size [pair!] /title text [string!] /at offset [pair!] /hidden
**
**  `size` is the CLIENT size - the frame is added on top of it, so the
**  window has room for exactly the requested number of pixels.
***********************************************************************/
COMMAND cmd_gui_open_window(RXIFRM *frm, void *ctx)
{
	REBHOB *hob;
	GUIWIN *win;
	REBINT  x = GUI_DEFAULT_POS, y = GUI_DEFAULT_POS;
	REBINT  w = (REBINT)RXA_PAIR(frm, 1).x;
	REBINT  h = (REBINT)RXA_PAIR(frm, 1).y;
	REBYTE *title = NULL;
	REBCNT  title_len = 0;
	REBCNT  flags = 0;

	if (w <= 0 || h <= 0) RETURN_ERROR(ERR_BAD_SIZE);

	if (RXA_REF(frm, 2)) { // /title
		int len = RL_GET_UTF8_STRING(RXA_SERIES(frm, 3), RXA_INDEX(frm, 3), (void**)&title);
		if (len > 0) title_len = (REBCNT)len;
	}
	if (RXA_REF(frm, 4)) { // /at
		x = (REBINT)RXA_PAIR(frm, 5).x;
		y = (REBINT)RXA_PAIR(frm, 5).y;
	}
	if (RXA_REF(frm, 7)) flags |= GUI_WIN_FIXED;       // /fixed
	if (RXA_REF(frm, 8)) flags |= GUI_WIN_BORDERLESS;  // /borderless
	if (RXA_REF(frm, 9)) flags |= GUI_WIN_TRANSPARENT; // /transparent

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWindow);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	win = (GUIWIN*)hob->data;
	win->hob = hob; // the window procedure tags its events with it

	if (!Gui_Open_Window(win, x, y, w, h, title, title_len, flags)) {
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WINDOW);
	}

	// The native window and every queued event point back at this context,
	// so the GC must leave it alone until the window is closed.
	hob->flags |= HANDLE_CONTEXT_LOCKED;

	Open_Windows++; // the device poll pumps only while one of these exists

	if (!RXA_REF(frm, 6)) Gui_Show_Window(win, TRUE); // /hidden

	RETURN_HANDLE(hob);
}


/***********************************************************************
**  gui-device / gui-device-polls
**
**  Diagnostics, and the only way a script can tell from Rebol that the
**  device was accepted and is being polled - which is the thing the
**  whole event model now rests on.
***********************************************************************/
COMMAND cmd_gui_gui_device(RXIFRM *frm, void *ctx)
{
	RXA_INT64(frm, 1) = (i64)Gui_Dev_Id;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_gui_gui_device_polls(RXIFRM *frm, void *ctx)
{
	RXA_INT64(frm, 1) = (i64)Gui_Dev_Polls;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_gui_gui_device_events(RXIFRM *frm, void *ctx)
{
	RXA_INT64(frm, 1) = (i64)Gui_Dev_Events;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_gui_gui_device_pumps(RXIFRM *frm, void *ctx)
{
	RXA_INT64(frm, 1) = (i64)Gui_Dev_Pumps;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_gui_gui_device_messages(RXIFRM *frm, void *ctx)
{
	RXA_INT64(frm, 1) = (i64)Gui_Dev_Msgs;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}


/***********************************************************************
**  gui-port-open / gui-port-close / gui-port-read
**      port [port!]
**
**  The three actors of the `gui` scheme, which exists so that the device
**  has somewhere to deliver the event that wakes WAIT. They are NOT
**  exported: the scheme is defined in this extension's own mezzanine and
**  is the only caller.
**
**  Each one hands the port's request to the device's command table, so a
**  failure here means the device refused - not that the command is a
**  polite no-op.
***********************************************************************/
COMMAND cmd_gui_gui_port_open(RXIFRM *frm, void *ctx)
{
	REBREQ *req = GUI_DEV_REQ(1);
	if (!req) RETURN_ERROR(ERR_NO_PORT_STATE);
	if (RL_DO_DEVICE(req, RDC_OPEN) < 0) RETURN_ERROR(ERR_DEVICE_FAIL);
	return RXR_VALUE; // the port, unchanged
}

COMMAND cmd_gui_gui_port_close(RXIFRM *frm, void *ctx)
{
	REBREQ *req = GUI_DEV_REQ(1);
	if (!req) RETURN_ERROR(ERR_NO_PORT_STATE);
	if (RL_DO_DEVICE(req, RDC_CLOSE) < 0) RETURN_ERROR(ERR_DEVICE_FAIL);
	return RXR_VALUE;
}

// Reports how many events `poll-events` would return, and drains nothing.
COMMAND cmd_gui_gui_port_read(RXIFRM *frm, void *ctx)
{
	REBREQ *req = GUI_DEV_REQ(1);
	if (!req) RETURN_ERROR(ERR_NO_PORT_STATE);
	if (RL_DO_DEVICE(req, RDC_READ) < 0) RETURN_ERROR(ERR_DEVICE_FAIL);
	RXA_INT64(frm, 1) = (i64)req->actual;
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}


/***********************************************************************
**  close-window window [handle!]
**
**  Destroys the native window. The handle stays valid and simply reports
**  `open?` as false afterwards, so Rebol code holding it cannot crash.
***********************************************************************/
COMMAND cmd_gui_close_window(RXIFRM *frm, void *ctx)
{
	GUIWIN *win = Frm_Window(frm, 1);

	if (!win) RETURN_ERROR(ERR_INVALID_HANDLE);

	// Closing an already closed window is not an error - it is what a
	// `close` event handler ends up doing when the user was quicker.
	if (win->handle) Gui_Close_Window(win); // -> Gui_Window_Closed()

	return RXR_TRUE;
}


COMMAND cmd_gui_show_window(RXIFRM *frm, void *ctx)
{
	GUIWIN *win = Frm_Window(frm, 1);
	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);
	Gui_Show_Window(win, TRUE);
	return RXR_TRUE;
}


COMMAND cmd_gui_hide_window(RXIFRM *frm, void *ctx)
{
	GUIWIN *win = Frm_Window(frm, 1);
	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);
	Gui_Show_Window(win, FALSE);
	return RXR_TRUE;
}


/***********************************************************************
**  Turns a queued drop payload into a drop HANDLE.
**
**  Called only from `poll-events`, which is on the interpreter's own
**  thread of control - the producer side must not allocate, which is
**  the whole reason the payload is plain C memory until here.
**
**  Everything the handle reports lives in its shared hob->series slot:
**
**      [0] the content - a block of file! for files, a string! for text
**      [1] the target  - the window or widget handle it was dropped on
**
**  Both are therefore marked by the collector, so a drop handle kept by
**  a script stays valid however long it is held, and the target cannot
**  dangle after its window closes - it reports itself as closed, the
**  same as any other widget handle would.
**
**  Returns NULL if anything could not be allocated; the caller then
**  reports the event with its target as the source rather than dropping
**  it, so a drop is never silently lost.
***********************************************************************/
static REBHOB *Make_Drop_Handle(REBHOB *target, GUIDROPDATA *data)
{
	REBHOB  *hob;
	GUIDROP *drop;
	REBSER  *slots;
	REBSER  *content;
	RXIARG   val;
	REBCNT   n;
	REBYTE  *at;

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiDrop);
	if (!hob) return NULL;

	// The slot block first, and attached to the handle BEFORE it is filled:
	// anything allocated after this point is reachable from the handle, so a
	// collection in the middle of filling it cannot take half of it away.
	slots = (REBSER*)RL_MAKE_BLOCK(2);
	if (!slots) return NULL;
	hob->series = slots;
	CLEARS(&val);
	RL_SET_VALUE(slots, 0, val, RXT_NONE);
	RL_SET_VALUE(slots, 1, val, RXT_NONE);

	drop = (GUIDROP*)hob->data;
	drop->hob   = hob;
	drop->kind  = data->kind;
	drop->count = data->count;

	if (data->kind == GUI_DROP_TEXT) {
		// One string, however many NULs the payload happens to hold.
		content = RL_DECODE_UTF_STRING(data->text, data->size ? data->size - 1 : 0,
		                               8, FALSE, FALSE);
		if (!content) return NULL;
		CLEARS(&val);
		val.series = content;
		val.index  = 0;
		RL_SET_VALUE(slots, 0, val, RXT_STRING);
	}
	else {
		// A block of file!, converted from the local path form. The block is
		// stored EMPTY first, for the same reason the slot block is attached
		// early: RL_TO_REBOL_PATH allocates, and the block has to be
		// reachable before it does.
		content = (REBSER*)RL_MAKE_BLOCK(data->count);
		if (!content) return NULL;
		CLEARS(&val);
		val.series = content;
		val.index  = 0;
		RL_SET_VALUE(slots, 0, val, RXT_BLOCK);

		at = data->text;
		for (n = 0; n < data->count; n++) {
			REBCNT len = (REBCNT)LEN_BYTES(at);
			REBSER *path = RL_TO_REBOL_PATH(at, len, 0);
			if (path) {
				CLEARS(&val);
				val.series = path;
				val.index  = 0;
				RL_SET_VALUE(content, n, val, RXT_FILE);
			}
			at += len + 1;
		}
	}

	// The target, so that a handler knows what it was dropped ON without
	// having to remember what it was hovering over.
	Set_Handle_Arg(&val, target);
	RL_SET_VALUE(slots, 1, val, RXT_HANDLE);

	return hob;
}


/***********************************************************************
**  The modifiers of a queued event, as the event!'s own flag bits.
**
**  GUI_FLAG_* is what the backends report; EVF_* is what `evt/flags`
**  reads back as a block of words. They are separate enums on purpose -
**  the backends must not have to know the core's bit numbering.
***********************************************************************/
static REBYTE Event_Modifier_Bits(REBINT value)
{
	REBYTE bits = 0;
	if (value & GUI_FLAG_SHIFT)   bits |= (1 << EVF_SHIFT);
	if (value & GUI_FLAG_CONTROL) bits |= (1 << EVF_CONTROL);
	if (value & GUI_FLAG_ALT)     bits |= (1 << EVF_ALT);
	if (value & GUI_FLAG_DOUBLE)  bits |= (1 << EVF_DOUBLE);
	return bits;
}


/***********************************************************************
**  poll-events
**
**  Dispatches everything the OS has waiting - which is what fills the
**  queue - and returns the collected events as a block of event!
**  values:
**
**      foreach evt poll-events [switch evt/type [...]]
**
**  Always returns a block, empty when nothing happened, so the caller
**  never has to test before iterating.
**
**  WHY event! rather than the flat four-value records this used to
**  return: the arity was fixed at the call site, so the day a GUI event
**  needed a fifth piece of information every `foreach [type source
**  position value]` in existence would have started reading the next
**  event's type as its own value - silently. An event! grows a field
**  instead. It also costs one REBVAL per event rather than four, and it
**  is what every other event source in Rebol already speaks, so one
**  handler can take a GUI event and a port event through one path.
**
**  The types are the core's own EVT_* codes, so a script switches on
**  the same words every other event source in Rebol reports - there is
**  no event vocabulary of this extension's own to learn.
**
**  What each field carries:
**
**      type     `click`, `change`, `scroll-line`, `menu-select`, ...
**      source   what produced it: a window, or the widget itself for
**               `click`, `change`, `focus` and `unfocus`
**      offset   client coordinates; the new client SIZE for `resize`
**      flags    shift / control / alt / double, where they apply
**      code     the wheel delta in lines, or a menu item's WORD
**
**  `offset` and `code` are the two readings of the event's one payload
**  word, so an event has one or the other and never both - which is why
**  a wheel event reports no position. The widget it happened to is in
**  `source`, which is the part anyone actually switches on.
***********************************************************************/
COMMAND cmd_gui_poll_events(RXIFRM *frm, void *ctx)
{
	REBSER *blk;
	REBCNT  count, n;
	RXIARG  val;
	REBEVT  ev;

	/*******************************************************************
	**  Pumps and drains, and never sleeps.
	**
	**  Sleeping is WAIT's job: the extension registers a device with
	**  RDO_AUTO_POLL, so the host pumps this same queue from inside
	**  OS_Wait and reports pending events back to it. That is what lets
	**  one sleep serve both queues - see Poll_Gui() in gui.c.
	*******************************************************************/
	Gui_Pump();

	if (Event_Dropped) {
		printf("GUI: dropped %u events (queue full)\n", Event_Dropped);
		Event_Dropped = 0;
	}

	count = QUEUE_COUNT();
	blk = (REBSER*)RL_MAKE_BLOCK(count);
	if (!blk) RETURN_ERROR(ERR_NO_HANDLE);

	// RL_Set_Value may expand the block, and an expansion can collect - so
	// the series is protected until it is safely stored in the frame.
	RL_PROTECT_GC(blk, 1);

	for (n = 0; n < count; n++) {
		GUIEVT *evt = QUEUE_AT(n);

		CLEARS(&ev);
		ev.type  = (u8)evt->type;

		// EVM_HANDLE is what lets the source be one of this extension's own
		// handles rather than a gob - and it is what the GC follows, so an
		// event still queued keeps its window or widget alive.
		ev.model = EVM_HANDLE;
		ev.hob   = evt->source;

		switch (evt->type) {
		case EVT_SCROLL_LINE:
			// The signed number of lines. There is no room for a position
			// as well - see the note above.
			ev.flags = (1 << EVF_HAS_CODE);
			ev.data  = (u32)evt->value;
			break;

		case EVT_MENU_SELECT:
			// The item's WORD, as a canon symbol id. EVF_HAS_SYM is what
			// makes `evt/code` read it back as a word instead of a number,
			// which is what keeps a menu handler a plain `switch`.
			ev.flags = (1 << EVF_HAS_SYM) | (1 << EVF_HAS_CODE);
			ev.data  = (u32)evt->value;
			break;

		case EVT_DROP_FILE:
		case EVT_DROP_TEXT: {
			// The source is a DROP handle rather than the target: it is what
			// carries the content, and it is made here because building it
			// allocates - see the note above Make_Drop_Handle.
			REBHOB *drop = Make_Drop_Handle(evt->source, evt->drop);
			if (drop) ev.hob = drop;
			ev.flags = (1 << EVF_HAS_XY);
			ev.data  = (((u32)(evt->y & 0xffff)) << 16) | ((u32)evt->x & 0xffff);
			Gui_Drop_Free(evt->drop);
			evt->drop = NULL;
			break;
		}

		default:
			// Everything else is positional: the pointer, or the new client
			// size for `resize`.
			ev.flags = (1 << EVF_HAS_XY) | Event_Modifier_Bits(evt->value);
			ev.data  = (((u32)(evt->y & 0xffff)) << 16) | ((u32)evt->x & 0xffff);
			break;
		}

		// An event fits whole into the value slot, so it crosses by value -
		// there is no series behind it. Copied rather than assigned through
		// the union member so that this does not depend on its spelling.
		CLEARS(&val);
		COPY_MEM(&val, &ev, sizeof(ev));
		RL_SET_VALUE(blk, n, val, RXT_EVENT);
	}

	RL_PROTECT_GC(blk, 0);
	Event_Tail += count;

	// The queue is empty again, so the next event may ring the doorbell.
	Event_Rung = FALSE;

	RXA_SERIES(frm, 1) = blk;
	RXA_INDEX(frm, 1) = 0;
	RXA_TYPE(frm, 1) = RXT_BLOCK;
	return RXR_VALUE;
}


/***********************************************************************
**  add-button / add-check / add-radio
**      window [handle!] text [string!] offset [pair!] size [pair!]
**      add-radio also takes /group id [integer!]
**
**  The widget handle is independent of the window handle: it can be kept,
**  dropped or released on its own, and it survives the window's death as
**  a closed widget rather than as a dangling pointer.
***********************************************************************/
static int Add_Button_Control(RXIFRM *frm, REBCNT kind)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBYTE    *text = NULL;
	REBCNT     text_len = 0;
	REBINT     x, y, w, h, req_w, req_h;
	int        len;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);

	len = RL_GET_UTF8_STRING(RXA_SERIES(frm, 2), RXA_INDEX(frm, 2), (void**)&text);
	if (len > 0) text_len = (REBCNT)len;

	x = (REBINT)RXA_PAIR(frm, 3).x;
	y = (REBINT)RXA_PAIR(frm, 3).y;
	w = (REBINT)RXA_PAIR(frm, 4).x;
	h = (REBINT)RXA_PAIR(frm, 4).y;
	if (w < 0 || h < 0) RETURN_ERROR(ERR_BAD_SIZE);

	// A zero axis asks for the natural size. It cannot be measured before
	// the control exists and has its font, so the control is created at a
	// placeholder and Attach_Widget() resizes it - which is also why the
	// REQUESTED size is what gets passed there.
	req_w = w; req_h = h;
	if (w <= 0) w = 1;
	if (h <= 0) h = 1;

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob   = hob;
	wid->kind  = kind;
	wid->owner  = win;
	wid->parent = panel; // read by the backend to pick the native parent

	// Only add-radio has this refinement, so only a radio may read it.
	if (kind == W_GUI_WIDGET_RADIO && RXA_REF(frm, 5))
		wid->group = (REBCNT)RXA_INT32(frm, 6);

	if (!Gui_Create_Button_Control(wid, win, x, y, w, h, text, text_len)) {
		wid->owner  = NULL;
		wid->parent = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}

	// Linked here rather than by the backend, so that the list has exactly
	// one owner and both platforms behave the same.
	Attach_Widget(wid, win, req_w, req_h);

	RETURN_HANDLE(hob);
}

COMMAND cmd_gui_add_button(RXIFRM *frm, void *ctx)
{
	return Add_Button_Control(frm, W_GUI_WIDGET_BUTTON);
}

COMMAND cmd_gui_add_check(RXIFRM *frm, void *ctx)
{
	return Add_Button_Control(frm, W_GUI_WIDGET_CHECK);
}

COMMAND cmd_gui_add_radio(RXIFRM *frm, void *ctx)
{
	return Add_Button_Control(frm, W_GUI_WIDGET_RADIO);
}


/***********************************************************************
**  add-image window [handle!] image [image!] offset [pair!] /size sz
**
**  The widget keeps a reference to the image, not a copy of it: drawing
**  into that same image and calling `redraw` is all it takes to change
**  what is on screen. Without /size the widget takes the image's own
**  size; with it, the image is scaled into the box.
***********************************************************************/
COMMAND cmd_gui_add_image(RXIFRM *frm, void *ctx)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBSER    *img = (REBSER*)RXA_IMAGE(frm, 2);
	REBINT     x, y, w, h;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);
	if (!img) RETURN_ERROR(ERR_BAD_IMAGE);

	x = (REBINT)RXA_PAIR(frm, 3).x;
	y = (REBINT)RXA_PAIR(frm, 3).y;

	if (RXA_REF(frm, 4)) { // /size
		w = (REBINT)RXA_PAIR(frm, 5).x;
		h = (REBINT)RXA_PAIR(frm, 5).y;
	} else {
		w = (REBINT)RXA_IMAGE_WIDTH(frm, 2);
		h = (REBINT)RXA_IMAGE_HEIGHT(frm, 2);
	}
	if (w <= 0 || h <= 0) RETURN_ERROR(ERR_BAD_SIZE);

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob   = hob;
	wid->kind  = W_GUI_WIDGET_IMAGE;
	wid->owner  = win;
	wid->parent = panel; // read by the backend to pick the native parent

	// The GC marks a handle context's series - which is exactly what keeps
	// the image alive for as long as a widget is showing it. The argument
	// is stored as it arrived, dimensions and all, rather than rebuilt
	// from the series: the index field an image! shares with them is the
	// trap this extension has been caught by before.
	if (!Hob_Set_Payload(hob, &RXA_ARG(frm, 2), RXT_IMAGE)) {
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_HANDLE);
	}

	if (!Gui_Create_Image(wid, win, x, y, w, h)) {
		wid->owner  = NULL;
		hob->series = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}

	Attach_Widget(wid, win, w, h);

	RETURN_HANDLE(hob);
}


/***********************************************************************
**  add-panel parent [handle!] offset [pair!] size [pair!]
**            /edge /title text [string!]
**
**  A panel is a widget like any other - it just happens to be something
**  other widgets can name as their parent.
**
**  /edge draws a frame around it and /title puts a caption in that
**  frame; a caption implies the frame, because a group box without one
**  is just floating text. Neither moves anything the panel holds: a
**  child is positioned from the panel's own top-left either way, so an
**  edge can be turned on later without relaying anything out.
***********************************************************************/
COMMAND cmd_gui_add_panel(RXIFRM *frm, void *ctx)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBYTE    *text = NULL;
	REBCNT     text_len = 0;
	REBINT     x, y, w, h;
	int        len;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);

	x = (REBINT)RXA_PAIR(frm, 2).x;
	y = (REBINT)RXA_PAIR(frm, 2).y;
	w = (REBINT)RXA_PAIR(frm, 3).x;
	h = (REBINT)RXA_PAIR(frm, 3).y;
	if (w <= 0 || h <= 0) RETURN_ERROR(ERR_BAD_SIZE);

	if (RXA_REF(frm, 5)) { // /title
		len = RL_GET_UTF8_STRING(RXA_SERIES(frm, 6), RXA_INDEX(frm, 6),
		                         (void**)&text);
		if (len > 0) text_len = (REBCNT)len;
	}

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob    = hob;
	wid->kind   = W_GUI_WIDGET_PANEL;
	wid->owner  = win;
	wid->parent = panel; // panels nest like anything else

	// /title implies /edge - the caption is drawn INTO the frame
	if (RXA_REF(frm, 4) || RXA_REF(frm, 5)) wid->state = GUI_PANEL_EDGE;

	if (!Gui_Create_Panel(wid, win, x, y, w, h, text, text_len)) {
		wid->owner  = NULL;
		wid->parent = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}

	Attach_Widget(wid, win, w, h);

	RETURN_HANDLE(hob);
}


COMMAND cmd_gui_remove_widget(RXIFRM *frm, void *ctx)
{
	GUIWIDGET *wid = Frm_Widget(frm, 1);

	if (!wid) RETURN_ERROR(ERR_INVALID_HANDLE);

	if (wid->handle) {
		// A panel takes its contents with it, so their handles are told
		// before the native control - and everything under it - is gone.
		if (Kind_Is_Container(wid->kind)) Close_Contents_Of(wid);
		Gui_Destroy_Widget(wid); // the native control
		Gui_Widget_Closed(wid);  // the Rebol side of it
	}
	return RXR_TRUE;
}


/***********************************************************************
**  The label and the two text entries only differ in which kind they
**  ask for, so the three commands below are one function with three
**  entry points - the arguments and the failure paths are identical.
**
**      add-<kind> window [handle!] text [string!] offset size [pair!]
***********************************************************************/
static int Add_Text_Control(RXIFRM *frm, REBCNT kind)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBYTE    *text = NULL;
	REBCNT     text_len = 0;
	REBINT     x, y, w, h, req_w, req_h;
	int        len;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);

	len = RL_GET_UTF8_STRING(RXA_SERIES(frm, 2), RXA_INDEX(frm, 2), (void**)&text);
	if (len > 0) text_len = (REBCNT)len;

	x = (REBINT)RXA_PAIR(frm, 3).x;
	y = (REBINT)RXA_PAIR(frm, 3).y;
	w = (REBINT)RXA_PAIR(frm, 4).x;
	h = (REBINT)RXA_PAIR(frm, 4).y;
	if (w < 0 || h < 0) RETURN_ERROR(ERR_BAD_SIZE);

	// A zero axis asks for the natural size - see Add_Button_Control.
	req_w = w; req_h = h;
	if (w <= 0) w = 1;
	if (h <= 0) h = 1;

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob   = hob;
	wid->kind  = kind; // read by the backend to pick the native control
	wid->owner  = win;
	wid->parent = panel; // read by the backend to pick the native parent

	if (!Gui_Create_Text_Control(wid, win, x, y, w, h, text, text_len)) {
		wid->owner  = NULL;
		wid->parent = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}

	Attach_Widget(wid, win, req_w, req_h);

	RETURN_HANDLE(hob);
}

/***********************************************************************
**  add-slider / add-progress
**      window [handle!] offset [pair!] size [pair!] /value val
**
**  Neither carries a label, so the arguments are one short of the rest.
***********************************************************************/
static int Add_Range_Control(RXIFRM *frm, REBCNT kind)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBINT     x, y, w, h;
	REBDEC     value = 0.0;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);

	x = (REBINT)RXA_PAIR(frm, 2).x;
	y = (REBINT)RXA_PAIR(frm, 2).y;
	w = (REBINT)RXA_PAIR(frm, 3).x;
	h = (REBINT)RXA_PAIR(frm, 3).y;
	if (w <= 0 || h <= 0) RETURN_ERROR(ERR_BAD_SIZE);

	if (RXA_REF(frm, 4)) { // /value
		value = RXA_DEC64(frm, 5);
		if (value < 0.0) value = 0.0;
		if (value > 1.0) value = 1.0;
	}

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob   = hob;
	wid->kind  = kind;
	wid->owner  = win;
	wid->parent = panel; // read by the backend to pick the native parent

	if (!Gui_Create_Range_Control(wid, win, x, y, w, h)) {
		wid->owner  = NULL;
		wid->parent = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}
	Gui_Widget_Set_Value(wid, value);

	Attach_Widget(wid, win, w, h);

	RETURN_HANDLE(hob);
}

/***********************************************************************
**  add-drop-down window items [block!] offset [pair!] size [pair!]
**                /index n [integer!]
***********************************************************************/
COMMAND cmd_gui_add_drop_down(RXIFRM *frm, void *ctx)
{
	REBHOB    *hob;
	GUIWIDGET *wid;
	GUIWIDGET *panel = NULL;
	GUIWIN    *win = Frm_Parent(frm, 1, &panel);
	REBINT     x, y, w, h, req_w, req_h;

	if (!win || !win->handle) RETURN_ERROR(ERR_INVALID_HANDLE);

	x = (REBINT)RXA_PAIR(frm, 3).x;
	y = (REBINT)RXA_PAIR(frm, 3).y;
	w = (REBINT)RXA_PAIR(frm, 4).x;
	h = (REBINT)RXA_PAIR(frm, 4).y;
	if (w < 0 || h < 0) RETURN_ERROR(ERR_BAD_SIZE);

	// A zero axis asks for the natural size - see Add_Button_Control.
	req_w = w; req_h = h;
	if (w <= 0) w = 1;
	if (h <= 0) h = 1;

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_GuiWidget);
	if (hob == NULL) RETURN_ERROR(ERR_NO_HANDLE);

	wid = (GUIWIDGET*)hob->data;
	CLEARS(wid); // every field defined, whatever the pool handed back
	wid->hob   = hob;
	wid->kind  = W_GUI_WIDGET_DROP_DOWN;
	wid->owner  = win;
	wid->parent = panel; // read by the backend to pick the native parent

	if (!Gui_Create_Drop_Down(wid, win, x, y, w, h)) {
		wid->owner  = NULL;
		wid->parent = NULL;
		RL_FREE_HANDLE_CONTEXT(hob);
		RETURN_ERROR(ERR_NO_WIDGET);
	}

	Block_To_Items(wid, RXA_SERIES(frm, 2));
	// Nothing is picked unless asked for - a drop-down which starts blank
	// is a normal thing to want.
	Gui_Widget_Set_Index(wid, RXA_REF(frm, 5) ? (REBINT)RXA_INT32(frm, 6) - 1 : -1);

	Attach_Widget(wid, win, req_w, req_h);

	RETURN_HANDLE(hob);
}


COMMAND cmd_gui_add_slider(RXIFRM *frm, void *ctx)
{
	return Add_Range_Control(frm, W_GUI_WIDGET_SLIDER);
}

COMMAND cmd_gui_add_progress(RXIFRM *frm, void *ctx)
{
	return Add_Range_Control(frm, W_GUI_WIDGET_PROGRESS);
}


COMMAND cmd_gui_add_text(RXIFRM *frm, void *ctx)
{
	return Add_Text_Control(frm, W_GUI_WIDGET_TEXT);
}

COMMAND cmd_gui_add_field(RXIFRM *frm, void *ctx)
{
	return Add_Text_Control(frm, W_GUI_WIDGET_FIELD);
}

COMMAND cmd_gui_add_area(RXIFRM *frm, void *ctx)
{
	return Add_Text_Control(frm, W_GUI_WIDGET_AREA);
}


/***********************************************************************
**  redraw target [handle!]
**
**  Takes either kind of handle, because "I changed the pixels, show them"
**  is the same request whether it is aimed at one widget or a window.
***********************************************************************/
COMMAND cmd_gui_redraw(RXIFRM *frm, void *ctx)
{
	GUIWIDGET *wid;
	GUIWIN    *win;

	if ((wid = Frm_Widget(frm, 1)) != NULL) {
		Gui_Widget_Redraw(wid);
		return RXR_TRUE;
	}
	if ((win = Frm_Window(frm, 1)) != NULL) {
		Gui_Window_Redraw(win);
		return RXR_TRUE;
	}
	RETURN_ERROR(ERR_INVALID_HANDLE);
}


//== handle callbacks =========================================================

int GuiWindow_free(void *hndl)
{
	REBHOB *hob;
	GUIWIN *win;

	if (!hndl) return 0;
	hob = (REBHOB*)hndl;
	win = (GUIWIN*)hob->data;
	if (!win) return 0;

	// Reached through an explicit `release`, or at shutdown. A window still
	// open at this point has to go; Gui_Window_Closed() then drops whatever
	// it had queued.
	if (win->handle) Gui_Close_Window(win);

	debug_print("releasing GUI window handle: %p\n", (void*)win);
	// The default font's family name and the menu's word table are plain
	// malloc'd memory owned by the window - the GC knows nothing about
	// either, so this is where they go. The menu BLOCK is in hob->series
	// and needs nothing: dropping the reference is enough.
	Font_Free(&win->font);
	Gui_Menu_Free(win);
	Menu_Free_Ids(win);
	hob->series = NULL;
	CLEARS(win);
	UNMARK_HOB(hob);
	return 0;
}


int GuiWindow_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
{
	GUIWIN *win = (GUIWIN*)hob->data;
	REBINT a, b;

	word = RL_FIND_WORD(Gui_arg_words, word);

	// A closed window still answers - with none, rather than an error.
	//
	// `word != 0` first: RL_FIND_WORD answers 0 for a word this extension
	// does not know, and PD_Handle only supplies `type` for a path this
	// callback REFUSED. Answering none here would take `closed/type` with
	// it, and a handle's type is true whether or not the window is gone.
	if (word != 0 && !win->handle
	    && word != W_GUI_ARG_OPENQ && word != W_GUI_ARG_ID) {
		*type = RXT_NONE;
		return PE_USE;
	}

	switch (word) {
	case W_GUI_ARG_TITLE: {
		REBSER *str = Gui_Get_Title(win);
		if (!str) { *type = RXT_NONE; break; }
		arg->series = str;
		arg->index  = 0;
		*type = RXT_STRING;
		break; }

	case W_GUI_ARG_SIZE:
		if (!Gui_Get_Size(win, &a, &b)) { *type = RXT_NONE; break; }
		arg->pair.x = (float)a;
		arg->pair.y = (float)b;
		*type = RXT_PAIR;
		break;

	case W_GUI_ARG_OFFSET:
		if (!Gui_Get_Offset(win, &a, &b)) { *type = RXT_NONE; break; }
		arg->pair.x = (float)a;
		arg->pair.y = (float)b;
		*type = RXT_PAIR;
		break;

	case W_GUI_ARG_ID:
		*type = RXT_INTEGER;
		arg->int64 = (i64)(REBUPT)win->handle;
		break;

	case W_GUI_ARG_OPENQ:
		*type = RXT_LOGIC;
		arg->int32a = (win->handle != NULL);
		break;

	// Sizes are logical units everywhere, so this is only needed to make
	// an IMAGE land on device pixels one for one - see the README.
	case W_GUI_ARG_SCALE:
		*type = RXT_DECIMAL;
		arg->dec64 = (double)Gui_Get_Scale(win);
		break;

	case W_GUI_ARG_RESIZABLEQ:
		*type = RXT_LOGIC;
		arg->int32a = Gui_Get_Resizable(win) ? 1 : 0;
		break;

	case W_GUI_ARG_BORDERQ:
		*type = RXT_LOGIC;
		arg->int32a = Gui_Get_Border(win) ? 1 : 0;
		break;

	// The client area's own colour, or none when the system's is used.
	// A see-through window has no colour to report either.
	case W_GUI_ARG_BACKGROUND:
		if (!GUI_COLOR_HAS(win->background)) { *type = RXT_NONE; break; }
		CLEARS(arg);
		arg->tuple_len      = 3;
		arg->tuple_bytes[0] = (REBYTE)GUI_COLOR_R(win->background);
		arg->tuple_bytes[1] = (REBYTE)GUI_COLOR_G(win->background);
		arg->tuple_bytes[2] = (REBYTE)GUI_COLOR_B(win->background);
		*type = RXT_TUPLE;
		break;

	case W_GUI_ARG_TRANSPARENTQ:
		*type = RXT_LOGIC;
		arg->int32a = GUI_BG_IS_CLEAR(win->background) ? 1 : 0;
		break;

	case W_GUI_ARG_DROPQ:
		*type = RXT_LOGIC;
		arg->int32a = (win->flags & GUIW_ACCEPTS_DROP) ? 1 : 0;
		break;

	/*******************************************************************
	**  What the NEXT widget will be created with - not a description of
	**  anything currently on screen. Unlike a widget's font, which is
	**  read back from the control, this is the extension's own note to
	**  itself, so it reports exactly what was set.
	*******************************************************************/
	case W_GUI_ARG_FONT:
		if (!win->font.name) { *type = RXT_NONE; break; }
		arg->series = RL_DECODE_UTF_STRING((REBYTE*)win->font.name,
			(REBCNT)strlen(win->font.name), 8, FALSE, FALSE);
		if (!arg->series) { *type = RXT_NONE; break; }
		arg->index = 0;
		*type = RXT_STRING;
		break;

	case W_GUI_ARG_FONT_SIZE:
		if (win->font.size <= 0) { *type = RXT_NONE; break; }
		*type = RXT_INTEGER;
		arg->int64 = (i64)win->font.size;
		break;

	case W_GUI_ARG_BOLDQ:
		*type = RXT_LOGIC;
		arg->int32a = ((win->font.style & GUI_FONT_BOLD) != 0);
		break;

	case W_GUI_ARG_ITALICQ:
		*type = RXT_LOGIC;
		arg->int32a = ((win->font.style & GUI_FONT_ITALIC) != 0);
		break;

	// The very block which was assigned, kept alive in hob->series.
	// Every widget the WINDOW holds directly - the ones inside a panel or
	// an image widget belong to that container's own list.
	case W_GUI_ARG_CHILDREN: {
		REBSER *kids = Hob_Children(hob, FALSE);
		// An empty block rather than none, so that a caller can always
		// `foreach` the answer without asking whether there is one. Not
		// stored: a container nobody put anything in keeps no slot.
		if (!kids) kids = (REBSER*)RL_MAKE_BLOCK(0);
		if (!kids) { *type = RXT_NONE; break; }
		arg->series = kids;
		arg->index  = 0;
		*type = RXT_BLOCK;
		break; }

	case W_GUI_ARG_MENU: {
		REBSER *menu = Hob_Payload(hob);
		if (!menu) { *type = RXT_NONE; break; }
		arg->series = menu;
		arg->index  = 0;
		*type = RXT_BLOCK;
		break; }

	/*******************************************************************
	**  Every item's word and whether it is selectable, as pairs. It
	**  reports ALL of them, while setting merges - so reading is a
	**  picture of the whole bar and writing is a change to part of it.
	*******************************************************************/
	case W_GUI_ARG_MENU_ENABLEDQ: {
		REBSER *blk;
		REBCNT  n, out = 0;
		RXIARG  val;

		blk = (REBSER*)RL_MAKE_BLOCK(win->menu_count * 2);
		if (!blk) { *type = RXT_NONE; break; }
		RL_PROTECT_GC(blk, 1);

		for (n = 0; n < win->menu_count; n++) {
			if (win->menu_ids[n] == 0) continue; // a label with no id
			CLEARS(&val);
			val.int32a = (i32)win->menu_ids[n];
			RL_SET_VALUE(blk, out++, val, RXT_WORD);
			CLEARS(&val);
			val.int32a = win->menu_on[n] ? 1 : 0;
			RL_SET_VALUE(blk, out++, val, RXT_LOGIC);
		}

		RL_PROTECT_GC(blk, 0);
		arg->series = blk;
		arg->index  = 0;
		*type = RXT_BLOCK;
		break; }

	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}


int GuiWindow_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
{
	GUIWIN *win = (GUIWIN*)hob->data;

	if (!win->handle) return PE_BAD_SET; // closed windows are read-only

	switch (RL_FIND_WORD(Gui_arg_words, word)) {
	case W_GUI_ARG_TITLE: {
		REBYTE *utf8 = NULL;
		int len;
		if (*type != RXT_STRING) return PE_BAD_SET_TYPE;
		len = RL_GET_UTF8_STRING((REBSER*)arg->series, arg->index, (void**)&utf8);
		if (len < 0) return PE_BAD_SET;
		Gui_Set_Title(win, utf8, (REBCNT)len);
		break; }

	case W_GUI_ARG_SIZE:
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		if (arg->pair.x <= 0 || arg->pair.y <= 0) return PE_BAD_RANGE;
		Gui_Set_Size(win, (REBINT)arg->pair.x, (REBINT)arg->pair.y);
		break;

	case W_GUI_ARG_OFFSET:
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		Gui_Set_Offset(win, (REBINT)arg->pair.x, (REBINT)arg->pair.y);
		break;

	/*******************************************************************
	**  Setting a default touches nothing that already exists - it is
	**  read once, by Attach_Widget, when the next widget is made.
	*******************************************************************/
	case W_GUI_ARG_FONT: {
		REBYTE *utf8 = NULL;
		int len = 0;
		if (*type == RXT_STRING) {
			len = RL_GET_UTF8_STRING((REBSER*)arg->series, arg->index,
			                         (void**)&utf8);
			if (len < 0) return PE_BAD_SET;
		} else if (*type != RXT_NONE) {
			return PE_BAD_SET_TYPE;
		}
		if (!Font_Set_Name(&win->font, utf8, (REBCNT)len)) return PE_BAD_SET;
		break; }

	case W_GUI_ARG_FONT_SIZE:
		if (*type == RXT_NONE) {
			win->font.size = 0;
		} else if (*type == RXT_INTEGER) {
			if (arg->int64 <= 0 || arg->int64 > 1000) return PE_BAD_RANGE;
			win->font.size = (REBINT)arg->int64;
		} else {
			return PE_BAD_SET_TYPE;
		}
		break;

	case W_GUI_ARG_BOLDQ:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		if (arg->int32a) win->font.style |=  GUI_FONT_BOLD;
		else             win->font.style &= ~(REBCNT)GUI_FONT_BOLD;
		break;

	case W_GUI_ARG_ITALICQ:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		if (arg->int32a) win->font.style |=  GUI_FONT_ITALIC;
		else             win->font.style &= ~(REBCNT)GUI_FONT_ITALIC;
		break;

	case W_GUI_ARG_RESIZABLEQ:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		Gui_Set_Resizable(win, arg->int32a ? TRUE : FALSE);
		break;

	// Taking the border off takes the title bar with it, so the window
	// stops reporting `close` and can only be moved by `offset`.
	case W_GUI_ARG_BORDERQ:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		Gui_Set_Border(win, arg->int32a ? TRUE : FALSE);
		break;

	// One field, three states, exactly as on a widget: giving a colour
	// turns see-through off, and `transparent?: false` goes back to the
	// system colour rather than to a colour set earlier.
	case W_GUI_ARG_BACKGROUND:
		if (*type == RXT_NONE) {
			win->background = 0;
		} else if (*type == RXT_TUPLE) {
			if (arg->tuple_len < 3) return PE_BAD_SET;
			win->background = GUI_COLOR_OF(arg->tuple_bytes[0],
			                               arg->tuple_bytes[1],
			                               arg->tuple_bytes[2]);
		} else {
			return PE_BAD_SET_TYPE;
		}
		Gui_Window_Set_Background(win);
		break;

	case W_GUI_ARG_TRANSPARENTQ:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		win->background = arg->int32a ? GUI_BG_CLEAR : 0;
		Gui_Window_Set_Background(win);
		break;

	case W_GUI_ARG_DROPQ:
		// Off by default: a window which silently swallows a drop is worse
		// than one which visibly refuses it, so accepting is asked for.
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		Gui_Window_Set_Drop(win, arg->int32a ? TRUE : FALSE);
		break;

	case W_GUI_ARG_MENU:
		// A menu is replaced whole, never edited in place: the block is
		// the description, and rebuilding from it is what keeps the two
		// from disagreeing. `none` takes the bar off.
		if (*type == RXT_NONE) {
			if (!Set_Menu(win, NULL, 0)) return PE_BAD_SET;
		} else if (*type == RXT_BLOCK) {
			if (!Set_Menu(win, (REBSER*)arg->series, arg->index))
				return PE_BAD_SET;
		} else {
			return PE_BAD_SET_TYPE;
		}
		break;

	/*******************************************************************
	**  Greying items out, as word/logic pairs. Only the words listed
	**  change - anything left out keeps whatever it had, so this is a
	**  small adjustment rather than a redeclaration of the menu.
	*******************************************************************/
	case W_GUI_ARG_MENU_ENABLEDQ: {
		REBSER *blk;
		REBCNT  n, t;
		RXIARG  item;
		REBCNT  word = 0;

		if (*type != RXT_BLOCK) return PE_BAD_SET_TYPE;
		blk = (REBSER*)arg->series;

		for (n = arg->index; (t = RL_GET_VALUE(blk, n, &item)) != 0; n++) {
			if (t == RXT_END) break;
			if (t == RXT_WORD) { word = (REBCNT)item.int32a; continue; }
			if (!word) continue;
			if (t == RXT_LOGIC || t == RXT_NONE) {
				REBOOL on = (t == RXT_LOGIC && item.int32a) ? TRUE : FALSE;
				REBCNT i;
				// Every item with that word, so one word used twice in a
				// menu turns both on or both off.
				for (i = 0; i < win->menu_count; i++) {
					if (win->menu_ids[i] != word) continue;
					win->menu_on[i] = on ? 1 : 0;
					Gui_Menu_Enable(win, i + 1, on);
				}
			}
			word = 0;
		}
		break; }

	default:
		return PE_BAD_SET;
	}
	return PE_OK;
}


int GuiWindow_mold(REBHOB *hob, REBSER *str)
{
	int len;
	GUIWIN *win;
	REBINT w = 0, h = 0;

	if (!str || !hob || !(win = (GUIWIN*)hob->data)) return 0;

	SERIES_TAIL(str) = 0;
	if (win->handle && Gui_Get_Size(win, &w, &h)) {
		APPEND_STRING(str, "0#%lx %dx%d", (unsigned long)(REBUPT)win->handle, w, h);
	} else {
		APPEND_STRING(str, "%s", "closed");
	}
	return len;
}


//== widget handle callbacks ==================================================

int GuiWidget_free(void *hndl)
{
	REBHOB    *hob;
	GUIWIDGET *wid;

	if (!hndl) return 0;
	hob = (REBHOB*)hndl;
	wid = (GUIWIDGET*)hob->data;
	if (!wid) return 0;

	if (wid->handle) {
		// `release` on a panel means the same as remove-widget on it.
		if (Kind_Is_Container(wid->kind)) Close_Contents_Of(wid);
		Gui_Destroy_Widget(wid);
		Gui_Widget_Closed(wid);
	}
	debug_print("releasing GUI widget handle: %p\n", (void*)wid);
	CLEARS(wid);
	UNMARK_HOB(hob);
	return 0;
}


int GuiWidget_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
{
	GUIWIDGET *wid = (GUIWIDGET*)hob->data;
	REBINT x, y, w, h;

	word = RL_FIND_WORD(Gui_arg_words, word);

	// A widget whose window has gone answers with none, like a closed
	// window does - except for `id`, which reports the null it now holds,
	// and for a word this extension does not know at all (word 0), which
	// has to be refused so that PD_Handle can still answer `type`.
	if (word != 0 && !wid->handle && word != W_GUI_ARG_ID) {
		*type = RXT_NONE;
		return PE_USE;
	}

	switch (word) {
	// Accessors which only make sense for one kind answer with none on the
	// others, rather than erroring - the same as a closed widget does.
	case W_GUI_ARG_TEXT: {
		REBSER *str;
		if (!Kind_Has_Text(wid->kind)) { *type = RXT_NONE; break; }
		str = Gui_Widget_Get_Text(wid);
		if (!str) { *type = RXT_NONE; break; }
		arg->series = str;
		arg->index  = 0;
		*type = RXT_STRING;
		break; }

	/*******************************************************************
	**  The image an image widget is showing - the very series it was
	**  given, not a copy, so drawing into it and calling `redraw` is
	**  how a widget is animated.
	**
	**  Filled in as an image and NOTHING else. RXIARG carries an image
	**  as {pointer, width:16, height:16} and a series as {pointer,
	**  index}, which puts the dimensions and the index on the same four
	**  bytes - so `index` here necessarily reads as (height<<16)|width,
	**  and the conversion behind a handle path must ignore it, exactly
	**  as it ignores it for an image passed the other way. An image!
	**  takes its size from its own series; there is nothing for an
	**  index to mean here.
	*******************************************************************/
	case W_GUI_ARG_IMAGE: {
		REBSER *img;
		if (wid->kind != W_GUI_WIDGET_IMAGE
		    || !(img = Hob_Payload(hob))) {
			*type = RXT_NONE;
			break;
		}
		CLEARS(arg);
		arg->image  = img;
		arg->width  = (int)IMG_WIDE(img);
		arg->height = (int)IMG_HIGH(img);
		*type = RXT_IMAGE;
		break; }

	case W_GUI_ARG_KIND:
		// The word list the kind was taken from is also how it is named.
		*type = RXT_WORD;
		arg->int32a = (i32)Gui_widget_words[wid->kind];
		break;

	case W_GUI_ARG_STATE:
		if (!Kind_Has_State(wid->kind)) { *type = RXT_NONE; break; }
		*type = RXT_LOGIC;
		// A checkbox toggles itself, so the control knows best; a radio's
		// truth is kept here, because AppKit interferes with the control.
		arg->int32a = (wid->kind == W_GUI_WIDGET_RADIO)
			? (wid->state != 0)
			: Gui_Widget_Get_State(wid);
		break;

	case W_GUI_ARG_EDGE:
		// Only a panel has a frame to draw, so only a panel answers.
		if (wid->kind != W_GUI_WIDGET_PANEL) { *type = RXT_NONE; break; }
		*type = RXT_LOGIC;
		arg->int32a = ((wid->state & GUI_PANEL_EDGE) != 0);
		break;

	/*******************************************************************
	**  Typography. All four come out of one question put to the
	**  control, so what is reported is what the control actually has -
	**  including a font this extension never set.
	*******************************************************************/
	case W_GUI_ARG_FONT:
	case W_GUI_ARG_FONT_SIZE:
	case W_GUI_ARG_BOLDQ:
	case W_GUI_ARG_ITALICQ: {
		REBSER *name = NULL;
		REBINT  size = 0;
		REBCNT  style = 0;

		if (!Kind_Has_Font(wid->kind)
		    || !Gui_Widget_Get_Font(wid, &name, &size, &style)) {
			*type = RXT_NONE;
			break;
		}
		switch (word) {
		case W_GUI_ARG_FONT:
			// None rather than an empty string: the control is using
			// whatever the platform hands out, which has no name here.
			if (!name) { *type = RXT_NONE; break; }
			arg->series = name;
			arg->index  = 0;
			*type = RXT_STRING;
			break;
		case W_GUI_ARG_FONT_SIZE:
			if (size <= 0) { *type = RXT_NONE; break; }
			*type = RXT_INTEGER;
			arg->int64 = (i64)size;
			break;
		case W_GUI_ARG_BOLDQ:
			*type = RXT_LOGIC;
			arg->int32a = ((style & GUI_FONT_BOLD) != 0);
			break;
		default: // W_GUI_ARG_ITALICQ
			*type = RXT_LOGIC;
			arg->int32a = ((style & GUI_FONT_ITALIC) != 0);
			break;
		}
		break; }

	case W_GUI_ARG_COLOR:
		// The colour is kept here rather than asked of the control, so a
		// Win32 push button - which ignores one - still reports what it
		// was given rather than pretending it was never asked.
		if (!Kind_Has_Font(wid->kind) || !GUI_COLOR_HAS(wid->color)) {
			*type = RXT_NONE;
			break;
		}
		CLEARS(arg);
		arg->tuple_len      = 3;
		arg->tuple_bytes[0] = (REBYTE)GUI_COLOR_R(wid->color);
		arg->tuple_bytes[1] = (REBYTE)GUI_COLOR_G(wid->color);
		arg->tuple_bytes[2] = (REBYTE)GUI_COLOR_B(wid->color);
		*type = RXT_TUPLE;
		break;

	case W_GUI_ARG_BACKGROUND:
		// `none` covers both "the platform's own" and "nothing at all" -
		// the second is what `transparent?` is for, and a transparent
		// widget has no colour to report.
		if (!Kind_Has_Font(wid->kind) || !GUI_COLOR_HAS(wid->background)) {
			*type = RXT_NONE;
			break;
		}
		CLEARS(arg);
		arg->tuple_len      = 3;
		arg->tuple_bytes[0] = (REBYTE)GUI_COLOR_R(wid->background);
		arg->tuple_bytes[1] = (REBYTE)GUI_COLOR_G(wid->background);
		arg->tuple_bytes[2] = (REBYTE)GUI_COLOR_B(wid->background);
		*type = RXT_TUPLE;
		break;

	case W_GUI_ARG_TRANSPARENTQ:
		if (!Kind_Has_Font(wid->kind)) { *type = RXT_NONE; break; }
		arg->int32a = GUI_BG_IS_CLEAR(wid->background) ? 1 : 0;
		*type = RXT_LOGIC;
		break;

	case W_GUI_ARG_GROUP:
		*type = RXT_INTEGER;
		arg->int64 = (i64)wid->group;
		break;

	case W_GUI_ARG_ITEMS: {
		REBSER *blk;
		if (wid->kind != W_GUI_WIDGET_DROP_DOWN) { *type = RXT_NONE; break; }
		blk = Items_To_Block(wid);
		if (!blk) { *type = RXT_NONE; break; }
		arg->series = blk;
		arg->index  = 0;
		*type = RXT_BLOCK;
		break; }

	case W_GUI_ARG_INDEX: {
		REBINT n;
		if (wid->kind != W_GUI_WIDGET_DROP_DOWN) { *type = RXT_NONE; break; }
		n = Gui_Widget_Get_Index(wid);
		*type = RXT_INTEGER;
		// Rebol counts from one, and zero means nothing is picked.
		arg->int64 = (i64)(n < 0 ? 0 : n + 1);
		break; }

	case W_GUI_ARG_VALUE:
		if (!Kind_Has_Value(wid->kind)) { *type = RXT_NONE; break; }
		// Reported as a percent!, which is what a fraction of a range
		// reads as in Rebol - `50%` rather than `0.5`.
		*type = RXT_PERCENT;
		arg->dec64 = (double)Gui_Widget_Get_Value(wid);
		break;

	// A fraction of the way down, as a percent! - the same way a slider
	// reports its position, because it is the same kind of answer.
	case W_GUI_ARG_SCROLL: {
		REBDEC at;
		if (!Kind_Scrolls(wid->kind)) { *type = RXT_NONE; break; }
		at = Gui_Widget_Get_Scroll(wid);
		if (at < 0.0) { *type = RXT_NONE; break; }
		*type = RXT_PERCENT;
		arg->dec64 = (double)at;
		break; }

	case W_GUI_ARG_SIZE:
		if (!Gui_Widget_Get_Box(wid, &x, &y, &w, &h)) { *type = RXT_NONE; break; }
		arg->pair.x = (float)w;
		arg->pair.y = (float)h;
		*type = RXT_PAIR;
		break;

	case W_GUI_ARG_OFFSET:
		if (!Gui_Widget_Get_Box(wid, &x, &y, &w, &h)) { *type = RXT_NONE; break; }
		arg->pair.x = (float)x;
		arg->pair.y = (float)y;
		*type = RXT_PAIR;
		break;

	case W_GUI_ARG_ID:
		*type = RXT_INTEGER;
		arg->int64 = (i64)(REBUPT)wid->handle;
		break;

	// Read from the widget rather than the control: see the note in gui.h
	// on why the flag is kept here.
	case W_GUI_ARG_READ_ONLYQ:
		if (!Kind_Has_Read_Only(wid->kind)) { *type = RXT_NONE; break; }
		*type = RXT_LOGIC;
		arg->int32a = (wid->state & GUI_TEXT_READ_ONLY) ? 1 : 0;
		break;

	case W_GUI_ARG_ENABLEDQ:
		if (!Kind_Has_Enabled(wid->kind)) { *type = RXT_NONE; break; }
		*type = RXT_LOGIC;
		arg->int32a = Gui_Widget_Get_Enabled(wid);
		break;

	// Whatever holds it: a panel if it is in one, otherwise the window.
	// Only a container has any; everything else answers none, which is
	// how a caller can tell the two apart without a list of kinds.
	case W_GUI_ARG_CHILDREN: {
		REBSER *kids;
		if (!Kind_Is_Container(wid->kind)) { *type = RXT_NONE; break; }
		kids = Hob_Children(hob, FALSE);
		if (!kids) kids = (REBSER*)RL_MAKE_BLOCK(0);
		if (!kids) { *type = RXT_NONE; break; }
		arg->series = kids;
		arg->index  = 0;
		*type = RXT_BLOCK;
		break; }

	case W_GUI_ARG_PARENT:
		if (wid->parent && ((GUIWIDGET*)wid->parent)->hob) {
			Set_Handle_Arg(arg, ((GUIWIDGET*)wid->parent)->hob);
			*type = RXT_HANDLE;
			break;
		}
		// fall through - no panel means the window holds it
	case W_GUI_ARG_WINDOW:
		if (!wid->owner || !wid->owner->hob) { *type = RXT_NONE; break; }
		Set_Handle_Arg(arg, wid->owner->hob);
		*type = RXT_HANDLE;
		break;

	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}


int GuiWidget_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
{
	GUIWIDGET *wid = (GUIWIDGET*)hob->data;
	REBINT x, y, w, h;

	if (!wid->handle) return PE_BAD_SET;

	switch (RL_FIND_WORD(Gui_arg_words, word)) {
	case W_GUI_ARG_ITEMS:
		if (wid->kind != W_GUI_WIDGET_DROP_DOWN) return PE_BAD_SET;
		if (*type != RXT_BLOCK) return PE_BAD_SET_TYPE;
		Block_To_Items(wid, (REBSER*)arg->series);
		break;

	case W_GUI_ARG_INDEX:
		if (wid->kind != W_GUI_WIDGET_DROP_DOWN) return PE_BAD_SET;
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		// Out of range - zero included - simply picks nothing.
		Gui_Widget_Set_Index(wid, (REBINT)arg->int64 - 1);
		break;

	case W_GUI_ARG_TEXT: {
		REBYTE *utf8 = NULL;
		int len;
		if (!Kind_Has_Text(wid->kind)) return PE_BAD_SET;
		// A drop-down shows whichever item is picked; `index` chooses it.
		if (wid->kind == W_GUI_WIDGET_DROP_DOWN) return PE_BAD_SET;
		if (*type != RXT_STRING) return PE_BAD_SET_TYPE;
		len = RL_GET_UTF8_STRING((REBSER*)arg->series, arg->index, (void**)&utf8);
		if (len < 0) return PE_BAD_SET;
		Gui_Widget_Set_Text(wid, utf8, (REBCNT)len);
		// A native control repaints itself when its text changes; a panel's
		// caption is drawn by the backend's own paint handler, so it has to
		// be asked.
		if (wid->kind == W_GUI_WIDGET_PANEL) Gui_Panel_Edge_Changed(wid);
		break; }

	// Swapping the image is just swapping the reference the GC marks; the
	// widget keeps its box and the new image is scaled into it.
	case W_GUI_ARG_IMAGE:
		if (wid->kind != W_GUI_WIDGET_IMAGE) return PE_BAD_SET;
		if (*type != RXT_IMAGE) return PE_BAD_SET_TYPE;
		if (!arg->image) return PE_BAD_SET;
		if (!Hob_Set_Payload(hob, arg, RXT_IMAGE)) return PE_BAD_SET;
		// Invalidated, not painted - `redraw` is the one thing that still
		// promises pixels on screen before it returns, and everything else
		// waits for the pump.
		Gui_Widget_Invalidate(wid);
		break;

	// Both halves of the box are read back first, so that setting one does
	// not disturb the other.
	/*******************************************************************
	**  A ZERO AXIS ASKS THE WIDGET, exactly as at creation - which is
	**  the whole of the re-fit interface:
	**
	**      lbl/size: 220x0   ;; keep the width, measure the height
	**      lbl/size: 0x0     ;; measure both
	**
	**  Nothing re-measures on its own, so this is what a script calls
	**  after changing a font or a label, when it wants the box to follow
	**  and has decided there is room for it.
	*******************************************************************/
	case W_GUI_ARG_SIZE: {
		REBOOL fit_w, fit_h;
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		if (arg->pair.x < 0 || arg->pair.y < 0) return PE_BAD_RANGE;
		if (!Gui_Widget_Get_Box(wid, &x, &y, &w, &h)) return PE_BAD_SET;

		fit_w = arg->pair.x == 0 ? TRUE : FALSE;
		fit_h = arg->pair.y == 0 ? TRUE : FALSE;

		// The given axes first, so that measuring the other one happens
		// against the box the caller is asking for.
		if (!fit_w) w = (REBINT)arg->pair.x;
		if (!fit_h) h = (REBINT)arg->pair.y;
		Gui_Widget_Set_Box(wid, x, y, w, h);

		// An image or a panel has no size of its own to report, so asking
		// one is refused rather than quietly ignored.
		if (!Fit_To_Content(wid, fit_w, fit_h)) return PE_BAD_SET;
		break; }

	case W_GUI_ARG_OFFSET:
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		if (!Gui_Widget_Get_Box(wid, &x, &y, &w, &h)) return PE_BAD_SET;
		Gui_Widget_Set_Box(wid, (REBINT)arg->pair.x, (REBINT)arg->pair.y, w, h);
		break;

	case W_GUI_ARG_STATE:
		if (!Kind_Has_State(wid->kind)) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		if (wid->kind == W_GUI_WIDGET_RADIO) {
			// Setting one from Rebol settles the group exactly as a click
			// does - including turning the others off.
			Sync_Radio_Group(wid, arg->int32a ? TRUE : FALSE);
		} else {
			wid->state = arg->int32a ? 1 : 0;
			Gui_Widget_Set_State(wid, wid->state ? TRUE : FALSE);
		}
		break;

	/*******************************************************************
	**  Typography. Each of the four writes one part of a font which is
	**  otherwise carried over from the control - see Set_Font_Part.
	*******************************************************************/
	case W_GUI_ARG_FONT: {
		REBYTE *utf8 = NULL;
		int     len = 0;
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		// `none` means "the platform's own font", which is what a NULL
		// name asks a backend for.
		if (*type == RXT_STRING) {
			len = RL_GET_UTF8_STRING((REBSER*)arg->series, arg->index,
			                         (void**)&utf8);
			if (len < 0) return PE_BAD_SET;
		} else if (*type != RXT_NONE) {
			return PE_BAD_SET_TYPE;
		}
		if (!Set_Font_Part(wid, FONT_PART_NAME, utf8, (REBCNT)len, 0, 0, 0))
			return PE_BAD_SET;
		break; }

	case W_GUI_ARG_FONT_SIZE: {
		REBINT size;
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		if (*type == RXT_NONE) {
			size = 0; // back to the platform's own size
		} else if (*type == RXT_INTEGER) {
			size = (REBINT)arg->int64;
			if (size <= 0 || size > 1000) return PE_BAD_RANGE;
		} else {
			return PE_BAD_SET_TYPE;
		}
		if (!Set_Font_Part(wid, FONT_PART_SIZE, NULL, 0, size, 0, 0))
			return PE_BAD_SET;
		break; }

	case W_GUI_ARG_BOLDQ:
	case W_GUI_ARG_ITALICQ: {
		REBCNT mask = (word == W_GUI_ARG_BOLDQ)
			? GUI_FONT_BOLD : GUI_FONT_ITALIC;
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		if (!Set_Font_Part(wid, FONT_PART_STYLE, NULL, 0, 0,
		                   arg->int32a ? mask : 0, mask))
			return PE_BAD_SET;
		break; }

	case W_GUI_ARG_COLOR:
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		if (*type == RXT_NONE) {
			wid->color = 0; // the platform decides again
		} else if (*type == RXT_TUPLE) {
			if (arg->tuple_len < 3) return PE_BAD_SET;
			// A fourth byte would be alpha, which no control here blends.
			wid->color = GUI_COLOR_OF(arg->tuple_bytes[0],
			                          arg->tuple_bytes[1],
			                          arg->tuple_bytes[2]);
		} else {
			return PE_BAD_SET_TYPE;
		}
		// A backend which cannot colour this particular control says so,
		// and the value stands anyway - the accessor reports what was
		// asked for, and the one platform gap is documented rather than
		// turned into an error at an arbitrary moment.
		Gui_Widget_Set_Color(wid);
		break;

	case W_GUI_ARG_BACKGROUND:
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		if (*type == RXT_NONE) {
			wid->background = 0; // the platform decides again
		} else if (*type == RXT_TUPLE) {
			if (arg->tuple_len < 3) return PE_BAD_SET;
			// Giving a colour is also how transparency is turned off: a
			// widget cannot both fill with something and show through.
			wid->background = GUI_COLOR_OF(arg->tuple_bytes[0],
			                               arg->tuple_bytes[1],
			                               arg->tuple_bytes[2]);
		} else {
			return PE_BAD_SET_TYPE;
		}
		Gui_Widget_Set_Background(wid);
		break;

	case W_GUI_ARG_TRANSPARENTQ:
		if (!Kind_Has_Font(wid->kind)) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		// Turning it off goes back to the platform's own background rather
		// than to a colour set earlier: one field holds both, because the
		// two are answers to the same question.
		wid->background = arg->int32a ? GUI_BG_CLEAR : 0;
		Gui_Widget_Set_Background(wid);
		break;

	case W_GUI_ARG_EDGE:
		// The backends read this flag at paint time, so turning a frame on
		// or off is a repaint and never a rebuild of the control - which is
		// also why nothing the panel holds moves.
		if (wid->kind != W_GUI_WIDGET_PANEL) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		if (arg->int32a) wid->state |=  GUI_PANEL_EDGE;
		else             wid->state &= ~(REBCNT)GUI_PANEL_EDGE;
		Gui_Panel_Edge_Changed(wid);
		break;

	case W_GUI_ARG_VALUE: {
		REBDEC value;
		if (!Kind_Has_Value(wid->kind)) return PE_BAD_SET;
		// A percent! is a decimal! underneath, so both arrive the same way.
		if (*type != RXT_PERCENT && *type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		value = (REBDEC)arg->dec64;
		if (value < 0.0) value = 0.0;
		if (value > 1.0) value = 1.0;
		Gui_Widget_Set_Value(wid, value);
		break; }

	/*******************************************************************
	**  A percent, or one of the words - which is the whole reason the
	**  two forms are both here. `100%` is what a computed position
	**  looks like; `'bottom` is what the call site usually means, and
	**  saying so beats a magic number that has to be recognised.
	*******************************************************************/
	case W_GUI_ARG_SCROLL: {
		REBDEC where;

		if (!Kind_Scrolls(wid->kind)) return PE_BAD_SET;

		if (*type == RXT_WORD) {
			switch (RL_FIND_WORD(Gui_scroll_words, (REBCNT)arg->int32a)) {
			case W_GUI_SCROLL_TOP:    where = 0.0; break;
			// `end` is `bottom` under the name that reads better after
			// appending to something.
			case W_GUI_SCROLL_BOTTOM:
			case W_GUI_SCROLL_END:    where = 1.0; break;
			default: return PE_BAD_SET;
			}
		} else if (*type == RXT_PERCENT || *type == RXT_DECIMAL) {
			where = (REBDEC)arg->dec64;
			if (where < 0.0) where = 0.0;
			if (where > 1.0) where = 1.0;
		} else {
			return PE_BAD_SET_TYPE;
		}

		Gui_Widget_Set_Scroll(wid, where);
		break; }

	case W_GUI_ARG_ENABLEDQ:
		if (!Kind_Has_Enabled(wid->kind)) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		Gui_Widget_Set_Enabled(wid, arg->int32a ? TRUE : FALSE);
		break;

	case W_GUI_ARG_READ_ONLYQ:
		if (!Kind_Has_Read_Only(wid->kind)) return PE_BAD_SET;
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		// Recorded before the backend is told, because that is where it
		// reads the answer from when it combines this with `enabled?`.
		if (arg->int32a) wid->state |=  GUI_TEXT_READ_ONLY;
		else             wid->state &= ~(REBCNT)GUI_TEXT_READ_ONLY;
		Gui_Widget_Set_Read_Only(wid, arg->int32a ? TRUE : FALSE);
		break;

	default:
		return PE_BAD_SET;
	}
	return PE_OK;
}


int GuiWidget_mold(REBHOB *hob, REBSER *str)
{
	int len;
	GUIWIDGET *wid;

	if (!str || !hob || !(wid = (GUIWIDGET*)hob->data)) return 0;

	SERIES_TAIL(str) = 0;
	if (wid->handle) {
		APPEND_STRING(str, "0#%lx %s", (unsigned long)(REBUPT)wid->handle,
			Kind_Name(wid->kind));
	} else {
		APPEND_STRING(str, "%s", "removed");
	}
	return len;
}


//== drop handle ==============================================================
//
// What a `drop-file` or a `drop-text` event carries as its source. It owns
// nothing outside hob->series, and nothing about it can be set - a drop is
// something that happened, not something to configure - so there is no
// set_path and the free callback has nothing to release.

int GuiDrop_free(void *hndl)
{
	REBHOB  *hob  = (REBHOB*)hndl;
	GUIDROP *drop = hob ? (GUIDROP*)hob->data : NULL;

	// The content and the target are Rebol values in hob->series, which the
	// collector handles on its own. Clearing is only tidiness.
	if (drop) CLEARS(drop);
	if (hob) UNMARK_HOB(hob);
	return 0;
}


int GuiDrop_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg)
{
	GUIDROP *drop = (GUIDROP*)hob->data;
	REBSER  *slots = hob->series;

	if (!drop || !slots) return PE_BAD_SELECT;

	switch (RL_FIND_WORD(Gui_arg_words, word)) {

	case W_GUI_ARG_KIND:
		*type = RXT_WORD;
		arg->int32a = (i32)Gui_drop_words[drop->kind];
		break;

	case W_GUI_ARG_DATA:
		// Whatever was put in slot 0 - a block of file!, or a string.
		*type = RL_GET_VALUE(slots, SLOT_PAYLOAD, arg);
		break;

	case W_GUI_ARG_COUNT:
		*type = RXT_INTEGER;
		arg->int64 = (i64)drop->count;
		break;

	case W_GUI_ARG_TARGET:
		*type = RL_GET_VALUE(slots, SLOT_CHILDREN, arg);
		break;

	case W_GUI_ARG_WINDOW: {
		// The target's own window, asked of the target - a drop on a widget
		// reports the widget, and this is how to get from it to the window.
		RXIARG   val;
		REBCNT   t = RL_GET_VALUE(slots, SLOT_CHILDREN, &val);
		REBHOB  *target;

		if (t != RXT_HANDLE || !(target = val.handle.hob)) { *type = RXT_NONE; break; }
		if (target->sym == Handle_GuiWindow) { *arg = val; *type = RXT_HANDLE; break; }
		if (target->sym == Handle_GuiWidget) {
			GUIWIDGET *wid = (GUIWIDGET*)target->data;
			if (wid && wid->owner && wid->owner->hob) {
				Set_Handle_Arg(arg, wid->owner->hob);
				*type = RXT_HANDLE;
				break;
			}
		}
		*type = RXT_NONE;
		break;
	}

	default:
		return PE_BAD_SELECT;
	}
	return PE_USE;
}


int GuiDrop_mold(REBHOB *hob, REBSER *str)
{
	int len;
	GUIDROP *drop;

	if (!str || !hob || !(drop = (GUIDROP*)hob->data)) return 0;

	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "%s %u",
		(drop->kind == GUI_DROP_TEXT) ? "text" : "files", drop->count);
	return len;
}
