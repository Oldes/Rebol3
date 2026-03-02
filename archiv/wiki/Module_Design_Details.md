# Module Design Details

## Main Docs

 * [Modules](https://web.archive.org/web/20130817192144/http://www.rebol.com/r3/docs/concepts/modules.html)
 * [Extensions](https://web.archive.org/web/20130817192144/http://www.rebol.com/r3/docs/concepts/extensions.html)

## A108 Proposed Changes

**This is a temporary section that reflects changes in A108, and will be merged eventually with the sections below.**

There are three basic **types** of loadable files:

programs (scripts) normal programs that load and run from a standard user context. User can load and run as many as desired, and each is added to the user context.

modules provide extended APIs. They can be publicly exported into the system's standard "lib" or kept private to the user program or to other modules.

data blocks of non-evaluated data, such as that used for small databases.

### List of Fields and Options

An **options** field will be added to the header. It can provide a list of additional variations to the primary type listed above. In addition to system supported options, users can also use this list for application or app-data specific options.

In addition to title, name, date, etc, the noteworthy fields and options of a file are:

 * checksum field - specifies a checksum to validate on load.
 * compress option - indicates that the content is compressed.
 * content option - the raw content (entire file) should be kept.
 * unbound option - loaded words are not bound to a context.
 * extension option - the content came from a DLL load, not a file, and may contain native functions (commands).
 * boot option - the module should be initialized in the lower mezz part of boot.
 * mezz option - the module should be initialized in the mezz-plus part of boot.
 * delay option - the module should not be initialized until IMPORTed.
 * private option - exported words are not made public in the lib.
 * isolate option - exported words are not directly referenced, but are appended to the user context.
 * encrypt field - content is encrypted with a specified method.
 * signed field - content is signed with a specified method.

## Classes of Modules

There are three main classes of modules:

 * [**Global modules**](#Global-Modules) are listed in the system modules "catalog" and export specific words to the global runtime library (the lib context), for use by any other code. They are usually loaded from the Needs header of a script or by calling the IMPORT function with a specific filename or URL.
 * [**Mixin modules**](#Mixin-Modules) are often also in the same system modules list, but only export their words to the specific script or module that loaded them in their Needs header. They are loaded the same way as global modules.
 * [**Inline modules**](#Inline-Modules) are inserted as source code directly into the code that uses them. They do not need to be separately loaded.

## Module Loading Sequence
 * Reading the module source (from a file) or retrieving it (as part of a DLL or in an embedded extension).
 * Loading the source.
 * Loading the header object to examine module attributes.
 * Loading the body (if not a delayed module.)
 * Processing Needs (dependent modules) as listed in the header object.
 * MAKE-ing the module (including adding its exports to the global export list.)
 * Importing the module into the script or other context.

### The Needs Header

## Delayed Load

The purpose of delayed load is "register" the existence of a module, but not be forced to load it, hence not allocate additional memory resources to it. In other words, the module name, version, and other header information is known to the system, but the data and code may still be in their raw UTF-8 binary source form (after stage 1 in the above loading sequence, before stage 2), or the code may be loaded but not yet made into a module (after stage 2, before stage 3). Making a module also tends to have side effects, so delaying a module delays those as well.

Some specifics to consider (here premade means it's already passed through the MAKE part of the process, regular means not a mixin):

 * You can't delay-load with IMPORT. You have to use LOAD-MODULE/delay instead.
 * All modules that are added to the module list, even the delayed ones, must have a name to refer to them with. LOAD-MODULE has an /as name option to give a name to unnamed modules, or to rename modules. You can't rename modules after they have been imported.
 * Premade modules can't be delayed since their code blocks and side effects have already been executed. Any /delay option should be ignored and the module imported.
 * There may be modules with the same name that already exist in the module list. These must be considered.
 * Premade modules should only be in the list if they have been imported. This can be assumed, and should be assured.
 * If an existing delayed module is found, a new module should only be added if its version is greater than or equal to than the existing one.
 * If an existing imported module is found, a new module should only be added if its version is greater than the existing one.
 * If an overriding module is added and the existing module is delayed, the existing reference should be replaced, not overriden.
 * If an overriding module is added and the existing module is already imported, the new module overrides the old but the old reference is still there.
 * If overriding a previously delayed module with new source, the old checksum is overriden by the new.
 * If instantiating a delayed module (making it into a module!) by name, its checksum is retained so that it can be checked later.
 * When a delayed regular module is finally made into a module!, the new module! replaces the delayed module (as a premade).

## Global Modules

Global modules are what people will think of as being "normal" modules. They have a type of 'module or 'extension.

When you import a global module, it is added to the system modules list and its exports are added to the runtime library context (lib). A module's exports are declared in a exports block in its header. There is also a 'export keyword in module sources, but words marked with it are just collected into the exports block. Any other values other than words in the exports block are ignored, which is handy for doc strings and other potential enhancements.

There is one runtime library context (lib), and all modules or scripts get their references to externally defined or predefined words from that (this is a general overview, the details are slightly more complex). The reason there is a single context is so that export conflicts can be resolved, and in-place upgrades of modules can migrate data from previous versions; more tricks are likely possible. This also has the benefit of allowing user scripts to be written without caring about or even explicitly referencing modules, most of the time.

The important thing to remember about global modules is that they are only created once, have their exports processed once, and are added to the system modules list once. The next time you try to import the module, the new import will be compared to the existing one and if the existing one matches (by name and version) the new one is assumed to be the same. If that comparison can be done before the new module is fully made (comparison only checks headers) then the new module will not be made, and the old module will be used instead; this is not an error, this is intentional.

If a new global module has a greater version than an already-imported module of the same name, the new module is added to the head of the system modules list, overriding the old. Old versions are kept in the system module list, but ignored for the most part. A new version of a module will often retrieve references to the old versions in order to migrate their running state to the new version. As a (unenforced) rule, only the newest running version of a module should be considered active.

### Issues and Questions
 * To what extent should we allow old module versions to be removed from the system modules list?A: For now, I am going with "never". (_BrianH_ 15:41, 26 July 2010 (EDT))
 * Should it be an error if you attempt to add a premade module to the system modules list and there is another premade module of the same name and greater-or-equal version already imported to the list?A: This is potentially a problem because it leads to data corruption and unwanted side effects. While triggering an error at this point may seem moot, since by this point most of the damage will have been done already in the module's code block, at least it will let the programmer know so that they can change their code to not do this in the future. While this might be the programmer's intention, it breaks the consistency constraints of the module system, and is unnecessary because such modules don't have to be imported to be useful. For now, I am going to trigger an assert in this case. We can come up with a better error later. (_BrianH_ 15:41, 26 July 2010 (EDT))A: In the new revision, this case is silently ignored and the new version is returned instead. This has the effect of allowing DO of a module to actually do the module, even if it isn't going to be imported. The references can be compared if necessary. We can decide later whether the old asserts are a better idea. (_BrianH_ 21:43, 12 October 2010 (EDT))
 * Should the previous situation be just ignored if the new module is the SAME? as the old, thus not really being an override?A: After some thought, this case should be safe to ignore. (_BrianH_ 15:41, 26 July 2010 (EDT))

## Mixin Modules

Mixin modules (or "mixins") usually have no name set in the header, or they can be explicitly marked as such by setting the type field in the header to 'mixin.

The exports of mixins are never added to the global system exports context. Instead, their exports are directly imported into the context of whatever requested them in their Needs header, either another module, or a for a user script the user context. What that means depends on what kind of script or module is doing the requesting, but that is basically what it does. The exports of a mixin are thus only available to the module that includes it in its Needs header, or to the user context if a user script Needs it, but not to the system at large.

Mixins would not ordinarily appear in the system module directory because they usually don't have names - the system module directory is referenced by name, and until recently was only used to contain imported modules. However, since the system module directory was recently extended to include delayed modules, there needed to be a way to specify the name of a mixin declaratively so it could go into the directory. To deal with this situation, you are now allowed to specify the type of a module or extension as 'mixin and it will be treated as a mixin module even though it has a name. The other method for specifying a name programmatically is to use the /as name option when calling SET-MODULE explicitly; DO and IMPORT never do this, but advanced code might.

Mixins are ordinarily remade every time they are imported. Since mixins don't generally have names unless they are designed for delayed load, they are not stored in the system module directory at all, and are reloaded every time as well. Since that is often an undesired inefficiency, mixins with a specified name are cached in the system module directory when they are imported the first time from source, but at the loaded source level, as if you delayed the module after the LOAD-MODULE stage. Then that loaded source deep-copied and reused to make new modules for every subsequent import. This is especially useful for mixins loaded from a file or URL. Of course you can also explicitly delay-load a mixin at the binary stage (or LOAD-EXTENSION object stage for mixin extensions).

Once we had the ability to explicitly specify a name for a module when we add it to the system modules directory, it became clear that it is too awkward to prevent premade mixin modules from being added that way while allowing premade global modules to do this. So we now support premade mixin modules. Premade mixins need to be added to the system modules directory explicitly after they are made, often by calling SET-MODULE with the /as name option explicitly. There is no way to tell the system to do this declaratively; this is intentional, because premade mixins are an advanced topic and all the declarative options are basic. Most of the time premade mixins are originally inline modules.

### How to Use

Mixins are used to implement all of the features that were deliberately left out of the regular module system because they were too confusing for regular, non-modular programmers. Conceptually, the exports of a mixin are added in place whenever the mixin is explicitly requested, and not added otherwise. This is even more the case if you use a regular mixin that is remade for every requester - that is almost like adding the exports of the mixin directly into your source, but with the rest of the mixin's code still safe in your own copy of the module.

Fun with mixins:

 * Selective import: You don't have to polute the global namespace with stuff you don't need.
 * Utility modules: You can make a set of functions that is useful for a particular task, and only import them when you need it without bothering the rest of the system.
 * Shared resources with an advanced API: A premade mixin can manage a shared resource and export an API to work with it. Since it's a mixin, you don't need to see the API if you don't need to. You can even have high-level functions that are exported to the global exports, functions which make use of the low-level API, without globally exporting the low-level functions - you can do this by putting the high-level functions in a global module and having that import the mixin module. You can even have that global module be an inline module inside the premade mixin, which would import its inner module explicitly with IMPORT.
 * Standard templates, advanced data structures, design patterns (aka real mixins): You can make a whole data structure with associated code that is meant to be dropped into a module and then customized for the situation, without having to copy and paste. A mixin can let you make a whole advanced design pattern, once, and then just reuse it wherever you need it. Without generics, macros or a templating system built into the language. Mixins can replace all of those.
 * Task-local data structures: In theory, regular mixins loaded into the user context would be remade for each user context. If the user context is task-local, so would be the mixin. Food for thought...

For example, the module system itself could be implemented as a premade mixin that exports most of the API, and manages the system module directory. We can probably get away with just adding LOAD, SAVE, IMPORT and INTERN to the global exports context. The rest of the API can just be exported from the 'module-tools mixin. The intrinsics module would need to import the 'module-tools mixin so that DO, MAKE-MODULE and BEGIN can use what they need. Or perhaps those 3 intrinsics would be added by 'module-tools itself, whichever you prefer.

### Issues and Questions
 * Not a question, but an interesting point: The type of the embedded module in an extension has to be set to 'extension in order for MAKE module! to special-case such modules - LOAD-MODULE does this automatically. So in order for extension modules of type 'mixin to continue to be treated as mixins after they are loaded, their names are retrieved after the load and then the name field in the header is set to none so that they will be considered mixins again. This means that if you delay an extension, you need to delay it after the LOAD-EXTENSION phase, but before the LOAD-MODULE, otherwise the name may be lost. This is a good idea to do for efficiency anyways.

## Inline Modules

Inline modules are created by MODULE or MAKE module! from inside another module or script. This is as opposed to regular script modules, which take up a whole file.

Inline modules have some interesting semantic differences as compared to script modules, and most of these relate to binding issues. Depending on how you create or preprocess an inline module you can do almost anything you want, including replicating the behavior of script modules. Modules can even contain inline modules with different characteristics, such as a global module that contains a mixin, a premade mixin that adds a regular mixin, and more.

If you want to add an inline module to the system modules directory you have to use the IMPORT function directly, or some low-level function like SET-MODULE. If you want to use the exports or other visible data of an inline module in the enclosing code, you have to access or export it directly because the IMPORT function can't affect already-bound code, for various extremely technical semantic reasons that are inherent and unavoidable in REBOL.

If your inline module has dependencies, you have to process those dependencies yourself. MODULE and MAKE module! do not process a module's Needs header, by design. The reasons for this for this are:

 * Security: You may not know where that header block came from, and it might include malicious code.
 * Analyzability: Static analysis of dependencies would be impossible.
 * Efficiency: Making a module! value is supposed to be a low-level operation with reasonably predictable overhead.
 * Safety: Because dependency cycles are too easy with inline modules.

Use IMPORT or DO-NEEDS instead, and manage your own dependencies. That is what IMPORT does itself for script modules.

### MAKE module!

The MAKE module! constructor calls an intrinsic MAKE-MODULE\* function, which does most of the work. MAKE-MODULE\* processes the 'export and 'hidden keywords, creates the local context and header object, applies the global runtime library context, and applies the collected mixins if provided. Then it creates the actual module value using TO module! with the header object and local context, and lastly executes the code block. Simple, fast, low-level, and modifying.

### The MODULE Function

MODULE is a mezzanine wrapper for MAKE module! which provides a nice set of friendly preprocessing steps:

 * The source blocks have their bindings removed with UNBIND/deep, so that bindings inherited from the enclosing module/script don't leak.
 * You can provide an optional object that contains collected mixins that should be applied. This object is like what DO-NEEDS/only returns.

It should be noted that MODULE does **not** deep-copy its arguments the way FUNC does, because module source tends to be larger than you would want to copy. If you really need to, you can copy your code blocks explicitly.

### Issues and Questions
 * The old way that the collected mixins were passed into MAKE module! was by adding a mixins field to the header of the prospective module, then setting that field to true after the mixins were retrieved. This turned out to be a dumb way to do this, and the mixins: true field did not turn out to be useful information. The new method is to instead pass in the mixins as a third element in the spec block. This new approach also has the effect of making MODULE simpler, and making it easier for code generators to create inline modules with mixins.

## Regular vs. Isolated Modules

An isolated module is marked as such with Isolated: true in the module header. A regular module is not.

Regardless of where a module came from or whether it is a mixin or not, a module can be isolated or not. An isolated module is not fully sandboxed, but all words in the module are made local, so any assignments to them don't affect the rest of the system. This is basically accomplished with some simple binding tricks.

For a regular module the module-local context is filled with the set-words at the top level of the code block, the exports, and any words marked with the 'hidden keyword. All other words are bound to the module's mixins, the global exports, and the system context, in that order. That order because it makes the system context take precedence, then the global exports, and finally the mixins. The local words take precedence over all of those.

For an isolated module, all words in the module are collected into the local module, no matter how deeply they are nested. Then those words are filled with initial values from the system, global exports and mixins, using the RESOLVE function. In that order, so you get the same precedences as with regular modules. There is no need to have local words take precedence because all of the words in the module will be local. This means that any changes to the source words don't make changes to the local words.

One other interesting difference between regular and isolated modules is that for regular modules the local words are initially unset, and for isolated modules the local words are initially filled in with corresponding values from the other sources, if any. This isn't much of a gotcha, but it is good to know.

You can isolate a module from the outside by importing it explicitly with the IMPORT/isolate function. This is especially useful for running scripts that are not very well written (by someone else, of course) if you are concerned with them making accidental changes to the global environment - this happens often with scripts that were originally written for R2. Use IMPORT/isolate/only for the full effect, so that the module won't have its exports put into the global exports list.

The best reason to isolate your own module using the Isolate: true header is predictability. Once the initial values are migrated to the local context, all changes to them must happen within the module (especially if they are hidden). Changes to those words in their source contexts don't affect your module. You can even PROTECT the words to make them read-only, even if doing that to the original words might make the system crash. It's a relatively advanced trick, but comes in handy on occasion.

### Issues and Questions
 * Isolation is not the same as sandboxing, but the technique would be an important first step in a sandboxing process. To really sandbox a module you would not only have to isolate the module, but also put together s special collection of safe functions to fill in the initial values with. Fortunately, it has been a goal in the R3 design to make as many functions as we can safe, so that they can be used in sandboxes - this is the reason for the dedicated reflection functions, for instance, instead of overloading the ordinal functions.

## User Scripts

In R3's modular system, the weirdest part is the attempt to emulate R2-style non-modular scripts. User scripts have no type field in their header, or at least not a type value that the system can handle ('module, 'extension, 'mixin). Many user scripts have no header at all.

User scripts are similar to isolated modules, except for three things:

 * All user scripts share the same user context, rather than each getting its own context.
 * But that context is task-local, in theory, so only user scripts run in the same task share the same context. This is not yet implemented in practice though, so for now they all share the same user context.
 * Though user scripts can have a Needs header that is handled much like that of an isolated module, that "fill in the initial values from the imports" thing only applies to words that are new in the script, that haven't been defined in the shared context already by previous scripts. This means that side effects carry over into subsequent scripts.

Added up, this means that the effect of user scripts is similar to building an isolated module incrementally, one script at a time. The system words and global exports are not immediately added to the user context though, not until the words are used in a script. This makes the incremental building possible.

The interesting thing about scripts is that every time you DO a string, it is a separate script. Every command you enter into the console is also a separate script. And these scripts in strings or command lines can have headers too, even be modules. The system/script object is only updated for file scripts though.

User scripts can have a Needs header and mixins - the mixins get applied to the user context, and are there for subsequent scripts. The object of collected mixins is applied by the DO-NEEDS function directly. If you want to apply the object yourself, DO-NEEDS/only returns it; DO-NEEDS/only is what DO or IMPORT calls when making modules.

### Issues and Questions
 * In general, when the system context is being built, no user scripts should be run, because the user context doesn't really exist until the system context is finished building.
 * So, when are we going to get around to making system/contexts/user task-local?

## Special Questions Related to Loading
 * What do we do when a module is not added to the module list due to a policy issue? Currently the add accessor returns none if it is a version issue, and triggers an error for a checksum violation. 
   **A:** We must list and look at each policy. We may need special error messages to explain things, because we don't want developers to be confused by what happened.**A:** After some analysis it is clear that the version validation for override eligibility needs to be done in the add accessor, but all other checks should be done in the get accessor. Binary checksums mean that we can make one every time we add binary source, so it doesn't need to be an add option anymore. The name of the module can be returned if the override was successful, none if not, an error triggered if there was some violation (like no name). With the name you can get the module with the get accessor. _BrianH_ 02:02, 22 July 2010 (EDT)
 * How do we determine (officially) that two modules are to be considered the same? Name and version? 
   **A:** Unfortunately it depends on how you define "same". I take it here to mean that the modules provide exactly the same function, but their code may differ (for example, as a developer is building it and importing it for testing.) So, the name and version should be fine. There's no perfect solution here, other than to issue global identifiers (e.g. via the REBOL.com server, I suppose) but that may be a hassle. 
   **A:** We don't need perfect, all we need is a standard. If we declare that name and version is what constitutes identity, it can be left to the programmer to make that true. _BrianH_ 02:02, 22 July 2010 (EDT)
 * Can we safely LOAD-EXTENSION more than once with the same extension? 
   **A:** It depends on the OS, because we would attempt to load the DLL a second time. That shouldn't be a problem, but I can imagine that some OSes don't allow it.
 * Does LOAD-EXTENSION on an embedded extension have any side-effects beyond creating an object? 
   **A:** Yes, there is a minor side-effect. There is an internal system list that relates extension handle identifiers (counting numbers) to their RX_CALL functions. This is currently a fixed-sized resource.
 * Does LOAD-EXTENSION on an embedded extension return the same source each time, or different copies of the same source? Testable by SAME? 
   **A:** Each embedded extension registers itself during boot with Reb_Extend(). That function currently copies the source code immediately to bring it into REBOL's memory management. After that, any other calls to LOAD-EXTENSION use the same source code.
 * Does LOAD-EXTENSION return source that is safe to modify? Protecting it might be good, for extension source. 
   **A:** It can be modified. We don't protect it, but we could.
 * Is it safe to delay the object returned by LOAD-EXTENSION instead of the source? 
   **A:** Yes, as long as there's some kind of reference kept to the source, otherwise it will GC. For boot extensions, the system/catalog/boot-exts will keep that from happening. 
   **A:** Given the answers to the last 4 questions, let's say yes, and do it. _BrianH_ 02:02, 22 July 2010 (EDT)
 * Should the checksum of an extension include the extension-specific source added? 
   **A:** A difficult question... we should think about this carefully. 
   **A:** No. Let's go with binary checksums instead, no modifications. _BrianH_ 02:02, 22 July 2010 (EDT)
 * Should the version in the header of a module be set to 0.0.0 if not a tuple? Currently it is, but it could just be treated that way with a little more code. 
   **A:** My gut says no, but the reasoning isn't crystal clear. A NONE version means "I don't care" where as a specific tuple, even 0.0.0 means that's the version. 
   **A:** The alternate code turned out to be simple enough, so we can go with treating it that way at runtime instead of modifying the header. _BrianH_ 02:02, 22 July 2010 (EDT)
 * If so, should module checksums be done after the version field is fixed? 
   **A:** We need to carefully construct the definition of "checksum" for a module... specifically for how we want to use it, with as little overhead as necessary. (We should avoid loading and molding a module just to compute the checksum. For example, what if it has 500KB of images within it.)
 * Currently full semantic normalization is done for checksums, but its overhead is high. Can we switch to binary checksums? If so, ignore the previous 3 questions.
   **A:** This is most likely the best compromise.
 * Is there a way to normalize binary checksums to avoid differences between line endings? 
   **A:** I just checked: DELINE will do what you need. 
   **A:** Given that DELINE would ruin unencoded compressed modules, it would probably be best to just say that R3 source is binary and use binary checksums. DELINE should be used in the code that generates the extension-embedded source in the first place, at least on platforms where you can't assume that \n is a REBOL-compatible newline. _BrianH_ 02:02, 22 July 2010 (EDT)

Retrieved from "[http://www.rebol.net/wiki/Module_Design_Details](https://web.archive.org/web/20130817192144/http://www.rebol.net/wiki/Module_Design_Details)"