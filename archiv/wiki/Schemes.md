# Schemes

## Related Scheme Docs

 * [Schemes:Notes](https://web.archive.org/web/20130614003009/http://www.rebol.net/wiki/Schemes:Notes)

## Origin of the Name "Scheme"

The name scheme comes from the standard description of a URI (URL). In this URL:

```rebol
   <a href="https://web.archive.org/web/20130614003009/http://www.rebol.net/index.html" class="external free" title="http://www.rebol.net/index.html" rel="nofollow">http://www.rebol.net/index.html</a>

```

HTTP is the scheme name, www.rebol.net is the host, and index.html is the file path. REBOL uses that same naming convention.

## Built-in schemes

A variety of schemes are built-into REBOL 3 and can be used immediately on boot.

Here is a partial list of built-in schemes:
|Scheme |Description |
|**system** |System port (primary event dispatcher) |
|**stdio** |Standard input and output |
|**stderr** |Standard error |
|**console** |REBOL interactive console |
|**file** |Local file access |
|**dir** |Local file directory access |
|**event** |GUI event handling |
|**DNS** |Domain name lookup |
|**TCP** |Transfer control protocol |
|**UDP** |User datagram protocol |
|**HTTP** |[Hypertext transfer protocol](https://web.archive.org/web/20130614003009/http://www.rebol.net/wiki/Scheme:_HTTP) |
|**shell** |External shell commands (call) |
|**serial** |Serial port access |
|**USB** |USB serial port access |
|**mutex** |Mutual exclusion locks | 

More (such as FTP, SMTP, POP) will be included as time and user-base contributions permit.

To obtain a list of the current schemes, you can write:

```rebol
   probe words-of system/schemes

```

or:

```rebol
  foreach val get system/schemes [print [val/name val/title]]

```

## Scheme object structure

The scheme object is defined in system/standard as:

```rebol
   scheme: context [
       name:       ; word of http, ftp, sound, etc.
       title:      ; user-friendly title for the scheme
       spec:       ; custom spec for scheme (if needed)
       info:       ; prototype info object returned from query
       actor:      ; standard action handler for scheme port functions
       awake:      ; standard awake handler for this scheme's ports
           none
   ]

```

Where:

 name  is the type of scheme. This is the word used in the first part of the URL, such as TCP, HTTP, FTP, etc.. It must be a valid REBOL word (e.g. cannot begin with a number).
 title  is the title of the scheme for user identification and documentation purposes.
 spec  an object used to specify additional features of the scheme.
 info  an prototype object that is instantiated for a query on the scheme.
 actor  the handler function that processes standard actions a port of this scheme type. For example, open, close, read, write, etc.
 awake  a callback function that is invoked when new events arrive for a port of this scheme type.

## Adding a new scheme

The make-scheme function creates a new scheme from a block object specification.

(show example)


Retrieved from "[http://www.rebol.net/wiki/Schemes](https://web.archive.org/web/20130614003009/http://www.rebol.net/wiki/Schemes)"