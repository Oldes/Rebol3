//
// Project: Rebol/GUI extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Win32 backend.
//
// The only file which knows about HWNDs. Its whole job is to turn window
// messages into Gui_Queue_Event() calls - it never calls back into the
// interpreter except to build a title string, so nothing here can run
// Rebol code from inside a modal OS loop.
//
// The W (wide) entry points are used explicitly rather than the TCHAR
// macros, so the build does not depend on UNICODE being defined. Rebol
// strings are UTF-8 and are converted at the boundary.
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM
#include <commctrl.h> // trackbar and progress bar
#include <shellapi.h> // DragQueryFile and friends
#define  COBJMACROS   // so the IDropTarget vtable can be used from plain C
#include <ole2.h>     // OLE drag and drop
// MAKE_MEM / FREE_MEM are malloc and free behind a macro, and neither
// rebol-extension.h nor windows.h is required to declare them - the wide
// string conversions and the font cache here both allocate.
#include <stdlib.h>
// The drag and drop trace prints; MSVC does not get stdio from windows.h.
#include <stdio.h>

// Windows uses this macro name too, and we want Rebol's meaning of it.
#undef IS_ERROR

#include "gen-gui.h"
#include "gui.h"

//***** Locals *****//

static const WCHAR *Class_Name = L"RebolGuiWindow";
static const WCHAR *Class_Name_Image = L"RebolGuiImage";
static const WCHAR *Class_Name_Panel = L"RebolGuiPanel";
static HINSTANCE App_Instance = NULL;
static REBOOL Class_Registered = FALSE;
static REBOOL Image_Class_Registered = FALSE;
static REBOOL Panel_Class_Registered = FALSE;
static HFONT  Default_Font = NULL;
static REBOOL Default_Font_Owned = FALSE;

// Raised around SetWindowTextW so that writing to an edit control from Rebol
// does not come back as a `change` event. SetWindowText delivers EN_CHANGE
// synchronously on this same thread, so a plain flag is enough.
static REBOOL Setting_Text = FALSE;

// WS_CLIPCHILDREN, for the same reason a panel has it: without it a
// parent's WM_PAINT paints straight over the controls it holds. The DC
// BeginPaint hands back covers the whole invalid region, children
// included, so one FillRect erases every control it crosses - and only
// the parts which then get a WM_PAINT of their own come back.
//
// What does NOT come back is anything in a control's NON-client area. An
// entry's sunken border is drawn on WM_NCPAINT, which invalidating the
// client area never raises, so the border stays missing until something
// else disturbs it. That is what a field with its top edge rubbed out
// after a neighbour was resized over it really was.
//
// With the flag, the parent is clipped out of every child rectangle and
// physically cannot do it.
#define WINDOW_STYLE   (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN)
#define WINDOW_EXSTYLE (0)

// What a window's frame is made of, as bits of the window style.
//
//   WS_THICKFRAME is the grab handle; WS_MAXIMIZEBOX goes with it, since
//   a window which cannot be dragged bigger should not have a button
//   which does it either.
//   WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX are the title bar and what
//   sits on it. Without them there is no close box, so no `close` event,
//   and nothing to drag the window by.
#define WINDOW_RESIZE_BITS (WS_THICKFRAME | WS_MAXIMIZEBOX)
#define WINDOW_BORDER_BITS (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

// Trackbars and progress bars work in whole steps, so the 0.0 - 1.0 range
// the extension speaks is carried as one part in RANGE_STEPS.
#define RANGE_STEPS 1000

// A combo box is created with the height of the WHOLE thing - the closed
// control plus the list it drops down - so room for the list has to be
// added to whatever height the caller asked for. Ask for too little and
// the list is a sliver; this is the classic Win32 trap with this control.
#define DROP_LIST_ROOM 220

#define HWND_OF(win)      ((HWND)((win)->handle))
#define HWND_OF_WID(wid)  ((HWND)((wid)->handle))
#define GUIWIN_OF(hwnd)   ((GUIWIN*)GetWindowLongPtrW((hwnd), GWLP_USERDATA))

// Defined with the drop target, far below, but needed by WM_NCDESTROY: a
// registered target must go while its HWND is still valid.
static void Gui_Window_Revoke_Drop(GUIWIN *win);

// Whether OLE came up on this thread, decided once in Gui_Init_Platform,
// and whether it was us who started it - see the note there.
static REBOOL Ole_Ready = FALSE;
static REBOOL Ole_Ours  = FALSE;


//== string conversion ========================================================

// UTF-8 (not necessarily terminated) -> a fresh, null terminated UTF-16
// buffer. Release it with FREE_MEM.
static WCHAR* To_Wide(const REBYTE *utf8, REBCNT len)
{
	int n;
	WCHAR *out;

	if (!utf8 || len == 0) return NULL;

	n = MultiByteToWideChar(CP_UTF8, 0, (const char*)utf8, (int)len, NULL, 0);
	if (n <= 0) return NULL;

	out = (WCHAR*)MAKE_MEM((n + 1) * sizeof(WCHAR));
	if (!out) return NULL;

	MultiByteToWideChar(CP_UTF8, 0, (const char*)utf8, (int)len, out, n);
	out[n] = 0;
	return out;
}

// Window text -> a fresh Rebol string series. Shared by the window title and
// the widget label, which are the same Win32 call underneath.
static REBSER* Text_Of(HWND hwnd)
{
	int    len;
	WCHAR *buf;
	REBSER *str;

	if (!hwnd) return NULL;

	len = GetWindowTextLengthW(hwnd);
	if (len <= 0) return RL_MAKE_STRING(0, FALSE);

	buf = (WCHAR*)MAKE_MEM((len + 1) * sizeof(WCHAR));
	if (!buf) return NULL;

	len = GetWindowTextW(hwnd, buf, len + 1);

	// REBUNI is 16 bits, so the wide buffer is passed through as is.
	str = RL_ENCODE_UTF8_STRING(buf, (REBCNT)len, TRUE, 0);
	FREE_MEM(buf);
	return str;
}

static REBOOL Set_Text_Of(HWND hwnd, const REBYTE *utf8, REBCNT len)
{
	WCHAR *wide;
	BOOL ok;

	if (!hwnd) return FALSE;

	wide = To_Wide(utf8, len);
	Setting_Text = TRUE;
	ok = SetWindowTextW(hwnd, wide ? wide : L"");
	Setting_Text = FALSE;
	if (wide) FREE_MEM(wide);
	return ok ? TRUE : FALSE;
}

// The font controls are created with by default is the ancient system font,
// so the shell's message font is used instead - the one every other dialog
// on the machine uses.
static HFONT Get_Default_Font(void)
{
	NONCLIENTMETRICSW metrics;

	if (Default_Font) return Default_Font;

	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
		Default_Font = CreateFontIndirectW(&metrics.lfMessageFont);
		Default_Font_Owned = (Default_Font != NULL);
	}
	if (!Default_Font) Default_Font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	return Default_Font;
}


/***********************************************************************
**  Fonts asked for from Rebol.
**
**  An HFONT is a GDI object which has to be deleted by whoever made it,
**  and a control does not own the one it is given. Rather than hang one
**  off every widget - and get it wrong at teardown exactly once - the
**  fonts are kept in a small cache, shared by every control that asks
**  for the same face, and deleted together in Gui_Quit_Platform().
**
**  A program restyling one label in a loop therefore creates one font,
**  not one per assignment.
***********************************************************************/
typedef struct Gui_Font_Cache {
	struct Gui_Font_Cache *next;
	HFONT  font;
	int    size;    // points
	REBCNT style;   // GUI_FONT_* bits
	WCHAR  name[LF_FACESIZE];
} FONTCACHE;

static FONTCACHE *Font_Cache = NULL;

/***********************************************************************
**  Logical units.
**
**  Everything above this file speaks LOGICAL units - 96 to the inch,
**  the same thing macOS calls a point - and this is where they become
**  device pixels and back. `240x26` therefore describes the same
**  physical size on a 96 DPI screen, on a 175% one, and on a Mac.
**
**  It has to be done here rather than left to the caller, because the
**  process is DPI aware: Windows does not scale anything for us, and
**  the shell's message font DOES come back already scaled for the
**  display. Without this, a script's coordinates stay 96-DPI-sized
**  while its text grows with the display - which is a label too small
**  for its own font at 125%, and a text entry that clips its line at
**  175%.
**
**  One scale for the process, cached: PROCESS_SYSTEM_DPI_AWARE means
**  the system DPI is what everything is drawn at, whichever monitor a
**  window is on. Per-monitor awareness would make this per window, and
**  would need WM_DPICHANGED as well.
***********************************************************************/
static int Gui_DPI = 96;

// Points per inch on this display, which is also what turns a point size
// into the pixel height a LOGFONT wants.
static int Screen_DPI(void)
{
	return Gui_DPI;
}

static void Read_Screen_DPI(void)
{
	HDC dc = GetDC(NULL);
	if (dc) {
		int y = GetDeviceCaps(dc, LOGPIXELSY);
		if (y > 0) Gui_DPI = y;
		ReleaseDC(NULL, dc);
	}
}

static REBINT To_Device(REBINT v)
{
	return (Gui_DPI == 96) ? v : (REBINT)MulDiv((int)v, Gui_DPI, 96);
}

static REBINT To_Logical(REBINT v)
{
	return (Gui_DPI == 96) ? v : (REBINT)MulDiv((int)v, 96, Gui_DPI);
}

// Whole boxes, which is how they nearly always travel.
static void Box_To_Device(REBINT *x, REBINT *y, REBINT *w, REBINT *h)
{
	if (Gui_DPI == 96) return;
	if (x) *x = To_Device(*x);
	if (y) *y = To_Device(*y);
	if (w) *w = To_Device(*w);
	if (h) *h = To_Device(*h);
}

static void Box_To_Logical(REBINT *x, REBINT *y, REBINT *w, REBINT *h)
{
	if (Gui_DPI == 96) return;
	if (x) *x = To_Logical(*x);
	if (y) *y = To_Logical(*y);
	if (w) *w = To_Logical(*w);
	if (h) *h = To_Logical(*h);
}

static void Free_Font_Cache(void)
{
	FONTCACHE *entry = Font_Cache;
	while (entry) {
		FONTCACHE *next = entry->next;
		if (entry->font) DeleteObject(entry->font);
		FREE_MEM(entry);
		entry = next;
	}
	Font_Cache = NULL;
}

// A NULL or empty name means the shell's message font family, and a size
// of 0 its size - which is how `font: none` and `font-size: none` arrive.
static HFONT Font_For(const WCHAR *name, int size, REBCNT style)
{
	FONTCACHE *entry;
	LOGFONTW   base, lf;
	HFONT      font;

	// Whatever is not being asked for comes from the default font, so a
	// size on its own keeps the shell's family rather than falling back to
	// something that looks nothing like the rest of the dialog.
	ZeroMemory(&base, sizeof(base));
	if (!GetObjectW(Get_Default_Font(), sizeof(base), &base)) return NULL;

	ZeroMemory(&lf, sizeof(lf));
	lf = base;
	if (name && name[0]) {
		lstrcpynW(lf.lfFaceName, name, LF_FACESIZE);
	}
	if (size > 0) {
		lf.lfHeight = -MulDiv(size, Screen_DPI(), 72);
		lf.lfWidth  = 0;
	}
	lf.lfWeight = (style & GUI_FONT_BOLD) ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = (style & GUI_FONT_ITALIC) ? TRUE : FALSE;

	for (entry = Font_Cache; entry; entry = entry->next) {
		if (entry->size == size && entry->style == style
		    && lstrcmpW(entry->name, lf.lfFaceName) == 0)
			return entry->font;
	}

	font = CreateFontIndirectW(&lf);
	if (!font) return NULL;

	entry = (FONTCACHE*)MAKE_MEM(sizeof(FONTCACHE));
	if (!entry) { DeleteObject(font); return NULL; }
	entry->font  = font;
	entry->size  = size;
	entry->style = style;
	lstrcpynW(entry->name, lf.lfFaceName, LF_FACESIZE);
	entry->next  = Font_Cache;
	Font_Cache   = entry;

	return font;
}


/***********************************************************************
**  A see-through window, and what fills the client area.
**
**  WS_EX_LAYERED with a COLOUR KEY: the client area is filled with a
**  colour the compositor then drops, so child controls keep painting
**  normally and are the only thing left on screen. Per-pixel alpha
**  would mean UpdateLayeredWindow, which does not composite child
**  windows at all - it would rule out every native control this
**  extension exists to place.
**
**  The cost of a key is that a widget painting exactly this colour
**  disappears too, so it is a colour nothing sensible picks: full
**  magenta, the traditional choice for the same reason.
***********************************************************************/
#define GUI_KEY_COLOR RGB(255, 0, 255)

// What the client area of `win` is filled with. NULL when the window
// has none of its own and the system colour brush should be used, which
// FillRect takes in its encoded form and WM_CTLCOLOR* does not - hence
// two callers and two ways of asking.
static COLORREF Window_Fill_Color(GUIWIN *win, REBOOL *have)
{
	*have = TRUE;
	if (!win)                                 { *have = FALSE; return 0; }
	if (GUI_BG_IS_CLEAR(win->background))     return GUI_KEY_COLOR;
	if (GUI_COLOR_HAS(win->background))
		return RGB(GUI_COLOR_R(win->background),
		           GUI_COLOR_G(win->background),
		           GUI_COLOR_B(win->background));
	*have = FALSE;
	return 0;
}


/***********************************************************************
**  Fills `rect` of `dc` with what the WINDOW's client area is - its own
**  colour, the key colour when it is see-through, or the system window
**  colour. A panel with no colour of its own uses this too, which is
**  what keeps a panel invisible on a dark window rather than a pale
**  slab on it.
***********************************************************************/
static void Fill_Window_Background(HDC dc, const RECT *rect, GUIWIN *win)
{
	REBOOL   have = FALSE;
	COLORREF rgb  = Window_Fill_Color(win, &have);

	if (have) {
		HBRUSH brush = CreateSolidBrush(rgb);
		if (brush) {
			FillRect(dc, rect, brush);
			DeleteObject(brush);
			return;
		}
	}
	FillRect(dc, rect, (HBRUSH)(COLOR_WINDOW + 1));
}

// The brush handed back for a widget with a background colour of its own.
// One slot, because WM_CTLCOLOR* is answered for one control at a time on
// this thread and the brush is used before the next answer is given.
static HBRUSH Ctl_Brush = NULL;

// Defined further down, and used before that: a transparent panel needs
// the first while painting itself, and a background change needs the
// second to reach a container's children.
static void Paint_Parent_Background(HWND hwnd, HDC dc);
static void Repaint_Widget(GUIWIDGET *wid, REBOOL now);


/***********************************************************************
**  The colour a transparent widget should show - when whatever holds it
**  has a flat one, which is nearly always.
**
**  Painting the container's own colour is INDISTINGUISHABLE from showing
**  through it, and it is worth a great deal: the control keeps erasing
**  itself normally, so it needs no subclass, and - the reason this
**  exists - the themed animation keeps working.
**
**  A themed check or radio cross-fades between states through
**  BufferedPaintAnimation, which paints into a memory DC and never
**  sends WM_ERASEBKGND. A control told "fill with nothing" therefore
**  animates from an empty buffer and vanishes for the length of the
**  fade. Handing it a real colour is what stops that.
**
**  Returns FALSE only when the answer is not a colour at all - an image
**  widget - where the pixels have to be fetched instead.
***********************************************************************/
static REBOOL Flat_Background_Of(GUIWIDGET *wid, COLORREF *rgb)
{
	GUIWIDGET *parent = wid ? (GUIWIDGET*)wid->parent : NULL;

	while (parent) {
		// Rendered pixels are not a colour.
		if (parent->kind == W_GUI_WIDGET_IMAGE) return FALSE;

		// A transparent container shows what IT sits on, so the question
		// moves up. A panel inside a panel inside the window ends here.
		if (GUI_BG_IS_CLEAR(parent->background)) {
			parent = (GUIWIDGET*)parent->parent;
			continue;
		}

		if (GUI_COLOR_HAS(parent->background)) {
			*rgb = RGB(GUI_COLOR_R(parent->background),
			           GUI_COLOR_G(parent->background),
			           GUI_COLOR_B(parent->background));
			return TRUE;
		}
		break; // a container with the platform's own background
	}

	// The window, or a container which left its background alone. A
	// SEE-THROUGH window is not a colour either - the key it fills with
	// is a colour the compositor removes, and a widget painting it would
	// have holes punched in it - so that falls to the render path.
	{	GUIWIN *owner = wid ? wid->owner : NULL;
		REBOOL  have  = FALSE;
		COLORREF own;

		if (owner && GUI_BG_IS_CLEAR(owner->background)) return FALSE;

		own = Window_Fill_Color(owner, &have);
		*rgb = have ? own : GetSysColor(COLOR_WINDOW);
	}
	return TRUE;
}


/***********************************************************************
**  Answering a control's WM_CTLCOLOR* message.
**
**  Win32 has no "text colour" property on a control: a control about to
**  paint asks its PARENT what to use, and this is that answer. The
**  colour therefore lives in the widget context, which is looked up
**  here from the child window the message came about.
**
**  Background stays COLOR_WINDOW throughout, which is what keeps a
**  label on the same background the window and the panels fill with.
***********************************************************************/
static LRESULT Ctl_Color(HDC dc, HWND child, GUIWIN *win)
{
	GUIWIDGET *wid = NULL;

	// The window's own widget list, rather than the child's GWLP_USERDATA.
	// Not every window that sends this is one of ours - a combo box has an
	// internal list box which sends WM_CTLCOLORLISTBOX in its own name, and
	// whatever sits in ITS user data is not a GUIWIDGET to be dereferenced.
	// The list is flat and window-wide, so one walk covers panels too.
	if (child && win) {
		GUIWIDGET *w = (GUIWIDGET*)win->widgets;
		for (; w; w = (GUIWIDGET*)w->next) {
			if ((HWND)w->handle == child) { wid = w; break; }
		}
	}

	SetTextColor(dc, (wid && GUI_COLOR_HAS(wid->color))
		? RGB(GUI_COLOR_R(wid->color),
		      GUI_COLOR_G(wid->color),
		      GUI_COLOR_B(wid->color))
		: GetSysColor(COLOR_WINDOWTEXT));

	// A colour to fill with: either the widget's own, or - for a
	// transparent one over a container whose background IS a colour - that
	// container's, which looks the same and behaves far better. See
	// Flat_Background_Of().
	if (wid) {
		COLORREF rgb;
		REBOOL   have = FALSE;

		if (GUI_BG_IS_CLEAR(wid->background)) {
			have = Flat_Background_Of(wid, &rgb);
		} else if (GUI_COLOR_HAS(wid->background)) {
			rgb  = RGB(GUI_COLOR_R(wid->background),
			           GUI_COLOR_G(wid->background),
			           GUI_COLOR_B(wid->background));
			have = TRUE;
		}

		if (have) {
			// The brush has to outlive this return - the control fills
			// with it immediately afterwards - and only one control is
			// answered at a time on this thread, so one slot is enough.
			// The previous brush is released on the way in rather than
			// left to leak.
			if (Ctl_Brush) DeleteObject(Ctl_Brush);
			Ctl_Brush = CreateSolidBrush(rgb);
			if (Ctl_Brush) {
				SetBkColor(dc, rgb);
				return (LRESULT)Ctl_Brush;
			}
		}

		// Transparent over something which is not a colour - rendered
		// pixels. TRANSPARENT stops the control filling as it draws the
		// glyphs, and a hollow brush stops it filling the rest; what
		// shows through is what Transparent_Proc() put there.
		if (GUI_BG_IS_CLEAR(wid->background)) {
			SetBkMode(dc, TRANSPARENT);
			return (LRESULT)GetStockObject(NULL_BRUSH);
		}
	}

	// The platform's own, which here means the window's.
	//
	// A REAL brush handle. `(HBRUSH)(COLOR_WINDOW + 1)` is the encoding
	// WNDCLASS.hbrBackground and FillRect accept, and it is NOT a handle:
	// returned from here it is an invalid one, and the control fills with
	// whatever it falls back to - COLOR_3DFACE grey, darker than the
	// window. Visible under the classic look, where a static, a check and
	// a radio paint their own background with this brush, and hidden under
	// visual styles, where the theme paints it and the brush is never
	// used. That is why it only showed with the old look.
	//
	// GetSysColorBrush hands back a cached brush owned by the system: it
	// needs no cleanup and must not be deleted.
	{	REBOOL   have = FALSE;
		COLORREF own  = Window_Fill_Color(win, &have);
		if (have) {
			if (Ctl_Brush) DeleteObject(Ctl_Brush);
			Ctl_Brush = CreateSolidBrush(own);
			if (Ctl_Brush) {
				SetBkColor(dc, own);
				return (LRESULT)Ctl_Brush;
			}
		}
	}
	SetBkColor(dc, GetSysColor(COLOR_WINDOW));
	return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
}


//== events ===================================================================

static REBINT Modifiers(void)
{
	REBINT flags = 0;
	if (GetKeyState(VK_SHIFT)   < 0) flags |= GUI_FLAG_SHIFT;
	if (GetKeyState(VK_CONTROL) < 0) flags |= GUI_FLAG_CONTROL;
	if (GetKeyState(VK_MENU)    < 0) flags |= GUI_FLAG_ALT;
	return flags;
}

// Queues a mouse event at the position carried by lParam.
static void Queue_Mouse(GUIWIN *win, REBCNT type, LPARAM lp, REBINT extra)
{
	if (!win || !win->hob) return;
	// Reported in logical units, like every other coordinate here.
	Gui_Queue_Event(win->hob, type,
	                To_Logical(GET_X_LPARAM(lp)), To_Logical(GET_Y_LPARAM(lp)),
	                Modifiers() | extra);
}

// The same for a child widget. A child covers its part of the window, so
// the parent stops hearing about the mouse there - the widget reports it
// instead, with itself as the source and its own coordinates.
static void Queue_Widget_Mouse(GUIWIDGET *wid, REBCNT type, LPARAM lp, REBINT extra)
{
	if (!wid || !wid->hob) return;
	Gui_Queue_Event(wid->hob, type,
	                To_Logical(GET_X_LPARAM(lp)), To_Logical(GET_Y_LPARAM(lp)),
	                Modifiers() | extra);
}


//== window procedure =========================================================

static LRESULT CALLBACK Gui_Window_Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	GUIWIN *win;

	// Attach the context before anything else can arrive, so that even the
	// creation-time messages find their window.
	if (msg == WM_NCCREATE) {
		CREATESTRUCTW *cs = (CREATESTRUCTW*)lp;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	win = GUIWIN_OF(hwnd);
	if (!win) return DefWindowProcW(hwnd, msg, wp, lp);

	switch (msg) {

	case WM_MOUSEMOVE:
		Queue_Mouse(win, EVT_MOVE, lp, 0);
		return 0;

	case WM_LBUTTONDBLCLK:
		Queue_Mouse(win, EVT_DOWN, lp, GUI_FLAG_DOUBLE);
		SetCapture(hwnd);
		return 0;
	case WM_LBUTTONDOWN:
		Queue_Mouse(win, EVT_DOWN, lp, 0);
		SetFocus(hwnd);
		SetCapture(hwnd);
		return 0;
	case WM_LBUTTONUP:
		Queue_Mouse(win, EVT_UP, lp, 0);
		ReleaseCapture();
		return 0;

	case WM_RBUTTONDBLCLK:
		Queue_Mouse(win, EVT_ALT_DOWN, lp, GUI_FLAG_DOUBLE);
		SetCapture(hwnd);
		return 0;
	case WM_RBUTTONDOWN:
		Queue_Mouse(win, EVT_ALT_DOWN, lp, 0);
		SetFocus(hwnd);
		SetCapture(hwnd);
		return 0;
	case WM_RBUTTONUP:
		Queue_Mouse(win, EVT_ALT_UP, lp, 0);
		ReleaseCapture();
		return 0;

	case WM_MBUTTONDBLCLK:
		Queue_Mouse(win, EVT_AUX_DOWN, lp, GUI_FLAG_DOUBLE);
		SetCapture(hwnd);
		return 0;
	case WM_MBUTTONDOWN:
		Queue_Mouse(win, EVT_AUX_DOWN, lp, 0);
		SetFocus(hwnd);
		SetCapture(hwnd);
		return 0;
	case WM_MBUTTONUP:
		Queue_Mouse(win, EVT_AUX_UP, lp, 0);
		ReleaseCapture();
		return 0;

	case WM_MOUSEWHEEL: {
		// Unlike the button messages, this one carries SCREEN coordinates.
		POINT pt;
		UINT  lines = 3;
		REBINT delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;

		SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
		if (lines == 0 || lines > 100) lines = 3; // also covers WHEEL_PAGESCROLL

		pt.x = GET_X_LPARAM(lp);
		pt.y = GET_Y_LPARAM(lp);
		ScreenToClient(hwnd, &pt);

		if (win->hob)
			Gui_Queue_Event(win->hob, EVT_SCROLL_LINE,
			                To_Logical(pt.x), To_Logical(pt.y),
			                delta * (REBINT)lines);
		return 0; }

	case WM_SIZE:
		if (wp != SIZE_MINIMIZED && win->hob)
			Gui_Queue_Event(win->hob, EVT_RESIZE,
			                To_Logical((REBINT)LOWORD(lp)),
			                To_Logical((REBINT)HIWORD(lp)), 0);
		return 0;

	case WM_CLOSE:
		// Only reported - closing is Rebol's decision, and doing it here
		// would destroy a window whose handle is still in use.
		if (win->hob)
			Gui_Queue_Event(win->hob, EVT_CLOSE, 0, 0, 0);
		return 0;

	case WM_DROPFILES: {
		/***************************************************************
		**  Dropped files.
		**
		**  WM_DROPFILES is the simple half of Win32 drag and drop: the
		**  shell has already done the dragging, and this arrives as an
		**  ordinary posted message - so it is dispatched by our own pump
		**  and there is no modal loop to be careful of. Dropped TEXT is
		**  the other half and needs a registered IDropTarget, which is
		**  not implemented: `drop-text` exists all the way through this
		**  extension, but only macOS produces one today.
		**
		**  The paths are copied out here and converted to file! values
		**  later, in `poll-events` - the window procedure must not
		**  allocate a Rebol series, which is the rule the whole event
		**  queue is built around.
		***************************************************************/
		HDROP  hdrop = (HDROP)wp;
		UINT   count = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
		POINT  pt;
		GUIDROPDATA *data;
		REBHOB *target = win->hob;
		HWND    child;
		UINT    n;

		DragQueryPoint(hdrop, &pt);   // client coordinates of this window

		// A drop lands on whatever is under the pointer, so a file dropped
		// on a widget reports the widget - the same rule a click follows.
		child = ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
		if (child && child != hwnd) {
			GUIWIDGET *wid = (GUIWIDGET*)GetWindowLongPtrW(child, GWLP_USERDATA);
			if (wid && wid->hob) target = wid->hob;
		}

		data = Gui_Drop_Payload(GUI_DROP_FILES, count * 160);
		if (data) {
			for (n = 0; n < count; n++) {
				WCHAR  wide[MAX_PATH * 2];
				REBYTE utf8[MAX_PATH * 6];
				REBCNT len;
				int    bytes;

				len = DragQueryFileW(hdrop, n, wide, (UINT)(sizeof(wide) / sizeof(WCHAR)));
				if (!len) continue;
				bytes = WideCharToMultiByte(CP_UTF8, 0, wide, (int)len,
				                            (char*)utf8, (int)sizeof(utf8), NULL, NULL);
				if (bytes > 0) Gui_Drop_Append(data, utf8, (REBCNT)bytes);
			}
			Gui_Queue_Drop(target, data, To_Logical(pt.x), To_Logical(pt.y));
		}

		DragFinish(hdrop);
		return 0; }

	case WM_COMMAND: {
		// Child controls report through their parent, and the child's HWND
		// arrives in lParam - which is why the control does not need an id.
		HWND child = (HWND)lp;
		GUIWIDGET *wid;
		REBCNT type;
		REBINT x = 0, y = 0, w = 0, h = 0;

		// A menu pick and an accelerator arrive here too, and are told
		// apart by having no control behind them: lParam is NULL, and the
		// notification code is 0 for a menu, 1 for an accelerator.
		if (!child && HIWORD(wp) <= 1) {
			Gui_Menu_Picked(win, (REBCNT)LOWORD(wp));
			return 0;
		}

		if (!child) break;

		wid = (GUIWIDGET*)GetWindowLongPtrW(child, GWLP_USERDATA);

		// Notification codes overlap between control families (BN_CLICKED
		// and CBN_ERRSPACE are both 0), so the combo box codes are only
		// read when the control really is one.
		if (wid && wid->kind == W_GUI_WIDGET_DROP_DOWN) {
			switch (HIWORD(wp)) {
			case CBN_SELCHANGE: type = EVT_CHANGE;  break;
			case CBN_SETFOCUS:  type = EVT_FOCUS;   break;
			case CBN_KILLFOCUS: type = EVT_UNFOCUS; break;
			default: goto not_handled;
			}
			if (wid->hob) {
				Gui_Widget_Get_Box(wid, &x, &y, &w, &h);
				Gui_Queue_Event(wid->hob, type, x, y, Modifiers());
			}
			return 0;
		}

		switch (HIWORD(wp)) {
		case BN_CLICKED:   type = EVT_CLICK;   break;
		// SetWindowText raises EN_CHANGE as well, and reporting our own
		// writes back as user edits would turn every `field/text: ...`
		// into an event.
		case EN_CHANGE:    if (Setting_Text) return 0;
		                   type = EVT_CHANGE;  break;
		case EN_SETFOCUS:  type = EVT_FOCUS;   break;
		case EN_KILLFOCUS: type = EVT_UNFOCUS; break;
		default: goto not_handled;
		}

		if (wid && wid->hob) {
			// The position slot carries the widget's own offset - a
			// notification has no cursor position of its own.
			Gui_Widget_Get_Box(wid, &x, &y, &w, &h);
			if (type == EVT_CLICK) {
				// Toggles settle their state before the event goes out.
				Gui_Widget_Activated(wid, x, y, Modifiers());
			} else {
				Gui_Queue_Event(wid->hob, type, x, y, Modifiers());
			}
		}
		return 0;
		not_handled: break; }

	// A trackbar reports through its parent, like the button family does -
	// but on the scroll messages rather than WM_COMMAND. lParam is the
	// control; a zero there is a real scrollbar, which we do not create.
	case WM_HSCROLL:
	case WM_VSCROLL: {
		HWND child = (HWND)lp;
		GUIWIDGET *wid;
		REBINT x = 0, y = 0, w = 0, h = 0;

		if (!child) break;

		wid = (GUIWIDGET*)GetWindowLongPtrW(child, GWLP_USERDATA);
		if (wid && wid->hob && wid->kind == W_GUI_WIDGET_SLIDER) {
			Gui_Widget_Get_Box(wid, &x, &y, &w, &h);
			Gui_Queue_Event(wid->hob, EVT_CHANGE, x, y, Modifiers());
		}
		return 0; }

	// Static labels paint themselves onto whatever the parent supplies;
	// this is what keeps them on the same background WM_PAINT fills with.
	// It is also the only place a control's TEXT colour can be given, so
	// `widget/color` is answered here rather than stored in the control.
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
		return Ctl_Color((HDC)wp, (HWND)lp, win);

	case WM_ERASEBKGND:
		return TRUE; // painted below, without the flicker

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		Fill_Window_Background(dc, &ps.rcPaint, win);
		EndPaint(hwnd, &ps);
		return 0; }

	// What a transparent child asks for. The whole client rect, not a
	// paint rect: the child shifted the origin of its own DC so that this
	// draws the part it covers, and clipping does the rest.
	case WM_PRINTCLIENT: {
		RECT rect;
		GetClientRect(hwnd, &rect);
		Fill_Window_Background((HDC)wp, &rect, win);
		return 0; }

	case WM_NCDESTROY:
		// The window is gone for good - whether we destroyed it or the
		// system did. Drop the queued events which point at the handle and
		// release the lock that kept it alive.
		//
		// The drop target goes FIRST, while the HWND is still valid: it
		// holds the GUIWIN this is about to finish with, and a registration
		// outliving its window is a dangling one.
		Gui_Window_Revoke_Drop(win);
		win->handle = NULL;
		win->flags &= ~GUIW_VISIBLE;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		// DestroyWindow destroys the menu it was given, so the handle is
		// dropped rather than freed - destroying it again would be a
		// double free. The accelerator table is ours and is not.
		win->menu = NULL;
		if (win->accel) {
			DestroyAcceleratorTable((HACCEL)win->accel);
			win->accel = NULL;
		}
		if (win->hob) Gui_Window_Closed(win->hob);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}


//== image widget procedure ===================================================
//
// A class of its own rather than an owner-drawn STATIC: the control paints
// itself straight from the image! series and reports its own mouse events,
// which is what makes it usable as a canvas.


/***********************************************************************
**  The image widget's pixels, into a DC the caller owns - WM_PAINT's and
**  a transparent child's WM_PRINTCLIENT alike.
***********************************************************************/
static void Paint_Image(HWND hwnd, HDC dc, GUIWIDGET *wid)
{
	RECT    rect;
	REBYTE *bits = NULL;
	REBINT  iw = 0, ih = 0;

	GetClientRect(hwnd, &rect);

	if (Gui_Widget_Pixels(wid, &bits, &iw, &ih)) {
		BITMAPINFO bmi;
		int mode;

		// image! is BGRA, which is exactly what a 32-bit BI_RGB DIB
		// is - so the pixels go to the screen untouched. A negative
		// height means the rows are stored top-down.
		ZeroMemory(&bmi, sizeof(bmi));
		bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth       = iw;
		bmi.bmiHeader.biHeight      = -ih;
		bmi.bmiHeader.biPlanes      = 1;
		bmi.bmiHeader.biBitCount    = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		mode = SetStretchBltMode(dc, COLORONCOLOR);
		StretchDIBits(dc,
			0, 0, rect.right, rect.bottom, // destination: the whole widget
			0, 0, iw, ih,                  // source: the whole image
			bits, &bmi, DIB_RGB_COLORS, SRCCOPY);
		SetStretchBltMode(dc, mode);
	} else {
		Fill_Window_Background(dc, &rect, wid ? wid->owner : NULL);
	}
}


static LRESULT CALLBACK Gui_Image_Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	GUIWIDGET *wid;

	if (msg == WM_NCCREATE) {
		CREATESTRUCTW *cs = (CREATESTRUCTW*)lp;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	wid = (GUIWIDGET*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (!wid) return DefWindowProcW(hwnd, msg, wp, lp);

	switch (msg) {

	// An image widget can hold other widgets, so it is a container like a
	// panel and has to get out of their way the same: a control notifies
	// ITS parent, and the window's procedure is the one that knows what to
	// do with a notification.
	case WM_COMMAND:
	case WM_HSCROLL:
	case WM_VSCROLL:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX: {
		HWND parent = GetParent(hwnd);
		if (parent) return SendMessageW(parent, msg, wp, lp);
		break; }

	case WM_ERASEBKGND:
		return TRUE; // WM_PAINT covers every pixel

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		Paint_Image(hwnd, dc, wid);
		EndPaint(hwnd, &ps);
		return 0; }

	// A transparent child of an image widget asks for this, and the answer
	// is the image itself - which is the whole point of letting an image
	// hold widgets: a caption ON the rendered pixels.
	case WM_PRINTCLIENT:
		Paint_Image(hwnd, (HDC)wp, wid);
		return 0;

	case WM_MOUSEMOVE:
		Queue_Widget_Mouse(wid, EVT_MOVE, lp, 0);
		return 0;

	case WM_LBUTTONDBLCLK:
		Queue_Widget_Mouse(wid, EVT_DOWN, lp, GUI_FLAG_DOUBLE);
		SetCapture(hwnd);
		return 0;
	case WM_LBUTTONDOWN:
		Queue_Widget_Mouse(wid, EVT_DOWN, lp, 0);
		SetCapture(hwnd);
		return 0;
	case WM_LBUTTONUP:
		Queue_Widget_Mouse(wid, EVT_UP, lp, 0);
		ReleaseCapture();
		return 0;

	case WM_RBUTTONDOWN:
		Queue_Widget_Mouse(wid, EVT_ALT_DOWN, lp, 0);
		return 0;
	case WM_RBUTTONUP:
		Queue_Widget_Mouse(wid, EVT_ALT_UP, lp, 0);
		return 0;

	case WM_MBUTTONDOWN:
		Queue_Widget_Mouse(wid, EVT_AUX_DOWN, lp, 0);
		return 0;
	case WM_MBUTTONUP:
		Queue_Widget_Mouse(wid, EVT_AUX_UP, lp, 0);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}


// How far in from the left edge a framed panel's caption starts. The same
// number is used on macOS, so the two look alike even though each measures
// the text with its own font.
#define PANEL_CAPTION_X To_Device(9)

/***********************************************************************
**  Everything a panel draws, into a DC the caller owns.
**
**  Split out of WM_PAINT so that WM_PRINTCLIENT can produce exactly the
**  same pixels: a transparent child renders its parent's background into
**  its own DC, and "the panel's background" has to mean the caption and
**  the frame too, not just the fill.
***********************************************************************/
static void Paint_Panel(HWND hwnd, HDC dc)
{
	RECT   rect, frame, gap;
	GUIWIDGET *wid = (GUIWIDGET*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	int    caption_len = GetWindowTextLengthW(hwnd);
	WCHAR *caption = NULL;
	HFONT  font, old_font = NULL;
	SIZE   text_size = {0, 0};
	int    inset = 0;
	HBRUSH bg = NULL;          // a colour of its own, if it was given one
	HBRUSH fill;               // what the background and the caption gap use

	GetClientRect(hwnd, &rect);

	if (wid && GUI_BG_IS_CLEAR(wid->background)) {
		// Transparent: what shows through is whatever holds the panel,
		// which is how a captioned frame can be put over an image.
		Paint_Parent_Background(hwnd, dc);
		fill = NULL;
	} else {
		if (wid && GUI_COLOR_HAS(wid->background)) {
			bg = CreateSolidBrush(RGB(GUI_COLOR_R(wid->background),
			                          GUI_COLOR_G(wid->background),
			                          GUI_COLOR_B(wid->background)));
		} else {
			// Otherwise whatever the WINDOW paints, so a panel is a place
			// to put things rather than a visible slab on it - including
			// on a window with a colour of its own.
			REBOOL   have = FALSE;
			COLORREF rgb  = Window_Fill_Color(wid ? wid->owner : NULL, &have);
			if (have) bg = CreateSolidBrush(rgb);
		}
		fill = bg ? bg : (HBRUSH)(COLOR_WINDOW + 1);
		FillRect(dc, &rect, fill);
	}

	if (!wid || !(wid->state & GUI_PANEL_EDGE)) {
		if (bg) DeleteObject(bg);
		return;
	}

	// The caption is kept as the panel's window text, and drawn with
	// the panel's own font - which is what makes `panel/text`,
	// `panel/font-size` and the rest work through the generic
	// accessors, with no special case above this file.
	font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
	if (!font) font = Get_Default_Font();
	if (font) old_font = (HFONT)SelectObject(dc, font);

	if (caption_len > 0) {
		caption = (WCHAR*)MAKE_MEM((caption_len + 1) * sizeof(WCHAR));
		if (caption) {
			caption_len = GetWindowTextW(hwnd, caption, caption_len + 1);
			// Without the extent there is no gap to leave and no line
			// to break, so an unmeasurable caption is simply not drawn
			// rather than drawn over the frame.
			if (!GetTextExtentPoint32W(dc, caption, caption_len, &text_size)
			    || text_size.cx <= 0) {
				FREE_MEM(caption);
				caption = NULL;
			} else {
				inset = text_size.cy / 2;
			}
		}
	}

	// The frame starts halfway down the caption, so the text sits ON the
	// line - a classic group box - and the whole thing stays inside the
	// panel's box, which is why no child ever has to move for it.
	frame = rect;
	frame.top += inset;
	DrawEdge(dc, &frame, EDGE_ETCHED, BF_RECT);

	if (caption) {
		// The line is broken by painting the background back over the
		// span the caption occupies. The panel owns that colour - it
		// filled the whole client area with it above - so this is exact
		// rather than a guess at what shows through.
		gap.left   = PANEL_CAPTION_X - To_Device(2);
		gap.top    = rect.top;
		gap.right  = gap.left + text_size.cx + To_Device(4);
		gap.bottom = rect.top + text_size.cy;
		if (gap.right > rect.right) gap.right = rect.right;
		// A transparent panel has no colour to paint the line out with,
		// so the caption is drawn over an unbroken frame instead. The
		// alternative would be re-fetching the parent for one strip,
		// which is a lot of work to hide four pixels of etching.
		if (fill) FillRect(dc, &gap, fill);

		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, GUI_COLOR_HAS(wid->color)
			? RGB(GUI_COLOR_R(wid->color),
			      GUI_COLOR_G(wid->color),
			      GUI_COLOR_B(wid->color))
			: GetSysColor(COLOR_WINDOWTEXT));
		TextOutW(dc, PANEL_CAPTION_X, rect.top, caption, caption_len);
		FREE_MEM(caption);
	}

	if (old_font) SelectObject(dc, old_font);
	if (bg) DeleteObject(bg);
}


//== panel procedure ==========================================================
//
// A container, and nothing more. The one thing it must do is get out of the
// way: a control reports to ITS parent, so everything a panel holds would
// notify the panel instead of the window, and the window's proc - which is
// where all the reporting lives - would never hear about it.
//
// So the notifications are passed straight up. The handlers there identify
// the control from lParam rather than from the window that received the
// message, so forwarding is all it takes.

static LRESULT CALLBACK Gui_Panel_Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {

	case WM_COMMAND:
	case WM_HSCROLL:
	case WM_VSCROLL:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX: {
		HWND parent = GetParent(hwnd);
		if (parent) return SendMessageW(parent, msg, wp, lp);
		break; }

	// A custom window class gets neither of these for free: DefWindowProc
	// stores no font, so a panel keeps its own in the class's extra word.
	// Without this, `panel/font-size` would be written and then read back
	// as whatever the shell's message font is.
	case WM_SETFONT:
		SetWindowLongPtrW(hwnd, 0, (LONG_PTR)wp);
		if (LOWORD(lp)) InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETFONT:
		return (LRESULT)GetWindowLongPtrW(hwnd, 0);

	case WM_ERASEBKGND:
		return TRUE; // WM_PAINT covers it

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		Paint_Panel(hwnd, dc);
		EndPaint(hwnd, &ps);
		return 0; }

	// What a transparent child asks for: the same drawing, into the DC it
	// hands over, so that what shows through the child is what would have
	// been under it. The caption and the frame are included, which is why
	// this shares Paint_Panel() rather than just filling.
	case WM_PRINTCLIENT:
		Paint_Panel(hwnd, (HDC)wp);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}


//== class registration =======================================================

static REBOOL Register_Panel_Class(void)
{
	WNDCLASSEXW wc;

	if (Panel_Class_Registered) return TRUE;

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = Gui_Panel_Proc;
	// One pointer of storage per panel, holding the HFONT it was given -
	// see WM_SETFONT in the procedure above.
	wc.cbWndExtra    = sizeof(LONG_PTR);
	wc.hInstance     = App_Instance;
	wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszClassName = Class_Name_Panel;

	if (!RegisterClassExW(&wc)) return FALSE;

	Panel_Class_Registered = TRUE;
	return TRUE;
}


static REBOOL Register_Image_Class(void)
{
	WNDCLASSEXW wc;

	if (Image_Class_Registered) return TRUE;

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc   = Gui_Image_Proc;
	wc.hInstance     = App_Instance;
	wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = NULL; // WM_PAINT does it
	wc.lpszClassName = Class_Name_Image;

	if (!RegisterClassExW(&wc)) return FALSE;

	Image_Class_Registered = TRUE;
	return TRUE;
}


static REBOOL Register_Class(void)
{
	WNDCLASSEXW wc;

	if (Class_Registered) return TRUE;

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc   = Gui_Window_Proc;
	wc.hInstance     = App_Instance;
	wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = NULL; // WM_PAINT does it
	wc.lpszClassName = Class_Name;
	wc.hIcon         = LoadIconW(App_Instance, MAKEINTRESOURCEW(101));

	if (!RegisterClassExW(&wc)) return FALSE;

	Class_Registered = TRUE;
	return TRUE;
}


//== platform API =============================================================

typedef HRESULT (WINAPI *SETPROCESSDPIAWARENESS_T)(int);
typedef BOOL    (WINAPI *SETPROCESSDPIAWARE_T)(void);

/***********************************************************************
**  One-time process setup.
**
**  NOTE: DPI awareness is a PROCESS wide setting, so importing this
**  module changes how the whole interpreter is scaled. It is done here
**  because it has to happen before the first window exists, and doing it
**  late is worse than doing it visibly.
***********************************************************************/
void Gui_Init_Platform(void)
{
	HMODULE shcore, user32;
	INITCOMMONCONTROLSEX controls;

	if (App_Instance == NULL) App_Instance = GetModuleHandleW(NULL);

	// The trackbar and the progress bar live in comctl32 and their classes
	// have to be registered before either can be created.
	controls.dwSize = sizeof(controls);
	controls.dwICC  = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&controls);

	/*******************************************************************
	**  OLE, for drag and drop.
	**
	**  RegisterDragDrop needs an initialised single-threaded apartment
	**  on the thread which owns the window. The interpreter may already
	**  have COM up: S_FALSE means it was already initialised compatibly
	**  and is not ours to shut down, and RPC_E_CHANGED_MODE means it is
	**  up as multi-threaded, where drag and drop cannot be registered at
	**  all - so drops fall back to WM_DROPFILES rather than failing the
	**  window.
	*******************************************************************/
	{
		HRESULT hr = OleInitialize(NULL);
		Ole_Ready = (hr == S_OK || hr == S_FALSE) ? TRUE : FALSE;
		Ole_Ours  = (hr == S_OK) ? TRUE : FALSE;
		if (!Ole_Ready) {
			// Said out loud rather than degraded quietly: a window which
			// then takes drops from Explorer and from nothing else looks
			// like a bug in the drop code, which is where the time goes.
			if (hr == RPC_E_CHANGED_MODE) {
				printf("GUI: COM is already initialised on this thread as "
				       "MULTI-THREADED, so OLE drag and drop cannot be "
				       "registered.\n"
				       "GUI: Drops fall back to WM_DROPFILES, which only "
				       "Explorer sends. The host should start COM on its GUI "
				       "thread with COINIT_APARTMENTTHREADED.\n");
			} else {
				printf("GUI: OleInitialize failed (0x%08lx) - drops fall back "
				       "to WM_DROPFILES, which only Explorer sends\n",
				       (unsigned long)hr);
			}
			fflush(stdout);
		}
	}

	shcore = LoadLibraryW(L"shcore.dll");
	if (shcore) {
		SETPROCESSDPIAWARENESS_T fn =
			(SETPROCESSDPIAWARENESS_T)GetProcAddress(shcore, "SetProcessDpiAwareness");
		if (fn) {
			fn(1); // PROCESS_SYSTEM_DPI_AWARE
			FreeLibrary(shcore);
			Read_Screen_DPI(); // AFTER declaring awareness - before it, the
			return;            // system reports a polite 96 whatever it is
		}
		FreeLibrary(shcore);
	}
	user32 = LoadLibraryW(L"user32.dll");
	if (user32) {
		SETPROCESSDPIAWARE_T fn =
			(SETPROCESSDPIAWARE_T)GetProcAddress(user32, "SetProcessDPIAware");
		if (fn) fn();
		FreeLibrary(user32);
	}
	Read_Screen_DPI();
}


void Gui_Quit_Platform(void)
{
	// Windows still open at this point belong to a process which is going
	// away; the system reclaims them. Only the class needs unregistering,
	// and only so that a reloaded extension can register it again.
	if (Class_Registered) {
		UnregisterClassW(Class_Name, App_Instance);
		Class_Registered = FALSE;
	}
	if (Image_Class_Registered) {
		UnregisterClassW(Class_Name_Image, App_Instance);
		Image_Class_Registered = FALSE;
	}
	// Only if it was ours - the interpreter's own COM is not to be shut
	// down by an extension being unloaded.
	if (Ole_Ours) {
		OleUninitialize();
		Ole_Ours = Ole_Ready = FALSE;
	}
	if (Panel_Class_Registered) {
		UnregisterClassW(Class_Name_Panel, App_Instance);
		Panel_Class_Registered = FALSE;
	}
	Free_Font_Cache(); // every HFONT made for a `font` or `font-size`
	if (Default_Font && Default_Font_Owned) {
		DeleteObject(Default_Font); // a stock object must not be deleted
		Default_Font_Owned = FALSE;
	}
	Default_Font = NULL;
}


REBOOL Gui_Open_Window(GUIWIN *win, REBINT x, REBINT y, REBINT w, REBINT h,
                       const REBYTE *title, REBCNT title_len, REBCNT flags)
{
	HWND  hwnd;
	RECT  rect;
	WCHAR *wide;
	DWORD style   = WINDOW_STYLE;
	DWORD exstyle = WINDOW_EXSTYLE;

	if (!Register_Class()) return FALSE;

	// See-through: the client area is filled with a colour the compositor
	// drops, leaving the controls on it. See GUI_KEY_COLOR.
	if (flags & GUI_WIN_TRANSPARENT) exstyle |= WS_EX_LAYERED;

	// A borderless window is WS_POPUP: no caption and no frame, so the
	// resize bits would have nothing to attach to either.
	if (flags & GUI_WIN_BORDERLESS) {
		style = (style & ~(WINDOW_BORDER_BITS | WINDOW_RESIZE_BITS)) | WS_POPUP;
	} else if (flags & GUI_WIN_FIXED) {
		style &= ~WINDOW_RESIZE_BITS;
	}

	// Logical in, device out - see the note on Gui_DPI. The sentinel is
	// not a coordinate and must not be scaled.
	if (x != GUI_DEFAULT_POS) x = To_Device(x);
	if (y != GUI_DEFAULT_POS) y = To_Device(y);
	w = To_Device(w);
	h = To_Device(h);

	// The requested size is the CLIENT size - grow it by the frame.
	rect.left = 0; rect.top = 0; rect.right = w; rect.bottom = h;
	AdjustWindowRectEx(&rect, style, FALSE, exstyle);

	// A missing - or empty - title gets a neutral default rather than an
	// empty title bar.
	wide = To_Wide(title, title_len);

	hwnd = CreateWindowExW(
		exstyle,
		Class_Name,
		wide ? wide : L"Rebol",
		style,
		(x == GUI_DEFAULT_POS) ? CW_USEDEFAULT : x,
		(y == GUI_DEFAULT_POS) ? CW_USEDEFAULT : y,
		rect.right - rect.left,
		rect.bottom - rect.top,
		NULL, NULL, App_Instance,
		win // arrives as lpCreateParams in WM_NCCREATE
	);

	if (wide) FREE_MEM(wide);
	if (!hwnd) return FALSE;

	win->handle = (void*)hwnd;
	win->flags  = 0;

	// The three states live in one field, so /transparent is recorded as
	// the same value `win/transparent?: true` would write - and applying
	// it is the same call, rather than a second path to keep in step.
	if (flags & GUI_WIN_TRANSPARENT) {
		win->background = GUI_BG_CLEAR;
		Gui_Window_Set_Background(win);
	}
	return TRUE;
}


/***********************************************************************
**  win->background, applied.
***********************************************************************/

//== drag and drop ============================================================
//
// Two protocols, and this implements the real one.
//
// DragAcceptFiles() only sets WS_EX_ACCEPTFILES, and WM_DROPFILES is then a
// COURTESY OF THE DRAG SOURCE: an application dragging files is expected to
// notice that style and post the message itself, with a DROPFILES structure
// in shared memory. Explorer still does, for compatibility going back to
// Windows 3.1. Anything written against OLE drag and drop - which is the
// documented way since Win32, and includes Total Commander, most archivers
// and every browser - calls DoDragDrop and talks only to an IDropTarget
// registered with RegisterDragDrop. With no such target the drop is simply
// refused and no message is sent, which is why the legacy path looked like
// it worked: it was being tested from the one source which still supports it.
//
// So a real IDropTarget is registered per window. It also gets three things
// the legacy protocol cannot express at all: CF_UNICODETEXT (so `drop-text`
// is not macOS-only), the drag-OVER feedback which tells the user whether a
// drop will be taken, and the position DURING the drag rather than after it.
//
// WM_DROPFILES is kept as a fallback: if OLE cannot be initialised on this
// thread the window falls back to DragAcceptFiles, and Explorer still works.

typedef struct Gui_Drop_Target {
	IDropTarget iface;   // FIRST: an IDropTarget* is a GUIDROPTARGET*
	LONG        refs;
	GUIWIN     *win;
	DWORD       effect;  // what the current drag would do; 0 when we take nothing
} GUIDROPTARGET;


/***********************************************************************
**  Does this data object carry something we take?
**
**  Files first: a drop which has both is a file drop, because that is
**  what the user thinks they are dragging.
***********************************************************************/
static REBCNT Drop_Format_Of(IDataObject *obj)
{
	FORMATETC fmt;

	fmt.ptd      = NULL;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex   = -1;
	fmt.tymed    = TYMED_HGLOBAL;

	fmt.cfFormat = CF_HDROP;
	if (IDataObject_QueryGetData(obj, &fmt) == S_OK) return GUI_DROP_FILES;

	fmt.cfFormat = CF_UNICODETEXT;
	if (IDataObject_QueryGetData(obj, &fmt) == S_OK) return GUI_DROP_TEXT;

	return 0;
}


// One UTF-16 string into the payload, as UTF-8.
static void Drop_Append_Wide(GUIDROPDATA *data, const WCHAR *wide, int len)
{
	int     bytes;
	REBYTE *utf8;

	if (len <= 0) return;
	bytes = WideCharToMultiByte(CP_UTF8, 0, wide, len, NULL, 0, NULL, NULL);
	if (bytes <= 0) return;

	utf8 = (REBYTE*)MAKE_MEM((size_t)bytes);
	if (!utf8) return;
	WideCharToMultiByte(CP_UTF8, 0, wide, len, (char*)utf8, bytes, NULL, NULL);
	Gui_Drop_Append(data, utf8, (REBCNT)bytes);
	FREE_MEM(utf8);
}


/***********************************************************************
**  Reads the content out of the data object into a payload.
**
**  Returns NULL when there is nothing to take, and the caller then
**  reports DROPEFFECT_NONE rather than pretending the drop succeeded.
***********************************************************************/
static GUIDROPDATA *Drop_Payload_Of(IDataObject *obj, REBCNT kind)
{
	FORMATETC    fmt;
	STGMEDIUM    med;
	GUIDROPDATA *data = NULL;

	fmt.ptd      = NULL;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex   = -1;
	fmt.tymed    = TYMED_HGLOBAL;
	fmt.cfFormat = (kind == GUI_DROP_TEXT) ? CF_UNICODETEXT : CF_HDROP;

	if (IDataObject_GetData(obj, &fmt, &med) != S_OK) return NULL;

	if (kind == GUI_DROP_TEXT) {
		const WCHAR *text = (const WCHAR*)GlobalLock(med.hGlobal);
		if (text) {
			data = Gui_Drop_Payload(GUI_DROP_TEXT, 0);
			if (data) Drop_Append_Wide(data, text, (int)wcslen(text));
			GlobalUnlock(med.hGlobal);
		}
	} else {
		HDROP hdrop = (HDROP)GlobalLock(med.hGlobal);
		if (hdrop) {
			UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
			UINT n;
			data = Gui_Drop_Payload(GUI_DROP_FILES, count * 160);
			if (data) {
				for (n = 0; n < count; n++) {
					WCHAR wide[MAX_PATH * 2];
					UINT  len = DragQueryFileW(hdrop, n, wide,
					                           (UINT)(sizeof(wide) / sizeof(WCHAR)));
					Drop_Append_Wide(data, wide, (int)len);
				}
			}
			GlobalUnlock(med.hGlobal);
		}
	}

	ReleaseStgMedium(&med);
	return data;
}


// The handle a drop on this window should be reported against: the widget
// under the pointer, or the window itself. `pt` is in SCREEN coordinates,
// which is what IDropTarget is given.
static REBHOB *Drop_Target_At(GUIWIN *win, POINTL pt, POINT *client)
{
	HWND  hwnd = HWND_OF(win);
	HWND  child;
	POINT p;

	p.x = pt.x;
	p.y = pt.y;
	ScreenToClient(hwnd, &p);
	*client = p;

	child = ChildWindowFromPointEx(hwnd, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
	if (child && child != hwnd) {
		GUIWIDGET *wid = (GUIWIDGET*)GetWindowLongPtrW(child, GWLP_USERDATA);
		if (wid && wid->hob) return wid->hob;
	}
	return win->hob;
}


//-- IUnknown -----------------------------------------------------------------

static HRESULT STDMETHODCALLTYPE Drop_QueryInterface(IDropTarget *self,
                                                     REFIID riid, void **out)
{
	if (!out) return E_POINTER;
	if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
		*out = self;
		IDropTarget_AddRef(self);
		return S_OK;
	}
	*out = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Drop_AddRef(IDropTarget *self)
{
	return (ULONG)InterlockedIncrement(&((GUIDROPTARGET*)self)->refs);
}

static ULONG STDMETHODCALLTYPE Drop_Release(IDropTarget *self)
{
	GUIDROPTARGET *t = (GUIDROPTARGET*)self;
	LONG refs = InterlockedDecrement(&t->refs);
	if (refs == 0) FREE_MEM(t);
	return (ULONG)refs;
}

//-- IDropTarget --------------------------------------------------------------

static HRESULT STDMETHODCALLTYPE Drop_DragEnter(IDropTarget *self,
                                                IDataObject *obj, DWORD keys,
                                                POINTL pt, DWORD *effect)
{
	GUIDROPTARGET *t = (GUIDROPTARGET*)self;

	// Decided once per drag and remembered: DragOver runs on every mouse
	// move, and asking the data object each time is needless traffic across
	// the process boundary.
	t->effect = (obj && Drop_Format_Of(obj)) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
	if (effect) *effect = t->effect;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE Drop_DragOver(IDropTarget *self, DWORD keys,
                                               POINTL pt, DWORD *effect)
{
	if (effect) *effect = ((GUIDROPTARGET*)self)->effect;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE Drop_DragLeave(IDropTarget *self)
{
	((GUIDROPTARGET*)self)->effect = DROPEFFECT_NONE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE Drop_Drop(IDropTarget *self, IDataObject *obj,
                                           DWORD keys, POINTL pt, DWORD *effect)
{
	GUIDROPTARGET *t = (GUIDROPTARGET*)self;
	GUIDROPDATA   *data;
	REBCNT         kind;
	REBHOB        *target;
	POINT          client;

	t->effect = DROPEFFECT_NONE;
	if (effect) *effect = DROPEFFECT_NONE;

	if (!obj || !t->win || !t->win->hob) return S_OK;
	if (!(kind = Drop_Format_Of(obj))) return S_OK;
	if (!(data = Drop_Payload_Of(obj, kind))) return S_OK;

	target = Drop_Target_At(t->win, pt, &client);
	Gui_Queue_Drop(target, data, To_Logical(client.x), To_Logical(client.y));

	if (effect) *effect = DROPEFFECT_COPY;
	return S_OK;
}

static IDropTargetVtbl Drop_Vtbl = {
	Drop_QueryInterface,
	Drop_AddRef,
	Drop_Release,
	Drop_DragEnter,
	Drop_DragOver,
	Drop_DragLeave,
	Drop_Drop
};

/***********************************************************************
**  Whether the window accepts drops.
**
**  The OLE path is the real one - see the note above the drop target.
**  DragAcceptFiles stays as a fallback for the case where OLE could not
**  be initialised on this thread: Explorer still works then, and a
**  window is never left looking as though it accepts drops when it
**  cannot, because both paths are turned on and off together.
**
**  RegisterDragDrop takes its own reference, so the one this function
**  creates is released here and the target is freed when the OS lets go
**  of it - which RevokeDragDrop is what triggers.
***********************************************************************/
void Gui_Window_Set_Drop(GUIWIN *win, REBOOL accept)
{
	if (!win) return;

	if (accept) {
		if (win->handle && Ole_Ready && !win->droptarget) {
			GUIDROPTARGET *t = (GUIDROPTARGET*)MAKE_CLEAR_MEM(sizeof(GUIDROPTARGET));
			if (t) {
				HRESULT hr;
				t->iface.lpVtbl = &Drop_Vtbl;
				t->refs = 1;
				t->win  = win;
				hr = RegisterDragDrop(HWND_OF(win), &t->iface);
				if (hr == S_OK) {
					win->droptarget = t;
				} else {
					printf("GUI: RegisterDragDrop failed (0x%08lx)\n",
					       (unsigned long)hr);
					fflush(stdout);
					IDropTarget_Release(&t->iface);
				}
			}
		}
		// Belt and braces: a source which only speaks the legacy protocol
		// still finds the style, whether or not the OLE target is up.
		if (win->handle) DragAcceptFiles(HWND_OF(win), TRUE);
		win->flags |= GUIW_ACCEPTS_DROP;
	}
	else {
		Gui_Window_Revoke_Drop(win);
		if (win->handle) DragAcceptFiles(HWND_OF(win), FALSE);
		win->flags &= ~GUIW_ACCEPTS_DROP;
	}
}


/***********************************************************************
**  Takes the drop target off a window, without changing what the
**  window says it accepts.
**
**  Separate because closing has to do it too: a registered target
**  outliving its HWND is a dangling registration, and the target holds
**  a GUIWIN pointer which is about to be freed.
***********************************************************************/
static void Gui_Window_Revoke_Drop(GUIWIN *win)
{
	GUIDROPTARGET *t;

	if (!win || !(t = (GUIDROPTARGET*)win->droptarget)) return;
	win->droptarget = NULL;

	if (win->handle) RevokeDragDrop(HWND_OF(win));
	IDropTarget_Release(&t->iface);
}


void Gui_Window_Set_Background(GUIWIN *win)
{
	HWND  hwnd;
	DWORD exstyle;
	REBOOL clear;

	if (!win || !win->handle) return;
	hwnd    = HWND_OF(win);
	clear   = GUI_BG_IS_CLEAR(win->background);
	exstyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

	if (clear) {
		if (!(exstyle & WS_EX_LAYERED))
			SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
			                  (LONG_PTR)(exstyle | WS_EX_LAYERED));
		// Only the key is dropped; alpha is left at fully opaque, so the
		// controls are not dimmed along with it.
		SetLayeredWindowAttributes(hwnd, GUI_KEY_COLOR, 255, LWA_COLORKEY);
	} else if (exstyle & WS_EX_LAYERED) {
		// Taking the style away is what makes the window solid again -
		// clearing the key alone would leave a layered window, which is
		// composited differently and needlessly.
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
		                  (LONG_PTR)(exstyle & ~WS_EX_LAYERED));
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
			| SWP_FRAMECHANGED);
	}

	// Every control on it may be showing this colour, and WS_CLIPCHILDREN
	// keeps a plain invalidation from reaching them.
	RedrawWindow(hwnd, NULL, NULL,
	             RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}


void Gui_Close_Window(GUIWIN *win)
{
	// DestroyWindow sends WM_NCDESTROY, and that is where win->handle is
	// cleared and the handle context unlocked - one path for every way a
	// window can disappear.
	if (win && win->handle) DestroyWindow(HWND_OF(win));
}


void Gui_Show_Window(GUIWIN *win, REBOOL show)
{
	if (!win || !win->handle) return;
	ShowWindow(HWND_OF(win), show ? SW_SHOWNORMAL : SW_HIDE);
	if (show) {
		// RDW_ALLCHILDREN, and not a plain UpdateWindow: the controls were
		// invalidated as they were created and are waiting for the pump,
		// so a window shown after its layout was built has to paint them
		// along with itself. That is what makes
		//
		//     win: open-window/hidden ... ; add-* ... ; show-window win
		//
		// appear complete in one go instead of a frame at a time.
		RedrawWindow(HWND_OF(win), NULL, NULL,
		             RDW_INVALIDATE | RDW_ERASE | RDW_FRAME
		             | RDW_ALLCHILDREN | RDW_UPDATENOW);
		SetForegroundWindow(HWND_OF(win));
		win->flags |= GUIW_VISIBLE;
	} else {
		win->flags &= ~GUIW_VISIBLE;
	}
}


/***********************************************************************
**  Drains the thread's message queue.
**
**  NOTE: this takes messages for EVERY window of the calling thread, not
**  only ours. That is what makes a plain `poll-events` loop work without
**  a host side event device - but it also means two extensions pumping
**  the same thread would steal each other's messages.
***********************************************************************/
// The GUIWIN behind a top-level window, or NULL for a window which is not
// one of ours. The class name is the test: GWLP_USERDATA on a window this
// extension did not create holds whatever its owner put there, which is
// not a GUIWIN to be dereferenced.
static GUIWIN* Our_Window(HWND hwnd)
{
	WCHAR cls[64];

	if (!hwnd) return NULL;
	if (!GetClassNameW(hwnd, cls, 64)) return NULL;
	if (lstrcmpW(cls, Class_Name) != 0) return NULL;
	return GUIWIN_OF(hwnd);
}


REBCNT Gui_Pump(void)
{
	MSG msg;
	REBCNT dispatched = 0;

	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		dispatched++;

		// A keyboard shortcut is not a property of a menu item on Win32 -
		// it is an entry in an accelerator table which SOMETHING has to
		// translate before the keystroke is dispatched, and this is the
		// only loop this extension owns. The message may have been aimed
		// at a control, so the window it belongs to is the root above it.
		//
		// Only for keyboard messages. Everything below is two USER32 calls
		// per message, and a themed control produces a great many messages
		// - animation timers, mouse tracking, buffered paint - none of
		// which can possibly be a shortcut.
		if (msg.message == WM_KEYDOWN    || msg.message == WM_SYSKEYDOWN
		 || msg.message == WM_KEYUP      || msg.message == WM_SYSKEYUP
		 || msg.message == WM_CHAR       || msg.message == WM_SYSCHAR) {
			GUIWIN *win = Our_Window(GetAncestor(msg.hwnd, GA_ROOT));
			if (win && win->accel && win->handle
			    && TranslateAcceleratorW(HWND_OF(win), (HACCEL)win->accel, &msg))
				continue;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return dispatched;
}


REBOOL Gui_Get_Size(GUIWIN *win, REBINT *w, REBINT *h)
{
	RECT r;
	if (!win || !win->handle || !GetClientRect(HWND_OF(win), &r)) return FALSE;
	*w = To_Logical(r.right - r.left);
	*h = To_Logical(r.bottom - r.top);
	return TRUE;
}


REBOOL Gui_Get_Offset(GUIWIN *win, REBINT *x, REBINT *y)
{
	RECT r;
	if (!win || !win->handle || !GetWindowRect(HWND_OF(win), &r)) return FALSE;
	*x = To_Logical(r.left);
	*y = To_Logical(r.top);
	return TRUE;
}


REBOOL Gui_Set_Size(GUIWIN *win, REBINT w, REBINT h)
{
	RECT r;
	if (!win || !win->handle) return FALSE;

	r.left = 0; r.top = 0; r.right = To_Device(w); r.bottom = To_Device(h);
	AdjustWindowRectEx(&r, (DWORD)GetWindowLongPtrW(HWND_OF(win), GWL_STYLE),
	                   FALSE, (DWORD)GetWindowLongPtrW(HWND_OF(win), GWL_EXSTYLE));

	return SetWindowPos(HWND_OF(win), NULL, 0, 0,
	                    r.right - r.left, r.bottom - r.top,
	                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) ? TRUE : FALSE;
}


REBOOL Gui_Set_Offset(GUIWIN *win, REBINT x, REBINT y)
{
	if (!win || !win->handle) return FALSE;
	return SetWindowPos(HWND_OF(win), NULL, To_Device(x), To_Device(y), 0, 0,
	                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) ? TRUE : FALSE;
}


// The client size before the frame changes - a menu bar appearing, a
// border going away - so that it can be given straight back.
static REBOOL Client_Size_Of(HWND hwnd, int *w, int *h)
{
	RECT r;
	if (!GetClientRect(hwnd, &r)) return FALSE;
	*w = r.right - r.left;
	*h = r.bottom - r.top;
	return TRUE;
}

/***********************************************************************
**  Puts back whatever the frame took.
**
**  Neither SetMenu() nor a style change resizes a window - they
**  re-split it, so a menu bar appears, or a border grows, by taking the
**  room out of the CLIENT area. Everything a caller laid out is
**  positioned in that area, so the window is grown by exactly what was
**  lost and the layout does not move.
**
**  Measured rather than computed with AdjustWindowRect: a menu bar can
**  wrap onto two rows, and the measurement is right either way.
***********************************************************************/
static void Keep_Client_Size(HWND hwnd, int was_w, int was_h)
{
	RECT r;
	int now_w, now_h;

	if (!Client_Size_Of(hwnd, &now_w, &now_h)) return;
	if ((now_w == was_w && now_h == was_h) || !GetWindowRect(hwnd, &r)) return;

	SetWindowPos(hwnd, NULL, 0, 0,
		(r.right  - r.left) + (was_w - now_w),
		(r.bottom - r.top)  + (was_h - now_h),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}


/***********************************************************************
**  The frame.
**
**  Read from the window rather than remembered, so what is reported is
**  what the window has - including a style someone else changed.
**
**  Changing it re-splits the window the way a menu bar does: the frame
**  grows or shrinks and the CLIENT area gives up or gains the
**  difference. Since everything in the window is laid out in that area,
**  the window is resized by what was lost, measured rather than
**  computed - Keep_Client_Size() again.
***********************************************************************/
static REBOOL Set_Window_Style_Bits(GUIWIN *win, DWORD off, DWORD on)
{
	HWND  hwnd;
	DWORD style, next;
	int   w, h;

	if (!win || !win->handle) return FALSE;
	hwnd = HWND_OF(win);

	style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
	next  = (style & ~off) | on;
	if (next == style) return TRUE;

	if (!Client_Size_Of(hwnd, &w, &h)) { w = h = 0; }

	SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)next);
	// SWP_FRAMECHANGED - without it the new style is stored but the frame
	// on screen is still the old one until something else recalculates it.
	SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
		| SWP_FRAMECHANGED);

	if (w && h) Keep_Client_Size(hwnd, w, h);
	return TRUE;
}


REBOOL Gui_Get_Resizable(GUIWIN *win)
{
	if (!win || !win->handle) return FALSE;
	return (GetWindowLongPtrW(HWND_OF(win), GWL_STYLE) & WS_THICKFRAME)
		? TRUE : FALSE;
}


REBOOL Gui_Set_Resizable(GUIWIN *win, REBOOL on)
{
	// A borderless window has no frame to grab, so this is only about the
	// bits; turning it on there does nothing visible until a border is.
	return on
		? Set_Window_Style_Bits(win, 0, WINDOW_RESIZE_BITS)
		: Set_Window_Style_Bits(win, WINDOW_RESIZE_BITS, 0);
}


REBOOL Gui_Get_Border(GUIWIN *win)
{
	if (!win || !win->handle) return FALSE;
	return (GetWindowLongPtrW(HWND_OF(win), GWL_STYLE) & WS_CAPTION)
		? TRUE : FALSE;
}


REBOOL Gui_Set_Border(GUIWIN *win, REBOOL on)
{
	if (!win) return FALSE;

	if (on) {
		// The resize bits are not restored here: whether the window can be
		// resized is its own property, and one it may never have had.
		return Set_Window_Style_Bits(win, WS_POPUP, WINDOW_BORDER_BITS);
	}
	return Set_Window_Style_Bits(win,
		WINDOW_BORDER_BITS | WINDOW_RESIZE_BITS, WS_POPUP);
}


REBSER* Gui_Get_Title(GUIWIN *win)
{
	if (!win || !win->handle) return NULL;
	return Text_Of(HWND_OF(win));
}


REBOOL Gui_Set_Title(GUIWIN *win, const REBYTE *utf8, REBCNT len)
{
	if (!win || !win->handle) return FALSE;
	return Set_Text_Of(HWND_OF(win), utf8, len);
}


//== menu bar =================================================================
//
// A Win32 menu belongs to the window, which is the easy half. The awkward
// halves are that a menu bar EATS CLIENT AREA - so a window given one would
// silently shrink under everything already laid out in it - and that a
// shortcut is not a property of a menu item at all, but an entry in a
// separate accelerator table which the message loop has to translate.

// Accelerators collected between Gui_Menu_Begin() and Gui_Menu_End(). One
// menu is built at a time, from a single thread, so a static is enough and
// nothing has to be grown per window.
#define GUI_MAX_ACCEL 128
static ACCEL Accel_Build[GUI_MAX_ACCEL];
static int   Accel_Count = 0;


REBOOL Gui_Menu_Begin(GUIWIN *win)
{
	if (!win || !win->handle) return FALSE;

	Accel_Count = 0;
	win->menu = (void*)CreateMenu();
	return win->menu != NULL;
}


void* Gui_Menu_Add_Popup(GUIWIN *win, void *parent,
                         const REBYTE *label, REBCNT len)
{
	HMENU  popup;
	WCHAR *wide;

	if (!win || !win->menu) return NULL;

	popup = CreatePopupMenu();
	if (!popup) return NULL;

	wide = To_Wide(label, len);
	AppendMenuW(parent ? (HMENU)parent : (HMENU)win->menu,
	            MF_STRING | MF_POPUP, (UINT_PTR)popup, wide ? wide : L"");
	if (wide) FREE_MEM(wide);

	return (void*)popup;
}


// "Ctrl+Shift+S", appended after a tab so that the menu right-aligns it.
// Windows does not read the accelerator table to label an item - the text
// is just text, and keeping the two in step is the caller's job, which
// here means this function's.
static void Append_Accel_Text(WCHAR *dst, size_t max, REBCNT key, REBCNT mods)
{
	WCHAR tail[40];
	int   n;

	lstrcpyW(tail, L"\tCtrl+");
	if (mods & GUI_FLAG_SHIFT) lstrcatW(tail, L"Shift+");
	if (mods & GUI_FLAG_ALT)   lstrcatW(tail, L"Alt+");
	n = lstrlenW(tail);
	tail[n++] = (WCHAR)((key >= 'a' && key <= 'z') ? key - 32 : key);
	tail[n] = 0;

	if ((size_t)(lstrlenW(dst) + lstrlenW(tail)) < max) lstrcatW(dst, tail);
}


void Gui_Menu_Add_Item(GUIWIN *win, void *parent,
                       const REBYTE *label, REBCNT len, REBCNT item_id,
                       REBCNT key, REBCNT mods)
{
	WCHAR  text[256];
	WCHAR *wide;

	if (!win || !win->menu) return;

	wide = To_Wide(label, len);
	lstrcpynW(text, wide ? wide : L"", 256);
	if (wide) FREE_MEM(wide);

	if (key) {
		Append_Accel_Text(text, 256, key, mods);

		if (Accel_Count < GUI_MAX_ACCEL) {
			ACCEL *a = &Accel_Build[Accel_Count++];
			a->fVirt = FVIRTKEY | FCONTROL;
			if (mods & GUI_FLAG_SHIFT) a->fVirt |= FSHIFT;
			if (mods & GUI_FLAG_ALT)   a->fVirt |= FALT;
			// A letter or a digit IS its own virtual key; anything else is
			// asked of the current keyboard layout.
			if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) {
				a->key = (WORD)key;
			} else if (key >= 'a' && key <= 'z') {
				a->key = (WORD)(key - 32);
			} else {
				a->key = (WORD)(VkKeyScanW((WCHAR)key) & 0xFF);
			}
			a->cmd = (WORD)item_id;
		}
	}

	AppendMenuW(parent ? (HMENU)parent : (HMENU)win->menu,
	            MF_STRING, (UINT_PTR)item_id, text);
}


void Gui_Menu_Add_Separator(GUIWIN *win, void *parent)
{
	if (!win || !win->menu) return;
	AppendMenuW(parent ? (HMENU)parent : (HMENU)win->menu,
	            MF_SEPARATOR, 0, NULL);
}


REBOOL Gui_Menu_End(GUIWIN *win)
{
	HWND hwnd;
	int  w, h;

	if (!win || !win->handle || !win->menu) return FALSE;
	hwnd = HWND_OF(win);

	if (!Client_Size_Of(hwnd, &w, &h)) { w = h = 0; }
	if (!SetMenu(hwnd, (HMENU)win->menu)) return FALSE;
	DrawMenuBar(hwnd);
	if (w && h) Keep_Client_Size(hwnd, w, h);

	if (Accel_Count > 0) {
		win->accel = (void*)CreateAcceleratorTableW(Accel_Build, Accel_Count);
	}
	Accel_Count = 0;
	return TRUE;
}


void Gui_Menu_Free(GUIWIN *win)
{
	if (!win) return;

	if (win->handle) {
		HWND hwnd = HWND_OF(win);
		int  w, h;
		if (win->menu) {
			if (!Client_Size_Of(hwnd, &w, &h)) { w = h = 0; }
			SetMenu(hwnd, NULL);
			DrawMenuBar(hwnd);
			// The room the bar was taking is given back the same way it
			// was taken, so removing a menu does not move anything either.
			if (w && h) Keep_Client_Size(hwnd, w, h);
		}
	}

	// A submenu is destroyed with the menu holding it, so the bar is the
	// only handle to destroy. It is NULL already when the window took it.
	if (win->menu) DestroyMenu((HMENU)win->menu);
	if (win->accel) DestroyAcceleratorTable((HACCEL)win->accel);
	win->menu  = NULL;
	win->accel = NULL;
}


void Gui_Menu_Enable(GUIWIN *win, REBCNT item_id, REBOOL enabled)
{
	if (!win || !win->menu) return;
	EnableMenuItem((HMENU)win->menu, (UINT)item_id,
	               MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
	if (win->handle) DrawMenuBar(HWND_OF(win));
}


//== transparency =============================================================
//
// A control with no background of its own has to show what is behind it,
// and on Win32 nothing puts it there. WS_CLIPCHILDREN - which the window
// and the panel both need, so that a parent cannot paint over the controls
// it holds - is exactly what stops the parent painting UNDER them too.
//
// So the control does it itself: its WM_ERASEBKGND asks the parent to
// render its own client area into the control's DC, with the origin
// shifted so that the part which lands inside the control is the part the
// control covers. Every class that can hold a widget answers
// WM_PRINTCLIENT for this - the window with its background, a panel with
// its frame and caption, an image widget with its pixels.
//
// This needs no theme API and no extra library: the possible parents are
// all our own classes.

/***********************************************************************
**  Paints what is behind `hwnd` into `dc`.
***********************************************************************/
static void Paint_Parent_Background(HWND hwnd, HDC dc)
{
	HWND  parent = GetParent(hwnd);
	POINT origin;
	int   saved;

	if (!parent || !dc) return;

	// Where this control's top-left sits in the parent's client area.
	origin.x = 0;
	origin.y = 0;
	MapWindowPoints(hwnd, parent, &origin, 1);

	saved = SaveDC(dc);
	// The parent draws in ITS coordinates; this makes those land in ours.
	OffsetWindowOrgEx(dc, origin.x, origin.y, NULL);
	SendMessageW(parent, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT | PRF_ERASEBKGND);
	RestoreDC(dc, saved);
}


// The original procedures of the system classes a transparent widget can
// be. One per class rather than one per control: every STATIC shares a
// procedure, and so does every BUTTON, so there is nothing per-widget to
// keep and nothing to unwind when a widget goes away.
static WNDPROC Static_Proc = NULL;
static WNDPROC Button_Proc = NULL;

/***********************************************************************
**  Paints a transparent control over pixels which are not a colour.
**
**  WM_PAINT rather than WM_ERASEBKGND, and the whole cycle taken over
**  rather than added to. A themed check or radio cross-fades between
**  states through BufferedPaintAnimation: it paints into a memory DC of
**  its own and never asks anyone to erase, so an erase hook is simply
**  not called during the fade and the control animates out of an empty
**  buffer.
**
**  Doing the compositing here - the parent's pixels, then the control
**  over them through WM_PRINTCLIENT - means the base procedure never
**  runs its own WM_PAINT, so there is no animation to go wrong. A
**  transparent control on a picture does not cross-fade, which is a
**  small price and the only way to be sure of what is behind it.
***********************************************************************/
static LRESULT CALLBACK Transparent_Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	GUIWIDGET *wid = (GUIWIDGET*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	WNDPROC    base = (wid && wid->kind == W_GUI_WIDGET_TEXT)
	                ? Static_Proc : Button_Proc;

	if (!base) return DefWindowProcW(hwnd, msg, wp, lp);

	// Only while it IS transparent, and only while what is behind it is
	// not a flat colour - otherwise the ordinary path is better in every
	// way. Turning either off leaves the subclass in place and stops
	// using it, which is safer than unhooking a procedure that may be on
	// the stack.
	if (wid && GUI_BG_IS_CLEAR(wid->background)) {
		COLORREF unused;

		if (msg == WM_ERASEBKGND && !Flat_Background_Of(wid, &unused)) {
			Paint_Parent_Background(hwnd, (HDC)wp);
			return TRUE;
		}

		if (msg == WM_PAINT && !Flat_Background_Of(wid, &unused)) {
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(hwnd, &ps);
			Paint_Parent_Background(hwnd, dc);
			// Statics and buttons both render themselves on request,
			// which is what makes this compositing rather than a
			// reimplementation of either.
			CallWindowProcW(base, hwnd, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT);
			EndPaint(hwnd, &ps);
			return 0;
		}
	}

	return CallWindowProcW(base, hwnd, msg, wp, lp);
}


// Installed the first time a widget is made transparent, never removed.
// A static and a button are different classes, so the procedure being
// replaced is remembered per class.
static void Subclass_For_Transparency(GUIWIDGET *wid)
{
	HWND    hwnd;
	WNDPROC previous;

	if (!wid || !wid->handle) return;
	hwnd = HWND_OF_WID(wid);

	previous = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
	if (previous == Transparent_Proc) return; // already done

	if (wid->kind == W_GUI_WIDGET_TEXT) {
		if (!Static_Proc) Static_Proc = previous;
	} else {
		if (!Button_Proc) Button_Proc = previous;
	}
	SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)Transparent_Proc);
}


void Gui_Widget_Set_Background(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return;

	// A check and a radio are BUTTONs, a label is a STATIC; the entries
	// and the drop-down have a frame and a background of their own which
	// showing through would only break, so they keep the colour and
	// ignore transparency.
	//
	// And only when what is behind is NOT a flat colour: a transparent
	// widget over the window or over a coloured panel is served by the
	// brush WM_CTLCOLOR* hands back, which keeps the themed animation and
	// needs no subclass at all.
	if (GUI_BG_IS_CLEAR(wid->background)
	    && (wid->kind == W_GUI_WIDGET_TEXT
	     || wid->kind == W_GUI_WIDGET_CHECK
	     || wid->kind == W_GUI_WIDGET_RADIO)) {
		COLORREF flat;
		if (!Flat_Background_Of(wid, &flat)) Subclass_For_Transparency(wid);
	}

	// A CONTAINER's background is also the background its transparent
	// children show, and WS_CLIPCHILDREN means invalidating it does not
	// reach them - so they are taken in explicitly, or they would keep
	// painting the colour the panel used to have.
	if (Kind_Is_Container(wid->kind)) {
		Repaint_Widget(wid, FALSE);
		return;
	}

	// TRUE erases: the old background has to go, and for a transparent
	// widget erasing is what fetches the parent's pixels.
	InvalidateRect(HWND_OF_WID(wid), NULL, TRUE);
}


//== widgets ==================================================================

// What a new control attaches to: the container holding it - a panel or an
// image widget - or the window. `wid->parent` is set before any creation
// call, and the kinds allowed there are decided in the shared layer.
static HWND Parent_Hwnd(GUIWIDGET *wid, GUIWIN *owner)
{
	if (wid->parent && ((GUIWIDGET*)wid->parent)->handle)
		return (HWND)((GUIWIDGET*)wid->parent)->handle;
	return HWND_OF(owner);
}


REBOOL Gui_Create_Panel(GUIWIDGET *wid, GUIWIN *owner,
                        REBINT x, REBINT y, REBINT w, REBINT h,
                        const REBYTE *text, REBCNT len)
{
	HWND hwnd;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);
	if (!Register_Panel_Class()) return FALSE;

	// WS_CLIPCHILDREN keeps the panel from painting over what it holds.
	//
	// This is deliberately NOT a BS_GROUPBOX button, which is how Win32
	// usually draws a captioned frame: a group box is not a container here
	// but a sibling drawn behind other controls, and parenting children to
	// one is where the classic repaint and tab-order trouble comes from.
	// The panel keeps being a real container and draws the frame itself,
	// which is also what lets the frame be turned on and off later.
	hwnd = CreateWindowExW(
		0, Class_Name_Panel, L"",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
		x, y, w, h,
		Parent_Hwnd(wid, owner),
		NULL,
		App_Instance, NULL
	);
	if (!hwnd) return FALSE;

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)wid);

	wid->handle = (void*)hwnd;

	// The caption lives in the panel's window text, so the generic text
	// accessor reaches it with no special case of its own.
	// No repaint here: Attach_Widget() redraws every new widget, and it is
	// the one place that decides so.
	if (text && len > 0) Set_Text_Of(hwnd, text, len);

	return TRUE;
}


void Gui_Panel_Edge_Changed(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return;
	// TRUE erases first: an edge which has just been turned off has to have
	// its own pixels painted over, not merely stop being drawn.
	InvalidateRect(HWND_OF_WID(wid), NULL, TRUE);
}


REBOOL Gui_Create_Button_Control(GUIWIDGET *wid, GUIWIN *owner,
                                 REBINT x, REBINT y, REBINT w, REBINT h,
                                 const REBYTE *text, REBCNT len)
{
	HWND   hwnd;
	WCHAR *wide;
	DWORD  style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);

	switch (wid->kind) {
	case W_GUI_WIDGET_CHECK:
		// AUTO: the control ticks itself and we read the result back.
		style |= BS_AUTOCHECKBOX;
		break;
	case W_GUI_WIDGET_RADIO:
		// NOT auto: BS_AUTORADIOBUTTON would group by sibling order and
		// WS_GROUP flags, which is not the grouping we promise. This one
		// only reports the click; the state is set from Gui_Widget_Set_State.
		style |= BS_RADIOBUTTON;
		break;
	default:
		style |= BS_PUSHBUTTON;
		break;
	}

	wide = To_Wide(text, len);
	hwnd = CreateWindowExW(
		0,
		L"BUTTON",
		wide ? wide : L"",
		style,
		x, y, w, h,
		Parent_Hwnd(wid, owner),
		NULL, // no control id - BN_CLICKED carries the HWND in lParam
		App_Instance, NULL
	);
	if (wide) FREE_MEM(wide);
	if (!hwnd) return FALSE;

	// How the parent's WM_COMMAND finds its way back to the Rebol handle.
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)wid);
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)Get_Default_Font(), TRUE);

	wid->handle = (void*)hwnd;
	return TRUE;
}


/***********************************************************************
**  The static label and the two edit controls.
**
**  All three are stock Win32 classes differing only in style bits, so
**  `wid->kind` picks the class and the flags and the rest is shared.
***********************************************************************/
REBOOL Gui_Create_Text_Control(GUIWIDGET *wid, GUIWIN *owner,
                               REBINT x, REBINT y, REBINT w, REBINT h,
                               const REBYTE *text, REBCNT len)
{
	HWND   hwnd;
	WCHAR *wide;
	const WCHAR *class_name;
	DWORD  style   = WS_CHILD | WS_VISIBLE;
	DWORD  exstyle = 0;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);

	switch (wid->kind) {
	case W_GUI_WIDGET_TEXT:
		class_name = L"STATIC";
		style |= SS_LEFT;
		break;

	case W_GUI_WIDGET_FIELD:
		class_name = L"EDIT";
		style   |= WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;
		exstyle |= WS_EX_CLIENTEDGE;
		break;

	case W_GUI_WIDGET_AREA:
		class_name = L"EDIT";
		style   |= WS_TABSTOP | ES_LEFT | ES_MULTILINE
		         | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
		exstyle |= WS_EX_CLIENTEDGE;
		break;

	default:
		return FALSE;
	}

	wide = To_Wide(text, len);
	hwnd = CreateWindowExW(
		exstyle,
		class_name,
		wide ? wide : L"",
		style,
		x, y, w, h,
		Parent_Hwnd(wid, owner),
		NULL, // notifications carry the child HWND in lParam
		App_Instance, NULL
	);
	if (wide) FREE_MEM(wide);
	if (!hwnd) return FALSE;

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)wid);
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)Get_Default_Font(), TRUE);

	wid->handle = (void*)hwnd;
	return TRUE;
}


REBOOL Gui_Create_Image(GUIWIDGET *wid, GUIWIN *owner,
                        REBINT x, REBINT y, REBINT w, REBINT h)
{
	HWND hwnd;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);
	if (!Register_Image_Class()) return FALSE;

	hwnd = CreateWindowExW(
		0,
		Class_Name_Image,
		L"",
		// WS_CLIPCHILDREN for the same reason a panel has it: an image
		// widget can hold other widgets now, and its blit covers every
		// pixel of its client area - including theirs, if not clipped out.
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
		x, y, w, h,
		Parent_Hwnd(wid, owner),
		NULL,
		App_Instance,
		wid // arrives as lpCreateParams in WM_NCCREATE
	);
	if (!hwnd) return FALSE;

	wid->handle = (void*)hwnd;
	return TRUE;
}


/***********************************************************************
**  A container's children have to be named, and this is why.
**
**  WS_CLIPCHILDREN keeps a container's own repaint out of the rectangles
**  its children occupy - which is what stops it painting over them. But
**  a TRANSPARENT child composites what is behind it as it paints, so
**  when an image widget's pixels change, a caption on top of it is
**  showing pixels which no longer exist and is not repainted by the
**  image's own invalidation. It keeps the picture it was last painted
**  over.
**
**  So a repaint of a container takes RDW_ALLCHILDREN. For a leaf widget
**  it would be pointless - it has no children - and the plain
**  invalidation is left alone there.
***********************************************************************/
static void Repaint_Widget(GUIWIDGET *wid, REBOOL now)
{
	UINT flags = RDW_INVALIDATE;

	if (!wid || !wid->handle) return;

	if (Kind_Is_Container(wid->kind))
		flags |= RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN;
	if (now)
		flags |= RDW_UPDATENOW;

	RedrawWindow(HWND_OF_WID(wid), NULL, NULL, flags);
}


void Gui_Widget_Redraw(GUIWIDGET *wid)
{
	// Painted now rather than whenever the queue next runs dry, so that
	// `redraw` means the pixels are on screen when it returns.
	Repaint_Widget(wid, TRUE);
}


void Gui_Widget_Invalidate(GUIWIDGET *wid)
{
	// Not now: the WM_PAINT this leaves behind is collected by the next
	// pump, along with every other widget invalidated since. See the note
	// in gui.h.
	Repaint_Widget(wid, FALSE);
}


void Gui_Window_Redraw(GUIWIN *win)
{
	if (!win || !win->handle) return;
	InvalidateRect(HWND_OF(win), NULL, TRUE); // children included
	UpdateWindow(HWND_OF(win));
}


void Gui_Destroy_Widget(GUIWIDGET *wid)
{
	// A widget whose window is already gone has a NULL handle: the OS
	// destroyed the control together with its parent.
	if (wid && wid->handle) DestroyWindow(HWND_OF_WID(wid));
}


REBSER* Gui_Widget_Get_Text(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return NULL;
	return Text_Of(HWND_OF_WID(wid));
}


REBOOL Gui_Widget_Set_Text(GUIWIDGET *wid, const REBYTE *utf8, REBCNT len)
{
	if (!wid || !wid->handle) return FALSE;
	return Set_Text_Of(HWND_OF_WID(wid), utf8, len);
}


//== typography ===============================================================

REBOOL Gui_Widget_Get_Font(GUIWIDGET *wid, REBSER **name, REBINT *size,
                           REBCNT *style)
{
	HWND     hwnd;
	HFONT    font;
	LOGFONTW lf;

	if (name)  *name  = NULL;
	if (size)  *size  = 0;
	if (style) *style = 0;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);

	// WM_GETFONT answers NULL for a control still using the system font,
	// which is not this extension's default - so the default is what an
	// unanswered question means here.
	font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
	if (!font) font = Get_Default_Font();
	if (!font || !GetObjectW(font, sizeof(lf), &lf)) return FALSE;

	if (name && lf.lfFaceName[0]) {
		*name = RL_ENCODE_UTF8_STRING(lf.lfFaceName,
			(REBCNT)lstrlenW(lf.lfFaceName), TRUE, 0);
	}
	if (size) {
		// lfHeight is negative for a character height, positive for a cell
		// height; both are pixels, and points is what Rebol asked about.
		int pixels = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
		*size = (REBINT)MulDiv(pixels, 72, Screen_DPI());
	}
	if (style) {
		if (lf.lfWeight >= FW_SEMIBOLD) *style |= GUI_FONT_BOLD;
		if (lf.lfItalic)                *style |= GUI_FONT_ITALIC;
	}
	return TRUE;
}


REBOOL Gui_Widget_Set_Font(GUIWIDGET *wid, const REBYTE *utf8, REBCNT len,
                           REBINT size, REBCNT style)
{
	HWND   hwnd;
	HFONT  font;
	WCHAR *wide = NULL;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);

	if (utf8 && len > 0) wide = To_Wide(utf8, len);
	font = Font_For(wide, (int)size, style);
	if (wide) FREE_MEM(wide);
	if (!font) return FALSE;

	// A control does not resize itself for a bigger font, so the box a
	// caller laid out is the box it keeps - which is the same promise the
	// panel's frame makes.
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));
	InvalidateRect(hwnd, NULL, TRUE);
	return TRUE;
}


REBOOL Gui_Widget_Set_Color(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return FALSE;

	// Nothing to apply: the colour is read out of the widget context by
	// the parent's WM_CTLCOLOR* handler, at the moment the control is
	// about to paint. All that is needed is to make it paint.
	InvalidateRect(HWND_OF_WID(wid), NULL, TRUE);

	// ... except on a push button, which never asks. Win32 draws a
	// BS_PUSHBUTTON's text itself, in the system colour, and only an
	// owner-drawn button can say otherwise.
	return (wid->kind == W_GUI_WIDGET_BUTTON) ? FALSE : TRUE;
}


REBDEC Gui_Get_Scale(GUIWIN *win)
{
	// One scale for the process - the window is not consulted, but it is in
	// the signature because macOS answers per screen.
	return (REBDEC)Gui_DPI / 96.0;
}


/***********************************************************************
**  What this widget needs for the text it is holding.
**
**  Answered in LOGICAL units, like every other size here, and only for
**  the kinds which have text - the caller has already decided that a
**  slider has no natural anything.
**
**  The padding numbers are the ones the shell's own dialogs use. They
**  are in logical units and scaled on the way out, so they hold at any
**  DPI.
***********************************************************************/
REBOOL Gui_Widget_Natural_Size(GUIWIDGET *wid, REBINT *w, REBINT *h)
{
	HWND    hwnd;
	HDC     dc;
	HFONT   font, old = NULL;
	TEXTMETRICW tm;
	SIZE    text = {0, 0};
	int     len;
	WCHAR  *caption = NULL;
	REBINT  pad_x = 0, pad_y = 0;
	REBINT  lines = 1;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);

	dc = GetDC(hwnd);
	if (!dc) return FALSE;

	font = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
	if (!font) font = Get_Default_Font();
	if (font) old = (HFONT)SelectObject(dc, font);

	GetTextMetricsW(dc, &tm);

	len = GetWindowTextLengthW(hwnd);
	if (len > 0) {
		caption = (WCHAR*)MAKE_MEM((len + 1) * sizeof(WCHAR));
		if (caption) {
			len = GetWindowTextW(hwnd, caption, len + 1);
			GetTextExtentPoint32W(dc, caption, len, &text);
			FREE_MEM(caption);
		}
	}

	if (old) SelectObject(dc, old);
	ReleaseDC(hwnd, dc);

	switch (wid->kind) {
	case W_GUI_WIDGET_BUTTON:
		pad_x = To_Device(24); pad_y = To_Device(12);
		break;
	case W_GUI_WIDGET_CHECK:
	case W_GUI_WIDGET_RADIO:
		// The box or the dot, and the gap after it.
		pad_x = GetSystemMetrics(SM_CXMENUCHECK) + To_Device(8);
		pad_y = To_Device(6);
		break;
	case W_GUI_WIDGET_TEXT:
		pad_y = To_Device(4);
		break;
	case W_GUI_WIDGET_AREA:
		// One line is not a useful multi-line box; four is the smallest
		// that looks like one.
		lines = 4;
		// fall through
	case W_GUI_WIDGET_FIELD:
	case W_GUI_WIDGET_DROP_DOWN:
		// The sunken border, plus the padding the control keeps inside it.
		pad_x = 2 * GetSystemMetrics(SM_CXEDGE) + To_Device(8);
		pad_y = 2 * GetSystemMetrics(SM_CYEDGE) + To_Device(8);
		// An entry's width should not be the width of whatever happens to
		// be in it - an empty one would come out a few pixels wide. About
		// twenty characters is what a dialog uses when it has no better
		// idea, and the caller can always give a width of its own.
		if (text.cx < 20 * tm.tmAveCharWidth) text.cx = 20 * tm.tmAveCharWidth;
		break;
	default:
		return FALSE; // no text, so no natural size to give
	}

	if (w) *w = To_Logical((REBINT)text.cx + pad_x);

	// tmHeight is ascent plus descent and NOTHING else: tmExternalLeading,
	// the gap the font asks for between its lines, is not in it. A line box
	// is the two together, which is what a multi-line control needs - and
	// for a single line it is a pixel or two of slack in the only direction
	// that matters, since a static draws from the top and anything short
	// clips the descenders.
	if (h) *h = To_Logical((REBINT)(tm.tmHeight + tm.tmExternalLeading) * lines
	                       + pad_y);
	return TRUE;
}


REBOOL Gui_Widget_Get_Box(GUIWIDGET *wid, REBINT *x, REBINT *y, REBINT *w, REBINT *h)
{
	RECT  r;
	POINT pt;
	HWND  hwnd, parent;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);

	// A combo box's window rectangle covers the dropped list as well, which
	// is not the box anyone laid out. The closed control is reported instead.
	if (wid->kind == W_GUI_WIDGET_DROP_DOWN) {
		if (!GetClientRect(hwnd, &r)) return FALSE;
		pt.x = 0; pt.y = 0;
		ClientToScreen(hwnd, &pt);
		parent = GetParent(hwnd);
		if (parent) ScreenToClient(parent, &pt);

		*x = pt.x;
		*y = pt.y;
		*w = r.right - r.left;
		*h = (REBINT)SendMessageW(hwnd, CB_GETITEMHEIGHT, (WPARAM)-1, 0)
		   + 2 * GetSystemMetrics(SM_CYEDGE);
		Box_To_Logical(x, y, w, h);
		return TRUE;
	}

	if (!GetWindowRect(hwnd, &r)) return FALSE;

	// GetWindowRect is in screen coordinates; the offset is wanted inside
	// the parent's client area.
	pt.x = r.left;
	pt.y = r.top;
	parent = GetParent(hwnd);
	if (parent) ScreenToClient(parent, &pt);

	*x = pt.x;
	*y = pt.y;
	*w = r.right - r.left;
	*h = r.bottom - r.top;
	Box_To_Logical(x, y, w, h);
	return TRUE;
}


REBOOL Gui_Widget_Set_Box(GUIWIDGET *wid, REBINT x, REBINT y, REBINT w, REBINT h)
{
	HWND hwnd, parent;
	RECT before, after, dirty;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);

	Box_To_Device(&x, &y, &w, &h);
	// ... and the same room has to be added back when it is moved. It is a
	// device-pixel constant, so it is added AFTER the conversion.
	if (wid->kind == W_GUI_WIDGET_DROP_DOWN) h += DROP_LIST_ROOM;

	// Where it is now, in the coordinates the new box is given in - the
	// client area of whatever holds it, a window or a panel.
	parent = GetParent(hwnd);
	if (parent && GetWindowRect(hwnd, &before))
		MapWindowPoints(NULL, parent, (POINT*)&before, 2);
	else
		parent = NULL;

	if (!MoveWindow(hwnd, x, y, w, h, TRUE)) return FALSE;

	/*******************************************************************
	**  A control which moved or shrank leaves its old rectangle behind,
	**  and that rectangle belongs to the PARENT: Windows invalidates it
	**  there, and the parent paints its background over the lot.
	**
	**  Which erases any SIBLING control living in that area - and the
	**  sibling is a window of its own, whose client area Windows still
	**  considers valid, so it is never sent a WM_PAINT and never comes
	**  back. Grow a label over a field, shrink it again, and the field
	**  is left half painted.
	**
	**  RDW_ALLCHILDREN over the union of where the control was and where
	**  it now is takes the siblings in with it. No RDW_UPDATENOW: this
	**  marks, and the pump paints - see Gui_Widget_Invalidate.
	*******************************************************************/
	if (parent) {
		SetRect(&after, x, y, x + w, y + h);
		UnionRect(&dirty, &before, &after);
		// RDW_FRAME as well as RDW_ERASE: a sibling is not clipped out of
		// another sibling, so one can still paint over another's sunken
		// border - and a border lives in the NON-client area, which
		// invalidating the client area alone never repaints.
		RedrawWindow(parent, &dirty, NULL,
		             RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
	}
	return TRUE;
}


REBOOL Gui_Create_Range_Control(GUIWIDGET *wid, GUIWIN *owner,
                                REBINT x, REBINT y, REBINT w, REBINT h)
{
	HWND  hwnd;
	const WCHAR *class_name;
	DWORD style = WS_CHILD | WS_VISIBLE;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);

	if (wid->kind == W_GUI_WIDGET_SLIDER) {
		class_name = TRACKBAR_CLASSW;
		style |= WS_TABSTOP | TBS_NOTICKS;
		// Taller than wide means upright - the same rule the old View
		// widgets used, and one less argument to pass.
		if (h > w) style |= TBS_VERT;
	} else {
		class_name = PROGRESS_CLASSW;
	}

	hwnd = CreateWindowExW(
		0, class_name, L"", style,
		x, y, w, h,
		Parent_Hwnd(wid, owner),
		NULL,
		App_Instance, NULL
	);
	if (!hwnd) return FALSE;

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)wid);

	if (wid->kind == W_GUI_WIDGET_SLIDER) {
		SendMessageW(hwnd, TBM_SETRANGE, (WPARAM)TRUE,
		             (LPARAM)MAKELONG(0, RANGE_STEPS));
		SendMessageW(hwnd, TBM_SETPAGESIZE, 0, (LPARAM)(RANGE_STEPS / 10));
	} else {
		SendMessageW(hwnd, PBM_SETRANGE32, 0, (LPARAM)RANGE_STEPS);
	}

	wid->handle = (void*)hwnd;
	return TRUE;
}


// A vertical trackbar puts position 0 at the TOP, which is upside down
// compared with what a caller means by 0%. Both directions are flipped
// here so that the extension's 0.0 is always the bottom.
static REBOOL Is_Vertical_Slider(GUIWIDGET *wid)
{
	return (wid->kind == W_GUI_WIDGET_SLIDER
	     && (GetWindowLongPtrW(HWND_OF_WID(wid), GWL_STYLE) & TBS_VERT))
		? TRUE : FALSE;
}


REBDEC Gui_Widget_Get_Value(GUIWIDGET *wid)
{
	LRESULT pos;

	if (!wid || !wid->handle) return 0.0;

	if (wid->kind == W_GUI_WIDGET_SLIDER) {
		pos = SendMessageW(HWND_OF_WID(wid), TBM_GETPOS, 0, 0);
		if (Is_Vertical_Slider(wid)) pos = RANGE_STEPS - pos;
	} else {
		pos = SendMessageW(HWND_OF_WID(wid), PBM_GETPOS, 0, 0);
	}
	return (REBDEC)pos / (REBDEC)RANGE_STEPS;
}


void Gui_Widget_Set_Value(GUIWIDGET *wid, REBDEC value)
{
	LRESULT pos;

	if (!wid || !wid->handle) return;

	pos = (LRESULT)(value * RANGE_STEPS + 0.5);
	if (pos < 0) pos = 0;
	if (pos > RANGE_STEPS) pos = RANGE_STEPS;

	if (wid->kind == W_GUI_WIDGET_SLIDER) {
		if (Is_Vertical_Slider(wid)) pos = RANGE_STEPS - pos;
		SendMessageW(HWND_OF_WID(wid), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)pos);
		return;
	}

	/*******************************************************************
	**  A themed progress bar does not jump to a new position - it
	**  SLIDES there, over a couple of hundred milliseconds, and a
	**  program setting it faster than that (a slider driving a meter,
	**  say) is left watching the bar trail behind by a visible margin.
	**  The classic look has no animation, which is why this only shows
	**  up once the v6 common controls are asked for.
	**
	**  The animation only plays when the position INCREASES. Going one
	**  step past and stepping back therefore lands exactly on the value
	**  with no animation left to play. The range is widened for a
	**  moment when the value is already at the top, so that there is a
	**  step to go past.
	*******************************************************************/
	{
		HWND hwnd = HWND_OF_WID(wid);

		if (pos >= RANGE_STEPS) {
			SendMessageW(hwnd, PBM_SETRANGE32, 0, (LPARAM)(RANGE_STEPS + 1));
			SendMessageW(hwnd, PBM_SETPOS, (WPARAM)(pos + 1), 0);
			SendMessageW(hwnd, PBM_SETPOS, (WPARAM)pos, 0);
			SendMessageW(hwnd, PBM_SETRANGE32, 0, (LPARAM)RANGE_STEPS);
		} else {
			SendMessageW(hwnd, PBM_SETPOS, (WPARAM)(pos + 1), 0);
			SendMessageW(hwnd, PBM_SETPOS, (WPARAM)pos, 0);
		}
	}
}


//-- drop-down ----------------------------------------------------------------

REBOOL Gui_Create_Drop_Down(GUIWIDGET *wid, GUIWIN *owner,
                            REBINT x, REBINT y, REBINT w, REBINT h)
{
	HWND hwnd;

	if (!wid || !owner || !owner->handle) return FALSE;
	// The caller's coordinates are logical units - see the note on Gui_DPI.
	Box_To_Device(&x, &y, &w, &h);

	hwnd = CreateWindowExW(
		0, L"COMBOBOX", L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
		| CBS_DROPDOWNLIST | CBS_HASSTRINGS,
		x, y, w, h + DROP_LIST_ROOM, // see DROP_LIST_ROOM
		Parent_Hwnd(wid, owner),
		NULL,
		App_Instance, NULL
	);
	if (!hwnd) return FALSE;

	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)wid);
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)Get_Default_Font(), TRUE);

	wid->handle = (void*)hwnd;
	return TRUE;
}


REBCNT Gui_Widget_Count_Items(GUIWIDGET *wid)
{
	LRESULT count;
	if (!wid || !wid->handle) return 0;
	count = SendMessageW(HWND_OF_WID(wid), CB_GETCOUNT, 0, 0);
	return (count == CB_ERR || count < 0) ? 0 : (REBCNT)count;
}


REBSER* Gui_Widget_Get_Item(GUIWIDGET *wid, REBCNT n)
{
	LRESULT len;
	WCHAR  *buf;
	REBSER *str;
	HWND    hwnd;

	if (!wid || !wid->handle) return NULL;
	hwnd = HWND_OF_WID(wid);

	len = SendMessageW(hwnd, CB_GETLBTEXTLEN, (WPARAM)n, 0);
	if (len == CB_ERR) return NULL;
	if (len == 0) return RL_MAKE_STRING(0, FALSE);

	buf = (WCHAR*)MAKE_MEM(((size_t)len + 1) * sizeof(WCHAR));
	if (!buf) return NULL;

	len = SendMessageW(hwnd, CB_GETLBTEXT, (WPARAM)n, (LPARAM)buf);
	if (len == CB_ERR) { FREE_MEM(buf); return NULL; }

	str = RL_ENCODE_UTF8_STRING(buf, (REBCNT)len, TRUE, 0);
	FREE_MEM(buf);
	return str;
}


REBOOL Gui_Widget_Add_Item(GUIWIDGET *wid, const REBYTE *utf8, REBCNT len)
{
	WCHAR *wide;
	LRESULT res;

	if (!wid || !wid->handle) return FALSE;

	wide = To_Wide(utf8, len);
	res = SendMessageW(HWND_OF_WID(wid), CB_ADDSTRING, 0,
	                   (LPARAM)(wide ? wide : L""));
	if (wide) FREE_MEM(wide);
	return (res == CB_ERR || res == CB_ERRSPACE) ? FALSE : TRUE;
}


void Gui_Widget_Clear_Items(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return;
	SendMessageW(HWND_OF_WID(wid), CB_RESETCONTENT, 0, 0);
}


REBINT Gui_Widget_Get_Index(GUIWIDGET *wid)
{
	LRESULT n;
	if (!wid || !wid->handle) return -1;
	n = SendMessageW(HWND_OF_WID(wid), CB_GETCURSEL, 0, 0);
	return (n == CB_ERR) ? -1 : (REBINT)n;
}


void Gui_Widget_Set_Index(GUIWIDGET *wid, REBINT n)
{
	if (!wid || !wid->handle) return;
	// CB_SETCURSEL with -1 clears the selection, which is what an index
	// out of range means here.
	SendMessageW(HWND_OF_WID(wid), CB_SETCURSEL, (WPARAM)n, 0);
}


REBOOL Gui_Widget_Get_State(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return FALSE;
	return (SendMessageW(HWND_OF_WID(wid), BM_GETCHECK, 0, 0) == BST_CHECKED)
		? TRUE : FALSE;
}


/***********************************************************************
**  Setting a check or a radio - but only when it is not already there.
**
**  Not an optimisation. With the v6 common controls a check and a radio
**  CROSS-FADE between states, and BM_SETCHECK restarts that animation
**  whether or not the state actually changed. The radio grouping above
**  this file re-asserts every radio in the window on every click, so a
**  redundant write here means every radio on screen begins a fade each
**  time any one of them is picked - which is exactly the sluggishness
**  the modern theme brings and the classic one does not.
**
**  The control is asked rather than trusting wid->state, because the
**  user clicking a checkbox changes the control without going through
**  this extension at all.
***********************************************************************/
void Gui_Widget_Set_State(GUIWIDGET *wid, REBOOL on)
{
	HWND    hwnd;
	LRESULT want, has;

	if (!wid || !wid->handle) return;
	hwnd = HWND_OF_WID(wid);

	want = on ? BST_CHECKED : BST_UNCHECKED;
	has  = SendMessageW(hwnd, BM_GETCHECK, 0, 0);
	if (has == want) return;

	SendMessageW(hwnd, BM_SETCHECK, (WPARAM)want, 0);
}


REBOOL Gui_Widget_Get_Enabled(GUIWIDGET *wid)
{
	if (!wid || !wid->handle) return FALSE;
	return IsWindowEnabled(HWND_OF_WID(wid)) ? TRUE : FALSE;
}


REBOOL Gui_Widget_Set_Enabled(GUIWIDGET *wid, REBOOL enabled)
{
	if (!wid || !wid->handle) return FALSE;
	EnableWindow(HWND_OF_WID(wid), enabled ? TRUE : FALSE);
	return TRUE;
}


/***********************************************************************
**  Scrolling, through the scrollbar rather than the text.
**
**  GetScrollInfo is what makes this short: an EDIT control's vertical
**  range is already in lines, with nPage the number visible, so the
**  fraction is the standard nPos / (nMax - nMin - nPage + 1) and no
**  font has to be measured to find out how many lines fit.
***********************************************************************/
static REBOOL Scroll_Span_Of(HWND hwnd, SCROLLINFO *si, REBINT *span)
{
	ZeroMemory(si, sizeof(*si));
	si->cbSize = sizeof(*si);
	si->fMask  = SIF_ALL;
	if (!GetScrollInfo(hwnd, SB_VERT, si)) return FALSE;

	*span = si->nMax - si->nMin - (REBINT)si->nPage + 1;
	return TRUE;
}


REBDEC Gui_Widget_Get_Scroll(GUIWIDGET *wid)
{
	SCROLLINFO si;
	REBINT     span = 0;

	if (!wid || !wid->handle) return -1.0;
	if (!Scroll_Span_Of(HWND_OF_WID(wid), &si, &span)) return -1.0;

	// Everything fits, so it is at the top and cannot be anywhere else.
	if (span <= 0) return 0.0;
	return (REBDEC)(si.nPos - si.nMin) / (REBDEC)span;
}


REBOOL Gui_Widget_Set_Scroll(GUIWIDGET *wid, REBDEC where)
{
	SCROLLINFO si;
	REBINT     span = 0, target, delta;
	HWND       hwnd;

	if (!wid || !wid->handle) return FALSE;
	hwnd = HWND_OF_WID(wid);
	if (!Scroll_Span_Of(hwnd, &si, &span)) return FALSE;
	if (span <= 0) return TRUE; // nothing to scroll, and that is not a failure

	target = si.nMin + (REBINT)((REBDEC)span * where + 0.5);
	if (target < si.nMin)        target = si.nMin;
	if (target > si.nMin + span) target = si.nMin + span;

	// EM_LINESCROLL takes a DELTA, not a position, and clamps it - which
	// is also why the end is reliable: an over-large delta lands there
	// rather than failing.
	delta = target - si.nPos;
	if (delta) SendMessageW(hwnd, EM_LINESCROLL, 0, (LPARAM)delta);
	return TRUE;
}


// ES_READONLY and WS_DISABLED are separate bits here, so the two compose
// without either being reconstructed from the other - the reason this is
// three lines on Windows and a combination on macOS.
REBOOL Gui_Widget_Set_Read_Only(GUIWIDGET *wid, REBOOL on)
{
	if (!wid || !wid->handle) return FALSE;
	SendMessageW(HWND_OF_WID(wid), EM_SETREADONLY, (WPARAM)(on ? TRUE : FALSE), 0);
	return TRUE;
}
