# Parse Project

## Project

See [Improvements to the PARSE function](https://web.archive.org/web/20150420170533/http://www.rebol.net/r3blogs/0155.html) (on R3 Blog) for project requirements.

Some ideas have been posted [REBOL Enhancement Proposal for the Parse dialect](https://web.archive.org/web/20150420170533/http://www.colellachiara.com/soft/Misc/parse-rep.html) (on Gabriele's site).

The primary editor/manager of this project is: Brian Hawley (BrianH).

The current and up-to-date R3 summary document for **parse** is [Parse Summary](https://web.archive.org/web/20150420170533/http://www.rebol.com/r3/docs/concepts/parsing-summary.html).

The goal of this page is to collect finished proposals for passing along to Carl to implement in REBOL 3. These proposals are currently considered pretty settled, unless there are really new ideas or unknown implementation issues.

### Guidelines

Discussion of these proposals should be posted in REBOL 3 chat, under the R3/Parse heading (#28). If you want to contribute to this page, start there. Membership is open: Anyone with [REBOL 3](https://web.archive.org/web/20150420170533/http://www.rebol.net/wiki/R3_Alpha) can join in.

There will be factors that affect these proposals that will be specific to changes in REBOL 3. Some of these changes are already in the [R3 public alpha releases](https://web.archive.org/web/20150420170533/http://www.rebol.net/wiki/R3_Alpha), and some are still being planned. The best approach is to test your code against the public alphas and ask for clarification in R3 chat. People who know and make the plans are in R3 chat almost every day, including Carl.

When making proposals here, there are some things you should keep in mind:

 * People are going to use this dialect to do real work, so consider practical implications first as they are what make a feature "important".
 * The scope of PARSE is language recognition. Going outside of that scope shouldn't be done lightly.
 * PARSE is implemented in native code, primarily by Carl. It doesn't change often. Carl's word on PARSE is the last word.
 * These changes are being proposed for REBOL 3.0, not 2.x. There are some semantic changes in REBOL 3 that may make some of these changes easier, and other changes required. If you have any confusion about REBOL 3's semantic changes, ask the editor, preferably in AltME. Testing the public alpha releases of R3 is not enough - some changes are still in the planning stage.
 * REBOL 3.0 strings are Unicode, so charsets might not work in the way that you might expect. In particular, complementing a charset in R3 requires a special flag in the charset because of memory issues. This was the original reason for the [NOT](#NOT) proposal, for instance.
 * REBOL 3.0 will have multitasking. We will not be accepting proposals that are not multitasking-safe.
 * PARSE is only one of many REBOL dialects that all work together. Not everything has to go into PARSE - some tasks are better done in other REBOL dialects.
 * PARSE rules are data. If your feature doesn't make it in you can always write a preprocessor for your rules in PARSE.
 * Simplicity and balance are nice and will make REBOL programming less stressful. Overly complex features will likely be rejected.
 * Don't be afraid to include stuff that only smart people will understand - we have a lot of those. Still, be nice to people who don't have Comp-Sci degrees - we have a lot of those too.
 * PARSE and REBOL have been around for years, and there is a lot of terminology that has specific usage in the REBOL community. We can be convinced to use other terms, but any attempt to just use different terminology without reference to the REBOL-standard terms will just lead to confusion.

The PARSE function of REBOL will not become a parser generator, though the dialect might be usable for others to create one if they so choose. PARSE rules make a good target language though.

Modification operations are out of the scope of language recognition and pattern matching; modification is usually done in REBOL code in the parentheses. However, modification of the data being parsed is a **really** common practice in existing REBOL code and will likely continue to be so - complaining will not make the practice go away. The [CHANGE](#CHANGE_1), [INSERT](#INSERT) and [REMOVE](#REMOVE_1) proposals below have been accepted as a minimal way to allow people to do what they are already doing, badly in many cases. It is unlikely that any other modification proposals will be accepted.

## Theory of PARSE

REBOL is designed as a practical language. It has been notoriously difficult to make REBOL fit theoretical models because its design was only inspired by theory, but driven by practice. This has not stopped the community from trying though, and those efforts bring certain advantages to REBOL. While conforming to a theory is not an end in itself, it does let you use some helpful techniques that have resulted from the research on that theory.

(Editor's Note: Thanks to Peta for researching most of this section.)

The PARSE dialect is an enhanced member of the family of Top-down parsing languages (TDPL family) including the [Top-down parsing language (TDPL)](http://en.wikipedia.org/wiki/Top-down_parsing_language), the [Generalized top-down parsing language (GTDPL)](http://en.wikipedia.org/wiki/Top-down_parsing_language#Generalized_TDPL) and the [Parsing expression grammar (PEG)](http://en.wikipedia.org/wiki/Parsing_expression_grammar) and uses the same "ordered choice" parsing method as the other members of the family.

The prioritization of the ordered choice (a.k.a. "alternative", but the "ordered choice" name is more appropriate since the order of rules matters) causes that the second choice in the

```rebol
 ["a" | "ab"]
```

rule never succeeds since the first one takes precedence. This causes the greediness of repetition operators, e.g.:

```rebol
 repeated-rule: [any rule]
```

has a recursive definition:

```rebol
 repeated-rule: [rule repeated-rule | none]
```

, which is greedy since the first choice always takes precedence when possible.

There is one formal difference between PARSE and the other members of the TDPL family: All other TDPL family members succeed when the start rule succeeds. As opposed to that, for PARSE to succeed the RULES rule (which is the PARSE counterpart of TDPL family start rule) has to succeed and the final PARSE position has to be the end of the input. This property is essential. It's part of the basic purpose of PARSE, and has been for years. If you are only concerned about a section of the data and are using the return value of the PARSE function, you can put [to end] on the end of your start (RULES) rule.

PARSE is provably able (see below) to recognize all the grammars the members of the TDPL family can recognize (and more), although it needs to use a couple of complicated idioms to represent the four basic family operators ([FAIL](#FAIL), [NOT](#NOT), [AND](#AND) and [THEN](#THEN)) it does not support directly yet.

The membership of the PARSE dialect in the above language family brings the following advantages all the family members have:

 * PARSE expressions make a good replacement for regular expressions because they are strictly more powerful. For example, a regular expression inherently cannot find matched pairs of parentheses because it is not recursive, but the PARSE dialect can.
 * PARSE makes a good replacement for an LL parser because it is strictly more powerful. For a grammar not recognizable by a LL parser see the Wikipedia example below.
 * Context free grammar parsers usually require a separate tokenization step because of the way they use lookahead. PARSE of string types does not require a separate tokenization step and the tokenization rules can be written in the same way as any other grammar rule.
 * When parsing block types, the data is actually in-memory datastructures that have been constructed at runtime or loaded by the LOAD function. No tokenizing is necessary because there is no lexical format in memory. This gives the block parsing dialect some additional operations that the string parsing dialect doesn't have. One of these, the INTO operation, is not shared with other members of the TDPL family. INTO allows PARSE to recognize already nested data structures that are out of scope for traditional parsers that are defined to operate on a series of elements. Within a given block, PARSE acts like a member of the TDPL family.
 * Many context free grammars contain inherent ambiguities, even when they're intended to describe unambiguous languages. The "dangling else" problem in C, C++, and Java is one example. These problems are often resolved by applying a rule outside of the grammar. In the PARSE dialect these ambiguities never arise because of prioritization.
 * PARSE has an unlimited lookahead capability, brought to PARSE by the ordered choice operator.

If your rules contain any modifying REBOL code that is executed in parentheses or modifying operations (that may be added as proposals here) then any changes made by those operations will not be undone on backtracking. (Editor's note: Just like that Icon code you couldn't debug.) You may need to consider this when trying to use modifying rules since they may interact with the unlimited lookahead capability PARSE has. As a result of modifying techniques, linear optimizations like those of [Packrat](https://web.archive.org/web/20150420170533/http://pdos.csail.mit.edu/~baford/packrat/) parsers are likely impossible in REBOL PARSE (though one can hope).

For those of you who are fans of REBOL history, it is no longer possible (even in REBOL 2) to explicitly set the parse position with a get-word to another series than the one being parsed already, a previously popular technique used for incremental parsing (and which crashed PARSE if misused). Another thing which won't crash anymore in REBOL 3 is off-the-end references to series - they get moved back to the nearest end. Safer, if more boring.

(Editor's Note: When someone in the REBOL community says "set the parse position" they are referring to explicitly setting the parse position with get-words. All other changes to the parse position are implicit effects of the other operations, and thus not usually mentioned.)

## Format

General format of entries:

 * name (as section heading): the suggested command/concept name
 * suggested by: a name we can hit up for more info
 * purpose: a **one line** statement of the concept
 * importance: why we need this change
 * syntax: how the parse operation will be called (optional)
 * examples: clear examples of usage and results

(Also a filter, because if we cannot say it easily, then the concept may be too complex.)

Editor's Note: Please keep the syntax sections of the proposals similar to the existing syntax sections, with alternates as list items and keywords capitalized. If there are entire alternate proposed groups of syntax, use separate syntax lists and discuss the differences. Do not use pre blocks for syntax. I'll try to keep things consistent.

## Priorities

Here is the current list of priorities linked to the feature descriptions below.

Implemented:

 * [x] [NOT](#NOT) (originally proposed as NOT 2)
 * [x] [AND](#AND)
 * [x] [FAIL](#FAIL)
 * [x] [THEN](#THEN)
 * [x] [QUOTE](#QUOTE)
 * [x] [/all by default](#.2Fall_by_default) (only for rule parsing, not simple parsing)
 * [x] [TO and THRU multiple](#TO_and_THRU_multiple)
 * [x] [IF (condition)](#IF_.28condition.29) (originally proposed as CHECK)
 * [x] [CHANGE 1](#CHANGE_1) (partially implemented)
 * [x] [REMOVE 1](#REMOVE_1)
 * [x] [INSERT](#INSERT) (partially implemented)
 * [x] [INTO strings](#INTO)
 * [x] [RETURN](#RETURN)
 * [x] [BREAK/return](#BREAK.2Freturn)
 * [x] [Set PARSE position to another series](#Set_PARSE_position_to_another_series) (though see [#1787](https://web.archive.org/web/20150420170533/http://issue.cc/r3/1787))
 * [x] [DO](#DO)

Deferred for later (hopefully):

 * [ ] [LIMIT](#LIMIT)
 * [ ] [USE](#USE)
 * [ ] [OF](#OF) (though maybe called something else)
 * [ ] [REVERSE](#REVERSE) (probably the REVERSE rule variant)
 * [ ] [CHANGE](#CHANGE_1) ONLY option
 * [ ] [CHANGE](#CHANGE_1) rule (expression)
 * [ ] [INSERT](#INSERT) ONLY option
 * [ ] [INSERT](#INSERT) (expression)
 * [ ] [/ignore stuff](#.2Fignore_stuff_option)
 * [ ] [n BREAK](#n_BREAK)
 * [ ] [UNTIL](#UNTIL)

Rejected:

 * [LIT](#LIT) (in favor of [QUOTE](#QUOTE))
 * [EITHER](#EITHER) (in favor of [THEN](#THEN))
 * [CHANGE 2](#CHANGE_2) (in favor of [CHANGE 1](#CHANGE_1))
 * [REMOVE 2](#REMOVE_2) (in favor of [REMOVE 1](#REMOVE_1))
 * [INTO type](#INTO_type) (in favor of [INTO](#INTO) without the type)
 * [BREAK only from loops](#BREAK_only_from_loops) (misunderstood what loops are in PARSE)
 * [STAY](#STAY) (apparently, in favor of the listed alternatives)
 * [TO or THRU NOT](#Notes_about_NOT) (TO and THRU have limited syntax)

## Requests

Here is a list of the requested improvements to PARSE.

### CASE and NO-CASE keyword pair

Suggested by: Ladislav, BrianH

Purpose: PARSE is able to perform either case-sensitive or case-insensitive matching depending on the /CASE refinement. This proposal allows to specify matching sensitivity in the PARSE rule.

Importance: Makes the Parse dialect more expressive, the /CASE refinement is "external" to the dialect.

Syntax:

```rebol
   case rule
   no-case rule

```

Using this syntax The CASE and NO-CASE keywords influence matching sensitivity in the given RULE. Both keywords take one argument, which may be a matching rule.

Example:

```rebol
   parse input [case "my string"]

```

should be equivalent to current

```rebol
   parse/case input ["my string"]

```

On the other hand, a rule such as

```rebol
   parse input [case "my string" no-case "another string"]

```

is not easily expressible in the current PARSE dialect. The last example looks like being equivalent to

```rebol
   parse input [case "my string" "another string"]

```

since the default mode in the RULE should be the NO-CASE state, because the /CASE refinement was not used.

Backward compatibility:

To maintain the backward compatibility, the /CASE PARSE refinement should still be allowed.

### OF

Suggested by: Carl

Purpose: Allows unordered matching sets.

Importance: Various dialects (e.g. the GUI) allow an unordered sequence of values, but these are difficult to efficiently parse using available methods.

Syntax:

 * OF [type ...]

The types in the argument block would be names of datatypes or typesets as words. Literal datatypes or typesets may also be accepted (?).

An OF statement could be used as the rule argument to COPY, SET or [RETURN](#RETURN) operations. The value returned would be a block of the values matched in the order specified, regardless of what order they appear in the series, and any missing values would be replaced with the value none.

Example:

```rebol
 of [integer! string! word!]

```

This accepts none or one of the specified datatypes in the input stream. Example matches would be:

```rebol
 1
 "str"
 word
 1 "str"
 "str" 1
 1 "str" word
 "str" 1 word
 word 1 "str"
 ...

```

The output result of OF would be the a block with the captured values in the order specified.

So:

```rebol
 1 --> [1 none none]
 1 "str" --> [1 "str" none]
 word 1 "str" -> [1 "str" word]

```

Alternate syntax proposal by Steeve Luc:

 * OF [type or OPT type ...]

In this proposal the values to be matched are mandatory unless marked as optional with the OPT keyword. Missing mandatory values would cause the rule to not match. The ordering and return characteristics would be the same.

Note: The name OF is supposed to imply that this would be a set OF values. Some other suggestions that have been made are:

 * FROM: FROM a set of values.
 * ALL: ALL of these values (for the mandatory values version above).
 * GATHER: GATHER some values in no particular order.

Future: A possible future enhancement would be to allow 0, 1, or more.

### QUOTE

Suggested by: Carl (refinement of LITERAL requested by others)

Purpose: Escapes values from the PARSE dialect.

Importance: The PARSE dialect uses a range of terminal tokens, and those cannot be used as direct matches during block parsing. For example, if you write the numeral 1, it is considered a count parameter to the command that follows. A solution is to provide a general escape mechanism for matching literal tokens.

Examples:

```rebol
 quote 1

```

matches a value 1 in the input stream, not a count of 1.

To further show the reasoning, consider:

```rebol
 2 quote 1

```

which would match 2 values of 1 in the input stream.

This can also be applied to words:

```rebol
 quote abc

```

matches the word abc in the input stream.

```rebol
 quote 'abc
 quote abc:
 quote :abc

```

will match 'abc, abc:, and :abc respectively.

You can also match full literal structures, which will be compared as if with the REBOL EQUAL? function. For example,

```rebol
 quote [a b c]

```

will match a nested block! with only the words a, b and c in it, in that order.

The great value of QUOTE is that a rule can directly match input tokens without evaluating a paren block (a production). Previously, you would have to write rules like this to do literal matching:

```rebol
 set w set-word! (if w = to-set-word 'abc [...])

```

Issues: In theory, this is a high-precedence (full strength) matching rule. This means that it is absolutely literal, so there would be no constructive forms (of any kind). That may be too limiting and may present an argument for providing a constructive sister of QUOTE.

### LIT

Suggested by: Peta

Purpose: For block parsing, escapes values from the PARSE dialect. Semi-constructive.

Importance: 

 * The PARSE dialect uses a range of terminal tokens, and those cannot be used as direct matches during block parsing. For example, if you write the numeral 1, it is considered a count parameter to the command that follows. A solution is to provide a general escape mechanism for matching literal tokens.
 * The great value of LIT is that a rule can directly match input tokens without evaluating a paren block (a production). Previously, you would have to write rules like this to do literal matching:

```rebol
 set w set-word! (if w = to-set-word 'abc [...])

```

 * This variant is more flexible than the non-constructive one, which may require an enhancement later.

Syntax:

 * LIT block
 * LIT paren
 * LIT other

The **LIT block** syntax can be used to match a sequence of values. A counterpart of string in string-matching. Non-constructive.

Examples:

 * To match a [1 2] sequence:

```rebol
 parse [1 2] [lit [1 2]]

```

 * To match a subblock:

```rebol
 parse [[1]] [lit [[1]]]

```

 * To match a paren:

```rebol
 parse [(1)] [lit [(1)]]

```

The **LIT paren** syntax is constructive. It evaluates the paren. The result is used for matching. If it is a block, then the LIT block case above applies, otherwise the LIT other case applies.

Examples:

 * To match 2:

```rebol
 parse [2] [lit (1 + 1)]

```

 * To match a [1 2] sequence:

```rebol
 parse [1 2] [lit (reduce [1 + 0 1 + 1])]

```

 * To match a paren:

```rebol
 parse [(1)] [lit (lit (1))]

```

The **LIT other** syntax is for other cases. Non-constructive.

Examples:

 * To match a value 1:

```rebol
 lit 1

```

 * To match two values of 1:

```rebol
 2 lit 1

```

 * To match a word:

```rebol
 lit abc

```

 * To match a lit-word:

```rebol
 lit 'abc

```

 * To match a set-word:

```rebol
 lit abc:

```

 * To match a get-word:

```rebol
 lit :abc

```

 * To match a path:

```rebol
 lit a/b/c

```

 * To match a lit-path:

```rebol
 lit 'a/b/c

```

### NOT

Suggested by: Peta

Purpose: Inverts the success result of a rule, only.

Importance:

 * With the addition of Unicode charsets, it is no longer practical to complement a charset for comparison reasons.
 * By not consuming input this would be a direct inversion of the rule.
 * Works as a look-ahead operation. PARSE, as a member of the TDPL family already has unlimited look-ahead capability. This operation allows even an inexperienced user to make use of it.
 * Replaces the tricky idiom (which proves, that the capability is already present in PARSE):

```rebol
 [rule (cont: [end skip]) | (cont: none)] cont

```

Explanation: Together with the AND operation this will make the Parse dialect directly compatible with the Parsing expression grammar. The behavior of this NOT operator is consistent with the other proposed basic operations of the TDPL family PARSE doesn't directly support yet:

 * Is consistent with the AND operation:

```rebol
 [AND rule] = [NOT NOT rule]

```

 * Is consistent with the EITHER operation:

```rebol
 [NOT rule] = [EITHER rule FAIL NONE]

```

 * Is consistent with itself:

```rebol
 [NOT NOT NOT rule] = [NOT rule]

```

Syntax:

 * NOT rule

Examples:

 * To match one character distinct from SPACE-CHAR the following rule would work:

```rebol
 not space-char skip

```

 * Can invert even the END rule:

```rebol
 not end

```

Notes: Regardless of the underlying series changes performed by the argument rule this NOT operation will be consistent with PARSE's other backtracking operations.

### AND

Suggested by: Peta, the name AT by Chris Ross-Gill

Purpose: A look-ahead rule: Matches the parse rule without changing the current position.

Importance:

 * Replaces the [here: rule :here] idiom which has problems with recursion-safety.
 * Compatible with the Parsing expression grammar - together with the [NOT](#NOT) operator it allows direct usage of PEG expressions in PARSE. See the Wikipedia example below, which is a PEG.
 * May be used in combination with the original (R2-type) INTO syntax for look-ahead type check. The semantics of INTO would still need to be expanded to support string types.
 * As a member of the TDPL family PARSE already has unlimited look-ahead capability. This operator allows even an inexperienced user to comfortably make use of the capability.

Syntax:

 * AND rule

Notes: The recursion-safe idiom known to do the same is:

```rebol
 [rule (cont: none) | (cont: [end skip])] [end skip] | cont

```

The name AT has been suggested as an alternative to the originally proposed AND name, but AND was finally chosen.

Examples:

 * Combine with the original INTO to check that the type of the value is a block!:

```rebol
 and block! into rule

```

 * Combine with modifying operations:

```rebol
 and insert "a" "a"

```

AND in this case resets the parse position, but not the effect of the [INSERT](#INSERT). This technique can be used to implement modify-then-verify or iterative processes.

 * (Advanced) Match an n #"a" n #"b" n #"c" string for any n >= 1 (a PEG taken from the Wikipedia, an example of a non-context-free grammar, which cannot be parsed by an LL parser):

```rebol
 nanb: [#"a" opt nanb #"b"] ; the same number n >= 1 of #"a"s and #"b"s
 nbnc: [#"b" opt nbnc #"c"] ; the same number n >= 1 of #"b"s and #"c"s
 nanbnc: [
     and [nanb #"c"] ; look-ahead test of the same number of #"a"s and #"b"s followed by a #"c"
     some #"a" ; skip all #"a"s
     nbnc
 ]

```

Explanation of the example:

 * The nanb rule makes sure that there are as many b's as there are a's, recursively (you can't do that with regular expressions).
 * The #"c" after nanb makes sure these are followed by at least one #"c" (this helps to make sure, that there aren't other #"b"s following).
 * Combined, those rules ensure that there are the same number of a's and b's followed by #"c"s.
 * By using the AND operation we are able to do this check as a look-ahead rule, providing context for the subsequent rules (you can't do that with a context-free grammar).
 * We then skip past the a's and use the nbnc rule to make sure that there are at least as many c's as there are b's.
 * The nanbnc rule ends after nbnc, making sure that there aren't any extra c's.
 * All together, this ensures that there are the same number of a's, b's and c's. Magic!

### STAY

Suggested by: Carl

Purpose: Executes its argument rule, ignoring whether or not it matches, and doesn't advance.

Syntax:

 * STAY rule

Importance:

 * Unlike [AND](#AND) and [NOT](#NOT), STAY doesn't care if its rule matches.
 * Useful for rules that have effect, like [CHANGE](#CHANGE_1), [REMOVE](#REMOVE_1) and [INSERT](#INSERT), and rules containing productions. Especially if you don't need to verify them.

Note: the proposed 

```rebol
   stay rule

```

is just a shortcut for

```rebol
   and [opt rule]

```

or

```rebol
   opt [and rule]

```

or

```rebol
   rule fail | none

```

or

```rebol
   opt [rule fail]

```

or

```rebol
   not [rule fail]

```

### EITHER

Suggested by: Peta

Purpose: Syntactic conditional pattern matching without dynamic rules.

Importance:

 * Increases the expressiveness of the PARSE dialect to match that of the [Generalized TDPL](https://web.archive.org/web/20150420170533/http://en.wikipedia.org/wiki/Top-down_parsing_language#Generalized_TDPL).
 * Together with [FAIL](#FAIL) it allows direct usage of the GTDPL in PARSE.
 * Dynamic rules tend to be a tricky process to write and debug, not to mention the potential binding issues if they aren't done just right. The more language patterns we can recognize without dynamic rules, the better.
 * Replaces the idiom:

```rebol
 [rule1 (cont: rule2) | (cont: rule3)] cont

```

 * This rule is not as important as the other proposed rules in this group ([FAIL](#FAIL), [NOT](#NOT), [AND](#AND)). Using the [NOT](#NOT) operator it can be replaced by a simple idiom:

```rebol
 [rule1 rule2 | not rule1 rule3]	 

```

Syntax:	 

 * EITHER rule1 rule2 rule3	 

Examples:

Parse to the #"a" delimiter, to the #"b" delimiter, or to the end of the input, whichever comes first:

```rebol
 any [either [#"a" | #"b"] [end skip] skip]

```

Editor's Note: The simpler but otherwise equivalent [THEN](#THEN) proposal below was accepted instead.

### THEN

Suggested by: Carl (as a variant of EITHER)

Purpose: Syntactic conditional pattern matching without dynamic rules.

Importance:

 * Increases the expressiveness of the PARSE dialect to match that of the [Generalized TDPL](https://web.archive.org/web/20150420170533/http://en.wikipedia.org/wiki/Top-down_parsing_language#Generalized_TDPL).
 * Together with [FAIL](#FAIL) it allows direct usage of the GTDPL in PARSE.
 * Dynamic rules tend to be a tricky process to write and debug, not to mention the potential binding issues if they aren't done just right. The more language patterns we can recognize without dynamic rules, the better.
 * Replaces the idiom:

```rebol
 [rule1 (cont: rule2) | (cont: rule3)] cont

```

Syntax:

 * THEN rule2 | rule3

If THEN is reached (any implied rule1 succeeded), then rule2 is matched and the next alternate (containing rule3) is skipped. If rule1 failed, the next alternate is backtracked to using the normal backtracking rules. If rule2 fails, then it backtracks to whatever rule4 alternate that there may be after the rule3 alternate, if any.

Examples: 

Parse to the #"a" delimiter, to the #"b" delimiter, or to the end of the input, whichever comes first:

```rebol
 any [[#"a" | #"b"] then [end skip] | skip]

```

Note: The word => has been suggested instead of THEN for this operation, but would require a REBOL syntax exception.

Editor's Note: THEN was accepted with that name, not as =>.

### FAIL

Suggested by: BrianH, Peta

Purpose: Deliberately cause the rule to fail and backtrack to the next alternative (the opposite of NONE).

Importance:

 * This is helpful when you want more fine-grained control over your parse rules.
 * It would make the 

```rebol
 end skip

```

idiom used by advanced PARSE programmers available to regular programmers.

 * Compatible with the TDPL - allows direct usage of the TDPL in Parse.

Syntax:

 * FAIL

For a non-useful example matching the string "ab":

```rebol
 ["a" (print "a") fail | "ab"]

```

### TO and THRU multiple

Suggested by: Pekr, Ladislav, et.al.

Purpose: Advance forward to one of multiple match points.

Importance: If there are multiple targets for scanning forward in a stream, it can be quite difficult to code, requiring a user to implement full granularity of the grammar (for example, in string parsing the use of charsets at a lexical level). However, if TO and THRU allow multiple targets, then this effort can be avoided.

Syntax:

 * TO rule
 * THRU rule

Where the syntax of the rule would be:

 * token
 * QUOTE token
 * [token | ...]
 * [QUOTE token | ...]

Rules would likely be composed of only simple tokens or bitsets rather than full rules, for efficiency. Some values are difficult to recognize without full rules, so a QUOTE keyword may be helpful to recognize some values, though likely not nested structures. TO and THRU are not greedy - they match the first possible match.

Example:

```rebol
 copy chars to [cr | lf | end]

```

TO or THRU fail if there is no point in the subsequent data where their argument rule can succeed. 

A correction to the existing TO or THRU behavior: When string parsing, TO or THRU "" should always succeed and be equivalent to NONE. We might also consider allowing TO or THRU NONE, with the same meaning. The recursive equivalents of TO and THRU succeed in these cases.

Recursive equivalents:

 * A: [THRU B] should be equivalent to A: [B | SKIP A]
 * A: [TO B] should be equivalent to A: [AND B | SKIP A]

Notes:

 * This can be very complicated to implement in a general way that has any reasonable performance. However, it should be possible to implement a useful subset, such as that shown in the above example (simple token matches, not grammar matches). It remains to be seen whether the NOT qualifier could be within the high performance subset.
 * An alternative strategy (suggested by Peta) would be to have the TO or THRU operation determine whether its rule matches the subset that can be performed with an optimized method, and then use that method when it can. We would need to determine whether the runtime overhead of rule analysis would overshadow the benefits (- only in case the rule analysis is not performed already to check for invalid expressions).

#### Notes about NOT

With [NOT](#NOT) (Editor's Note: Peta's suggestion and my favorite), TO NOT and THRU NOT would be equivalent because [NOT](#NOT) consumes no input. The rule can fail only in specific circumstances:

 * TO or THRU NOT NONE would always fail, even at the end.
 * TO or THRU NOT END would fail at the end of the data.
 * More generally, TO or THRU NOT RULE can fail only if the given RULE would succeed at the end of the input.

Editor's Note: With the same rules that would cause TO NOT to fail, the greediness of ANY will cause it to go into an endless loop, a common error in PARSE usage. That would be a good argument for a non-greedy or at least safe equivalent.

However, TO NOT RULE isn't generally equivalent to ANY RULE, e.g.:

```rebol
to [not "ab"]

```

isn't equivalent to

```rebol
any "ab"

```

, since e.g.

```rebol
parse "ababab" [a position: (print ["success" index? position])]

```

the A: [TO [NOT "ab"]] rule would succeed reaching index 2 in the given string, while A: [ANY "ab"] would advance to the tail of the input.

TO [NOT RULE | END] is equivalent to ANY [AND RULE SKIP].

### WHILE operator

Suggested by: Ladislav, Brian, name by Carl

Purpose: Iterate RULE while it matches.

Importance: The simplest possible iteration.

Syntax:

 * WHILE rule

Recursive definition/equivalent:

 * A: [WHILE B] should be equivalent to A: [B A | none]

### ANY operator

Suggested by: Carl, Pekr

The operator should stop iterating, when the input position doesn't advance.

Purpose: Iterate rule while it matches, but eliminate endless loops.

Note: the

```rebol
   any rule

```

expression is equivalent to:

```rebol
   while [and [rule a:] b: if (greater? index? a index? b) :a]

```

### SOME operator

Suggested by: Carl, Pekr

The operator should stop iterating, when the input position doesn't advance.

Purpose: Iterate rule while it matches, but eliminate endless loops.

Note: the

```rebol
   some rule

```

expression is equivalent to

```rebol
   and [rule a:] b: opt [if (greater? index? a index? b) :a any rule]

```

### UNTIL operator

Suggested by: Ladislav

Purpose: Iterate RULEB until RULEA is matched.

Importance: Increases the expressiveness of Parse without needing BREAK to stop the iteration.

Syntax:

 * UNTIL rulea ruleb

Recursive definition/equivalent:

 * A: [UNTIL B C] should be equivalent to A: [B | C A]

Examples:

 * a rule advancing past the "abc" substring on success

```rebol
 until "abc" ["a" | "b" | "c"]

```

 * a rule advancing to the start of the "abc" substring on success

```rebol
 until [and "abc"] ["a" | "b" | "c"]

```

**Note:** In recent releases, UNTIL can be implemented using WHILE:

```rebol
 while [b accept | c | reject]

```

### ACCEPT keyword

The ACCEPT keyword terminates repetition of the current rule matching, treating the match as successful. (see the example in UNTIL)

### REJECT keyword

The REJECT keyword terminates repetition of the current rule matching, treating the match as unsuccessful. (see the example in UNTIL)

### CHANGE 1

Suggested by: BrianH (renamed by Carl)

Purpose: Modification of the data being parsed (generalization of other suggestions).

Importance: The most frequently requested ability for PARSE, based on help requests from people who messed up the workarounds.

Syntax:

 * CHANGE rule value
 * CHANGE ONLY rule value
 * CHANGE NONE value
 * CHANGE ONLY NONE value
 * CHANGE rule (expression)
 * CHANGE ONLY rule (expression)
 * CHANGE NONE (expression)
 * CHANGE ONLY NONE (expression)
 * CHANGE rule NONE

If specified the rule would be matched before the change is made, failing and backtracking if not matched. The ONLY modifier would be used in block parsing, and would insert the result of the expression as a block rather than inline, like CHANGE/ONLY. The NONE keyword would mean that nothing would be removed or inserted, respectively.

Replacement values can be one of these:

 * A word (or path?) which refers to the value (dereferenced, not executed).
 * A REBOL expression calculated in parentheses.
 * Some other literal value (except word types and parens), treated as a terminal (like QUOTE).

After this operation finishes the position is set to the position after the replacement value, just like the return value of the CHANGE function in REBOL. The current position at the end of the matching rule is used as the other end of the segment to be changed, with CHANGE/PART compatibility rules applying.

Examples:

 * Removing a series of values.

```rebol
 a: "12abc34"
 parse a [some [to alpha change [some alpha] none]]
 a = "1234"

```

 * Changing a series of values to another value.

```rebol
 a: "12abc34"
 parse a [some [to alpha change [some alpha] "zzzz"]]
 a = "12zzzz34"

```

 * Inserting a value after a pattern.

```rebol
 a: "12abc34"
 parse a [some [to alpha some alpha change none "zzzz"]]
 a = "12abczzzz34"

```

 * Changing a series of values to a calculated block.

```rebol
 a: [1 2 a b c 3 4]
 parse a [some [to word! change only [some word!] (array/initial 4 'z)]]
 a = [1 2 [z z z z] 3 4]

```

Note: All modification operations are irreversible, and so will not be undone on backtracking. As noted in the theory section above, it may not be easy to backtrack to a "reasonable" position, but backtracking issues can be controlled with careful use.

### CHANGE 2

Suggested by: Carl (variant of [CHANGE 1](#CHANGE_1))

Purpose: Modification of the data being parsed (generalization of other suggestions).

Importance: The most frequently requested ability for PARSE, based on help requests from people who messed up the workarounds.

Syntax:

 * CHANGE pos value
 * CHANGE ONLY pos value
 * CHANGE pos (expression)
 * CHANGE ONLY pos (expression)

The pos argument is a series position set earlier, used as one end of the portion to be changed. (Editor's addition) Or pos could be an integer offset advancing from the current position, used as an end point. The other end of the portion is the current position at the point of the rule. CHANGE/PART compatibility would apply to the start and end positions. The ONLY modifier would be used in block parsing, and would insert the value or result of the expression as a block rather than inline, like CHANGE/ONLY. The NONE keyword of [CHANGE 1](#CHANGE_1) would not be necessary - REMOVE or INSERT could be used instead.

Replacement values can be one of these:

 * A word or path which refers to the value (dereferenced, not executed).
 * A REBOL expression calculated in parentheses.
 * Some other literal value (except word types and parens), treated as a terminal (like QUOTE).

After this operation finishes the position is set to the position after the replacement value, just like the return value of the CHANGE function in REBOL.

Examples:

 * Removing a series of values from a string.

```rebol
 a: "12abc34"
 parse a [some [to alpha b: some alpha change b ""]]
 a = "1234"

```

 * Changing a series of values to another value.

```rebol
 a: "12abc34"
 parse a [some [to alpha b: some alpha change b "zzzz"]]
 a = "12zzzz34"

```

 * Inserting a value after a pattern using a series position.

```rebol
 a: "12abc34"
 parse a [some [to alpha some alpha b: change b "zzzz"]]
 a = "12abczzzz34"

```

 * Inserting a value after a pattern using an integer offset.

```rebol
 a: "12abc34"
 parse a [some [to alpha some alpha change 0 "zzzz"]]
 a = "12abczzzz34"

```

 * Changing a series of values to a calculated block.

```rebol
 a: [1 2 a b c 3 4]
 parse a [some [to word! b: some word! change only b (array/initial 4 'z)]]
 a = [1 2 [z z z z] 3 4]

```

Note: All modification operations are irreversible, and so will not be undone on backtracking. As noted in the theory section above, it may not be easy to backtrack to a "reasonable" position, but backtracking issues can be controlled with careful use.

### REMOVE 1

Suggested by: BrianH

Purpose: Shortcut for [CHANGE](#CHANGE_1) rule NONE

Importance: Ease of use.

Syntax:

 * REMOVE rule

Example:

```rebol
 a: "12abc34"
 parse a [some [to alpha remove [some alpha]]]
 a = "1234"

```

Note: See [CHANGE 1](#CHANGE_1) for behavior.

### REMOVE 2

Suggested by: Carl (variant of [REMOVE 1](#REMOVE_1))

Purpose: Shortcut for [CHANGE](#CHANGE_2) pos "", or [CHANGE](#CHANGE_2) pos [] for block parsing.

Importance: Ease of use.

Syntax:

 * REMOVE pos

Example:

```rebol
 a: "12abc34"
 parse a [some [to alpha b: some alpha remove b]]
 a = "1234"

```

Note: See [CHANGE 2](#CHANGE_2) for behavior.

### INSERT

Suggested by: BrianH

Purpose: Insert a value (shortcut for CHANGE).

Importance: Ease of use.

Syntax:

 * INSERT value
 * INSERT ONLY value
 * INSERT (expression)
 * INSERT ONLY (expression)

Examples:

 * Inserting a value after a pattern.

```rebol
 a: "12abc34"
 parse a [some [to alpha some alpha insert "zzzz"]]
 a = "12abczzzz34"

```

 * Changing a series of values to a calculated block.

```rebol
 a: [1 2 a b c 3 4]
 parse a [some [to word! some word! insert only (array/initial 4 'z)]]
 a = [1 2 a b c [z z z z] 3 4]

```

Note: See CHANGE for behavior.

### USE

Suggested by: BrianH

Purpose: Recursion-safe and multitasking-safe local words.

Importance: Essential for working with hierarchical structures or syntax.

Syntax:

 * USE [vars] [rules]

Examples:

 * Recursive length printing.

```rebol
 rule: [
     use [a b] [
         "b" (print 1) |
         a: "a" rule "a" b: (print subtract index? b index? a)
     ]
 ]

```

```rebol
 parse "aba" rule
 ; Prints
 1
 3

```

```rebol
 parse "aabaa" rule
 ; Prints
 1
 3
 5

```

The [USE-RULE function](https://web.archive.org/web/20150420170533/http://www.rebol.org/view-script.r?script=use-rule.r) defined in the script library implements this as the default variant, but it has also a /NO-REBIND variant, which does not rebind the given rules block, i.e. takes care of doing the binding just once.

Notes: The rule argument to USE would likely need to be have BIND/copy applied, and only nested blocks and parens would be bound, just like with USE in REBOL. It is the initial rebinding that makes this multitasking-safe. We might be able to use the semantics of R3 function! contexts with a little memoization - the overhead should be compared to the BIND/copy overhead to see which is better. However, this would prevent the bound words from being used outside the extent of the USE operation.

In REBOL 2 (and in the public REBOL 3 alpha from January 2008), the PARSE keywords can be used as variables without influencing the PARSE functionality, like here:

```rebol
rebol/version ; == 2.99.4.3.1
parse [] b: [(end: 11) end (print [end same? bind? second b bind? 'end])]
; 11 true

```

Similarly, the binding of the PARSE keywords does not change and does not influence the functionality, as can be proven too.

### IF (condition)

Suggested by: BrianH, Ladislav (originally proposed as CHECK)

Purpose: Direction of parse rules based on semantic criteria.

Importance: Powerful feature for advanced parse rules. There are some patterns that can't be recognized easily by just syntax rules - sometimes the difference is semantic. This is a feature required by some more advanced language processors including parsers of some programming languages.

Syntax:

 * IF (expression)

The REBOL expression in the paren following the IF keyword is executed like any other paren. If the result of the expression would be accepted as true by REBOL's IF function, parsing continues as usual; if not, the rule fails as if there were a [FAIL](#FAIL) there.

Previously you had to do something like this:

```rebol
 (cont: unless condition [[end skip]]) cont

```

Examples:

 * Odd numbers only.

```rebol
 [set x integer! if (odd? x)]

```

 * Word not defined yet.

```rebol
 ['define set x word! if (not find words x)]

```

### REVERSE

Suggested by: Carl

Purpose: Reverses parse direction in the input stream.

Importance: Some patterns are easier to match by advancing forward, then reversing the direction of the pattern match. (Often this is a shortcut for what would be a more complicated approach.)

Example: 

```rebol
 to lf reverse [remove cr]

```

This would advance to the LF char, then look back one for CR and remove it if found.

Notes: There are two interpretations of this proposal that should be considered.

Syntax 1:

 * REVERSE

In this version, the parse direction would be changed indefinitely. The parse direction should probably be saved on block entry and restored on backtrack, like the position - if there is no subsequent backtracking then any directional changes would stick until the next REVERSE.

Syntax 2:

 * REVERSE rule

In this version, only the argument rule would be checked in reverse. The parse direction would be restored after the rule is finished.

For either version, we need to consider whether string literals would be matched in reverse when the direction is reversed in string parsing. Whichever is more efficient would probably be fine.

### BREAK only from loops

Suggested by: BrianH

Purpose: Have the BREAK operation break to SOME or ANY, rather than just out of a block.

Importance: It would make BREAK work the way people would expect, and more useful.

Notes: Right now you can't put a BREAK in a nested block or a referred-to rule. This limits its usefulness by quite a bit. It would be helpful if PARSE's BREAK acted like the BREAK function and escaped from the closest loop, no matter how far it is nested in grouping blocks.

Editor's Note: Based on a misunderstanding of what constitutes a loop. Rejected.

### n BREAK

Suggested by: Steeve Luc

Purpose: To specify how many block levels you want to break from.

Importance: This allows you to create more complex rules and still break out of them if necessary.

Syntax:

 * n BREAK

Where n is a positive integer. This syntax mirrors the number repetition operator.

Discussion: If the proposal to redefine [BREAK](#BREAK) as breaking to a loop is implemented, then this would let you break from n nested loops. Otherwise, it would be to break out of n nested blocks.

### BREAK/return

Suggested by: Peter Wood (as RETURN), many others who assumed it worked already.

Purpose: Make the REBOL BREAK/return function work within the PARSE function.

Importance: Ease of use and consistency.

Notes: This function would be called from within parens, and would work just like BREAK/return does for loops. If it is not called, PARSE returns what it would normally return.

Example:

```rebol
 >> parse "a" [alpha (break/return 1)]
 == 1
 >> parse "1" [alpha (break/return 1)]
 == false

```

Note: There is a question of whether this change could introduce an unnecessary inconsistency. The goal of the proposal can be easily achieved by putting the parse call into a function body, a loop, or a catch control function, e.g. like this:

```rebol
 >> loop 1 [parse "a" [alpha (break/return 1)]]
 == 1
 >> loop 1 [parse "1" [alpha (break/return 1)]]
 == false

```

### RETURN

Suggested by: BrianH (inspired by Peter Wood)

Purpose: Return a recognized series from the PARSE function.

Importance: Ease of use, this time for parse-and-grab.

Syntax:

 * RETURN rule

Notes:

 * RETURN stops the parsing immediately and returns from the PARSE function.
 * For block parse RETURN copies like COPY, rather than references like SET.
 * If the BREAK/return proposal is accepted this may not be necessary.

Examples:

 * Return the first tag in a string.

```rebol
 >> parse "abc<hello>def" [to "<" return thru ">"]
 == "<hello>"
 >> parse "abcdef" [to "<" return thru ">"]
 == false

```

### INTO type

Suggested by: BrianH, Steeve Luc

Purpose: Making INTO recognize more types in block parsing, including switching to string parsing for string types. This is a generalization of the INTO-STRING proposal.

Importance: Incredibly helpful for advanced dialects and mixed block and string dialects.

Syntax:

 * INTO type rule

Notes:

 * The type parameter of INTO would be limited to series types and typesets that include only series types.
 * The type or typeset would need to match before the rule is tested. Failure of either counts as failure.
 * An INTO statement could be used as the rule argument to COPY, SET or [RETURN](#RETURN) operations.
 * Any position-setting or directional operations in the rule only refer to position within the value under consideration, not the outer context.

Examples:

 * Parsing a string in a block.

parse [1 "abc" 2] [integer! into string! [some alpha] integer!]

Editor's Note: Rejected in favor of the typeless [INTO](#INTO) proposal below.

### INTO

Suggested by: Peta

Purpose: Making INTO recognize more types in block parsing, including switching to string parsing for string types.

Importance: Incredibly helpful for advanced dialects and mixed block and string dialects. No INTO syntax change makes it more backwards compatible - this is just a semantic change.

Syntax:

 * INTO rule

Notes:

The existing INTO syntax would be extended to string types, though still only be in the block parsing dialect. PARSE would switch into string parsing if the value in question is a string type. As with the above proposal an INTO statement could be used as the rule argument to COPY, SET or [RETURN](#RETURN) operations.

Notes:

 * An INTO statement could be used as the rule argument to COPY, SET or [RETURN](#RETURN) operations.
 * Any position-setting or directional operations in the rule only refer to position within the value under consideration, not the outer context.
 * This would lose the feature of having INTO fail when presented with string values, in favor of switching modes. This could be a subtle semantic change that might affect existing rules even though the syntax is the same.
 * Since there are differences between the string and block parsing dialects, it would be a good idea to check the type ahead of time with the [AND](#AND) proposal above.

Examples:

 * Parsing a string in a block.

```rebol
 parse [1 "abc" 2] [integer! and string! into [some alpha] integer!]

```

### Set PARSE position to another series

Suggested by: Peta

Purpose:

 * Allow the PARSE position to be set explicitely to another series

Importance:

 * Citing the editor's note: "For those of you who are fans of REBOL history, it is no longer possible (even in REBOL 2) to set the parse position to another series than the one being parsed already, a previously popular technique used for incremental parsing (and which crashed PARSE if misused)."	 
 * PARSE now detects attempts to explicitly set its position into another series and "forbids them". Since the INTO operator does just that (it sets the PARSE position into another series), such an operation is provably always possible. Therefore, as an alternative to the current behavior, it is possible for PARSE to do the appropriate bookkeeping to make sure no crash can occur.

Issues:	 

 * There is one property, which can be influenced by this: The final check, whether the start rule has finished at the END OF INPUT position. In case this proposal was accepted, the END OF INPUT test can be reasonably defined by testing, whether the final position is at the end of the series parsed as the last one.

Syntax:

 * :position

Editor's Note: INTO works through recursion, not parse position setting. Accepted, but it currently has a problem with backtracking (see [[1787](https://web.archive.org/web/20150420170533/http://issue.cc/r3/1787|CC)] for details, at least it doesn't crash).

### DO

Suggested by: Gabriele

Purpose: Putting REBOL code in your dialects. Block parse dialect only.

Syntax:

 * DO rule

At the current parse position, a single expression of REBOL code will be evaluated, using the algorithm of the REBOL DO/next function. The parse position is then changed to the position after the expression, as would be returned by DO/next. No binding is performed by the DO operation, but it does not restrict any binding explicitly done by the expression.

The single result of the expression is then matched by the argument rule as if the value were contained in a nested block and the rule used as the argument of INTO. If the expression result is of a block type it will be treated as if it is nested, not inline. Any rule that expects to match more than one value will fail, as there will only be one value to be matched. The SKIP rule will match any value, including #[none], but not unset values. The NONE rule will match either #[none] or unset values. An empty rule block will only match unset values (Question: Should the NONE rule also behave this way? Decision for Carl - he knows the internals).

DO will fail if:

 * There is no expression to be executed.
 * The expression is executed correctly but the rule doesn't match.

DO will pass along any unhandled errors the expression generates.

A DO statement could be used as the rule argument to COPY, SET or [RETURN](#RETURN) operations. The result used by those operations will be the value of the expression, not a block containing the value.

Notes: Gabriele's original proposal expresses the concern that failure of a DO operation is likely a sign of a serious problem and thus should generate an error rather than just fail. This concern should be considered.

Examples (adapted from Gabriele's REP):

```rebol
 parse [1 + 1] [set result do integer!]

```

Should evaluate the expression "1 + 1" and set the word 'RESULT to the resulting value (2). The PARSE cursor should be advanced after the expression (i.e. to the tail of the block, in this case).

```rebol
 circle-rule: [
     'circle 
         set center do pair!
         set radius do number! (radius: to-integer radius + 0.5)
 ]

```

```rebol
 parse [circle as-pair x y d / 2] circle-rule

```

Should evaluate "as-pair x y" and set 'center to the result, then should evaluate "d / 2" and set 'radius to the result, etc.

```rebol
 parse [circle 10 20] circle-rule

```

This would fail if DO fails as proposed, but if DO generates errors this would raise an ERROR! like:

```rebol
 ** Parse Error: Expecting pair!, not 10
 ** Near: 10 20

```

(i.e. with the NEAR field reporting the position of the PARSE cursor before the evaluation).

```rebol
   parse [circle 5x5 7 / 0] circle-rule
   ** Math Error: Attempt to divide by zero
   ** Near: 7 / 0

```

### LIMIT

Suggested by: Carl

Purpose: Limits lookahead distance.

Importance: Performance for some cases. Useful in cases where a TO or THRU target is expected to be hit within a certain distance, but a miss might be quite large (the entire input.)

Syntax:

```rebol
 limit integer! | series!

```

Examples:

```rebol
 limit 1000 to newline
 limit here thru "test"

```

Discussion:

Limits the lookahead scan to a given range. Tells the parser to force a failure if the index is greater or equal to the limit that has been set.

In the example above, the code scans forward looking for a new line, but if it's not found within 1000 characters, the code fails.

Limit could be used to handle situations where you would otherwise have to do a recursive call to parse within a production, or repeated calls to parse with more specific rules. Instead you would find the limit in one pass, backtrack with stay or fail, then do a more specific search within the limit.

Disadvantages:

Could make some programmers lazy.

### /all by default

Suggested by: Carl, to much acclaim

Purpose, Importance: It's what people do the most anyways.

Discussion:

The behavior of string parsing without /all has been found to be so tricky that most experts don't use the default behavior except for by accident. This proposal will get rid of the old default that causes so much trouble, and let us propose a new option that works better.

Disadvantages:

We'll need to get rid of the (now unnecessary) /all refinement. The aforementioned new option will probably need to be implemented too.

### /ignore stuff option

Suggested by: BrianH

Purpose: To simplify parsing by letting you specify stuff to ignore silently.

Importance: The old default string parse behavior was too tricky to use, but good in theory. It would be useful to be able to be more specific about what you want to ignore, and could be extended to block parsing too.

Discussion:

For string parsing, the stuff parameter could be one of a char! or bitset!, or a string! that would be treated like a parameter to make bitset!. For block parsing, stuff could be a datatype!, a typeset!, or a block of types treated like a parameter to make typeset!. If any of the stuff is specified in the parse rule operation currently being considered, then parse pays attention to it. Otherwise, if anything matching the stuff is found in the input it is skipped as if it isn't there. 

Any stuff in the middle of a range to be copied with the copy operation will be copied - it doesn't completely disappear. The stuff is only ignored by the matcher. (This is where the discussion part comes in though.)

Advantages:

It might allow programmers to be lazy.

## Examples

Here are some examples of the use of multiple proposals to show how they work together.

 * Make directory tree in place (advanced). This uses [USE](#USE), [AND](#AND), [CHANGE 2](#CHANGE_2), [INTO](#INTO), and [REVERSE rule](#REVERSE).

```rebol
 use [r d res] [
     parse res: read d: %./ r: [
         use [ds f] [
             any [
                 (ds: d)
                 and [
                     set f into [to end reverse "/"] (
                         d: join d f
                         f: read d
                     )
                     change only -1 f
                 ] into r
                 (d: ds)
                 | skip
             ]
         ]
     ]
     res
 ]

```

 * Explained:

```rebol
 use [r d res] [ ; External words from standard USE statement

     ; r is the rule, d is the directory, res is the block returned by reading d.
     parse res: read d: %./ r: [

         use [ds f] [ ; These words override any outer words.

             any [

             ; Check for directory filename.
                 (ds: d) ; This maintains a recursive directory stack.

                 and [ ; Make the change in look-ahead.

                     ; Set f to the filename if it is a directory else fail.
                     ; You don't need to check for a file! - that's all READ returns.
                     set f into [to end reverse "/"] ; string parsing

                     ; f is a directory filename, so process it.
                     (
                         d: join d f ; Add the directory name to the current path.
                         f: read d   ; Read the directory into a block.
                     )
                     ; f is now a block of file! values.

                     ; The position has advanced, so we are currently after the directory name.
                     ; Change the previous value (the directory name) to the block, nested.
                     change only -1 f

                 ] ; Back in the same position as before the AT.

                 ; We only get this far if the last rule succeeded and changed the value
                 ; to a block, so we don't have to check for a block! type value.

                 ; Now recurse into the new block.
                 into r

                 (d: ds) ; Pop the directory stack.

             ; Otherwise backtrack and skip.
                 | skip

             ] ; end any

         ] ; end use

     ] ; end parse

     res ; This is the expanded directory block.
 ]

```

Retrieved from "[http://www.rebol.net/wiki/Parse_Project](https://web.archive.org/web/20150420170533/http://www.rebol.net/wiki/Parse_Project)"