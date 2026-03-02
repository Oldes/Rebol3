# TCP Port Details

This document provides lower level details about the R3 TCP port implementation.

## Basic Concept

TCP ports are a very low level mechanism. It has been implemented in R3 to be extremely efficient and as close to the raw TCP socket mechanism as possible.

NOTE: Most applications do not need to use TCP ports directly. Instead, they use higher level TCP protocols such as HTTP. This point is crucial. Please remember it.

TCP ports operate as asynchronous co-routines using the lower-level [net device](https://web.archive.org/web/20131013022447/http://www.rebol.net/wiki/Devices). In general, a TCP port gets things done via a small function that your higher level protocol uses to:

 * **Issue an action (a command) such as READ or WRITE**
 * **Process events that are the result of actions**

This function is called the **awake** handler for the TCP port.

## How TCP Ports Work

TCP ports are divided into two main parts:

 * The TCP port (within the [RRTE](https://web.archive.org/web/20131013022447/http://www.rebol.net/wiki/RRTE) layer)
 * The TCP device (part of the [RHAE](https://web.archive.org/web/20131013022447/http://www.rebol.net/wiki/RHAE) layer)

These two layers communicate via the **IO-Request** structure that is used for all REBOL I/O.

The IO-Request is handled in two ways:

 * It is passed from the TCP port action (RRTE) to the TCP device (RHAE).
 * It is signaled back to the TCP port action by being attached to an event that gets handled (by the general RRTE event handling mechanism).

### Write Action

A WRITE first passes through the RRTE TCP port actor that then calls the RHAE TCP device.

The port actor preps the request and checks for any higher level issues.

The port device processes the request and determines the continuation state (DONE, PEND, or ERROR).

The details of the port actor for WRITE are:

 * Set port/data to WRITE content (binary string) value. (Mainly to keep it GC safe.)
 * Obtain binary string as specified. The buffer is not copied. This is a low level mechanism.
 * Determine start position from index.
 * Determine length from tail-index or from /part if specified.
 * Set IO-request length and data. Zero the **actual** field (the length actually transferred).
 * Call the TCP device with the IO-request
 * Check for errors
 * Check for immediate completion. If done, set port/data to NONE.

The details of the port device for WRITE are:

 * Check the IO-request for valid connection flag. This is not a socket check, it is a request state flag check. A lost connection at the socket level is detected by recv() or by a send() error.
 * Set the IO-request mode to WRITE.
 * Clip the write length to our TCP packet maximum size.
 * Call send() to begin TCP packet transfer.
 * Check result, for transfer or error.
 * For transfer (note that zero is valid), increment IO-request data buffer position and actual counter.
 * Check if **actual** matches original length. If it does, signal WROTE event, return DONE result.
 * If **actual** does not match length, return PEND result.
 * If error from send(), if it is WOULDBLOCK, then return PEND flag, else return ERROR result.

If a WROTE signal occurred:

 * The IO-Request is attached to an event.
 * The event is queued to the system event port. This is in the RHAE context, not the RRTE context.
 * The CPU eventually returns to RRTE context (all device handlers satisfied).
 * The system port will process the above event and forward it to the TCP port actor.
 * The port actor will be dispatched with an update event and process it. This is mandatory to update the IO-request state fields from the RRTE side prior to user-specified AWAKE function.
 * The port/data is set to NONE, indicating completion.
 * The port AWAKE function is called with the WROTE event.

### Read Action

A READ first passes through the RRTE TCP port actor that then calls the RHAE TCP device. Note that this action is not symmetric with respect to WRITE. A WRITE can run to full buffer transfer, whereas a READ must be segmented by packet arrival.

The port actor preps the request and checks for any higher level issues.

The port device processes the request and determines the continuation state (DONE, PEND, or ERROR).

The details of the port actor for READ are: 

 * Check IO-Request connection flag. This is not a socket check, it is a request state flag check.
 * Check the port/data for an existing buffer.
 * If no buffer found, allocate one that is of the default size (32,000). Note: not 32K.
 * Compute buffer space available.
 * If available space is less than half the default size, extend the buffer.
 * Recompute buffer space available.
 * Setup the IO-request data and length fields. Data is the buffer tail position. Length is the buffer space available from above. Clear the **actual** field.
 * Call the TCP device with READ command.
 * Check result for error. If error, throw it.

The details of the port device for READ are:

 * Check the IO-request for valid connection flag. This is not a socket check, it is a request state flag check. A lost connection at the socket level is detected by the recv() call itself.
 * Set the IO-request mode to READ.
 * Call recv() to begin TCP read transfer.
 * Check result for transfer or error.
 * For transfer (note that zero is not valid), signal READ event, return DONE result. Note that TCP is allowed to slice up the transfer however it wants. This may be a partial read.
 * If result is zero, then TCP closed the socket gracefully. Signal a CLOSE event, clear IO-Request connection state, and return DONE result.
 * If error from send(), if it is WOULDBLOCK, then return PEND flag, else return ERROR result.

The READ signal is processed similar to the WROTE signal, as described above. The only difference is that the port actor's UPDATE event will set the tail position for the port/data buffer. This provides the proper binary series length indication.

Retrieved from "[http://www.rebol.net/wiki/TCP_Port_Details](https://web.archive.org/web/20131013022447/http://www.rebol.net/wiki/TCP_Port_Details)"