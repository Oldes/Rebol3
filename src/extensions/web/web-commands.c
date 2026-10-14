//
// Project: Rebol/Web extension
// SPDX-License-Identifier: Apache-2.0
// ===========================================================================
// Command implementations of the browser bridge (WebAssembly build only).
//
// One function per command; the enum, the declarations, the dispatch table
// and the `_init` handler are all generated from web.reb.
//
// Only binary! values cross the boundary in both directions - the
// mezzanine does all string conversions.
//

#include "gen-web.h"
#include <emscripten.h>


//== JavaScript side ==========================================================

// NOTE: EM_JS bodies go through the C preprocessor, so they must not
// contain regular expression literals or backslashes outside of string
// literals (they would be mangled - for example `\n` becomes a newline).
// An empty string must be written as "" - '' is an invalid (empty)
// character constant for the preprocessor.
//
// Sends a synchronous XMLHttpRequest. The response (or the error message)
// is kept in Module.webResponse as Uint8Arrays, from where the response-*
// commands copy it. Returns the HTTP status, or 0 when there is no response
// (network failure, CORS refusal, invalid URL...).
//
// Module.XMLHttpRequest may replace the browser's implementation (tests).
// Module.rebolHttpProxy, when set, is prefixed to the URI encoded URL.
EM_JS(int, JS_Web_Request, (
	const char *method, int method_len,
	const char *url,    int url_len,
	const char *hdrs,   int hdrs_len,
	const unsigned char *data, int data_len, int has_data
), {
	var bytes = function (s) {
		var a = new Uint8Array(s.length);
		for (var i = 0; i < s.length; i++) a[i] = s.charCodeAt(i) & 255;
		return a;
	};
	var text = function (s) { return new TextEncoder().encode(String(s)); };
	var empty = new Uint8Array(0);
	var r = Module.webResponse = {body: empty, headers: empty, error: empty};
	try {
		var target = UTF8ToString(url, url_len);
		if (Module.rebolHttpProxy) target = Module.rebolHttpProxy + encodeURIComponent(target);
		var XHR = Module.XMLHttpRequest || globalThis.XMLHttpRequest;
		if (!XHR) throw new Error('XMLHttpRequest is not available');
		var xhr = new XHR();
		xhr.open(UTF8ToString(method, method_len), target, false);
		// A synchronous request cannot use responseType, so the body is
		// received as a "binary string" (one char per byte).
		xhr.overrideMimeType('text/plain; charset=x-user-defined');
		// headers controlled by the browser (setting them only logs errors)
		var skip = ['host', 'connection', 'content-length', 'accept-encoding',
		            'accept-charset', 'user-agent', 'cookie', 'referer'];
		UTF8ToString(hdrs, hdrs_len).split(String.fromCharCode(10)).forEach(function (line) {
			var i = line.indexOf(':');
			if (i <= 0) return;
			var name = line.slice(0, i).trim();
			if (skip.indexOf(name.toLowerCase()) >= 0) return;
			xhr.setRequestHeader(name, line.slice(i + 1).trim()); // trim also drops CR
		});
		xhr.send(has_data ? HEAPU8.slice(data, data + data_len) : null);
		r.body    = bytes(xhr.responseText || "");
		r.headers = bytes(xhr.getAllResponseHeaders() || "");
		if (!xhr.status) r.error = text('Request failed (network error or blocked by CORS)');
		return xhr.status | 0;
	} catch (e) {
		r.error = text((e && e.message) || e);
		return 0;
	}
});

// Installs the JS side of the extension (called once from Web_Init):
//
// Module.webMold(value, depth)
//     converts a JS value to Rebol source text (see JS_Web_Eval).
//
// Module.rebolDispatch(id, info)
//     calls the Rebol handler `id` registered by `dom/on`, with the event
//     `info` object (as a map!). The evaluation ends with an empty paren,
//     so only errors are printed.
//
// globalThis.rebolDom
//     DOM helpers used by the `dom` object of the extension. Event handlers
//     are delegated from `document` (capture phase, so also non-bubbling
//     events work) and are always dispatched asynchronously (setTimeout),
//     so Rebol is never re-entered from inside a running evaluation - for
//     example when Rebol code itself triggers an event through `js`.
EM_JS(void, JS_Web_Install, (void), {
	var ctrl = {0: '^@', 9: '^-', 10: '^/', 13: '^M', 27: '^['};
	var str = function (s) {
		var out = '"';
		for (var i = 0; i < s.length; i++) {
			var ch = s.charAt(i), c = s.charCodeAt(i);
			if (ch === '"') out += '^"';
			else if (ch === '^') out += '^^';
			else if (c in ctrl) out += ctrl[c];
			else if (c < 32 || c === 127) out += '^(' + c.toString(16) + ')';
			else out += ch;
		}
		return out + '"';
	};
	var isWord = function (k) {
		if (!k.length) return false;
		for (var i = 0; i < k.length; i++) {
			var c = k.charAt(i);
			var alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';
			var other = (c >= '0' && c <= '9') || c === '-' || c === '?' || c === '!';
			if (!alpha && !(i > 0 && other)) return false;
		}
		return true;
	};
	var hex = '0123456789ABCDEF';
	var mold = function (v, depth) {
		if (v === null || v === undefined || depth > 32) return '#(none)';
		switch (typeof v) {
		case 'boolean': return v ? '#(true)' : '#(false)';
		case 'bigint':  return String(v);
		case 'string':  return str(v);
		case 'number':
			if (v !== v) return '1.#NaN';
			if (v === Infinity) return '1.#INF';
			if (v === -Infinity) return '-1.#INF';
			if (Number.isInteger(v) && Math.abs(v) <= Number.MAX_SAFE_INTEGER) return String(v);
			var d = String(v).split('+').join(""); // 1e+21 -> 1e21
			if (d.indexOf('.') < 0 && d.indexOf('e') < 0) d += '.0';
			return d;
		}
		if (v instanceof ArrayBuffer) v = new Uint8Array(v);
		if (ArrayBuffer.isView(v)) {
			var b = new Uint8Array(v.buffer, v.byteOffset, v.byteLength), h = '#{';
			for (var i = 0; i < b.length; i++) h += hex.charAt(b[i] >> 4) + hex.charAt(b[i] & 15);
			return h + '}';
		}
		if (Array.isArray(v)) {
			return '[' + v.map(function (x) { return mold(x, depth + 1); }).join(' ') + ']';
		}
		if (v instanceof Date) return str(isNaN(v) ? "" : v.toISOString());
		if (Object.prototype.toString.call(v) === '[object Object]') {
			return '#[' + Object.keys(v).map(function (k) {
				return (isWord(k) ? k + ':' : str(k)) + ' ' + mold(v[k], depth + 1);
			}).join(' ') + ']';
		}
		return str(String(v));
	};
	Module.webMold = mold;

	Module.rebolDispatch = function (id, info) {
		var ptr = stringToNewUTF8('dom/dispatch ' + id + ' ' + mold(info, 0) + ' ()');
		try { Module._r3_do(ptr); } finally { Module._free(ptr); }
	};

	var all = function (sel) { return Array.prototype.slice.call(document.querySelectorAll(sel)); };
	var each = function (sel, fn) { var a = all(sel); a.forEach(fn); return a.length; };
	var opt = function (v) { return v === undefined ? null : v; };
	var dom = globalThis.rebolDom = {
		handlers: {},
		get: function (sel, prop) {
			var e = document.querySelector(sel);
			return e ? e[prop] : null;
		},
		set: function (sel, prop, val) {
			return each(sel, function (e) { e[prop] = val; });
		},
		attr: function (sel, name, val) {
			return each(sel, function (e) {
				if (val === null) e.removeAttribute(name); else e.setAttribute(name, String(val));
			});
		},
		style: function (sel, props) {
			return each(sel, function (e) {
				for (var k in props) e.style.setProperty(k, props[k] === null ? "" : String(props[k]));
			});
		},
		cls: function (sel, name, mode) {
			return each(sel, function (e) { e.classList[mode](name); });
		},
		append: function (sel, tag, content, html) {
			var p = document.querySelector(sel);
			if (!p) return 0;
			var e = document.createElement(tag);
			if (content !== null) { if (html) e.innerHTML = content; else e.textContent = content; }
			p.appendChild(e);
			return 1;
		},
		remove: function (sel) {
			return each(sel, function (e) { e.remove(); });
		},
		on: function (sel, type, id) {
			var fn = function (e) {
				var t = (e.target && e.target.closest) ? e.target.closest(sel) : null;
				if (!t) return;
				var info = {
					type: e.type, id: t.id || null,
					value: opt(t.value), checked: opt(t.checked),
					key: opt(e.key), x: opt(e.clientX), y: opt(e.clientY)
				};
				setTimeout(function () { Module.rebolDispatch(id, info); }, 0);
			};
			dom.handlers[id] = {type: type, fn: fn};
			document.addEventListener(type, fn, true);
			return id;
		},
		off: function (id) {
			var h = dom.handlers[id];
			if (!h) return false;
			document.removeEventListener(h.type, h.fn, true);
			delete dom.handlers[id];
			return true;
		}
	};
});

// Evaluates JavaScript code in the page's global scope (indirect eval).
// The result is converted to Rebol source text (Module.webMold) and kept as
// the RESULT part; a thrown exception is kept as the ERROR part.
// Returns 1 on success, else 0.
//
// Module.webMold conversion: string -> string!, number -> integer! (when it is a safe
// integer) or decimal! (NaN and Infinity as 1.#NaN and 1.#INF), boolean ->
// logic!, null/undefined -> none, array -> block!, plain object -> map!, typed array/ArrayBuffer ->
// binary!, Date -> ISO string, anything else (functions, DOM nodes...) ->
// its String() form. Values nested deeper than 32 levels become none.
EM_JS(int, JS_Web_Eval, (const char *code, int code_len), {
	var empty = new Uint8Array(0);
	var r = Module.webResponse || {body: empty, headers: empty};
	Module.webResponse = r;
	r.error = r.result = empty;
	var text = function (s) { return new TextEncoder().encode(String(s)); };
	try {
		var value = (0, eval)(UTF8ToString(code, code_len));
		r.result = text(Module.webMold(value, 0));
		return 1;
	} catch (e) {
		r.error = text((e instanceof Error) ? (e.name + ': ' + e.message) : String(e));
		return 0;
	}
});

// The parts kept on the JS side, indexed by WEB_PART_*.
// Length of a part of the last response or evaluation.
EM_JS(int, JS_Web_Part_Length, (int part), {
	var r = Module.webResponse;
	if (!r) return 0;
	var p = [r.body, r.headers, r.error, r.result][part];
	return p ? p.length : 0;
});

// Copies a part of the last response or evaluation to `dst`.
EM_JS(void, JS_Web_Part_Copy, (int part, unsigned char *dst), {
	var r = Module.webResponse;
	var p = r && [r.body, r.headers, r.error, r.result][part];
	if (p) HEAPU8.set(p, dst);
});


// Called from Web_Init (web.c) - an EM_JS function is better not called
// directly from another translation unit.
void Web_Install_JS(void) {
	JS_Web_Install();
}


// Passes rich output to the page: Module.onRebolShow(kind, bytes), where
// kind is "html" or "image" and bytes is a Uint8Array (a copy).
// Returns 0 when the page has no such handler (for example in Node.js).
EM_JS(int, JS_Web_Show, (const char *kind, int kind_len, const unsigned char *data, int data_len), {
	var show = Module.onRebolShow;
	if (typeof show !== "function") return 0;
	show(UTF8ToString(kind, kind_len), HEAPU8.slice(data, data + data_len));
	return 1;
});


//== helpers ==================================================================

// Pointer to the binary argument's data at its index, and the byte count.
static const char *Arg_Bytes(RXIFRM *frm, int n, int *len)
{
	REBSER *ser  = RXA_SERIES(frm, n);
	REBCNT  idx  = RXA_INDEX(frm, n);
	REBCNT  tail = SERIES_TAIL(ser);

	*len = (tail > idx) ? (int)(tail - idx) : 0;
	return (const char *)(SERIES_DATA(ser) + idx);
}

// Returns a part of the last response as a new binary.
static int Return_Part(RXIFRM *frm, int part)
{
	int len = JS_Web_Part_Length(part);
	REBSER *ser = RL_MAKE_STRING(len + 1, FALSE); // byte series

	if (!ser) return RXR_NONE;
	if (len > 0) JS_Web_Part_Copy(part, SERIES_DATA(ser));
	SERIES_TAIL(ser) = len;

	RXA_SERIES(frm, 1) = ser;
	RXA_INDEX(frm, 1)  = 0;
	RXA_TYPE(frm, 1)   = RXT_BINARY;
	return RXR_VALUE;
}


//== commands =================================================================

COMMAND cmd_web_request(RXIFRM *frm, void *ctx) {
	int method_len, url_len, hdrs_len, data_len = 0;
	const char *method = Arg_Bytes(frm, 1, &method_len);
	const char *url    = Arg_Bytes(frm, 2, &url_len);
	const char *hdrs   = Arg_Bytes(frm, 3, &hdrs_len);
	const char *data   = NULL;
	int has_data = RXA_TYPE(frm, 4) == RXT_BINARY;

	if (has_data) data = Arg_Bytes(frm, 4, &data_len);

	RXA_INT64(frm, 1) = (i64)JS_Web_Request(
		method, method_len, url, url_len, hdrs, hdrs_len,
		(const unsigned char *)data, data_len, has_data
	);
	RXA_TYPE(frm, 1) = RXT_INTEGER;
	return RXR_VALUE;
}

COMMAND cmd_web_response_body(RXIFRM *frm, void *ctx) {
	return Return_Part(frm, WEB_PART_BODY);
}

COMMAND cmd_web_response_headers(RXIFRM *frm, void *ctx) {
	return Return_Part(frm, WEB_PART_HEADERS);
}

COMMAND cmd_web_response_error(RXIFRM *frm, void *ctx) {
	return Return_Part(frm, WEB_PART_ERROR);
}

COMMAND cmd_web_js_eval(RXIFRM *frm, void *ctx) {
	int code_len;
	const char *code = Arg_Bytes(frm, 1, &code_len);

	RXA_LOGIC(frm, 1) = JS_Web_Eval(code, code_len) != 0;
	RXA_TYPE(frm, 1)  = RXT_LOGIC;
	return RXR_VALUE;
}

COMMAND cmd_web_js_result(RXIFRM *frm, void *ctx) {
	return Return_Part(frm, WEB_PART_RESULT);
}

COMMAND cmd_web_show_rich(RXIFRM *frm, void *ctx) {
	int kind_len, data_len;
	const char *kind = Arg_Bytes(frm, 1, &kind_len);
	const char *data = Arg_Bytes(frm, 2, &data_len);

	RXA_LOGIC(frm, 1) = JS_Web_Show(kind, kind_len, (const unsigned char *)data, data_len) != 0;
	RXA_TYPE(frm, 1)  = RXT_LOGIC;
	return RXR_VALUE;
}
