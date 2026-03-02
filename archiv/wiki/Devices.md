# Devices

## Main Goals

The main goals of the R3 device sub-system are:

 * **Provide a standardized interface mechanism to OS devices and features.**
 * **Deliver extremely high performance with minimal overhead.**
 * **Utilize an asynchronous mechanism by default.**
 * **Make new devices very easy to implement (and maintain.)**
 * **Assure that it is implementable over all operating systems.**
 * **Distributed as part of the open source host interface.**

It should be noted that the device sub-system is compile-time based not run-time based. It is designed for performance, not for run-time extensibility (which often generates extra overhead.)

## Project Status

 * This sub-system has been implemented and used now within R3 for a couple years.
 * The primary devices have been written. See the list below.
 * A few devices still need to be written. For example, we need a sound device.
 * Some devices may benefit from additional features. For example, the TCP/UDP device could support connections to different network interfaces.

## Basic Concepts

These are the basic ideas behind the implementation:

 * Devices interface to code via [ports](Ports__Synchronous_and_Asynchronous_Operations.md).
 * The device interface is generally asynchronous; however, synchronous devices work within the same mechanism.
 * Devices are statically implemented within the system (at this time you cannot dynamically add devices.)
 * Devices are implemented for optimal performance, minimizing port-to-device data and command mapping requirements.
 * Devices respond to a standard set of commands (listed below)
 * The system defines a device structure (REBDEV) that includes device title, version, date, command map, etc.
 * The system defines a request structure (REBREQ) that includes target device id, port back-reference, command, error, modes, flags, state, timeout, data, and other fields.

## Implemented Devices

These devices have been defined:

RDI_SYSTEM system device
RDI_STDIO standard I/O stream
RDI_CONSOLE console I/O (not implemented)
RDI_FILE file I/O and directory lookup
RDI_EVENT general event handling
RDI_NET TCP/UDP network interface
RDI_DNS domain name lookup interface
RDI_CLIPBOARD OS clipboard I/O

## Device Commands

Devices implement a standard set of commands:

RDC_INIT init device driver resources
RDC_QUIT cleanup device driver resources
RDC_OPEN open device unit (port)
RDC_CLOSE close device unit
RDC_READ read from unit
RDC_WRITE write to unit
RDC_POLL check for activity
RDC_CONNECT connect (in or out)
RDC_QUERY query unit info
RDC_MODIFY set modes (also get modes)
RDC_CREATE create unit target
RDC_DELETE delete unit target
RDC_RENAME rename unit target
RDC_LOOKUP; map name in a domain

## Device Function API

The API divides its functions into those that are request-based and those that are device-general.

The request-based functions are:

OS_Do_Device(REBREQ \*req, REBCNT command) tell a device to perform a command (non-blocking by default)
OS_Abort_Device(REBREQ \*req) abort an earlier device request

The device-general functions are:

OS_Call_Device(REBINT device, REBCNT command) general device commands (for init, quit, make, free, etc.)
OS_Make_Devreq(int device) allocate and initialize a request structure
OS_Poll_Devices(void) poll devices for activity
OS_Wait(REBCNT millisec, REBCNT res) wait for a period of time and/or device activity with a given temporal resolution

## The Asynchronous Model

**Devices are asynchronous by default.** That is, when a request is made, the device is allowed to return immediately while the request is being processed. Of course, devices are allowed to operate synchronously as well. That is simply a subset of the asynchronous model.

In order to implement an asynchronous model over all operating systems, it is necessary to allow the device itself to manage and assess it's own progress and completion. This isolates the OS mechanism from the higher level device architecture.

The only asynchronous technique that can span all operating systems is polling' based. This allows OS subsystems to use different synchronization methods, e.g. GUI via event handlers, OS operations via signals, and network events via the BSD select() interface.

The device polling mechanism implemented in R3 is function-based (allowing devices to manage their state however they want), but the device dispatch overhead is kept absolutely minimal. Note that device polling may be necessary on some devices, even if they have no pending requests (e.g. interrupts.)

When a request is completed its status changes from DR_PEND to DR_DONE or DR_ERROR.

Of course, there is more to say about this model, but those are the basic concepts.

See [TCP Port Details](TCP_Port_Details.md) for a step-by-step description of a specific I/O operation.

Retrieved from "[http://www.rebol.net/wiki/Devices](https://web.archive.org/web/20130614001920/http://www.rebol.net/wiki/Devices)"