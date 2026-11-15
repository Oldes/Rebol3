/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2013 REBOL Technologies
**  Copyright 2013-2026 Rebol Open Source Developers
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Title: Device: Serial port access for Posix
**  Author: Carl Sassenrath, Joshua Shireman, Shixin Zeng
**
************************************************************************
**
**  NOTE to PROGRAMMERS:
**
**    1. Keep code clear and simple.
**    2. Document unusual code, reasoning, or gotchas.
**    3. Use same style for code, vars, indent(4), comments, etc.
**    4. Keep in mind Linux, OS X, BSD, big/little endian CPUs.
**    5. Test everything, then test it again.
**
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <termios.h>

#include "reb-host.h"
#include "host-lib.h"

#define MAX_SERIAL_PATH 128

// Hardware (RTS/CTS) flow control flag has different names:
#if defined(CRTSCTS)
#define SERIAL_RTSCTS CRTSCTS
#elif defined(CNEW_RTSCTS)
#define SERIAL_RTSCTS CNEW_RTSCTS
#endif


/***********************************************************************
**
**	Local Functions
**
***********************************************************************/

// Baud rates and their termios speed constants.
// On Linux these constants are not equal to the numeric rates
// (B9600 is 13), so the rate must not be used as a speed directly!
static const struct {REBINT baud; speed_t speed;} Baud_Speeds[] = {
	{50, B50}, {75, B75}, {110, B110}, {134, B134}, {150, B150},
	{200, B200}, {300, B300}, {600, B600}, {1200, B1200}, {1800, B1800},
	{2400, B2400}, {4800, B4800}, {9600, B9600}, {19200, B19200},
	{38400, B38400},
#ifdef B57600
	{57600, B57600},
#endif
#ifdef B115200
	{115200, B115200},
#endif
#ifdef B230400
	{230400, B230400},
#endif
#ifdef B460800
	{460800, B460800},
#endif
#ifdef B500000
	{500000, B500000},
#endif
#ifdef B576000
	{576000, B576000},
#endif
#ifdef B921600
	{921600, B921600},
#endif
#ifdef B1000000
	{1000000, B1000000},
#endif
#ifdef B1152000
	{1152000, B1152000},
#endif
#ifdef B1500000
	{1500000, B1500000},
#endif
#ifdef B2000000
	{2000000, B2000000},
#endif
#ifdef B2500000
	{2500000, B2500000},
#endif
#ifdef B3000000
	{3000000, B3000000},
#endif
#ifdef B3500000
	{3500000, B3500000},
#endif
#ifdef B4000000
	{4000000, B4000000},
#endif
};

static REBOOL Get_Baud_Speed(REBINT baud, speed_t *speed)
{
	// Converts the numeric baud rate to the termios speed constant.
	// Returns FALSE when the rate is not supported.
	size_t n;
	if (baud <= 0) return FALSE;
	for (n = 0; n < sizeof(Baud_Speeds) / sizeof(Baud_Speeds[0]); n++) {
		if (Baud_Speeds[n].baud == baud) {
			*speed = Baud_Speeds[n].speed;
			return TRUE;
		}
	}
#if B9600 == 9600
	// Speeds are plain numbers on this system (BSD, macOS),
	// so let the driver decide if a non-standard rate is usable.
	*speed = (speed_t)baud;
	return TRUE;
#else
	return FALSE;
#endif
}


static struct termios *Get_Serial_Settings(int ttyfd)
{
	struct termios *attr = NULL;
	attr= (struct termios *) MAKE_NEW(struct termios);
	if (attr != NULL) {
        	if (tcgetattr(ttyfd,attr) == -1) {
          		FREE_MEM(attr);
          		attr = NULL;
        	}
      	}
	return attr;
}


static REBOOL Set_Serial_Settings(int ttyfd, REBREQ *req)
{
	REBINT n;
	struct termios attr = {};
	speed_t speed;
#ifdef DEBUG_SERIAL
	printf("setting attributes: speed %d\n", req->serial.baud);
#endif

	if (!Get_Baud_Speed(req->serial.baud, &speed)) return FALSE;

	if (cfsetospeed(&attr, speed)!=0 || cfsetispeed(&attr, speed)!=0)
		return FALSE;

	// TTY has many attributes. Refer to "man tcgetattr" for descriptions.
	// C-flags - control modes:
	attr.c_cflag |= CREAD | CLOCAL;

	attr.c_cflag &= ~CSIZE; /* clear data size bits */

	switch (req->serial.data_bits) {
		case 5:
			attr.c_cflag |= CS5;
			break;
		case 6:
			attr.c_cflag |= CS6;
			break;
		case 7:
			attr.c_cflag |= CS7;
			break;
		case 8:
		default:
			attr.c_cflag |= CS8;
	}

	switch (req->serial.parity) {
		case SERIAL_PARITY_ODD:
			attr.c_cflag |= PARENB;
			attr.c_cflag |= PARODD;
			break;
		case SERIAL_PARITY_EVEN:
			attr.c_cflag |= PARENB;
			attr.c_cflag &= ~PARODD;
			break;
		case SERIAL_PARITY_NONE:
		default:
			attr.c_cflag &= ~PARENB;
			break;
	}

	switch (req->serial.stop_bits) {
		case 2:
			attr.c_cflag |= CSTOPB;
			break;
		case 1:
		default:
			attr.c_cflag &= ~CSTOPB;
			break;
	}

	switch (req->serial.flow_control) {
		case SERIAL_FLOW_CONTROL_HARDWARE:
#ifdef SERIAL_RTSCTS
			attr.c_cflag |= SERIAL_RTSCTS;
			break;
#else
			return FALSE; // not supported on this system
#endif
		case SERIAL_FLOW_CONTROL_SOFTWARE:
			// XON/XOFF is an input mode (attr is zeroed, so RTS/CTS is off)
			attr.c_iflag |= IXON | IXOFF;
			break;
		case SERIAL_FLOW_CONTROL_NONE:
		default:
			break;
	}

	// L-flags - local modes:
	attr.c_lflag = 0; // raw, not ICANON

	// I-flags - input modes:
	attr.c_iflag |= IGNPAR;
	
	// O-flags - output modes:
	attr.c_oflag = 0;

	// Control characters:
	// R3 devices are non-blocking (polled for changes):
	attr.c_cc[VMIN]  = 0;
	attr.c_cc[VTIME] = 0;

	// Make sure OS queues are empty:
	tcflush(ttyfd, TCIFLUSH);

	// Set new attributes:
	if (tcsetattr(ttyfd, TCSANOW, &attr)) return FALSE;

	return TRUE;
}

/***********************************************************************
**
*/	DEVICE_CMD Open_Serial(REBREQ *req)
/*
**		serial.path = the /dev name for the serial port
**		serial.baud = speed (baudrate)
**
***********************************************************************/
{
	REBCHR *path;
	REBCHR devpath[MAX_SERIAL_PATH];
	REBINT h;

	if (!(path = req->serial.path)) {
		req->error = -RFE_BAD_PATH;
		return DR_ERROR;
	}

	if (path[0] != '/') { //relative path
		strcpy(&devpath[0], "/dev/");
		strncpy(&devpath[5], path, MAX_SERIAL_PATH-6);
		path = &devpath[0];
	}
	h = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (h < 0) {
		req->error = -RFE_OPEN_FAIL;
		return DR_ERROR;
	}

	//Getting prior atttributes:
	req->serial.prior_attr = Get_Serial_Settings(h);
	if (tcgetattr(h, req->serial.prior_attr)) {
		close(h);
		return DR_ERROR;
	}

	if (!Set_Serial_Settings(h, req)) {
		close(h);
		req->error = -RFE_OPEN_FAIL;
		return DR_ERROR;
	}		

	req->id = h;
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_Serial(REBREQ *req)
/*
***********************************************************************/
{
	if (req->id) {
		//Warning: should free req->serial.prior_attr termios struct?
		tcsetattr(req->id, TCSANOW, req->serial.prior_attr);
		close(req->id);
		req->id = 0;
	}
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Read_Serial(REBREQ *req)
/*
***********************************************************************/
{
	ssize_t result = 0;
	if (!req->id) {
		req->error = -RFE_NO_HANDLE;
		return DR_ERROR;
	}

	result = read(req->id, req->data, req->length);
#ifdef DEBUG_SERIAL
	printf("read %d ret: %d\n", req->length, result);
#endif
	if (result < 0) {
		req->error = -RFE_BAD_READ;
		OS_Signal_Device(req, EVT_ERROR);
		return DR_ERROR;
	} else if (result == 0) {
		return DR_PEND;
	} else {
		req->actual = result;
		OS_Signal_Device(req, EVT_READ);
	}

	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Write_Serial(REBREQ *req)
/*
***********************************************************************/
{
	REBINT result = 0, len = 0;
	len = req->length - req->actual;
	if (!req->id) {
		req->error = -RFE_NO_HANDLE;
		return DR_ERROR;
	}

	if (len <= 0) return DR_DONE;

	result = write(req->id, req->data, len);
#ifdef DEBUG_SERIAL
	printf("write %d ret: %d\n", len, result);
#endif
	if (result < 0) {
		if (errno == EAGAIN) {
			return DR_PEND;
		}
		req->error = -RFE_BAD_WRITE;
		OS_Signal_Device(req, EVT_ERROR);
		return DR_ERROR;
	}
	req->actual += result;
	req->data += result;
	if (req->actual >= req->length) {
		OS_Signal_Device(req, EVT_WROTE);
		return DR_DONE;
	} else {
		SET_FLAG(req->flags, RRF_ACTIVE); /* notify OS_WAIT of activity */
		return DR_PEND;
	}
}


/***********************************************************************
**
*/	DEVICE_CMD Query_Serial(REBREQ *req)
/*
***********************************************************************/
{
#ifdef QUERY_IMPLEMENTED
	struct pollfd pfd;

	if (req->id) {
		pfd.fd = req->id;
		pfd.events = POLLIN;
		n = poll(&pfd, 1, 0);
	}
#endif
	return DR_DONE;
}

/***********************************************************************
**
*/	DEVICE_CMD Modify_Serial(REBREQ *req)
/*
***********************************************************************/
{
	REBOOL value = req->modify.value;
	int status;
	int mode = req->modify.mode;

	if (!req->id) {
		req->error = -RFE_NO_HANDLE;
		return DR_ERROR;
	}

	if (mode == 1) {
		ioctl(req->id, value ? TIOCSBRK : TIOCCBRK);
		return DR_DONE;
	}
	if (ioctl(req->id, TIOCMGET, &status) == -1) return DR_ERROR;
	if (mode == 2) {
		// (request-to-send) signal
		if (value) {
			status |= TIOCM_RTS;
		} else {
			status &= ~TIOCM_RTS;
		}
	} else {
		// (data-terminal-ready) signal
		if (value) {
			status |= TIOCM_DTR;
		} else {
			status &= ~TIOCM_DTR;
		}
	}
	if (ioctl(req->id, TIOCMSET, &status) == -1) return DR_ERROR;
	return DR_DONE;
}


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] = {
	0,
	0,
	Open_Serial,
	Close_Serial,
	Read_Serial,
	Write_Serial,
	0,  // poll
	0,	// connect
	Query_Serial,
	Modify_Serial,
	0,	// create
	0,	// delete
	0	// rename
};

DEFINE_DEV(Dev_Serial, "Serial IO", 1, Dev_Cmds, RDC_MAX, sizeof(REBREQ));