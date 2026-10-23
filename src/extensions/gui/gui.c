//
// Project: Rebol/GUI extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Entry points for the GUI extension.
//
//   REB_EXT defined ... standalone gui-x64.rebx
//   REB_EXT absent .... compiled into the host
//
// One-time setup lives in Gui_Init(), called from the generated `_init`
// command when the module body evaluates - not from the entry points, so
// that `Options: [delay]` can postpone it until the module is imported.
//

#include "gen-gui.h"
#include "gui.h"

#ifdef REB_EXT
// Standalone builds are their own binary and must supply the storage.
// Embedded builds use host-lib.c's definition, declared extern by reb-lib.h.
RL_LIB *RL;
#endif

static char *init_block = GUI_EXT_INIT_CODE;

// Symbols of the registered handle types. Declared in the generated header
// (from the spec's `c-header:`), defined here.
REBCNT Handle_GuiWindow = 0;
REBCNT Handle_GuiWidget = 0;
REBCNT Handle_GuiDrop   = 0;


/***********************************************************************
**  The GUI device.
**
**  Two jobs, and they are worth keeping apart.
**
**  1. RDO_AUTO_POLL makes the host call Poll_Gui() from OS_Wait, even
**     with nothing pending - so the OS message queue is pumped while the
**     interpreter is sitting in WAIT. That is what lets `wait` do the
**     sleeping. Before the device existed, a GUI loop had to sleep
**     inside the extension, and for as long as a window was open nothing
**     serviced Rebol's own queue: ports, timers and awake handlers were
**     all starved. Now one sleep covers both, because the host owns it.
**
**  2. When that pump puts something in the extension's own event queue,
**     the poll PUSHES a Rebol event with RL_Event. The signal RL_Event
**     sets is what wakes WAIT, so a click is delivered at once instead
**     of at the end of the timeout. A device cannot ask to be woken by
**     its return code - see the note on Poll_Gui.
**
**  The pushed event needs a port to be delivered to, so the device
**  serves one: the `gui` scheme in the mezzanine, opened once when the
**  module is imported. Nothing travels through that port - GUI events
**  stay in the extension's queue and come out of `poll-events`. It is a
**  doorbell, and RDC_READ answers how many events are waiting behind it.
***********************************************************************/

int    Gui_Dev_Id     = 0; // 0 = not registered (a real id is >= RDI_MAX)
REBCNT Gui_Dev_Polls  = 0; // polls,               read by `gui-device-polls`
REBCNT Gui_Dev_Events = 0; // events pushed,       read by `gui-device-events`

// Polls which got as far as pumping, and OS messages that pump dispatched.
// The pair is diagnostic and exists because the failure it catches is
// invisible from Rebol: the OS queue belongs to the thread, so anything
// else in the process which removes from it takes this window's messages
// away before the pump can see them. Polls climbing while messages stay
// at zero, with the mouse over a window, means exactly that.
REBCNT Gui_Dev_Pumps  = 0; // read by `gui-device-pumps`
REBCNT Gui_Dev_Msgs   = 0; // read by `gui-device-messages`

// The one port this device serves. A second `open` replaces it, which is
// the same single-unit arrangement the test device uses - there is one
// window system and one queue, so there is nothing for a second port to
// report on.
static REBREQ *Gui_Dev_Port = 0;

static DEVICE_CMD Init_Gui(REBREQ *dr) {
	REBDEV *dev = (REBDEV*)dr;
	SET_FLAG(dev->flags, RDF_INIT);
	return DR_DONE;
}

static DEVICE_CMD Quit_Gui(REBREQ *dr) {
	Gui_Dev_Port = 0;
	return DR_DONE;
}

static DEVICE_CMD Open_Gui(REBREQ *req) {
	Gui_Dev_Port = req;
	SET_OPEN(req);
	return DR_DONE;
}

static DEVICE_CMD Close_Gui(REBREQ *req) {
	if (Gui_Dev_Port == req) Gui_Dev_Port = 0;
	SET_CLOSED(req);
	return DR_DONE;
}

// How many events `poll-events` would hand back right now. Reporting it
// through the device rather than from a command is deliberate: it proves
// the request reached this command table instead of being a no-op.
static DEVICE_CMD Read_Gui(REBREQ *req) {
	if (!IS_OPEN(req)) { req->error = RDE_NO_INIT; return DR_ERROR; }
	req->actual = Gui_Event_Count();
	return DR_DONE;
}

/***********************************************************************
**  Rings the doorbell: one Rebol event, so that WAIT returns now.
**
**  The event carries the port SERIES (EVM_PORT), not the request. That
**  is what Get_Event_Var resolves for event/port and what Mark_Event
**  marks, so the port cannot be collected while the event is queued.
**
**  EVT_PENDING is what this is: "there is something waiting", which is
**  the whole of what the doorbell says. It is not an EVT_READ - nothing
**  was read, and the events themselves never travel through this port.
**
**  RL_Event appends. RL_Update_Event, which looks the more economical of
**  the two, addresses the event it replaces by model and type ALONE - so
**  it would silently overwrite an unhandled event belonging to some
**  other port. Flooding is avoided by Gui_Ring_Doorbell() instead, which
**  says yes once per batch of queued events and not again until
**  `poll-events` has drained them.
***********************************************************************/
static void Signal_Gui(void) {
	REBEVT evt;

	// No port means nobody asked to be woken. A program driving
	// `poll-events` from a loop of its own is a perfectly good way to use
	// this extension and needs no event at all.
	if (!Gui_Dev_Port || !Gui_Dev_Port->port) return;

	CLEARS(&evt);
	evt.type  = EVT_PENDING;
	evt.model = EVM_PORT;
	evt.port  = (REBSER*)Gui_Dev_Port->port;

	RL_EVENT(&evt);
	Gui_Dev_Events++;
}

/***********************************************************************
**  ALWAYS DR_DONE. An RDC_POLL handler has no other valid answer, and
**  this is why the event above is pushed rather than reported.
**
**  The request this is handed is not a request at all: OS_Poll_Devices
**  declares one REBREQ on its own C stack, sets only `device` on it and
**  lends it to every auto-polled device in turn. OS_Do_Device then does
**
**      if (result > 0) Attach_Request(&dev->pending, req);
**
**  so DR_PEND (which is 1 - DR_DONE is the zero) links that stack
**  temporary into Dev_Gui.pending and leaves it there after the frame is
**  gone. Every later walk of the pending list reads whatever has since
**  been written over that stack, and the damage is not confined to this
**  device: the walk is the host's, and the event device is on the same
**  list. Reporting DR_PEND from a poll is how an extension stops WAIT
**  from working at all - not how it asks to be woken sooner. It also
**  makes OS_Wait answer -1 on every poll, so WAIT spins.
***********************************************************************/
static DEVICE_CMD Poll_Gui(REBREQ *dr) {
	Gui_Dev_Polls++;

	// Nothing to pump with no window open: an extension which was
	// imported and never used must not wake the window system on every
	// WAIT the program makes.
	if (!Gui_Windows_Open()) return DR_DONE;

	Gui_Dev_Pumps++;
	Gui_Dev_Msgs += Gui_Pump();

	if (Gui_Ring_Doorbell()) Signal_Gui();

	return DR_DONE;
}

static DEVICE_CMD_FUNC Gui_Dev_Cmds[RDC_MAX] = {
	Init_Gui,
	Quit_Gui,
	Open_Gui,
	Close_Gui,
	Read_Gui,
	0,       // RDC_WRITE
	Poll_Gui,
	0,       // RDC_CONNECT
	0,       // RDC_QUERY
};

DEFINE_DEV(Dev_Gui, "Rebol GUI device", 1, Gui_Dev_Cmds, RDC_MAX, 0);


// Registers both handle types and does whatever the platform needs before
// the first window exists (DPI awareness on Windows, NSApplication on
// macOS). Runs when the module body evaluates, so it happens at the same
// point in both build modes - and only on first import under `delay`.
//
// Returns plain TRUE/FALSE, NOT an RXR_* code: RXR_FALSE is 3, which is
// truthy in C. The generated handler maps the result onto RXR_TRUE/RXR_FALSE.
int Gui_Init(void) {
	REBHSP spec;

	// Both callbacks take the HOB, not the raw data pointer - they need
	// hob->flags to drop the lock a live native object holds on its handle.
	spec.size     = sizeof(GUIWIN);
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = GuiWindow_free;
	spec.get_path = GuiWindow_get_path;
	spec.set_path = GuiWindow_set_path;
	spec.mold     = GuiWindow_mold;

	Handle_GuiWindow = RL_REGISTER_HANDLE_SPEC(cb_cast("GUI-WINDOW"), &spec);
	if (Handle_GuiWindow == 0) return FALSE;

	spec.size     = sizeof(GUIWIDGET);
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = GuiWidget_free;
	spec.get_path = GuiWidget_get_path;
	spec.set_path = GuiWidget_set_path;
	spec.mold     = GuiWidget_mold;

	Handle_GuiWidget = RL_REGISTER_HANDLE_SPEC(cb_cast("GUI-WIDGET"), &spec);
	if (Handle_GuiWidget == 0) return FALSE;

	// A drop is read-only - it describes something which already happened -
	// so it has no set_path, and it needs no lock of its own: nothing on the
	// C side keeps a pointer to it once `poll-events` has handed it over.
	spec.size     = sizeof(GUIDROP);
	spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE;
	spec.free     = GuiDrop_free;
	spec.get_path = GuiDrop_get_path;
	spec.set_path = NULL;
	spec.mold     = GuiDrop_mold;

	Handle_GuiDrop = RL_REGISTER_HANDLE_SPEC(cb_cast("GUI-DROP"), &spec);
	if (Handle_GuiDrop == 0) return FALSE;

	// The menu dialect's separator. Every other word it knows comes from a
	// `words:` list in the specification; this one is mapped by name
	// because `---` would generate an unreadable enum name.
	Word_Separator = RL_MAP_WORD((REBYTE*)"---");

	Gui_Init_Platform();

	// Last, and after the platform is up: the first poll can arrive as soon
	// as the interpreter waits, and Poll_Gui() pumps a queue which has to
	// exist by then. DEFINE_DEV leaves flags zeroed, so ask for polling
	// before registering.
	SET_FLAG(Dev_Gui.flags, RDO_AUTO_POLL);
	Gui_Dev_Id = RL_REGISTER_DEVICE(&Dev_Gui, sizeof(REBDEV));

	return TRUE;
}


#ifdef REB_EXT

/***********************************************************************
**  Standalone extension library
***********************************************************************/

RXIEXT const char *RX_Init(int opts, RL_LIB *lib) {
	REBYTE ver[8];
	RL = lib;
	RL_VERSION(ver);

	if (MIN_REBOL_VERSION > VERSION(ver[1], ver[2], ver[3])) return 0;
	if (!CHECK_STRUCT_ALIGN) {
		trace("CHECK_STRUCT_ALIGN failed!");
		return 0;
	}
	return init_block;
}

RXIEXT int RX_Quit(int opts) {
	// Windows still open at shutdown are destroyed here; their handles are
	// about to go away with the interpreter, so nothing can reach them.
	Gui_Quit_Platform();
	return 0;
}

// Reports the RL_API ABI this was built against, so `load-extension`
// can refuse an incompatible host. An absent symbol means ABI 0.
RXIEXT int RX_Abi(void) {
	return RL_ABI_VERSION;
}

// Resolved by name, so the spelling is fixed. The bounds-checked
// dispatcher is generated into gen-gui.c.
RXIEXT int RX_Call(int cmd, RXIFRM *frm, void *ctx) {
	return Gui_RX_Call(cmd, frm, ctx);
}

#endif
