# Port Implementation

## Port Definition

**A port is an active instance of a [scheme](Schemes.md).**

For example, when you open a file, the system creates a port from the file scheme. The port keeps track of the I/O for that specific file, as well as specific information about the file, such as date, size, and current index position.

### Port object structure

The standard port object has been substantially reduced in size and complexity from REBOL 2. The new object is defined by system/standard as:

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

Where:

 spec  is the object used to define the port. It holds the scheme name and other fields, such as the host for network ports. The format of the spec object depends on the scheme of the port.
 scheme  points back to the scheme object for this port. It is normally used by lower layers that may need to check scheme related values.
 actor  a function that is called for handling port actions. Normally, it is the same as that of the scheme, but it can be unique to a specific port. This function can be a native or mezzanine.
 awake  a callback function called when new events arrive for the port. This is normally the same as that of the scheme, but it can be unique to a specific port.
 state  private state data. This is an object or a C-structure in the case of native port schemes. Scripts should not access the state! The format and definition of the state is allowed to change between versions.
 data  a buffer that is used for the most common types of ports (those that transfer data).

This object is converted to a PORT datatype during the port's initialization sequence.

Specific types of ports (schemes) are allowed to extend this structure for their own purposes, but developers should avoid unnecessary fields (bloat).

Make note about return value. A TRUE will satisfy a WAIT, but other values do not.

## Architecture

The diagram below shows the basic relationship between **[schemes](Schemes.md), ports, requests,** and **devices**:

[](https://web.archive.org/web/20130614001941/http://www.rebol.net/wiki/Image:Port-flow.png)

Definitions:

 Scheme  a [scheme](Schemes.md) specifies a type of port, such as file, TCP, event, sound, etc. The scheme creates a port, which is:
 Port  a specific instance of a scheme. For example, when you read a file, the scheme is used to create a port that accesses the file and its data. Each open file has its own port.
 Request  is a lower level (C) structure used to communicate between a port and a device. In the case of a file, it passes fields like the file name, date, size, and data to be transferred.
 Device  is native C code that implements a standard set of access methods for a port. Example methods include open, close, read, write, create, and delete. This level is where file or network I/O actually happens.

Only ports that require native support need requests and devices. Higher level ports, such as HTTP, do not directly create requests. Typically, they use lower level ports, such a TCP, and those do use requests.

**The primary benefit of this design is that it formalizes a method for accessing a wide range of devices without expanding the REBOL-to-environment API for each device.** (In a way, it implements a mini operating system to keep REBOL as system-independent as possible.)

Also note that requests and devices do not appear within REBOL code itself. **They are purposely kept isolated in order to eliminate potential interdependencies in future versions of REBOL (or improvements to the device code itself.)**

Retrieved from "[http://www.rebol.net/wiki/Port_Implementation](https://web.archive.org/web/20130614001941/http://www.rebol.net/wiki/Port_Implementation)"