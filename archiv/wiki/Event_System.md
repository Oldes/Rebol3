# Event System

## Definition

In general, the event system operates similar to the I/O kernel of an operating system. Events isolate I/O port actions from their asynchronous requests and results. Events are a signaling mechanism. We call them events because they include more than just signal information, but also specific REBOL port and device state information as well.

Each time an I/O operation occurs, such as reading from a network device, a precise sequence of events occur to coordinate the interaction between the I/O devices, their ports (REBOL language programming model), and the end-user application.

### An example

To help illustrate the overall concept, consider this code:

```rebol
   port: open tcp://10.10.10.1
   wait port

```

It opens a TCP port to the specified host. Most users don't need to know more than that. The port gets opened and WAIT returns.

However, if you create a new port scheme or I/O device, you will need to understand how the event system works in greater detail. The sections below describe the precise sequence of operations for the example shown above.

## Event Datatype

The Event! datatype holds events.

Event is a direct datatype holding its value within its REBOL value (like integer! and pair!) not a reference datatype. This makes it more efficient, requiring no allocation for most event situations.

The main event fields are:

type a word indicating the general type of event.
port a reference to the event's port, such as a TCP port or GUI event port, but can also reference other objects in general.

The GUI port also supports these special event fields. Note that not all these fields may be valid for all events.

window the window (gob) where the event occurred (for GUI events).
offset a pair indicating where the GUI event occurred. 
code event codes as words or chars.
key keyboard or other button indicators as a char!
flags special flags such as double-click, control, shift.
data special event data, such as a file path for DROP_FILE events.

## WAIT function

The WAIT function can wait on events from one port, multiple ports, or a timer. These are all valid examples:

```rebol
  wait port
  wait [port1 port2]
  wait 10   ; seconds
  wait 1:00 ; one hour
  wait [port1 port2 10]

```

The WAIT function will return a block of ports that have satisfied their WAIT conditions, or NONE in the case of timeout.

**The WAIT function is critical to event processing.** By calling WAIT, you are allowing all types of events to be processed. During that processing additional REBOL code is called via the AWAKE functions that are specified for each port.

Note that when you call WAIT, you allow all events to process, not just the ones that you specify in the WAIT argument. For example, if you are running a GUI, but you wait for a network TCP port, the GUI events will be processed during the WAIT.

In essence, calling WAIT launches your program into event-driven mode of operation. Many types of GUI applications use this mode of operations.

## Quick review of port structure

In the steps outlined below, keep in mind that a [port](Ports__Synchronous_and_Asynchronous_Operations.md) has the structure:

```rebol
   port: context [
       spec:       ; published specification of the port
       scheme:     ; scheme object used for this port
       actor:      ; port action handler (script driven)
       awake:      ; port awake function (event driven)
       state:      ; internal state values (private)
       data:       ; data buffer (usually binary or block)
           none
   ]

```

It is the ACTOR and AWAKE functions that are primarily relevant in the description below. The ACTOR is called to handle program requests to a port (downward acting). The AWAKE is called to handle events returning from the external device (upward acting).

Both the ACTOR and the AWAKE are functions (native or mezzanine) and internally dispatch code sections that are specific to the actions and events taking place.

## Dispatching TCP OPEN action

The OPEN function in the example above gets processed like this:

 * OPEN action is dispatched to URL (aka string) datatype
 * OPEN, as a special port action redirects from URL to PORT datatype
 * OPEN gets dispatched to TCP port scheme ACTOR function (a native)
 * TCP ACTOR function dispatches OPEN action (next section).

## TCP action for OPEN

The TCP port handles the OPEN request as follows. The single port OPEN action actually involves two calls to the lower level device: one for the OPEN (creation of the socket) and the other for CONNECT (establishing the TCP connection). Here is the sequence:

 * OPEN allocates and initializes a TCP request structure
 * OPEN action sends TCP OPEN command (via request) to DO_DEVICE()
 * DO_DEVICE dispatches TCP device with OPEN command
 * TCP OPEN command creates socket and returns DR_DONE
 * DO_DEVICE notices command complete, and returns to TCP port actor
 * OPEN action now sends TCP CONNECT command to DO_DEVICE()
 * TCP CONNECT command begins asynchronous connect, returns DR_PEND
 * DO_DEVICE notices pending result and links request to TCP device pending queue

## System WAIT sequence

Eventually, the program calls WAIT and this sequence happens:

 * The system port is checked for queued events.
 * If an event is found, system port processes event (see below).
 * OS_WAIT is called to check for device activity.
 * Devices are polled for activity, specifically TCP device is checked.
 * TCP CONNECT has finished.
 * Signal_Event is called to queue the request to the system port event queue.
 * Repeat from 1 above as long as no WAIT-specified port awoke and the timeout has not expired.
 * If a port awoke, return the port.
 * If not, and WAIT timeout is specified return NONE.

## System event processing

When an event is found above (in WAIT), this occurs:

 * This system port examines its events queue (a block in the state field).
 * The system port examines its wake list (a block in the data field).
 * If both are empty, no wake is needed, return.
 * If port found on either above list, call the system port awake function (see REBOL code below).
 * If an event has happened, call the AWAKE function for its related port. Note that this will happen even for ports not specified in the WAIT code above. Port AWAKE functions evaluate independently and in the order they were queued.
 * Append any new AWAKE ports to the WAKE list.
 * Update the WAIT port list (just one port in this case, TCP). If the event did not happen, it will be removed from the list (the result returned from WAIT).
 * If the WAIT port list does contain waked port, remove it from the system wake list (it is done).
 * Return TRUE if WAIT condition is satisfied.

### Other Events

Other external events, such as those generated by graphics extensions are handled by creating an event structure and passing it to the Reb_Event() API function. The event is appended to the event queue block stored in the system port state field.

![](https://web.archive.org/web/20130614012456/http://www.rebol.net/wiki/Image:System-event-port.gif)

Various wrapper functions for these types of events can be found in host-event.c, the Add_Event_XY() function for example.

### System port AWAKE function

The SYSTEM port scheme defines the AWAKE function as:

```rebol

awake: func [
    sport "System port (State block holds events)"
    ports "Port list (Copy of block passed to WAIT)"
    /local event port waked
][
    waked: sport/data ; The wake list (pending awakes)

    ; Process all events (even if no awake ports).
    ; Do only 8 events at a time (to prevent polling lockout).
    loop 8 [
        if not event: take sport/state [break]
        port: event/port
        if wake-up port event [
            ; Add port to wake list:
            if not find waked port [append waked port]
        ]
    ]

    ; No wake ports (just a timer), return now.
    if not block? ports [return none]

    ; Remove ports not awakened from return block,
    ; and remove ports awakened from the wake list:
    remove-each p ports [not take find waked p]

    not tail? ports ; anything left to wake-up from WAIT?
]

```

**It is important to note that the WAIT function processes all port events even if it is called with a timeout value.** This is how the port awake handlers are able to run on top of the stack.

### The wake-up function

The wake-up function combines the port update action and the port's awake in the following manner:

 * For ports with native actors, it calls the port's update action to make the port object fully consistent with internal native structures (e.g. the actual length of data read).
 * Calls the port's awake function with the event as an argument.
 * Checks if the return from the awake is 'bypass. If not, a TRUE value is returned (otherwise FALSE returned).

### Preventing a lockout condition

It is possible that events enter the system port queue so quickly that the function does not exit for "long" periods. (This happens when port awake functions themselves may be directly causing events, such as when TCP is doing a recv() operation.) When that happens, devices do not get polled, and a high priority event such as user ESCAPE will not be immediately seen.

To avoid help relieve that situation, the system awake function limits the number of events it will process (set to eight, but we will need to determine what is best.)

### Recursive usage

Recursive usage of WAIT is generally not advised. However, in practice we have found that it is difficult to avoid. For example, some GUI events may need to wait for short periods of time before continuing. Writing those in an asynchronous fashion solves the problem, but such techniques can be very difficult (due to the need to store and resume the full state).

Therefore, it is our intent to be sure that the above mechanism works for recursive WAIT situations.

## Awake Handlers

add notes here about how awake handlers work

You may be wondering how events such as mouse movement get processed for a graphical user interface. It is the line above:

```rebol
       if wake-up port event [

```

that makes those events happen.

The GUI event port's AWAKE function is called for each event.

## Timestamping

REBOL 3 removes timestamps from the EVENT datatype (note, timestamps, not timers). In most cases, timestamps are not used, so they just serve to slow the event system down.

If we discover that timestamps are necessary for specific types of events, we can add a special timestamp event. It would immediately follow its related event in the queue.

## Performance

The above design is intended to offer great flexibility in the operation of ports and events. Although there are quite a few steps described above, each is just a few lines of code, so the above operations happen quickly. The overhead of the R3 event system is insignificant compared to the overhead of the native OS I/O system.

If you write an event AWAKE function for your own port scheme, it is advised that you keep it simple and fast. If you do significant processing in your AWAKE, you may delay the results for other important sub-systems, such as the GUI.

For cases where an operation could potentially block (or slow down) the entire system, it is better to use a REBOL task and coordinate with it via events. That is the subject for a separate section.

Retrieved from "[http://www.rebol.net/wiki/Event_System](https://web.archive.org/web/20130614012456/http://www.rebol.net/wiki/Event_System)"