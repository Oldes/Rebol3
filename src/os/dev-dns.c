/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  Copyright 2012-2026 Rebol Open Source Contributors
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
**  Title: Device: DNS access
**  Author: Carl Sassenrath
**  Purpose: Calls local DNS services for domain name lookup.
**  Notes:
**      See MS WSAAsyncGetHost* details regarding multiple requests.
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

#include "reb-host.h"
#include "host-lib.h"

#ifdef INCLUDE_NET_INTERFACES
#define USE_WINSOCK2 // for GetAdaptersAddresses (see sys-net.h)
#endif
#include "sys-net.h"

#ifdef INCLUDE_NET_INTERFACES
#ifndef TO_WINDOWS
#include <ifaddrs.h>
#include <net/if.h>
#endif
#define MAX_NET_INTERFACES 64
#endif

extern DEVICE_CMD Init_Net(REBREQ *); // Share same init
extern DEVICE_CMD Quit_Net(REBREQ *);

#ifdef HAS_ASYNC_DNS
// Async DNS requires a window handle to signal completion (WSAASync)
extern HWND Event_Handle;
#endif

/***********************************************************************
**
*/	DEVICE_CMD Open_DNS(REBREQ *sock)
/*
***********************************************************************/
{
	SET_OPEN(sock);
	return DR_DONE;
}


/***********************************************************************
**
*/	DEVICE_CMD Close_DNS(REBREQ *sock)
/*
**		Note: valid even if not open.
**
***********************************************************************/
{
	// Terminate a pending request:
#ifdef HAS_ASYNC_DNS
	if (GET_FLAG(sock->flags, RRF_PENDING)) {
		CLR_FLAG(sock->flags, RRF_PENDING);
		if (sock->handle) WSACancelAsyncRequest(sock->handle);
	}
#endif
	if (sock->net.host_info) OS_Free(sock->net.host_info);
	sock->net.host_info = 0;
	sock->handle = 0;
	SET_CLOSED(sock);
	return DR_DONE; // Removes it from device's pending list (if needed)
}


/***********************************************************************
**
*/	DEVICE_CMD Read_DNS(REBREQ *sock)
/*
**		Initiate the GetHost request and return immediately.
**		Note the temporary results buffer (must be freed later).
**
***********************************************************************/
{
	void *host;
#ifdef HAS_ASYNC_DNS
	HANDLE handle;
#else
	HOSTENT *he;
#endif

	host = OS_Make(MAXGETHOSTSTRUCT); // be sure to free it
	sock->net.host_info = host; // stores allocated buffer, deallocated on close or on error

#ifdef HAS_ASYNC_DNS
	// WINDOWS version
	if (!GET_FLAG(sock->modes, RST_REVERSE)) {// hostname lookup
		if (sock->data == NULL) {
			DWORD dwSize = MAXGETHOSTSTRUCT;
			if (GetComputerNameExA(ComputerNameDnsFullyQualified, host, &dwSize)) {
				((REBYTE*)host)[dwSize] = 0;
				sock->data = host;
				SET_FLAG(sock->modes, RST_REVERSE);
				SET_FLAG(sock->flags, RRF_DONE);
				return DR_DONE;
			}
			goto error;
		}
		handle = WSAAsyncGetHostByName(Event_Handle, WM_DNS, cs_cast(sock->data), host, MAXGETHOSTSTRUCT);
	} else
		handle = WSAAsyncGetHostByAddr(Event_Handle, WM_DNS, (char*)&(sock->net.remote_ip), 4, AF_INET, host, MAXGETHOSTSTRUCT);

	if (handle != 0) {
		sock->net.host_info = host;
		sock->handle = handle;
		return DR_PEND; // keep it on pending list
	}
#else
	// POSIX version
	// Use old-style blocking DNS (mainly for testing purposes):
	if (GET_FLAG(sock->modes, RST_REVERSE)) {
		he = gethostbyaddr((char*)&sock->net.remote_ip, 4, AF_INET);
		if (he) {
			sock->cdata = cb_cast(he->h_name);
			SET_FLAG(sock->flags, RRF_DONE);
			return DR_DONE;
		}
	}
	else if (sock->data == NULL) {
		if(0 == gethostname(host, MAXGETHOSTSTRUCT)) {
			sock->data = host;
			SET_FLAG(sock->modes, RST_REVERSE);
			SET_FLAG(sock->flags, RRF_DONE);
			return DR_DONE;
		}
	}
	else {
		he = gethostbyname(cs_cast(sock->data));
		if (he) {
			COPY_MEM((char*)&(sock->net.remote_ip), (char *)(*he->h_addr_list), 4); //he->h_length);
			SET_FLAG(sock->flags, RRF_DONE);
			return DR_DONE;
		}
	}
#endif
error:
	OS_Free(host);
	sock->net.host_info = 0;

	sock->error = GET_ERROR;
	//OS_Signal_Device(sock, EVT_ERROR);
	return DR_ERROR; // Remove it from pending list
}


/***********************************************************************
**
*/	DEVICE_CMD Poll_DNS(REBREQ *dr)
/*
**		Check for completed DNS requests. These are marked with
**		RRF_DONE by the windows message event handler (dev-event.c).
**		Completed requests are removed from the pending queue and
**		event is signalled (for awake dispatch).
**
***********************************************************************/
{
	REBDEV *dev = (REBDEV*)dr;  // to keep compiler happy
	REBREQ **prior = &dev->pending;
	REBREQ *req;
	REBOOL change = FALSE;
	HOSTENT *host;

	// Scan the pending request list:
	for (req = *prior; req; req = *prior) {

		// If done or error, remove command from list:
		if (GET_FLAG(req->flags, RRF_DONE)) { // req->error may be set
			*prior = req->next;
			req->next = 0;
			CLR_FLAG(req->flags, RRF_PENDING);

			if (!req->error) { // success!
				host = (HOSTENT*)req->net.host_info;
				if (GET_FLAG(req->modes, RST_REVERSE))
					req->data = (REBYTE*)host->h_name;
				else
					COPY_MEM((char*)&(req->net.remote_ip), (char *)(*host->h_addr_list), 4); //he->h_length);
				OS_Signal_Device(req, EVT_READ);
			}
			else
				OS_Signal_Device(req, EVT_ERROR);
			change = TRUE;
		}
		else prior = &req->next;
	}

	return change;
}


#ifdef INCLUDE_NET_INTERFACES
/***********************************************************************
**
*/	DEVICE_CMD Query_DNS(REBREQ *req)
/*
**		Collect the IPv4 interfaces of the local host (synchronous).
**
**		On success, req->data holds an allocated array of REBNIF
**		records and req->length their count.
**		The caller must free req->data with OS_Free.
**
***********************************************************************/
{
	REBNIF *nifs, *nif;
	REBCNT count = 0;
#ifdef TO_WINDOWS
	IP_ADAPTER_ADDRESSES *list = NULL, *a;
	IP_ADAPTER_UNICAST_ADDRESS *u;
	ULONG size = 16 * 1024;
	ULONG result = ERROR_BUFFER_OVERFLOW;
	int n;
#else
	struct ifaddrs *list, *ifa;
#endif

	req->data = NULL;
	req->length = 0;

	nifs = OS_Make(MAX_NET_INTERFACES * sizeof(REBNIF));
	if (!nifs) return DR_ERROR;
	memset(nifs, 0, MAX_NET_INTERFACES * sizeof(REBNIF));

#ifdef TO_WINDOWS
	// The required size may change between calls, so try it a few times:
	for (n = 0; n < 3 && result == ERROR_BUFFER_OVERFLOW; n++) {
		if (list) OS_Free(list);
		list = OS_Make(size);
		if (!list) goto error;
		result = GetAdaptersAddresses(AF_INET,
			GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
			NULL, list, &size);
	}
	if (result != NO_ERROR) {
		if (list) OS_Free(list);
		req->error = result;
		OS_Free(nifs);
		return DR_ERROR;
	}
	for (a = list; a; a = a->Next) {
		for (u = a->FirstUnicastAddress; u && count < MAX_NET_INTERFACES; u = u->Next) {
			if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
			nif = &nifs[count++];
			nif->ip = ((struct sockaddr_in *)u->Address.lpSockaddr)->sin_addr.s_addr;
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)
			// OnLinkPrefixLength is available since Vista
			if (u->OnLinkPrefixLength > 0 && u->OnLinkPrefixLength <= 32)
				nif->netmask = htonl(0xFFFFFFFFUL << (32 - u->OnLinkPrefixLength));
#endif
			if (a->OperStatus == IfOperStatusUp) SET_FLAG(nif->flags, RNIF_UP);
			if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) SET_FLAG(nif->flags, RNIF_LOOPBACK);
			// Friendly name (like "Ethernet") as UTF-8; empty if it does not fit.
			if (!WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1,
					nif->name, sizeof(nif->name), NULL, NULL))
				nif->name[0] = 0;
		}
	}
	OS_Free(list);
#else
	if (getifaddrs(&list)) goto error;
	for (ifa = list; ifa && count < MAX_NET_INTERFACES; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
		nif = &nifs[count++];
		nif->ip = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if (ifa->ifa_netmask)
			nif->netmask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
		if (ifa->ifa_flags & IFF_UP) SET_FLAG(nif->flags, RNIF_UP);
		if (ifa->ifa_flags & IFF_LOOPBACK) SET_FLAG(nif->flags, RNIF_LOOPBACK);
		strncpy(nif->name, ifa->ifa_name, sizeof(nif->name) - 1);
	}
	freeifaddrs(list);
#endif

	req->data = (REBYTE*)nifs;
	req->length = count;
	return DR_DONE;

error:
	req->error = GET_ERROR;
	OS_Free(nifs);
	return DR_ERROR;
}
#else
#define Query_DNS 0
#endif // INCLUDE_NET_INTERFACES


/***********************************************************************
**
**	Command Dispatch Table (RDC_ enum order)
**
***********************************************************************/

static DEVICE_CMD_FUNC Dev_Cmds[RDC_MAX] =
{
	Init_Net,	// Shared with the network device (called once per device)
	Quit_Net,	// Shared with the network device (called once per device)
	Open_DNS,
	Close_DNS,
	Read_DNS,
	0,	// write
	Poll_DNS,
	0,	// connect
	Query_DNS,
};

DEFINE_DEV(Dev_DNS, "DNS", 1, Dev_Cmds, RDC_MAX, 0);