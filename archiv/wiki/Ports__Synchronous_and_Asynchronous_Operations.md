# Ports: Synchronous and Asynchronous Operations

## Controlling Factors

When a port action is requested, it can be processed in a blocking (synchronous) or non-blocking (asynchronous) fashion depending on a few factors:

 * **What is the default mode for the scheme of the port.** For example, in 3.0 the file I/O scheme always blocks. The file scheme has no other choice. Async IO is not allowed.
 * **How the port is used.** There are two modes: single request and multi-request. The single request mode is provided as a programming shortcut, so the port will always block. Multi-requests can be blocking or unblocking, depending on the factors listed here.
 * **How the port is initialized.** For a port to operate non-blocking, an AWAKE handler function must be provided to process the callback events.
 * **The port action that is called.** For example, MAKE and QUERY are synchronous, but READ and WRITE can be asynchronous.

## Examples

Three of the above differences can be shown here with HTTP.

A single request, will block until the request is completed or an error occurs:

```rebol
   data: read http://www.rebol.com/data.r

```

Here is another example that uploads data and gets a result:

```rebol
   result: write http://www.rebol.com/cgi-bin/updata.r data ; will do POST as application/x-www-form-urlencoded
   result: write http://www.rebol.com/cgi-bin/updata.r reduce ['post [Content-type: "text/x-rebol"] data]

```

If multiple requests are desired, they can be made, but each will block until the request has finished or an error has occurred:

```rebol
   port: open http://www.rebol.com
   data: write port [get %/data.r]
   index: write port [get %/index.txt]
   result: write port reduce ['post %/cgi-bin/upload.r new-data]
   close port

```

Multiple requests can also be made asynchronously if an AWAKE handler has been provided:

```rebol
   port: make port! http://www.rebol.com
   port/awake: func [event] [
       switch event/type [
           connect ready [
               write event/port [get %/]
           ]
           done [
               result: copy event/port
               return true
           ]
       ]
       false
   ]
   open port
   wait port

```

The AWAKE handler provides the requests and process the responses.

## Waiting on a Port

When non-blocking (async) mode is used, a program synchronize with the port by using the WAIT function. The WAIT will block until the port's AWAKE handler returns a TRUE result.

In the last example above, if the AWAKE handler supports it, you can write code such as:

```rebol
  port: make port! http://www.rebol.com
  port/awake: :my-http-awake-handler
  open port
  wait port

```

Here, the OPEN in the AWAKE handler must return TRUE for the WAIT condition to be satisfied.
The code could then continue with:

```rebol
  data: read port
  wait port

```

and now it will wait for the READ to complete (assuming that the AWAKE handler also returns TRUE when the read has finished).

Of course, this "mixed-mode" of operation will rarely be used (because normally your handler will do all the work), but it is made available to give you a full range of control over how your port actions are processed.

WAIT also supports a timeout condition. You can write:

```rebol
  wait [port 10]

```

meaning wait for the port, or 10 seconds, whichever comes first.

WAIT can also specify multiple ports at the same time. For full details see the WAIT function.

Retrieved from "[http://www.rebol.net/wiki/Ports:_Synchronous_and_Asynchronous_Operations](https://web.archive.org/web/20150423001708/http://www.rebol.net/wiki/Ports:_Synchronous_and_Asynchronous_Operations)"