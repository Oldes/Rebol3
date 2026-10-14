// Smoke tests for the Rebol/Base wasm build.
// Usage: node make/wasm/test-wasm.cjs path/to/r3.js
// Exit code 0 when all tests pass.

'use strict';
const path = require('path');

const file = process.argv[2];
if (!file) {
	console.error('Usage: node test-wasm.cjs path/to/r3.js');
	process.exit(2);
}
const createRebol = require(path.resolve(file));

const stripAnsi = (s) => s.replace(/\x1B\[[0-9;?]*[A-Za-z]/g, '');

let failed = 0, passed = 0;

function check(name, cond, detail) {
	if (cond) { passed++; console.log('  ok   ' + name); }
	else { failed++; console.log('  FAIL ' + name + '\n       got: ' + JSON.stringify(detail)); }
}

// Creates an interpreter instance with a helper to evaluate code.
async function makeRebol(options) {
	const io = {output: '', errput: '', inputs: []};
	const R3 = await createRebol(Object.assign({
		onRebolOutput: (text) => { io.output += text; },
		onRebolInput:  () => (io.inputs.length ? io.inputs.shift() : null),
		printErr:      (text) => { io.errput += text + '\n'; }, // Emscripten runtime messages
	}, options));
	io.run = (src) => {
		io.output = '';
		const ptr = R3.stringToNewUTF8(src);
		const res = R3._r3_do(ptr);
		R3._free(ptr);
		return {res, out: stripAnsi(io.output)};
	};
	io.init = () => {
		io.output = '';
		const res = R3._r3_init();
		return {res, out: stripAnsi(io.output)};
	};
	return {R3, io};
}

// Fake XMLHttpRequest for the web extension tests (passed as
// Module.XMLHttpRequest). Records every request in `xhrLog`.
const xhrLog = [];
const latin1 = (utf8) => Buffer.from(utf8, 'utf8').toString('latin1'); // "binary string"
function fakeResponse(req) {
	const path = req.url.replace(/^https?:\/\/[^/]+/, '');
	switch (path) {
	case '/hello.txt':
		return {status: 200, headers: 'content-type: text/plain; charset=utf-8\r\ncontent-length: 5\r\n', body: 'hello'};
	case '/utf8.txt':
		return {status: 200, headers: 'content-type: text/plain; charset=utf-8\r\n', body: latin1('ěščř')};
	case '/bin':
		return {status: 200, headers: 'content-type: application/octet-stream\r\n', body: '\x00\x01\xFF'};
	case '/echo':
		return {status: 200, headers: 'content-type: text/plain\r\n', body: req.body || ''};
	case '/cors':
		return {throw: 'NetworkError: Failed to execute send'};
	default:
		return {status: 404, headers: 'content-type: text/plain\r\n', body: 'Not Found'};
	}
}
class FakeXHR {
	open(method, url, async) { this.req = {method, url, async, headers: {}}; xhrLog.push(this.req); }
	overrideMimeType(type) { this.req.mime = type; }
	setRequestHeader(name, value) { this.req.headers[name.toLowerCase()] = value; }
	send(body) {
		this.req.body = body ? Buffer.from(body).toString('latin1') : null;
		const res = fakeResponse(this.req);
		if (res.throw) throw new Error(res.throw);
		this.status = res.status;
		this.responseText = res.body;
		this.headers = res.headers;
	}
	getAllResponseHeaders() { return this.headers; }
}
const lastXhr = () => xhrLog[xhrLog.length - 1];

// Minimal fake DOM for the `dom` tests (only what rebolDom uses).
// Selectors: #id, .class or tag name.
class FakeElement {
	constructor(tag) {
		this.tagName = tag.toUpperCase();
		this.id = ''; this.textContent = ''; this.innerHTML = '';
		this.attrs = {}; this.children = []; this.parentNode = null;
		this.classes = new Set(); this.styles = {};
		const self = this;
		this.classList = {
			add: (n) => self.classes.add(n),
			remove: (n) => self.classes.delete(n),
			toggle: (n) => (self.classes.has(n) ? self.classes.delete(n) : self.classes.add(n)),
		};
		this.style = {setProperty: (k, v) => { self.styles[k] = v; }};
	}
	setAttribute(n, v) { this.attrs[n] = String(v); }
	removeAttribute(n) { delete this.attrs[n]; }
	appendChild(c) { c.parentNode = this; this.children.push(c); return c; }
	remove() {
		if (!this.parentNode) return;
		const a = this.parentNode.children;
		a.splice(a.indexOf(this), 1);
		this.parentNode = null;
	}
	matches(sel) {
		if (sel[0] === '#') return this.id === sel.slice(1);
		if (sel[0] === '.') return this.classes.has(sel.slice(1));
		return this.tagName === sel.toUpperCase();
	}
	closest(sel) {
		for (let e = this; e; e = e.parentNode) if (e.matches(sel)) return e;
		return null;
	}
}
function makeDocument() {
	const body = new FakeElement('body');
	const listeners = [];
	const walk = (e, out) => { for (const c of e.children) { out.push(c); walk(c, out); } return out; };
	const el = (tag, props, parent) => (parent || body).appendChild(Object.assign(new FakeElement(tag), props));
	const doc = {
		body, el,
		createElement: (tag) => new FakeElement(tag),
		querySelectorAll: (sel) => walk(body, []).filter((e) => e.matches(sel)),
		querySelector: (sel) => doc.querySelectorAll(sel)[0] || null,
		addEventListener: (type, fn) => listeners.push({type, fn}),
		removeEventListener: (type, fn) => {
			const i = listeners.findIndex((l) => l.type === type && l.fn === fn);
			if (i >= 0) listeners.splice(i, 1);
		},
		fire: (type, target, extra) => {
			for (const l of listeners.slice()) if (l.type === type) l.fn(Object.assign({type, target}, extra || {}));
		},
		listenerCount: () => listeners.length,
	};
	return doc;
}
const tick = () => new Promise((resolve) => setTimeout(resolve, 10));

async function main() {
	console.log('===start-group=== "wasm host"');
	{
		const {R3, io} = await makeRebol({rebolBoot: 'secure none\nwasm-boot-value: 42'});
		const run = io.run;
		let r;

		check('r3_do before r3_init returns -1', R3._r3_do(0) === -1, null);

		r = io.init();
		check('r3_init returns 0', r.res === 0, r);
		check('successful boot code prints no result', !r.out.includes('=='), r);
		check('second r3_init is a no-op', R3._r3_init() === 0, null);

		r = run('wasm-boot-value');
		check('boot code was evaluated', r.out.includes('== 42'), r);

		r = run('1 + 2');
		check('evaluation result is printed', r.out.includes('== 3'), r);

		run('wasm-test-value: 10');
		r = run('wasm-test-value * 2');
		check('context persists between calls', r.out.includes('== 20'), r);

		r = run('print "ěščř 😀"');
		check('UTF-8 output', r.out.includes('ěščř 😀'), r);

		r = run('1 / 0');
		check('error is reported', /error/i.test(r.out), r);
		r = run('3 * 3');
		check('interpreter works after error', r.out.includes('== 9'), r);

		r = run('print tty?');
		check('tty? is false (no terminal in the browser)', r.out.includes('false'), r);

		r = run('write %/tmp/wasm-test.txt "hello" read/string %/tmp/wasm-test.txt');
		check('file write/read allowed by boot `secure none`', r.out.includes('== "hello"'), r);

		io.inputs = ['typed text'];
		r = run('ask ""');
		check('ask reads a line', r.out.includes('== "typed text"'), r);

		io.inputs = [];
		r = run('ask ""');
		check('ask at end of input does not crash', r.res !== undefined, r);

		// Using PRINT, so the result does not depend on how logic! values are molded.
		r = run('print error? try [read http://example.com]');
		check('network is not available but does not crash', r.out.includes('true'), r);

		io.errput = '';
		r = run('t: now/precise wait 0.05 print 0:0:0.04 <= difference now/precise t');
		check('wait honors the timeout', r.out.includes('true'), r);
		check('wait does not use unsupported syscalls', !io.errput.includes('unsupported syscall'), io.errput);
	}
	{
		// Command line arguments: `-q` and values after `--` for the script.
		const {io} = await makeRebol({rebolArgs: ['-q', '--', 'one', 'two']});
		let r = io.init();
		check('r3_init with arguments returns 0', r.res === 0, r);
		r = io.run('print system/options/quiet');
		check('-q sets system/options/quiet', r.out.includes('true'), r);
		r = io.run('print mold system/options/args');
		check('arguments after -- are in system/options/args', r.out.includes('"one" "two"'), r);
	}
	{
		// Without arguments the defaults are kept.
		const {io} = await makeRebol({});
		io.init();
		const r = io.run('print system/options/quiet');
		check('without -q system/options/quiet is false', r.out.includes('false'), r);
	}
	console.log('===end-group===');
	console.log('===start-group=== "web extension (http/https schemes)"');
	{
		const {R3, io} = await makeRebol({rebolArgs: ['-q'], XMLHttpRequest: FakeXHR});
		const run = io.run;
		let r;
		io.init();

		r = run('read http://example.test/hello.txt');
		check('read text is decoded to string', r.out.includes('== "hello"'), r);
		check('request is synchronous GET', lastXhr().method === 'GET' && lastXhr().async === false, lastXhr());
		check('response is read as binary string', /x-user-defined/.test(lastXhr().mime), lastXhr());

		r = run('read/binary http://example.test/hello.txt');
		check('read/binary keeps binary', r.out.includes('#{68656C6C6F}'), r);

		r = run('read http://example.test/bin');
		check('binary content type stays binary (all byte values)', r.out.includes('#{0001FF}'), r);

		r = run('read http://example.test/utf8.txt');
		check('UTF-8 body', r.out.includes('"ěščř"'), r);

		r = run('read/lines http://example.test/hello.txt');
		check('read/lines', r.out.includes('== ["hello"]'), r);

		r = run('result: read/all http://example.test/hello.txt  print [result/1 result/2/content-length result/3]');
		check('read/all returns [status headers body]', r.out.includes('200 5 hello'), r);

		r = run('print error? try [read http://example.test/missing]');
		check('404 is an error', r.out.includes('true'), r);
		r = run('print first read/all http://example.test/missing');
		check('read/all returns 404 without error', r.out.includes('404'), r);

		r = run('err: try [read http://example.test/cors]  print [err/id]');
		check('network failure is no-connect error', r.out.includes('no-connect'), r);
		r = run('print err/arg2');
		check('error message is passed from JS', r.out.includes('NetworkError'), r);

		r = run('write http://example.test/echo "a=1"');
		check('write posts data', r.out.includes('== "a=1"') && lastXhr().method === 'POST', [r, lastXhr()]);
		check('write sets form content type', /x-www-form-urlencoded/.test(lastXhr().headers['content-type']), lastXhr());

		r = run('write http://example.test/echo #{0001FF}');
		check('write binary data', lastXhr().body === '\x00\x01\xFF' && /octet-stream/.test(lastXhr().headers['content-type']), lastXhr());

		r = run('write http://example.test/echo [PUT [X-Test: "1" User-Agent: "me"] "data"]');
		check('write dialect sets method and headers', lastXhr().method === 'PUT' && lastXhr().headers['x-test'] === '1', lastXhr());
		check('browser controlled headers are skipped', !('user-agent' in lastXhr().headers), lastXhr());

		r = run('read https://example.test/hello.txt');
		check('https scheme', r.out.includes('== "hello"') && lastXhr().url === 'https://example.test/hello.txt', [r, lastXhr()]);

		r = run('print query http://example.test/hello.txt \'status-code');
		check('query uses HEAD', r.out.includes('200') && lastXhr().method === 'HEAD', [r, lastXhr()]);

		R3.rebolHttpProxy = 'https://proxy.test/?url=';
		run('read http://example.test/hello.txt');
		check('proxy prefix is used', lastXhr().url === 'https://proxy.test/?url=' + encodeURIComponent('http://example.test/hello.txt'), lastXhr());
		R3.rebolHttpProxy = null;
	}
	console.log('===end-group===');
	console.log('===start-group=== "web extension (dom)"');
	{
		const doc = globalThis.document = makeDocument();
		const title = doc.el('h1', {id: 'title'});
		const input = doc.el('input', {id: 'name', value: 'Oldes'});
		const list  = doc.el('ul', {id: 'list'});
		const btn   = doc.el('button', {id: 'btn'});
		doc.el('p', {}).classList.add('old');
		doc.el('p', {}).classList.add('old');

		const {io} = await makeRebol({rebolArgs: ['-q']});
		const run = io.run;
		let r;
		io.init();

		r = run('dom/text "#title" "Hello"');
		check('dom/text sets text, returns count', r.out.includes('== 1') && title.textContent === 'Hello', [r, title.textContent]);

		run('dom/text "#title" {a"b\\c^/d}');
		check('strings are escaped for JS', title.textContent === 'a"b\\c\nd', title.textContent);

		run('dom/html "#title" "<b>x</b>"');
		check('dom/html', title.innerHTML === '<b>x</b>', title.innerHTML);

		r = run('dom/value "#name"');
		check('dom/value', r.out.includes('== "Oldes"'), r);

		run('dom/set "#name" \'value "new"');
		r = run('dom/get "#name" \'value');
		check('dom/set and dom/get a property', input.value === 'new' && r.out.includes('== "new"'), [r, input.value]);

		r = run('print none? dom/get "#missing" \'value');
		check('dom/get of no element is none', r.out.includes('true'), r);

		run('dom/attr "#btn" \'title "Run"');
		check('dom/attr sets', btn.attrs.title === 'Run', btn.attrs);
		run('dom/attr "#btn" \'title none');
		check('dom/attr none removes', !('title' in btn.attrs), btn.attrs);

		run('dom/style "#btn" [color: "red" padding: "8px" margin: 0]');
		check('dom/style', btn.styles.color === 'red' && btn.styles.padding === '8px' && btn.styles.margin === '0', btn.styles);

		run('dom/class "#btn" \'active');
		check('dom/class adds', btn.classes.has('active'), [...btn.classes]);
		run('dom/class/remove "#btn" \'active');
		check('dom/class/remove', !btn.classes.has('active'), [...btn.classes]);
		run('dom/class/toggle "#btn" "on"');
		check('dom/class/toggle', btn.classes.has('on'), [...btn.classes]);

		r = run('dom/append "#list" \'li "item"');
		check('dom/append text', r.out.includes('== 1') && list.children[0].textContent === 'item', [r, list.children]);
		run('dom/append/html "#list" \'li "<i>x</i>"');
		check('dom/append/html', list.children[1].innerHTML === '<i>x</i>', list.children[1]);
		r = run('dom/append "#missing" \'li none');
		check('dom/append to no element returns 0', r.out.includes('== 0'), r);

		r = run('dom/count ".old"');
		check('dom/count', r.out.includes('== 2'), r);
		r = run('dom/remove ".old"');
		check('dom/remove returns count', r.out.includes('== 2') && doc.querySelectorAll('.old').length === 0, r);

		// events
		run('clicked: none  click-id: dom/on "#btn" \'click [clicked: event/id]');
		doc.fire('click', btn, {clientX: 5, clientY: 6});
		r = run('print mold clicked');
		check('handler is not run synchronously', r.out.includes('none'), r);
		await tick();
		r = run('print mold clicked');
		check('dom/on handler gets the event', r.out.includes('"btn"'), r);

		run('ev: none  dom/on "li" \'click [ev: event]');
		run('dom/append "#list" \'li "new"');
		doc.fire('click', list.children[2], {key: null});
		await tick();
		r = run('print [ev/type ev/id ev/x]');
		check('delegated handler works for elements added later', r.out.includes('click none none'), r);

		run('n: 0  dom/on "#btn" \'click [n: n + 1]');
		r = run('js "document.fire(\'click\', document.querySelector(\'#btn\'))"  print n');
		check('event triggered from Rebol does not re-enter it', r.out.includes('0'), r);
		await tick();
		r = run('print n');
		check('...and is handled after the evaluation', r.out.includes('1'), r);

		const before = doc.listenerCount();
		r = run('print dom/off click-id');
		check('dom/off removes the listener', r.out.includes('true') && doc.listenerCount() === before - 1, [r, before, doc.listenerCount()]);
		run('clicked: none');
		doc.fire('click', btn);
		await tick();
		r = run('print mold clicked');
		check('removed handler is not called', r.out.includes('none'), r);

		run('dom/on "#title" \'click [1 / 0]');
		io.output = '';
		doc.fire('click', title);
		await tick();
		check('error in a handler is printed', /error/i.test(stripAnsi(io.output)), io.output);
		r = run('2 + 2');
		check('interpreter works after a handler error', r.out.includes('== 4'), r);

		delete globalThis.document;
	}
	console.log('===end-group===');
	console.log('===start-group=== "web extension (show)"');
	{
		const shown = [];
		const {io} = await makeRebol({
			rebolArgs: ['-q'], rebolBoot: 'secure none',
			onRebolShow: (kind, bytes) => shown.push({kind, text: Buffer.from(bytes).toString('utf8'), bytes: [...bytes]}),
		});
		const run = io.run;
		const last = () => shown[shown.length - 1];
		let r;
		io.init();

		r = run('show "<b>bold</b>"');
		check('html is passed as is', last().kind === 'html' && last().text === '<b>bold</b>', last());
		check('show returns unset (prints nothing)', !r.out.includes('=='), r);

		run('show [[1 "a<b"] [none 2.5]]');
		check('table with escaped cells', last().text === '<table><tr><td>1</td><td>a&lt;b</td></tr><tr><td>none</td><td>2.5</td></tr></table>', last());

		run('show/header [[Name Year] ["Rebol" 1997]]');
		check('/header makes th cells in the first row only', last().text === '<table><tr><th>Name</th><th>Year</th></tr><tr><td>Rebol</td><td>1997</td></tr></table>', last());

		run('show ["a" "b"]');
		check('rows which are not blocks are single cells', last().text === '<table><tr><td>a</td></tr><tr><td>b</td></tr></table>', last());

		run('show #{89504E47}');
		check('binary is an image', last().kind === 'image' && last().bytes.join() === '137,80,78,71', last());

		run('write %/tmp/pic.svg {<svg/>}  show %/tmp/pic.svg');
		check('file is read and shown as an image', last().kind === 'image' && last().text === '<svg/>', last());

		const hex = (bytes) => Buffer.from(bytes).toString('hex').toUpperCase();

		run('show make image! [2x1 #{FF000000FF00}]');
		check('image! is passed as PNG', last().kind === 'image' && last().bytes.slice(0, 4).join() === '137,80,78,71', last());

		run('img: make image! [2x1 #{FF000000FF00} #{80FF}]  show next img');
		r = run('print enbase encode \'png img 16');
		check('image! is shown from its head (with alpha)', r.out.includes(hex(last().bytes)), [r, hex(last().bytes)]);

		r = run('print error? try [show %/tmp/missing.png]');
		check('missing file is an error', r.out.includes('true'), r);
	}
	{
		// Without Module.onRebolShow (no page) only a placeholder is printed.
		const {io} = await makeRebol({rebolArgs: ['-q']});
		io.init();
		const r = io.run('show "<i>x</i>"');
		check('without a page a placeholder is printed', r.out.includes('[html: 8 bytes]'), r);
	}
	console.log('===end-group===');
	console.log('===start-group=== "web extension (js)"');
	{
		const {io} = await makeRebol({rebolArgs: ['-q']});
		const run = io.run;
		let r;
		io.init();

		r = run('js "1 + 2"');
		check('number result', r.out.includes('== 3'), r);

		globalThis.wasmTestStr = 'a"b^c\n\t';
		r = run('print [length? s: js "wasmTestStr"  s == "a^\"b^^c^/^-"]');
		check('string with escaped chars round-trips', r.out.includes('7 true'), r);

		r = run('print mold collect [foreach v js "[1, 2.5, \'x\', true, null]" [keep type? v]]');
		check('array -> block of typed values', r.out.includes('[integer! decimal! string! logic! none!]'), r);

		r = run('m: js "({a: 1, \'b c\': \'x\', n: {k: [2]}})"  print [map? m  m/a  select m "b c"  m/n/k/1]');
		check('object -> map (word and string keys, nested)', r.out.includes('true 1 x 2'), r);

		r = run('js "new Uint8Array([0, 1, 255])"');
		check('typed array -> binary', r.out.includes('#{0001FF}'), r);

		r = run('print [decimal? n: js "NaN"  (mold n) = mold 1.#NaN]');
		check('NaN -> 1.#NaN', r.out.includes('true true'), r);
		r = run('print [(js "Infinity") = 1.#INF  (js "-Infinity") = -1.#INF]');
		check('Infinity -> 1.#INF, -Infinity -> -1.#INF', r.out.includes('true true'), r);

		r = run('print js "Math"');
		check('other objects -> their string form', r.out.includes('[object Math]'), r);

		r = run('js "globalThis.wasmFromRebol = 6 * 7"');
		check('code runs in the global scope', globalThis.wasmFromRebol === 42, globalThis.wasmFromRebol);

		r = run('err: try [js "throw new Error(\'boom\')"]  print [error? err  err/arg1]');
		check('exception -> Rebol error with the message', r.out.includes('true JavaScript: Error: boom'), r);

		r = run('print error? try [js "1 +"]');
		check('syntax error -> Rebol error', r.out.includes('true'), r);

		r = run('3 * 3');
		check('interpreter works after JS errors', r.out.includes('== 9'), r);
	}
	console.log('===end-group===');
	console.log('===start-group=== "wasm host (other instances)"');
	{
		// Without boot code the default security policy is kept.
		const {io} = await makeRebol({});
		io.init();
		const r = io.run('either error? err: try [write %/tmp/wasm-test.txt "hello"] [err/id][read/string %/tmp/wasm-test.txt]');
		check('without boot code, write returns (written or denied)', /== ("hello"|security)/.test(r.out), r);
	}
	{
		// A failing boot code is reported, but the interpreter stays usable.
		const {io} = await makeRebol({rebolBoot: '1 / 0'});
		let r = io.init();
		check('r3_init returns 0 even when boot code fails', r.res === 0, r);
		check('boot code error is printed', /error/i.test(r.out), r);
		r = io.run('2 + 2');
		check('interpreter works after boot error', r.out.includes('== 4'), r);
	}
	console.log('===end-group===');
	console.log(`passed: ${passed} failed: ${failed}`);
	process.exit(failed ? 1 : 0);
}

main().catch((e) => {
	console.error(e);
	process.exit(1);
});
