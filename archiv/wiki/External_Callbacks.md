# External Callbacks

## User Comments

Comments here.

## Callback Directions

**WARNING: The documentation below is older and obsolete.**

There are two fundamental types of cross-domain callbacks:

 * Callbacks that pass from the **REBOL domain** to the **C domain**.
 * Callbacks that pass from the **C domain** to the **REBOL domain**.

The first is not difficult. It is done by way of a COMMAND in an extension module.

The second is more difficult and is the subject of this document.

## Types of C Callbacks

There are two types of callbacks from C to REBOL:

 * **Synchronous callback** - a direct call
 * **Asynchronous callback** - an event driven call

### Synchronous Callback

A **synchronous callback** passes control back through the REBOL Extension API, which generates the necessary stack frame, and re-enters the interpreter's function evaluator. It's like calling a function via APPLY.

The arguments to the function are passed in a standard REBOL extension format, and the result is returned in that same format. Simple immediate values and series values with offsets can be passed and returned. More complex datatypes cannot be passed, and when required, must be handled with context references (for example by passing a context-relative word or block relative index to locate the more complex datatype.)

The disadvantages of this method are:

 * The C stack now holds state from both the REBOL and C domains.
 * An exception (such as an ERROR or THROW) within the REBOL may throw the C stack completely past the C domain, losing the ability to conclude any state related to that domain (e.g. allocation, open files, etc.)
 * GC can occur during the REBOL code, invalidating any REBOL series that may have been created exclusively in the C domain and not otherwise referenced from REBOL.

It should be noted that good coding practices can prevent most of these problems. In addition, the
APPLY function can be told via an option to catch exceptions and return them directly, rather than throwing the stack past the C domain.

### Asynchronous callback

An event can be used to trigger a callback. This happens in an asynchronous fashion. The C code calls the extension API which builds an event to hold the callback's function and argument list. The event is queued to the system port's event list, and a special signal bit is set, and the interpreter will interrupt evaluation to process the callback function.

This method allows, for example, C code to register specific C-based callbacks with an operating system service that executes those callbacks in a fashion similar to an interrupt.

REBOL devices use this method as the primary mechanism for all asynchronous I/O operations.

The main disadvantage of this method is that a programmer must be careful not to evaluate code that may conflict with the callback function. In addition, an event driven callback has no return value (it will be ignored) so any results must be stored in a contextual variable (object or module).

## Special Issues

### Allocation and GC

**REBOL uses automatic memory management, so REBOL and C code cannot be directly merged or referenced.**

In addition, if the C code allocates REBOL series such as a string, image, or block, that memory is "volatile" because without proper referencing, it can be targeted for garbage collection.

Solutions to this problem are:

 * A mode where **GC is intentionally disabled** within the callback, to prevent series GC.
 * A **protection flag** set on extension-created REBOL series which prevents these from being collected. Such series would need to be tracked and freed by the C code at a later time.

### Threading

Callbacks, as with extensions in general, should be designed and compiled with usual thread-safe best-practices. This includes things like:

 * Only linking with (or loading of) multi-thread safe system libraries. (E.g. on Windows VisualC, see the library linkage options to select the correct library.)
 * Avoiding the use of shared memory spaces (e.g. shared, static function vars, non-thread-local globals or non-constant arrays or structs).
 * If a target OS does not support thread-local variables or separate data-segment virtual memory allocation on per-thread library loading, then your code must use local (stacked) variables only. (Hopefully, there are no such OSes out there.)

Note: If we missed any, please add them.

### Struct Passing

**C structs and REBOL are not directly compatible.** Therefore, it is the responsibility of the C extension code to deal with the "marshaling" of values between the two domains.

For example, a callback may need to return multiple values to REBOL. This can be done with a REBOL block for simple structs or with an object for more complex structs. The allocation of the block or object can be passed in from REBOL, done in advance by the C command, or done dynamically within the callback, assuming that it has access to the R3 Extension Lib API.

Retrieved from "[http://www.rebol.net/wiki/External_Callbacks](https://web.archive.org/web/20130816025020/http://www.rebol.net/wiki/External_Callbacks)"