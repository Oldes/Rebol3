# IPC - Inter-process communication

## To daemon or not to daemon

This is the first question. Not because we need to answer it first, but because we need to keep it in mind when answering the other questions. For example, the format of endpoints may be different if they are only for local use versus net use.

Should we use a daemon, so there is only one model for IPC on a single machine as well as across the net? Can we still do everything we would want to do on a single machine without a daemon? What problems or challenges does a daemon pose? Can the implementation be such that if the endpoints are local, it can use something like a memory mapped file, and only require the daemon if remote endpoints are used?

See also: [Threading](#Threading)

## Endpoints

What do endpoints (addresses) look like?

Can, and should they be standard URLs, with the scheme defining the protocol?

Jabber uses an email format for Jabber IDs. The resource-id identifies specific objects that are associated with a user, such as devices or locations.

```rebol
   user@domain.com/resource-id
```

What kind of "processes" are we talking about as endpoints? Is it about communication between R3 tasks (within a single process), about multiple R3 processes on the same machine, R3 processes across a LAN? WAN? Heterogenous processes?

## Messages

What do messages look like? Can it be any simpler than this?

```rebol
   [opt header  body]
```

For interop purposes, how do you map from native REBOL format to JSON, XML, or other formats?

## Interface/API

What methods do we need? What kind of state management if any? Synchronus versus asynchronous and the ability to wait for a given message. e.g. the tuplespace model where a worker asks for a tuple matching a given pattern and "blocks" until it is notified when a matching tuple arrives.

What kind of messages do we have?

 * request
 * reponse
 * error

Should it work basically like HTTP? RestMS?

## Threading

Do we use the same mechanism for inter-thread messaging?

Is this at all possible?

Is this simply mimicked using a common REBOL API running over two different engines?

Advantages:
 * Consistency in API
 * Possible ability to control rebol threads directly using process-level as well as thread-level IPC

Disadvantages:
 * Depending on chosen model and/or target platform, it might be suboptimal performance-wise.
 * Not all IPC models might support this both conceptually and/or programatically.

## Implementation

Should this be coded right into the host-kit as a default part of REBOL or as an extension?

If an extension model is used, should it be included by default in the host or loaded at run-time?

## Interop

 * What other systems and mechanisms might we want to interoperate with? 
 * What is the model for translation to and from other formats?

## Other things to consider

 * Discovery
 * Security
 * Presence (and its associated overhead)
 * Persistence
 * Service guarantees
 * Leases
 * Channels
 * Workflow
 * Monitoring
 * DHT
 * Routing

## References and other models

 * General IPC - [http://en.wikipedia.org/wiki/Interprocess_communication](http://en.wikipedia.org/wiki/Interprocess_communication)
 * Unix IPC - [http://www.catb.org/esr/writings/taoup/html/ch07s02.html](http://www.catb.org/esr/writings/taoup/html/ch07s02.html)
 * WIndows IPC - [http://msdn.microsoft.com/en-us/library/aa365574](http://msdn.microsoft.com/en-us/library/aa365574)
 * RFC822 - [http://www.faqs.org/rfcs/rfc822.html](https://datatracker.ietf.org/doc/html/rfc822)
 * ØMQ - [http://zeromq.org/](https://zeromq.org/)
 * Spread - [http://www.spread.org/](https://www.spread.org/)
 * Tuplespace  - Simple API, flexible, generative communication model, pattern matching model [http://www.gigaspaces.com/](https://web.archive.org/web/20130819091816/http://www.gigaspaces.com/)
 * Ice - [http://www.zeroc.com/ice.html](https://web.archive.org/web/20130819091816/http://www.zeroc.com/ice.html) GPL
 * Jabber/XMPP
 * Atom+APP/RSS(+SSE) - Discovery model, pub-sub model
 * FeedSync
 * Google - Wave, Protocol buffers
 * AWS SQS
 * ESB
 * SEDA - [http://www.eecs.harvard.edu/~mdw/proj/seda/](https://web.archive.org/web/20130819091816/http://www.eecs.harvard.edu/~mdw/proj/seda/)
 * RestMS - [http://www.restms.org/](https://web.archive.org/web/20130819091816/http://www.restms.org/)
 * Live Mesh
 * Android - Intent filters, Data Scheme, Data Authority
 * map-reduce
 * Eclipse - Plug-in based framework
 * E - Promises, communicating event loops [http://www.erights.org/](https://web.archive.org/web/20130819091816/http://www.erights.org/)
 * Erlang - Lightweight processes, pattern matching [http://www.erlang.org/index.html](https://web.archive.org/web/20130819091816/http://www.erlang.org/index.html)
 * Limbo - channels [http://en.wikipedia.org/wiki/Interprocess_communication](https://web.archive.org/web/20130819091816/http://en.wikipedia.org/wiki/Interprocess_communication) §4.2.5 §8.2.9 §9.8 §9.13 §12.4
 * NetLogo - Agents, visualization [http://ccl.northwestern.edu/netlogo/](https://web.archive.org/web/20130819091816/http://ccl.northwestern.edu/netlogo/)
 * Termite Scheme - a variant of Scheme intended for distributed computing, inspired by Erlang [http://code.google.com/p/termite/](https://web.archive.org/web/20130819091816/http://code.google.com/p/termite/)
 * AMQP - [http://www.amqp.org/confluence/display/AMQP/Advanced+Message+Queuing+Protocol](https://web.archive.org/web/20130819091816/http://www.amqp.org/confluence/display/AMQP/Advanced+Message+Queuing+Protocol)
 * JMS
 * MQ
 * SOAP - [http://en.wikipedia.org/wiki/SOAP](https://web.archive.org/web/20130819091816/http://en.wikipedia.org/wiki/SOAP)
 * Tibco SmartSockets - [http://www.tibco.com/multimedia/ds-smartsockets_tcm8-827.pdf](https://web.archive.org/web/20130819091816/http://www.tibco.com/multimedia/ds-smartsockets_tcm8-827.pdf)
 * WCF - [http://msdn.microsoft.com/en-us/library/ee354381.aspx](https://web.archive.org/web/20130819091816/http://msdn.microsoft.com/en-us/library/ee354381.aspx)
 * WebDAV - [http://www.webdav.org/](https://web.archive.org/web/20130819091816/http://www.webdav.org/)  [http://en.wikipedia.org/wiki/WebDAV](https://web.archive.org/web/20130819091816/http://en.wikipedia.org/wiki/WebDAV)
 * WSDL - [http://www.w3.org/TR/wsdl](https://web.archive.org/web/20130819091816/http://www.w3.org/TR/wsdl)
 * Yahoo Pipes - [http://pipes.yahoo.com/pipes/](https://web.archive.org/web/20130819091816/http://pipes.yahoo.com/pipes/)
 * XML-RPC

## Dialect thoughts

 * connect
 * disconnect
 * find/discover
 * join
 * call
 * send
 * receive/recv
 * reply
 * respond
 * tag
 * log
 * answer/hello?
 * hello
 * goodbye
 * ask
 * query
 * lookup
 * start
 * stop
 * restart
 * shutdown
 * quit
 * leave
 * check
 * who's-there?
 * lease
 * expired?
 * still-working?
 * job status
 * service provider
 * monitor
 * service monitor
 * job monitor
 * request producer/job maker
 * job manager/job controller

Retrieved from "[http://www.rebol.net/wiki/IPC_-_Inter-process_communication](https://web.archive.org/web/20130819091816/http://www.rebol.net/wiki/IPC_-_Inter-process_communication)"