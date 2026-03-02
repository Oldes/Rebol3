# Objects enhancements

## Get/Set Accessors

Many, if not most, modern object oriented languages have support for a concept called accessor methods  [object methods](http://en.wikipedia.org/wiki/Method_%28computer_science%29). Basically, these are simple methods which control access to some or all object internals via some form of indirection.

Most of the languages which support them do not have direct language syntax support for accessors, including the Java code included in the link above, and REBOL. Instead, these languages just use methods with naming conventions that mark them as accessors to the reader, if not the compiler. See [JavaBeans](http://en.wikipedia.org/wiki/JavaBeans) for such an example, or the SET-\* and GET-\* accessors in REBOL 3's GUI.

Some languages have adopted direct syntax support for accessor functions, which enable the programmer to use them without using function call syntax, or even necessarily realizing that they are calling functions. With careful API design this can make it possible to extend the semantics of the language with new behavior, while allowing the syntax to continue to superficially resemble setting/getting values to/from variables or arrays. This is exceedingly helpful for object-oriented languages that don't otherwise have much of a way to not be object-oriented when it's needed (Self), and for formerly procedural languages that have had object-orientation grafted on later, but still have a lot of procedural-style programmers to feed (Delphi and its derivatives). It is also useful for wrapping component-oriented systems in syntax that makes sense within a regular language (Delphi's derivatives made by Microsoft, Vala).

### Proposal

It has been suggested that syntax support for method-based accessors be added to R3's object! semantics, but really, only one accessor would be required: [The set accessor](https://web.archive.org/web/20130614002751/http://www.rebol.net/r3blogs/0019.html). The get accessor is unnecessary because we don't need parentheses to call a function, so all we have to do is assign a function to an appropriate word. None of the other accessors are possible to apply to an object!; we would need user-defined datatypes for those. The only one left standing is having a way to call a function when a set-word or set-path expression is encountered, instead of plain assignment.

### Discussion

Advantages:

 * You don't have to understand a naming convention and call functions directly.
 * You can type-check assignment, or apply other constraints.
 * You can make read-only or write-only variables.

Disadvantages:

 * It's easier to write code that implements the named accessors than it would be to write set-word accessors.
 * It isn't necessary, at least since PROTECT/hide was implemented. You can do all that stuff without them. All of the access controls that accessors are supposed to solve are supported in R3 already.
 * Code that uses your set accessor code will be incomprehensible at first glance, and will require full reading of your source and docs to understand.
 * The overhead of the accessor will be hidden, and look like a simple, efficient assignment when it's really a function call.
 * It won't help you at all with wrapping component code; you still have to use user-defined datatypes for those to work in some natural-seeming way.

In their current design, neither REBOL 2 (e.g. R2) nor REBOL 3 (e.g. R3) have any real support for defining or manipulating access to object content or state. As R3 is intent on larger and more structured application development, support for Accessor methods feels like a logical "next step" for object! datatype evolution. _Moliad_ 20:42, 3 May 2010 (EDT)

That is no longer so for R3: There is no declarative syntax support for access control, but plenty of procedural support. Note that since that blog post, the (UN)PROTECT changes have been made (or proposed in some cases), user-defined datatypes were proposed, and an entire GUI system with accessors, was created without any language support for making those accessors look like assignments. The importance of this proposal has lessened accordingly. _BrianH_ 23:24, 3 May 2010 (EDT)

Retrieved from "[http://www.rebol.net/wiki/Objects_enhancements](https://web.archive.org/web/20130614002751/http://www.rebol.net/wiki/Objects_enhancements)"