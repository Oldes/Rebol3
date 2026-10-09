# Rebol 3 extension API: contracts and lifetimes

This document describes how a native extension talks to the interpreter: how
commands are numbered and called, how values cross the boundary, who owns
memory, and how handles, callbacks and errors behave. It complements the
generated function reference (`reb-lib-doc.txt`, produced from the comments in
`src/core/a-lib.c`), which documents each `RL_*` function on its own.

Everything here is derived from the sources and applies to the `matrix` branch
(3.22.x). Parts marked *(matrix)* are not on `master` yet. Source references
name the file and function so they survive line changes.

The worked reference for nearly every topic below is the `xtest` extension
(`src/extensions/xtest/`), which exercises argument passing, callbacks,
handles with path accessors, the command context and structs.

## 1. Library entry points

A DLL extension (`.rebx`) exports these symbols; `load-extension`
(`REBNATIVE(load_extension)` in `src/core/f-extension.c`) resolves them by name:

| Symbol | Required | Purpose |
|---|---|---|
| `RX_Abi` *(matrix)* | no | Returns the `RL_ABI_VERSION` the extension was built with. Missing means ABI 0. The library is refused unless `RL_MIN_SUPPORTED_ABI <= abi <= RL_ABI_VERSION`. Checked before anything else is called. |
| `RX_Init` | yes | `const char *RX_Init(int opts, RL_LIB *lib)`. Store `lib` in the global `RL`, check compatibility, and return the module source (UTF-8, NUL-terminated). Returning `NULL` fails the load with `extension-init`. |
| `RX_Call` | for commands | `int RX_Call(int cmd, RXIFRM *frm, void *ctx)`. Dispatches a command by index. May be absent if the module defines no commands. |
| `RX_Quit` | no | **Never called by the current host.** Do not rely on it for cleanup. |

`RX_Init` is the place to reject an incompatible host. The conventional checks
are `RL_VERSION` against a minimum version and `CHECK_STRUCT_ALIGN` (from
`reb-lib.h`), which compares `sizeof(REBREQ)` and `sizeof(REBEVT)` with the
host's values. `CHECK_STRUCT_ALIGN` is a compile-time size check, not a
version gate; `RX_Abi` is the version gate.

The returned source is copied by the host (`Copy_Bytes`), so a static string
is fine.

`locate-extension` (`src/mezz/sys-load.reb`) searches the modules directory
for, in order: `name-abiN-arch.rebx`, `name-abiN.rebx`, `name-arch.rebx`,
`name.rebx`, then the OS- and system-qualified names. The suffix is always
`.rebx`.

Only `RX_Abi`, `RX_Init`, `RX_Call` and `RX_Quit` are looked up. Hiding every
other symbol is recommended but not required.

## 2. Command ids

The module source returned by `RX_Init` is loaded by `load-ext-module`
(`src/mezz/sys-load.reb`). Unless the source defines its own `command`
function, the loader injects:

```rebol
cmd-index: 0
command: func [args [integer! block!]] [
    make command! reduce [args self ++ cmd-index]
]
```

So:

- Ids start at **0** and are assigned in **evaluation order**: each call to
  `command` takes the next id. It is not declaration order as such. A
  `command` call inside an `if` that does not run consumes no id, and the ids
  of later commands shift.
- Every evaluated `command` counts, whether or not its word is listed in
  `Exports:`. `Exports:` only controls which words `import` puts into the
  importing context; non-exported commands remain reachable through the
  module.
- The generator `make/tools/make-extension.r3` emits commands in the order of
  the spec's `commands:` block ("Order is significant - it fixes the command
  indices").

The host only ever sees the module source. Any generator-only metadata must be
stripped before `RX_Init` returns it.

### Accepted command specs

A command spec is an ordinary function spec, so doc strings, refinements,
type blocks and quoted parameter forms are accepted. `Make_Command`
(`f-extension.c`) rejects a parameter whose typeset contains a type outside
`RXT_ALLOWED_TYPES` (`gen-exttypes.h`): `unset!`, `none!`, `handle!`,
`logic!`, `integer!`, `decimal!`, `percent!`, `char!`, `pair!`, `tuple!`,
`time!`, `date!`, every word type, `issue!`, every string type, every block
and path type, `binary!`, `bitset!`, `vector!`, `image!`, `gob!`, `object!`,
`module!`, `port!`, `struct!` and `event!`.

Notably **not** allowed: `function!` and the other function types, `map!`,
`typeset!`, `datatype!`, `money!` and `error!`. This is why callbacks are
addressed by object and word (section 9).

### At most 15 argument slots

A frame holds `MAX_RXI_ARGS` (15) slots. A refinement takes a slot of its own
and is followed by its arguments, so `[a /part len]` uses slots 1, 2 and 3.
The limit is checked only when the command is **called**
(`Do_Command` raises `bad-command`), not when it is defined.

## 3. The call frame

`RXIFRM` is an array of 16 `RXIARG` unions of 16 bytes each (`reb-ext.h`).

- Slot 0 is metadata. `RXA_COUNT(frm)` (`args[0].bytes[0]`) is the argument
  count; `RXA_TYPE(frm, n)` (`args[0].bytes[n]`) is the `RXT_*` type of slot
  `n`.
- Slots `1..count` hold the arguments, 1-based, in spec order.
- **Slot 1 is also the result slot.** On return, the host reads slot 1 and
  `RXA_TYPE(frm, 1)` for `RXR_VALUE`, and slot 1 as a message for
  `RXR_ERROR` (section 7). Overwriting argument 1 is therefore normal.
- `RXR_BLOCK` turns slots `1..RXA_COUNT(frm)` into a new block, each slot
  converted with its own `RXA_TYPE`. To return a block of computed values you
  set the count and types yourself.

### Return codes (`enum rxi_return`)

| Code | Result of the command |
|---|---|
| `RXR_UNSET` | `unset!` |
| `RXR_NONE` | `none` |
| `RXR_TRUE`, `RXR_FALSE` | `true` / `false` |
| `RXR_VALUE` | slot 1, typed by `RXA_TYPE(frm, 1)` |
| `RXR_BLOCK` | a block built from slots `1..count` |
| `RXR_ERROR` | raises a `command-fail` error (section 7) |
| `RXR_BAD_ARGS` | same as `RXR_ERROR` under normal calls |
| `RXR_NO_COMMAND` | **`unset!`, silently** (falls to the default case) |

`RXR_FALSE` is 3, which is true in C. Helper functions that report success
should return plain `TRUE`/`FALSE` and let the dispatcher map them.

### Type tags

`RXT_*` values (`ext-types.h`) are part of the ABI and are not consecutive:
`RXT_END` 0, `RXT_UNSET` 1, `RXT_NONE` 2, `RXT_HANDLE` 3, `RXT_LOGIC` 4,
`RXT_INTEGER` 5, `RXT_DECIMAL` 6, `RXT_PERCENT` 7, `RXT_CHAR` 10, `RXT_PAIR` 11,
`RXT_TUPLE` 12, `RXT_TIME` 13, `RXT_DATE` 14, `RXT_WORD` 16, `RXT_SET_WORD` 17,
`RXT_GET_WORD` 18, `RXT_LIT_WORD` 19, `RXT_REFINEMENT` 20, `RXT_ISSUE` 21,
`RXT_STRING` 24, `RXT_FILE` 25, `RXT_EMAIL` 26, `RXT_REF` 27, `RXT_URL` 28,
`RXT_TAG` 29, `RXT_BLOCK` 32, `RXT_PAREN` 33, `RXT_PATH` 34, `RXT_SET_PATH` 35,
`RXT_GET_PATH` 36, `RXT_LIT_PATH` 37, `RXT_BINARY` 40, `RXT_BITSET` 41,
`RXT_VECTOR` 42, `RXT_IMAGE` 43, `RXT_GOB` 47, `RXT_OBJECT` 48, `RXT_MODULE` 49,
`RXT_PORT` 50, `RXT_STRUCT` 54, `RXT_EVENT` 55.

### Which `RXIARG` field holds what

Conversion is `Value_To_RXI` / `RXI_To_Value` in `f-extension.c`. The same
mapping is used for command arguments, results, `RL_Get_Value`,
`RL_Get_Field`, `RL_Set_Value`, `RL_Set_Field` and callback arguments.

| Types | Field | Accessor |
|---|---|---|
| integer, decimal, percent, pair, time | the 64 bits as stored (`int64`, `dec64`, `pair`) | `RXA_INT64`, `RXA_DEC64`, `RXA_PAIR`, `RXA_TIME` |
| logic, char | `int32a` | `RXA_LOGIC`, `RXA_CHAR` |
| every word type, issue | canonical word id in `int32a` | `RXA_WORD` |
| date | `datetime.date` + `datetime.time` | `RXA_DATE`, `RXA_DATE_TIME` |
| string, file, email, ref, url, tag, block, paren, paths, binary, bitset, gob | `series` + zero-based `index` | `RXA_SERIES`, `RXA_INDEX` |
| object, module, port | the context frame series in `addr`; `index` is meaningless (zero) | `RXA_OBJECT` |
| handle | `handle.ptr`/`handle.hob`, `handle.type` (the handle's word id), `handle.flags` | `RXA_HANDLE*` |
| tuple | `tuple_len` + `tuple_bytes` | `RXA_TUPLE*` |
| image | `image`, `width`, `height`, `image_index` | `RXA_IMAGE*` |
| vector | `vector.series`, `index`, `info` | `RXA_VECTOR_*` |
| struct | `structure.series`, `offset`, `id` | `RXA_STRUCT_*`, `RXA_STRUCT_INFO` |
| event | the whole `REBEVT` | `RXA_EVENT*` |
| unset, none | nothing | |

Word ids are always the **canonical** id, the same value `RL_Map_Word`
returns, so a word received as an argument can be compared directly with a
mapped word.

## 4. Series

### Indexes and lengths

- `RXA_INDEX` is **zero-based**.
- Strings are stored as **UTF-8** bytes. For a byte-wide string, `RXA_INDEX`
  and `RL_Series(s, RXI_SER_TAIL)` are **byte** offsets, not character
  counts. `length?` and `index?` in Rebol count characters, so they differ for
  non-ASCII text.
- `RL_Series(series, what)` returns `RXI_SER_DATA` (pointer to the first
  unit), `RXI_SER_TAIL` (used units), `RXI_SER_SIZE` (capacity),
  `RXI_SER_WIDE` (bytes per unit) and `RXI_SER_LEFT` (free units past the
  tail). The data at `index` is `data + index * wide`; there are `tail - index`
  units from there.

### Reading strings

`RL_Get_UTF8_String(series, index, &ptr)` returns the byte length from
`index` to the tail:

- For a byte-wide (UTF-8) string, `ptr` points **into the series** - a
  borrowed view.
- For a wide (unicode) series, the host encodes a **new** series and `ptr`
  points into it. That series is not referenced by anything and lives only
  until the next garbage collection (see "When the GC can run" below).

`RL_Get_String(series, index, &ptr, needs_wide)` returns a negative length
for byte data and a positive one for wide data; with `needs_wide` it widens
the series **in place**, which changes the argument the caller passed.

Neither function copies for the byte case, and neither NUL-terminates a view
that does not end at the tail. Use the returned length.

### When the GC can run

Collection is not triggered by allocation itself. An allocation that exhausts
the ballast only raises `SIG_RECYCLE`, and the collector runs the next time
the evaluator checks signals (`c-do.c`). So during a command:

- If the command evaluates **no Rebol code**, the GC cannot run, and every
  series it allocated survives until `RX_Call` returns.
- If the command evaluates Rebol code - `RL_Do_String`, `RL_Do_Binary`,
  `RL_Do_Block`, `RL_Do_Commands`, a synchronous `RL_Callback`, or
  `RL_Get_Value_Resolved` on a path - the GC may run inside that call.
  Argument series stay alive (they are on the data stack). Series the command
  allocated survive only if they are referenced from Rebol data, are among the
  5 most recently created series (`MAX_SAFE_SERIES` nursery), or are
  protected with `RL_Protect_GC(series, 1)`. Unprotect with
  `RL_Protect_GC(series, 0)`; a protected series is never freed.

The GC does not move series, but **expanding** a series can move its data. A
data pointer obtained earlier is invalid after `RL_Expand_Series`,
`RL_Set_Char` past the tail, `RL_Set_Value` past the tail, or any Rebol code
that could modify the series.

### Keeping series after the call returns

A `REBSER *`, a view pointer or a raw data pointer must not be kept after
`RX_Call` returns unless something the GC marks still references the series.
The supported way is a context handle's `hob->series` field, which the GC
marks for as long as the handle is alive (`m-gc.c`), or `RL_Protect_GC`.

### Returning strings and binaries

`RL_Make_String(size, unicode)` with `unicode = FALSE` allocates a byte series
with capacity `size` and tail 0; it is the allocator for both `string!`
(UTF-8 bytes) and `binary!`. There is no separate binary allocator. With
`unicode = TRUE` it allocates a wide series. The usual sequence:

```c
REBSER *s = RL_MAKE_STRING(len, FALSE);   // capacity len, tail 0
RL_EXPAND_SERIES(s, 0, len);              // tail = len (no reallocation: capacity suffices)
memcpy((void*)RL_SERIES(s, RXI_SER_DATA), bytes, len);
RXA_SERIES(frm, 1) = s;
RXA_INDEX(frm, 1)  = 0;
RXA_TYPE(frm, 1)   = RXT_BINARY;          // or RXT_STRING for UTF-8 text
return RXR_VALUE;
```

In-tree code sets the tail directly with `SERIES_TAIL(s)` (see
`APPEND_STRING` in `reb-ext-common.h` *(matrix)*).

### Blocks

- `RL_Make_Block(size)` allocates an empty block with room for `size` values.
- `RL_Set_Value(block, index, value, type)` writes at `index`, or appends when
  `index` is at or past the tail (and returns `TRUE`). It is the supported
  append primitive.
- `RL_Get_Value(block, index, &arg)` returns the `RXT_*` type, or 0 past the
  tail. Values use the field mapping in section 3, so word-like values give
  their canonical id in `int32a`, and series values give `series` plus a
  zero-based `index`.
- To return a block of values you built, prefer `RXR_VALUE` with
  `RXA_TYPE(frm, 1) = RXT_BLOCK`. `RXR_BLOCK` builds the block from the frame
  slots instead, so it is limited to 15 values.

## 5. Objects

- `object!`, `module!` and `port!` arguments arrive as the context frame series
  in `RXA_OBJECT(frm, n)` (the `addr` field). `RXA_TYPE` is `RXT_OBJECT` (48),
  `RXT_MODULE` (49) or `RXT_PORT` (50). `RXA_INDEX` has no meaning for them.
- `RL_Get_Field(obj, word, &arg)` takes a canonical word id - from
  `RL_Map_Word`, from a word argument, or from `RL_Words_Of_Object`; they are
  the same ids. It returns the field's `RXT_*` type, or **0** (`RXT_END`) when
  the object has no such field. It reads the stored value: no getter or other
  Rebol code runs.
- `RL_Set_Field(obj, word, value, type)` returns `type`, or 0 when the field
  does not exist or is protected. It cannot add fields.
- `RL_Words_Of_Object(obj)` returns a **copy**: a `malloc`ed array whose
  element 0 is the count plus one, followed by the canonical ids and a 0
  terminator. It does not change when the object does. The caller frees it
  with `free()`. The same applies to `RL_Map_Words` and to the string
  returned by `RL_Word_String`.
- There is no object allocator in the API. To return a new object, evaluate
  source with `RL_Do_String` and return its result, or fill an object the
  caller passed in.

## 6. Handles

### Registering a handle type

```c
REBHSP spec;
spec.size     = sizeof(MY_CONTEXT);          // bytes allocated per handle
spec.flags    = HANDLE_REQUIRES_HOB_ON_FREE; // or 0
spec.free     = My_free;      // int (*)(void *)
spec.get_path = My_get_path;  // int (*)(REBHOB *, REBCNT word, REBCNT *type, RXIARG *arg)
spec.set_path = My_set_path;  // same signature
spec.mold     = My_mold;      // int (*)(REBHOB *, REBSER *)
My_Handle = RL_REGISTER_HANDLE_SPEC(cb_cast("my-handle"), &spec);
```

- `REBHSP` is `{ size, flags, free, get_path, set_path, mold }`, in that order
  (`sys-value.h`). Any callback may be `NULL`.
- The fields are **copied** into the host table (`Register_Handle_Spec` in
  `c-handle.c`), so a stack-allocated spec is fine.
- The return value is the handle name's **word id** (not a table index), or
  `NOT_FOUND` for an invalid name. Pass this id to `RL_Make_Handle_Context`
  and compare it with `RXA_HANDLE_TYPE` to type-check arguments.
- One extension may register several handle types.
- **Names are global** across the whole interpreter, and registration is
  first-wins:
  - Same name, same `size`: the existing registration is returned
    **unchanged**. The new callbacks are silently ignored.
  - Same name, different `size`: raises a `handle-exists` error.

  Use names unlikely to collide, such as a prefix with your extension's
  name. `system/catalog/handles` lists what is registered.
- At most **64** handle types can be registered in total; the 65th
  registration **crashes** the interpreter (`RP_MAX_HANDLES`).

### Creating and returning a handle

`RL_Make_Handle_Context(sym)` allocates a `REBHOB` whose `data` points to
`spec.size` zeroed bytes. Your context lives in that memory. `hob->series` is
an optional series the GC will mark while the handle lives; use it to keep
related Rebol data alive.

To return it (`RETURN_HANDLE` in `reb-ext-common.h` *(matrix)*):

```c
RXA_HANDLE(frm, 1)       = hob;
RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
RXA_HANDLE_FLAGS(frm, 1) = hob->flags;   // includes HANDLE_CONTEXT
RXA_TYPE(frm, 1)         = RXT_HANDLE;
return RXR_VALUE;
```

`HANDLE_CONTEXT` must be in the flags, or the value is treated as a plain
pointer handle: it gets no callbacks and no GC tracking. When a handle comes in as an
argument, check `RXA_TYPE(frm, n) == RXT_HANDLE && RXA_HANDLE_TYPE(frm, n) ==
My_Handle` (`FRM_IS_HANDLE` *(matrix)*), then use
`RXA_HANDLE_CONTEXT(frm, n)` for the `REBHOB *`. A released context has
`HANDLE_CONTEXT_USED` cleared.

### Callbacks

| Callback | Called | Receives |
|---|---|---|
| `free` | when the GC collects an unreferenced handle, or on `RL_Free_Handle_Context` (`Free_Hob` in `m-pools.c`) | `hob->data` by default; the `REBHOB *` itself when `spec.flags` has `HANDLE_REQUIRES_HOB_ON_FREE`. In that mode, if the callback marks the hob (`MARK_HOB`), the handle is kept alive instead of being freed. `HANDLE_REQUIRES_HOB_ON_FREE` affects only `free`. After `free`, the host zeroes and releases `data`. |
| `mold` | when the handle is molded or formed (`Mold_Handle` in `s-mold.c`) | the hob and a shared scratch string. Write your text from the **start** of that string (reset its tail to 0) and return the number of bytes written. The host appends that many bytes after `#(handle! name `. Return 0 to add nothing. No NUL terminator is needed. |
| `get_path` | on `handle/word` (`PD_Handle` in `t-handle.c`) | the canonical id of `word`; fill `*type` and `*arg` (same field mapping as section 3) and return `PE_USE`. Anything else is reported as a bad selector. |
| `set_path` | on `handle/word: value` | the canonical id of `word`, and the new value in `*type`/`*arg`. Return `PE_OK` to accept, `PE_BAD_SET_TYPE` for a wrong type, `PE_BAD_SET` for a field that cannot be set. |

- Path callbacks see one selector at a time. A longer path continues on the
  value `get_path` returned.
- `handle/type` is answered by the host for every handle, even when
  `get_path` does not know the word.
- The path callbacks may use `RL_*` word functions such as `RL_Find_Word` and
  `RL_Map_Word`. `free` runs during garbage collection and must not allocate
  Rebol series or evaluate Rebol code.

### Path result codes (`enum Path_Eval_Result`)

| Code | Meaning to the host |
|---|---|
| `PE_OK` | set accepted |
| `PE_SET` | (internal) value was stored by the evaluator |
| `PE_USE` | get: use the value placed in `*type`/`*arg` |
| `PE_NONE` | result is `none` |
| `PE_BAD_SELECT` | unknown selector |
| `PE_BAD_SET` | the field cannot be set |
| `PE_BAD_RANGE` | selector out of range |
| `PE_BAD_SET_TYPE` | wrong type for this field |
| `PE_BAD_ARGUMENT` | invalid argument |

## 7. Errors

To fail a command, put a pointer to a NUL-terminated C string in
`RXA_SERIES(frm, 1)` and return `RXR_ERROR` (`RETURN_ERROR` in
`reb-ext-common.h` *(matrix)*):

```c
RXA_SERIES(frm, 1) = (REBSER*)"cannot open device";
return RXR_ERROR;
```

The host copies the bytes into a new string and raises `command-fail` with it
(`Do_Command` in `f-extension.c`), so a static string is safe. A `NULL`
message raises the error with `none`. The error is catchable with `try`.

- `RXR_BAD_ARGS` currently behaves exactly like `RXR_ERROR`.
- There is no way to return a structured `error!` value. To raise a specific
  error, evaluate `cause-error` (or a `make error!`) with `RL_Do_String`
  instead of returning.
- Commands run through `RL_Do_Commands` behave differently: `RXR_ERROR`,
  `RXR_BAD_ARGS` and `RXR_NO_COMMAND` all yield `unset!` **without raising an
  error** (`Do_Commands` in `f-extension.c`).

## 8. Command context and `RL_Do_Commands`

`RL_Do_Commands(block, flags, ctx)` evaluates a block made only of command
calls, optionally with `set-word:` targets. Arguments must be literal values,
words, paths or parens; there is no general evaluation. It runs
**synchronously** and returns nothing.

If `ctx` is not `NULL`, the host stores the block in `ctx->block` and, before
each command, its 0-based position in `ctx->index`. The same pointer is
passed as the third argument of `RX_Call`. `ctx->envr` is yours. The context
only needs to live for the duration of the `RL_Do_Commands` call. For ordinary
command calls, `ctx` is `NULL`.

## 9. Callbacks into Rebol

A callback names a function by an **object and a word**; a function value
cannot be passed to a command at all (section 2). The canonical command shape
is `command [ctx [object!] fn [word!]]`, read as
`RXA_OBJECT(frm, 1)` and `RXA_WORD(frm, 2)`.

```c
typedef struct rxi_callback_info {
    u32     flags;   // RXC_* bits (see below)
    REBSER *obj;     // object holding the function
    u32     word;    // word id of the function
    RXIARG *args;    // args[0] = count + types, args[1..] = values
    RXIARG  result;  // result value (sync) or error info
} RXICBI;
```

Arguments use the frame convention: `RXI_COUNT(args)` and
`RXI_TYPE(args, n)` live in `args[0]`, and values go in `args[1..n]`. If the
function takes fewer arguments the extra ones are ignored; missing ones are
filled with `none`. Each argument is type-checked against the function spec.

### Flags are bit numbers

`RXC_NONE` 0, `RXC_ASYNC` 1, `RXC_QUEUED` 2, `RXC_DONE` 3, `RXC_ALLOC` 4 are
**bit indexes**. The host tests them with `GET_FLAG`, i.e. `1 << n`. Set them
with `SET_FLAG(cbi->flags, RXC_ASYNC)` or `cbi->flags |= 1u << RXC_ASYNC`.
Writing `cbi->flags = RXC_ASYNC | RXC_ALLOC` sets the wrong bits, so the call
silently runs **synchronously**.

| Bit | Set by | Meaning |
|---|---|---|
| `RXC_ASYNC` | extension | queue the call instead of running it now |
| `RXC_QUEUED` | host | the event was queued |
| `RXC_DONE` | host | the queued call ran (only when `RXC_ALLOC` is not set) |
| `RXC_ALLOC` | extension | the host frees `cbi` and `cbi->args` after the call |

### Synchronous

Without `RXC_ASYNC`, `RL_Callback` calls the function immediately and returns
the result's `RXT_*` type, with the value in `cbi->result`. `cbi` and `args`
may live on the C stack. On failure it returns 0 and `GET_EXT_ERROR(&result)`
holds `RXE_NO_WORD` (no such word in the object), `RXE_NOT_FUNC` (the word is
not a function) or `RXE_BAD_ARGS` (`result.int32b` is the offending argument
number).

The callback runs Rebol code, so the GC may run (section 4). If the Rebol code
raises an error, the interpreter unwinds with `longjmp` **through the
extension's C frames**. Do not hold locks or unreleased resources across a
synchronous callback. Callbacks may be nested and may call other commands.

### Asynchronous

With `RXC_ASYNC`, `RL_Callback` queues an `EVT_CALLBACK` event and returns
nonzero if it was queued (0 if the event queue is full). The function runs
later, when Rebol processes events: during `wait`. `cbi` and `args` are read
after `RX_Call` returns, so they must not live on the stack:

- **Fire and forget:** set `RXC_ASYNC` and `RXC_ALLOC`, and allocate both
  `cbi` and `args` with `malloc`/`calloc`. The host releases them with
  `free()` (`REBNATIVE(do_callback)`). They must come from the same C runtime
  as the host.
- **Pollable:** set only `RXC_ASYNC`. After the call runs, the host sets
  `RXC_DONE` and leaves `cbi->result` filled; you free the memory.

An error inside an asynchronous callback is raised in the code that processed
the event.

## 10. Memory returned by the API

| Function | Memory | Release |
|---|---|---|
| `RL_Map_Words`, `RL_Words_Of_Object`, `RL_Word_String` | `malloc` | `free()` |
| `RL_Alloc(size)` | interpreter's accounted allocator | `RL_Free(ptr, size)` with the same size |
| `RL_Mem_Alloc` | pooled, hidden header | `RL_Mem_Free` |
| `RL_Make_*` series, handle contexts | garbage collected | nothing (see section 4 and 6) |
| async `RXICBI` with `RXC_ALLOC` | `malloc` by the extension | `free()` by the host |

Memory the host gives back to the core with a known size (for example a
codec's output) must come from `RL_Alloc`.

## 11. Known issues

These are behaviours of the current host that extension authors should know
about. They are worth fixing upstream.

- **Handle name collisions are silent** when the sizes match: the second
  extension's callbacks are ignored (section 6).
- **The 65th handle type crashes** the interpreter instead of raising an error.
- **`load-extension` never checks the extension table bound.** `Ext_List` has
  64 entries and `Ext_Next` is not checked before use, so loading a 65th
  extension writes past the array.
- **`RX_Quit` is never called.**
- **`RXR_NO_COMMAND` yields `unset!`** instead of an error, for normal calls
  and for `RL_Do_Commands`.
- **`RL_Do_Commands` swallows `RXR_ERROR` and `RXR_BAD_ARGS`** and continues
  with `unset!`.
- **The 15-slot limit is checked at call time**, so a command spec that is too
  long is accepted and fails on every call.
- **`RXR_BAD_ARGS` is indistinguishable from `RXR_ERROR`.**
