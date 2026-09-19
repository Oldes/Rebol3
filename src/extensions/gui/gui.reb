REBOL [
	Title:   "Rebol GUI extension"
	Name:    gui
	Version: 0.3.0
	Needs:   3.22.8
	Author:  @Oldes
	License: Apache-2.0
	Options: [delay]
	Exports: [
		open-window close-window show-window hide-window
		add-button add-image add-text add-field add-area
		add-check add-radio add-slider add-progress add-drop-down add-panel
		remove-widget redraw
		gui-device gui-device-polls gui-device-events
		gui-device-pumps gui-device-messages
		poll-events do-events
	]
	Purpose: {
		A minimal, GOB-free windowing extension.

		This first version does exactly one thing: it opens native windows
		of a requested client size and title, and hands mouse events back
		to Rebol as plain values. There is no compositor, no DRAW dialect
		and no dependency on the host's View sources - drawing is expected
		to arrive later as an image! blitted into the window.

		GUI events are NOT posted to system/ports/event: the extension
		keeps its own queue and `poll-events` drains it into a block of
		event! values. Each one names its window or widget through the
		EVM_HANDLE model, so a context handle crosses the boundary where
		a REBGOB used to have to, and nothing in the host has to make
		sense of a gob.

		What IS posted is a single wake event, on a port of this
		extension's own (the `gui` scheme), whenever the OS pump has put
		something in that queue. It carries no data - it exists so that
		WAIT returns the moment a click arrives rather than at the end of
		its timeout.
	}
]

;; ---------------------------------------------------------------------------
;; C-side configuration
c-prefix: GUI

;; Extra declarations the generated header must carry.
;;
;; The payloads are deliberately opaque here: `handle` is a void* rather than
;; an HWND or an NSView*, so that gen-gui.h stays free of windows.h and of
;; AppKit, and the command sources can be compiled without either.
;;
;; Widgets are kept in an intrusive list on their window. That list is what
;; lets a closing window take its children down with it - the OS destroys the
;; native controls, and this is how the Rebol handles hear about it.
c-header: {
extern REBCNT Handle_GuiWindow;
extern REBCNT Handle_GuiWidget;
extern REBCNT Word_Separator; // the menu dialect's `---`

// The device this extension registers so that WAIT pumps the OS message
// queue and is woken by a pushed event. See the note above Poll_Gui().
extern REBDEV Dev_Gui;
extern int    Gui_Dev_Id;
extern REBCNT Gui_Dev_Polls;
extern REBCNT Gui_Dev_Events;
extern REBCNT Gui_Dev_Pumps;
extern REBCNT Gui_Dev_Msgs;

// How this extension describes a font when it is not inside a control.
//
// The name is a plain UTF-8 C string owned by the struct, NOT a Rebol
// series: a handle context has exactly one GC-marked slot, it is already
// shared between a payload and the children, and a font name is not a Rebol
// value anyway - so it is invisible to the collector and whoever owns a
// GUIFONT frees its name.
//
// A widget does not carry one of these. Its font lives in the native
// control, which is asked at every read - so a font set by any other means
// is reported honestly, and nothing can drift out of step. Only a WINDOW
// keeps a GUIFONT, because a default has to survive until the next widget
// is created.
typedef struct Gui_Font_Spec {
	char   *name;   // family name; NULL means the platform's own
	REBINT  size;   // in points; 0 means the platform's own
	REBCNT  style;  // GUI_FONT_* bits
} GUIFONT;

#define GUI_FONT_BOLD    1
#define GUI_FONT_ITALIC  2

typedef struct Gui_Window_Context {
	void   *handle;  // native window (HWND / NSWindow*)
	REBHOB *hob;     // back reference, so the window proc can tag its events
	REBCNT  flags;   // GUIW_* bits
	void   *widgets; // head of the child widget list (GUIWIDGET*)
	GUIFONT font;    // what a widget created from now on starts with; it is
	                 // read at creation and never again, so restyling a
	                 // window does not reach back into what it already holds

	REBCNT  background; // the client area, in the same three states a
	                 // widget's has: 0 for the system window colour,
	                 // GUI_COLOR_SET|rgb for a colour of its own, and
	                 // GUI_BG_CLEAR for see-through. A widget showing its
	                 // parent's background resolves to this

	// The menu bar. `menu` is the native object (HMENU / NSMenu*) and
	// `accel` the Win32 accelerator table, which has no counterpart on
	// macOS - a key equivalent there belongs to the item itself.
	//
	// The BLOCK the caller assigned is kept in the payload half of
	// hob->series - see the note on the shared slot in gui-commands.c -
	// and that is what `win/menu` reads back.
	void   *menu;
	void   *accel;
	REBCNT *menu_ids;   // item id N (1-based) is the word menu_ids[N-1]
	REBYTE *menu_on;    // ... and menu_on[N-1] is whether it is enabled
	REBCNT  menu_count;
} GUIWIN;

// An image widget holds no pixels of its own: the image! it was given lives in
// the payload half of `hob->series`, which the GC marks, and the backend reads
// the dimensions and the data from it at every paint. So drawing into that same
// image from Rebol - or from another extension - shows up on the next redraw,
// with nothing copied in between.
//
// The other half of that slot is the container's children - see the note on
// the shared slot in gui-commands.c. Which is why nothing here touches
// hob->series directly.
typedef struct Gui_Widget_Context {
	void   *handle;  // native control (HWND / NSView*)
	REBHOB *hob;     // back reference, as above; hob->series carries the
	                 // image! and the children
	REBCNT  kind;    // W_GUI_WIDGET_* - what the control is
	GUIWIN *owner;   // WINDOW it ends up in, however deeply nested; NULL once
	                 // that window is gone
	void   *parent;  // containing panel or image widget (GUIWIDGET*), NULL
	                 // when the window holds it directly
	void   *next;    // next widget of the same window (GUIWIDGET*) - the list
	                 // is FLAT and window-wide, whatever the nesting, so one
	                 // walk still reaches every widget at teardown
	REBCNT  group;   // radio group id; 0 for every other kind
	REBCNT  state;   // check / radio: 1 when on. The extension is the source
	                 // of truth here, not the native control - see the radio
	                 // grouping note in gui-commands.c.
	                 // panel: GUI_PANEL_EDGE when it draws a frame - read by
	                 // the backend at paint time, so it can be turned on and
	                 // off without touching the native control
	                 // field / area: GUI_TEXT_READ_ONLY. Kept rather than
	                 // read back because macOS answers "enabled" and
	                 // "editable" with the same property, so the two have
	                 // to be combined from something
	REBCNT  background; // what is painted BEHIND the text, in three states:
	                 //   0                  the platform's own background,
	                 //                      which is the parent's colour
	                 //   GUI_COLOR_SET|rgb  filled with that colour
	                 //   GUI_BG_CLEAR       nothing is filled at all, and
	                 //                      whatever is behind shows through
	                 // Kept here for the same reason `color` is: on Win32
	                 // the PARENT is asked for a child's background brush,
	                 // message by message, so the control itself has
	                 // nowhere to hold one
	REBCNT  color;   // text colour: 0 when the platform decides, otherwise
	                 // GUI_COLOR_SET | 0xRRGGBB. Kept here rather than in the
	                 // control because Win32 does not store one: the PARENT
	                 // is asked for it, message by message, as each control
	                 // is about to paint
} GUIWIDGET;

#define GUIW_VISIBLE  1

// Passed to Gui_Open_Window(). Everything a window's frame can be is
// decided at creation and changeable afterwards through `resizable?` and
// `border?`, so these say only what it STARTS as.
#define GUI_WIN_FIXED       1
#define GUI_WIN_BORDERLESS  2
#define GUI_WIN_TRANSPARENT 4

// wid->state of a panel
#define GUI_PANEL_EDGE 1

// wid->state of a field or an area
#define GUI_TEXT_READ_ONLY 1

// wid->color. The top byte is the "has one" flag, which is why a colour of
// 0.0.0 is still distinguishable from no colour at all.
#define GUI_COLOR_SET        0xFF000000
#define GUI_COLOR_HAS(c)     (((c) & GUI_COLOR_SET) != 0)
#define GUI_COLOR_R(c)       (((c) >> 16) & 0xFF)
#define GUI_COLOR_G(c)       (((c) >>  8) & 0xFF)
#define GUI_COLOR_B(c)        ((c)        & 0xFF)
#define GUI_COLOR_OF(r,g,b)  (GUI_COLOR_SET | ((REBCNT)(r) << 16) \
                                            | ((REBCNT)(g) <<  8) \
                                            |  (REBCNT)(b))

// wid->background only: nothing is filled. Any value with the top byte
// clear is impossible for a colour, so 1 cannot be mistaken for one - and
// 0 is still "the platform's own".
#define GUI_BG_CLEAR         1
#define GUI_BG_IS_CLEAR(b)   ((b) == GUI_BG_CLEAR)
}

;; ---------------------------------------------------------------------------
;; Words resolved at init time through RL_MAP_WORDS.
;;
;; ORDER IS SIGNIFICANT: each generated W_GUI_<LIST>_* enum starts at 1 (after
;; the _0 sentinel) and `Gui_<list>_words` is the very same 1-based array, so
;; the C side can emit a word with a plain `Gui_<list>_words[n]`.
;;
;; There is no `event:` list: an event reports its type with the core's own
;; EVT_* code, from system/catalog/event-types, so the backends name those
;; directly and this extension defines no event words of its own.
;;
;; The `arg:` list is not written here - the generator collects it from the
;; `handles:` field below.
;; NOTE: append to these lists, never insert - the enum values are positions.
words: [
	;; Words the menu dialect understands beyond the labels and the item
	;; ids themselves. The separator `---` is NOT here: it would generate
	;; `W_GUI_MENU____`, so it is mapped by name in Gui_Init() instead.
	menu: [
		shift control alt  ;; extra modifiers in a shortcut block
	]
	;; What `area/scroll:` accepts instead of a percent. `end` is `bottom`
	;; under another name, because both read well in different sentences.
	scroll: [
		top bottom end
	]
	widget: [
		button
		image
		text            ;; a static label
		field           ;; one line of editable text
		area            ;; several lines of editable text, with a scrollbar
		check           ;; a checkbox, toggled on its own
		radio           ;; one of a group; see `group` below
		slider          ;; draggable, reports `change`
		progress        ;; shows a value, takes no input
		drop-down       ;; pick one of a list; reports `change`
		panel           ;; holds other widgets; see `parent` below
	]
]

;; ---------------------------------------------------------------------------
;; Handle types and their path accessors.
handles: [
	window: [
		"GUI window handle"
		;NAME    GET       SET       DESCRIPTION
		title    string!   string!   "Text shown in the title bar"
		size     pair!     pair!     "Size of the client area in pixels"
		offset   pair!     pair!     "Position of the top-left corner on the screen"
		id       integer!  none      "Native window handle as an integer"
		open?    logic!    none      "False once the window has been closed"
		scale    decimal!  none      "Device pixels per unit of size - 1.0 at 100%, 1.75 at 175%, 2.0 on a Retina Mac"
		resizable? logic!  logic!    "Whether the user can resize it"
		border?    logic!  logic!    "Whether it has a title bar and a frame; a borderless window cannot be moved or closed by the user"
		background tuple!  [tuple! none!] "Colour of the client area; none for the system window colour"
		transparent? logic! logic!   "Whether the client area is see-through to whatever is behind the window"
		;; Defaults for widgets created AFTERWARDS - see the note in the README.
		font      string!  [string! none!] "Font family widgets are created with; none for the system font"
		font-size integer! [integer! none!] "Point size widgets are created with; none for the system size"
		bold?     logic!   logic!    "Whether widgets are created bold"
		italic?   logic!   logic!    "Whether widgets are created italic"
		children  block!   none      "Widgets the window holds directly, in the order they were added"
		menu      block!   [block! none!] "The menu bar, as the dialect described in the README; none removes it"
		menu-enabled? block! block!  "Which items are greyed out, as word/logic pairs; setting merges, it does not replace"
	]
	widget: [
		"GUI widget handle - a native control inside a window"
		;NAME    GET       SET       DESCRIPTION
		text     string!   string!   "Label or contents; the caption of a framed panel; the selected item of a drop-down, which is read-only; none for an image"
		items    block!    block!    "Strings a drop-down offers; none for other kinds"
		index    integer!  integer!  "Which item is picked, 1-based; 0 for none"
		image    image!    image!    "Image shown by an image widget, none for other kinds"
		size     pair!     pair!     "Size of the control; a zero axis asks it what that axis needs, the same as at creation"
		offset   pair!     pair!     "Position inside whatever holds it - a window or a panel"
		id       integer!  none      "Native control handle as an integer"
		kind     word!     none      "What the control is: button, image, text, field, area, check, radio, slider, progress or drop-down"
		value    percent!  [percent! decimal!] "Position of a slider or a progress bar; none for other kinds"
		state    logic!    logic!    "Whether a check or a radio is on; none for other kinds"
		edge     logic!    logic!    "Whether a panel draws a frame around itself; none for other kinds"
		;; Typography. Every kind which has `text` has these; the rest answer none.
		font      string!  [string! none!] "Font family; none puts it back to the system font"
		font-size integer! [integer! none!] "Point size; none puts it back to the system size"
		bold?     logic!   logic!    "Whether the text is bold"
		italic?   logic!   logic!    "Whether the text is italic"
		color     tuple!   [tuple! none!] "Text colour; none lets the platform decide"
		background tuple!  [tuple! none!] "Colour painted behind the text; none lets the platform decide"
		transparent? logic! logic!        "Whether nothing is painted behind it at all, so whatever the widget sits on shows through"
		children  block!   none      "Widgets a container holds, in the order they were added; none for a kind which cannot hold any"
		read-only? logic!  logic!    "Whether a field or an area refuses to be edited while staying selectable; none for other kinds"
		scroll    percent!  [percent! decimal! word!] "How far an area is scrolled; set a percent, or one of top, bottom and end; none for kinds which do not scroll"
		group    integer!  none      "Which radio group it belongs to; 0 for everything else"
		enabled? logic!    logic!    "Whether the control responds to the user"
		parent   handle!   none      "Whatever holds it - a window, or a panel; none once gone"
		window   handle!   none      "The window it ends up in, however deeply nested"
	]
]

;; ---------------------------------------------------------------------------
;; Commands. Order is significant - it fixes the command indices, so new ones
;; are appended rather than inserted.
commands: [
	open-window: [
		"Creates a window and returns its handle"
		size [pair!] "Size of the client area"
		/title text [string!] "Text shown in the title bar"
		/at offset [pair!] "Position of the top-left corner on the screen"
		/hidden "Creates the window without showing it"
		/fixed  "The user cannot resize it"
		/borderless "No title bar and no frame - see the note in the README"
		/transparent "The client area is see-through to whatever is behind the window"
	]
	close-window: ["Destroys the window" window [handle!]]
	show-window:  ["Makes the window visible" window [handle!]]
	hide-window:  ["Hides the window without destroying it" window [handle!]]
	poll-events:  ["Dispatches pending OS messages and returns the collected events"]
	add-button: [
		"Creates a native push button inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!] "Label"
		offset [pair!]   "Position inside the client area"
		size   [pair!]   "Size of the button"
	]
	remove-widget: ["Destroys a widget" widget [handle!]]
	add-image: [
		"Creates an image widget inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		image  [image!]  "Shown as is; the widget keeps a reference, not a copy"
		offset [pair!]   "Position inside the client area"
		/size sz [pair!] "Scales the image to this size (default: the image's own)"
	]
	redraw: [
		"Repaints a window or a widget - use after drawing into a displayed image"
		target [handle!]
	]
	add-text: [
		"Creates a static label inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!]
		offset [pair!]   "Position inside the client area"
		size   [pair!]
	]
	add-field: [
		"Creates a one-line text entry inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!] "Initial contents"
		offset [pair!]   "Position inside the client area"
		size   [pair!]
	]
	add-area: [
		"Creates a multi-line text entry inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!] "Initial contents"
		offset [pair!]   "Position inside the client area"
		size   [pair!]
	]
	add-check: [
		"Creates a checkbox inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!] "Label"
		offset [pair!]   "Position inside the client area"
		size   [pair!]
	]
	add-radio: [
		"Creates a radio button inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		text   [string!] "Label"
		offset [pair!]   "Position inside the client area"
		size   [pair!]
		/group id [integer!] {Radios sharing an id turn each other off (default: 0)}
	]
	add-slider: [
		"Creates a slider inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		offset [pair!] "Position inside the client area"
		size   [pair!] "Taller than wide makes it vertical"
		/value val [percent! decimal!] "Initial position (default: 0%)"
	]
	add-progress: [
		"Creates a progress bar inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		offset [pair!] "Position inside the client area"
		size   [pair!]
		/value val [percent! decimal!] "Initial position (default: 0%)"
	]
	add-drop-down: [
		"Creates a drop-down list inside a window and returns its handle"
		parent [handle!] "Window or panel to put it in"
		items  [block!] "Strings to offer"
		offset [pair!]  "Position inside the client area"
		size   [pair!]  "Of the closed control; room for the list is added"
		/index n [integer!] "Item picked to start with, 1-based (default: none)"
	]
	add-panel: [
		"Creates a panel - a widget which holds other widgets - and returns its handle"
		parent [handle!] "Window or panel to put it in"
		offset [pair!]   "Position inside the client area"
		size   [pair!]
		/edge  "Draws a frame around it"
		/title text [string!] "Caption set into the frame; implies /edge"
	]
	;; APPENDED, not inserted - the position in this block is the command
	;; index, so anything put in the middle renumbers everything after it.
	gui-device:        ["Returns the id of the device this extension registered"]
	gui-device-polls:  ["Returns how many times the host has polled it"]
	gui-device-events: ["Returns how many wake events the device has pushed"]

	;; The `gui` scheme's actors. Internal - the scheme is defined in this
	;; extension's own mezzanine and is the only caller.
	gui-port-open:  ["Attaches a port to the GUI device" port [port!]]
	gui-port-close: ["Detaches it again" port [port!]]
	gui-port-read:  ["Returns how many events are waiting" port [port!]]

	;; Diagnostic pair. `pumps` is polls which got as far as pumping, and
	;; `messages` is what those pumps dispatched - the two numbers that
	;; distinguish "nobody is pumping" from "somebody else drained the OS
	;; queue before we looked".
	gui-device-pumps:    ["Returns how many polls reached the OS pump"]
	gui-device-messages: ["Returns how many OS messages those pumps dispatched"]
]

;; ---------------------------------------------------------------------------
;; Module body.
mezzanine: [
	;; -----------------------------------------------------------------------
	;; The event port - a doorbell, not a channel.
	;;
	;; The device pushes one Rebol event whenever the OS pump has put
	;; something in the extension's queue, and an event has to be delivered
	;; somewhere: EVM_PORT events resolve back to this port, its `awake` is
	;; what puts it on WAIT's waked list, and the GC marks it for as long as
	;; an event of its own is queued.
	;;
	;; The GUI events themselves never travel through it. They stay in the
	;; extension's queue and come out of `poll-events`. All this port says is
	;; "there is something to drain" - and `read` on it answers how much.
	;;
	;; It is open for the life of the module, which is what keeps the device's
	;; one port slot pointing at something valid. A program writing its own
	;; loop can wait on it alongside anything else:
	;;
	;;     wait [my-socket gui/event-port 1]
	sys/make-scheme [
		title: "Rebol/GUI event doorbell"
		name:  'gui
		actor: object [
			open:  func [port] [gui-port-open  port]
			close: func [port] [gui-port-close port]
			read:  func [port] [gui-port-read  port]
		]
	]

	event-port: try [open [scheme: 'gui]]

	;; Returning TRUE is what wakes WAIT. Without an awake handler the system
	;; port takes the event off its queue, finds nothing to call, and drops
	;; it - the doorbell would ring into an empty hall and every wait would
	;; sleep out its full timeout.
	if port? event-port [event-port/awake: func [event] [true]]

	;; `poll-events` returns a block of event! values, so a handler takes
	;; ONE argument and reads what it needs by name:
	;;
	;;     foreach evt poll-events [
	;;         switch evt/type [
	;;             click  [print [evt/source "at" evt/offset]]
	;;             change [...]
	;;         ]
	;;     ]
	;;
	;; `source` is the window for window events and the WIDGET itself for a
	;; click, a change and a focus change - use `evt/source/window` to get
	;; back to the window it is in.
	;;
	;; The type words are the core's own: this extension defines no event
	;; vocabulary of its own, so they are the ones in
	;; system/catalog/event-types that every other event source reports.
	do-events: function [
		"Pumps window events until the given window is closed"
		window  [handle!]
		handler [any-function!] "Called with one event! for each event"
		/rate delay [number!] {Longest it may sleep with nothing to do (default: 0.05)}
	][
		;; `wait` does the sleeping, and that is the whole point: the
		;; extension registers a device with RDO_AUTO_POLL, so the host pumps
		;; the OS message queue from inside OS_Wait - and everything else
		;; Rebol has waiting is serviced by the same sleep.
		;;
		;; Waiting on the event port as well as the delay is what makes this
		;; responsive rather than merely correct: the device pushes an event
		;; when the queue grows, so `wait` returns as soon as a click arrives
		;; and the delay is only a ceiling. Without the port it still works,
		;; just at the rate of the ceiling.
		delay: any [delay 0.05]
		wake:  either port? event-port [reduce [event-port delay]][delay]

		forever [
			foreach evt poll-events [
				handler evt
				;; `close` only reports the request - closing is ours to do
				if all [evt/type = 'close  evt/source = window] [
					close-window window
				]
				;; the handler is allowed to close it as well, and the rest
				;; of this batch would then refer to widgets which are gone
				unless window/open? [break]
			]
			unless window/open? [exit]
			wait wake
		]
	]
]
