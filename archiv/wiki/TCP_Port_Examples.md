# TCP Port Examples

## TCP Server

**Warning: This code is out of date and being updated.**

Here is an example TCP server that opens port 8080 and receives new connections. Each connection then gets its own asynchronous handler function (which in this case, simply reads and writes text.)

```rebol

print "Serving port 8080..."

open-subport: func [port] [
    print "=== Creating sub-port"
    port/awake: func [event /local port] [
        print ["=== Subport event:" event/type]
        port: event/port
        switch event/type [
            read [
                print ["    " to-string port/data]
                write port to-binary "got it!"
            ]
            wrote [read port]
            close [close port]
        ]
    ]
    write port to-binary "connected^/"
]

server: open tcp://:8080

server/awake: func [event] [
    print ["*** Server event:" event/type]
    if event/type = 'accept [
        open-subport first event/port
    ]
    false
]

wait 30
close server
print "Done serving"
halt

```

## TCP Client

Here is a client that connects to port 8080, then sends strings to the server, and receives a response back from the server.

```rebol

print "Connecting to port 8080..."

msg-count: 0

client: open tcp://localhost:8080

client/awake: func [event /local port] [
    print ["=== Client event:" event/type]
    port: event/port
    switch event/type [
        lookup [open port]
        connect [write port to-binary "hello"]
        read [
            print ["    " to-string port/data]
            ++ msg-count
            either msg-count < 10 [
                write port to-binary reform ["ok" msg-count]
            ][
                close port
                return true
            ]
        ]
        wrote [read event/port]
    ]
    false
]

wait [client 30]
close client
print "Done"
halt

```

## Notes
 * Both the server and the client use asynchronous AWAKE handlers to process events. An AWAKE handler is a simple callback function that is called when the state of the port changes, such as when it opens, has sent its data, has read new data, etc.
 * The READ and WRITE functions **initiate the read and write actions within the handlers**. That is, you must call READ in order for the READ action to occur (like setting the async mode in R2 async). When the read has data, the READ event will happen. If your handler hangs, chances are you forgot this step.
 * The port uses its port/data buffer for both sending and receiving data. If you want, you can allocate your own buffer for that.
 * Data is transferred as binary by default. If you need to convert the line terminations, use the ENLINE and DELINE functions.
 * Notice that the client must include an extra step to perform the DNS lookup. When the lookup event occurs, it calls OPEN again to open the TCP/IP connection. And, if you want, you can use OPEN? to check if the connection is open.
 * Although the server uses WAIT with a timeout, an actual server would not do that. It's done in this example just to stop the server after 30 seconds.
 * For larger data transfers, WRITE and READ operations will be broken into packets. You can use a length accumulator or special symbol sequence to know when the transfer is complete. Also, you should segment your WRITEs into chunks of 32K or less.
 * If you use a local firewall, it may block these examples. Set it to allow port 8080 to connect.

## Output

Here's what you will see. First, run the server, then run the client.

```rebol

Serving port 8080...
*** Server event: accept
=== Creating sub-port
=== Subport event: wrote
=== Subport event: read
     hello
=== Subport event: wrote
=== Subport event: read
     ok 1
=== Subport event: wrote
=== Subport event: read
     ok 2
=== Subport event: wrote
=== Subport event: read
     ok 3
=== Subport event: wrote
=== Subport event: read
     ok 4
=== Subport event: wrote
=== Subport event: read
     ok 5
=== Subport event: wrote
=== Subport event: read
     ok 6
=== Subport event: wrote
=== Subport event: read
     ok 7
=== Subport event: wrote
=== Subport event: read
     ok 8
=== Subport event: wrote
=== Subport event: read
     ok 9
=== Subport event: wrote
=== Subport event: close
Done serving

```

The client will show:

```rebol

Connecting to port 8080...
=== Client event: lookup
=== Client event: connect
=== Client event: wrote
=== Client event: read
     connected
got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
=== Client event: wrote
=== Client event: read
     got it!
Done

```

Retrieved from "[http://www.rebol.net/wiki/TCP_Port_Examples](https://web.archive.org/web/20131013022452/http://www.rebol.net/wiki/TCP_Port_Examples)"