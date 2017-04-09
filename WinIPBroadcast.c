// Copyright (C) 2009 Etienne Dechamps

/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// WinIPBroadcast 1.5 by Etienne Dechamps <etienne@edechamps.fr>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <Winsock2.h>
#include <Mswsock.h>
#include <Ws2tcpip.h>
#include <Iphlpapi.h>
#include <windows.h>

#define SERVICE_NAME TEXT("WinIPBroadcast")
#define SERVICE_DESC (TEXT("Sends global IP broadcast packets to all network interfaces instead of just the preferred route. If this service is disabled, applications using global IP broadcast (e.g. server browsers) might not function properly."))

#define IP_HEADER_SIZE 20
#define IP_SRCADDR_POS 12
#define IP_DSTADDR_POS 16
#define IP_TTL_POS 8

#define UDP_HEADER_SIZE 8
#define UDP_CHECKSUM_POS 6

#define FORWARDTABLE_INITIAL_SIZE 4096

HANDLE mainThread;
SERVICE_STATUS_HANDLE serviceStatus;
ULONG loopbackAddress;
ULONG broadcastAddress;
SOCKET listenSocket;
PMIB_IPFORWARDTABLE forwardTable;
ULONG forwardTableSize;

void quit(void);

LPTSTR errorString(int err)
{
	static TCHAR buffer[1024];

	if (!FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		0,
		buffer,
		sizeof(buffer),
		NULL
	))
		wcsncpy_s(buffer, sizeof(buffer), TEXT("(cannot format error message)"), sizeof(buffer));
		
	return buffer;
}


void socketError(LPTSTR func, BOOL fatal)
{
	fwprintf(stderr, TEXT("%serror: %s failed with error code %d: %s"), (fatal ? TEXT("fatal ") : TEXT("")), func, WSAGetLastError(), errorString(WSAGetLastError()));
	if (fatal)
		quit();
}

void systemError(LPTSTR func, BOOL fatal)
{
	fwprintf(stderr, TEXT("%serror: %s failed with error code %d: %s"), (fatal ? TEXT("fatal ") : TEXT("")), func, GetLastError(), errorString(GetLastError()));
	if (fatal)
		quit();
}

void initListenSocket(void)
{
	struct sockaddr_in addr;

	listenSocket = WSASocket(AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);
	if (listenSocket == INVALID_SOCKET)
		socketError(TEXT("WSASocket"), TRUE);
		
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = loopbackAddress;
		
	if (bind(listenSocket, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR)
		socketError(TEXT("bind"), TRUE);
}

void getForwardTable()
{
	int result;

retry:

	result = GetIpForwardTable(forwardTable, &forwardTableSize, FALSE);

	switch (result)
	{
	case NO_ERROR:
		break;

	case ERROR_INSUFFICIENT_BUFFER:
		free(forwardTable);
		forwardTable = malloc(forwardTableSize);
		goto retry;

	default:
		fwprintf(stderr, TEXT("error: GetIpForwardTable failed with error code %d: %s"), result, errorString(result));
		quit();
	}
}

BOOL findLocalAddressInBroadcastRoutes(const ULONG *srcAddress) {
	for (DWORD i = 0; i < forwardTable->dwNumEntries; i++)
	{
		PMIB_IPFORWARDROW row = &forwardTable->table[i];

		if (row->dwForwardDest != broadcastAddress)
			continue;

		if (row->dwForwardMask != ULONG_MAX)
			continue;

		if (row->dwForwardType != MIB_IPROUTE_TYPE_DIRECT)
			continue;

		if (row->dwForwardNextHop == *srcAddress)
			return TRUE;
	}

	return FALSE;
}

DWORD getBroadcastPacket(char *buffer, size_t size, ULONG *srcAddress)
{
	DWORD len;
	WSABUF wsaBuffer;
	DWORD flags;
	
	wsaBuffer.buf = buffer;
	wsaBuffer.len = size;
	
	flags = 0;

	getForwardTable();
	
	while (TRUE)
	{
		if (WSARecv(listenSocket, &wsaBuffer, 1, &len, &flags, NULL, NULL) != 0)
			socketError(TEXT("WSARecvMsg"), TRUE);
		
		if (len < IP_HEADER_SIZE + UDP_HEADER_SIZE)
			continue;

		if (*((ULONG *)&buffer[IP_DSTADDR_POS]) != broadcastAddress)
			continue;

		*srcAddress = *((ULONG *) &buffer[IP_SRCADDR_POS]);

		// We will also receive broadcast packets coming from the outside. Filter those out.
		if (!findLocalAddressInBroadcastRoutes(srcAddress))
			continue;

		break;
	}

	return len;
}

void computeUdpChecksum(char *payload, uint16_t payloadSize, DWORD srcAddress, DWORD dstAddress)
{
	WORD *buf = (WORD *)payload;
	uint16_t length = payloadSize;
	WORD *src = (WORD *)&srcAddress, *dst = (WORD *)&dstAddress;
	DWORD checksum;
	
	*(WORD *)&payload[UDP_CHECKSUM_POS] = 0;

	checksum = 0;
	while (length > 1)
	{
		checksum += *buf++;
		if (checksum & 0x80000000)
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		length -= 2;
	}

	if (length & 1)
		checksum += *((unsigned char *)buf);

	checksum += *(src++);
	checksum += *src;

	checksum += *(dst++);
	checksum += *dst;

	checksum += htons(IPPROTO_UDP);
	checksum += htons(payloadSize);

	while (checksum >> 16)
		checksum = (checksum & 0xFFFF) + (checksum >> 16);

	*(WORD *)&payload[UDP_CHECKSUM_POS] = (WORD)(~checksum);
}

void sendBroadcast(ULONG srcAddress, char *payload, uint16_t payloadSize)
{
	SOCKET socket;
	WSABUF wsaBuffer;
	ULONG block;
	BOOL broadcastOpt;
	DWORD len;
	struct sockaddr_in srcAddr, dstAddr;

	socket = WSASocket(AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);
	if (socket == INVALID_SOCKET)
	{
		socketError(TEXT("WSASocket"), FALSE);
		closesocket(socket);
		return;
	}
	
	block = 0;
	if (WSAIoctl(socket, FIONBIO, &block, sizeof(block), NULL, 0, &len, NULL, NULL) == SOCKET_ERROR)
	{
		socketError(TEXT("WSAIoctl(FIONBIO)"), FALSE);
		closesocket(socket);
		return;
	}
		
	srcAddr.sin_family = AF_INET;
	srcAddr.sin_port = 0;
	srcAddr.sin_addr.s_addr = srcAddress;
		
	if (bind(socket, (SOCKADDR *)&srcAddr, sizeof(srcAddr)) == SOCKET_ERROR)
	{
		socketError(TEXT("bind"), FALSE);
		closesocket(socket);
		return;
	}
	
	broadcastOpt = TRUE;
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcastOpt, sizeof(broadcastOpt)) == SOCKET_ERROR)
	{
		socketError(TEXT("setsockopt(SO_BROADCAST)"), FALSE);
		closesocket(socket);
		return;
	}

	// Any packet we send here will also appear in the input. This can result in an infinite relay loop.
	// To avoid this, we explicitly set the minimum possible TTL. Incoming packets with this TTL will not be relayed.
	//
	// TODO: this is arguably not the best way to solve this problem, because it could also result in packets
	// getting dropped along the route. Theoretically it should make no difference because broadcast packets can't be
	// routed anyway (TTL or not), but one can imagine special setups where that might matter. There is also the
	// problem that if the sending process has the same idea and also sets the TTL to 1, its packets won't be relayed.
	// A better way would be to implement some kind of content-based deduplication, but that's way more complex.
	DWORD ttl = 1;
	if (setsockopt(socket, IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)) == SOCKET_ERROR)
	{
		socketError(TEXT("setsockopt(IP_TTL)"), FALSE);
		closesocket(socket);
		return;
	}
	
	dstAddr.sin_family = AF_INET;
	dstAddr.sin_port = 0;
	dstAddr.sin_addr.s_addr = broadcastAddress;
	
	computeUdpChecksum(payload, payloadSize, srcAddress, broadcastAddress);
	
	wsaBuffer.len = payloadSize;
	wsaBuffer.buf = payload;
	
	if (WSASendTo(socket, &wsaBuffer, 1, &len, 0, (SOCKADDR *)&dstAddr, sizeof(dstAddr), NULL, NULL) != 0)
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			socketError(TEXT("WSASend"), FALSE);
		
	closesocket(socket);
}

void relayBroadcast(char *payload, uint16_t payloadSize, ULONG srcAddress)
{
	DWORD i;
	PMIB_IPFORWARDROW row;
	
	for (i = 0; i < forwardTable->dwNumEntries; i++)
	{
		row = &forwardTable->table[i];
		
		if (row->dwForwardDest != broadcastAddress)
			continue;
			
		if (row->dwForwardMask != ULONG_MAX)
			continue;
			
		if (row->dwForwardType != MIB_IPROUTE_TYPE_DIRECT)
			continue;
			
		if (row->dwForwardNextHop == loopbackAddress || row->dwForwardNextHop == srcAddress)
			continue;
			
		sendBroadcast(row->dwForwardNextHop, payload, payloadSize);
	}
}

void loop()
{
	int result;
	WORD version;
	WSADATA wsaData;
	char buffer[4096];
	DWORD len;
	ULONG srcAddress;
	
	version = MAKEWORD(2, 2);
	result = WSAStartup(version, &wsaData);
	if (result != 0)
		socketError(TEXT("WSAStartup"), TRUE);
		
	loopbackAddress = inet_addr("127.0.0.1");
	broadcastAddress = inet_addr("255.255.255.255");
	forwardTable = malloc(FORWARDTABLE_INITIAL_SIZE);
	forwardTableSize = FORWARDTABLE_INITIAL_SIZE;
		
	initListenSocket();
	
	while (TRUE)
	{
		len = getBroadcastPacket(buffer, sizeof(buffer), &srcAddress);

		// If TTL is one, drop the packet to avoid relay loops.
		if (*((uint8_t*)&buffer[IP_TTL_POS]) <= 1)
			continue;

		relayBroadcast(buffer + IP_HEADER_SIZE, (uint16_t)(len) - IP_HEADER_SIZE, srcAddress);
	}
}

void serviceReport(DWORD state, DWORD exitCode)
{
	SERVICE_STATUS status;
	
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = state;
	status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
	status.dwWin32ExitCode = exitCode;
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;
	
	SetServiceStatus(serviceStatus, &status);
}

VOID WINAPI serviceControlHandler(DWORD controlCode)
{
	switch (controlCode)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			serviceReport(SERVICE_STOPPED, NO_ERROR);
			if (mainThread)
				TerminateThread(mainThread, 0);
			else
				ExitProcess(0);
			break;
			
		case SERVICE_CONTROL_INTERROGATE:
			serviceReport(SERVICE_RUNNING, NO_ERROR);
			break;
	}
}


VOID WINAPI serviceMain(DWORD argc, LPTSTR *argv)
{
	serviceStatus = RegisterServiceCtrlHandler(SERVICE_NAME, serviceControlHandler);
	if (!serviceStatus)
		systemError(TEXT("RegisterServiceCtrlHandler"), FALSE);
		
	serviceReport(SERVICE_RUNNING, NO_ERROR);
	
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
	
	loop();
}

void serviceInstall(void)
{
	SC_HANDLE manager;
	SC_HANDLE service;
	TCHAR path[MAX_PATH];
	SERVICE_DESCRIPTION description;
	
	if (!GetModuleFileName(NULL, path, MAX_PATH))
		systemError(TEXT("GetModuleFilename"), TRUE);
		
	manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!manager)
		systemError(TEXT("OpenSCManager"), TRUE);
		
	service = CreateService(manager, SERVICE_NAME, TEXT("WinIPBroadcast"), SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, TEXT("NT AUTHORITY\\LocalService"), NULL);
	if (!service)
	{
		CloseServiceHandle(manager);
		systemError(TEXT("CreateService"), TRUE);
	}
	
	description.lpDescription = SERVICE_DESC;
	
	ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &description);
	SERVICE_REQUIRED_PRIVILEGES_INFO required_privileges = { TEXT("SeChangeNotifyPrivilege\0") };
	ChangeServiceConfig2(service, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &required_privileges);
	SERVICE_SID_INFO sid = { SERVICE_SID_TYPE_UNRESTRICTED };
	ChangeServiceConfig2(service, SERVICE_CONFIG_SERVICE_SID_INFO, &sid);
		
	CloseServiceHandle(service);
	CloseServiceHandle(manager);
}

void serviceRemove(void)
{
	SC_HANDLE manager;
	SC_HANDLE service;
	
	manager = OpenSCManager(NULL, NULL, 0);
	if (!manager)
		systemError(TEXT("OpenSCManager"), TRUE);
		
	service = OpenService(manager, TEXT("WinIPBroadcast"), DELETE);
	if (!service)
	{
		CloseServiceHandle(manager);
		systemError(TEXT("OpenService"), TRUE);
	}
	
	if (!DeleteService(service))
		systemError(TEXT("DeleteService"), TRUE);
	
	CloseServiceHandle(service);
	CloseServiceHandle(manager);
}

void usage(void)
{
	fwprintf(stderr, TEXT("usage: WinIPBroadcast < install | remove | run >\n"));
	fwprintf(stderr, TEXT("WinIPBroadcast 1.5 by  Etienne Dechamps <etienne@edechamps.fr>\n"));
	fwprintf(stderr, TEXT("https://github.com/dechamps/WinIPBroadcast\n"));
	quit();
}

int main(int argc, char* argv[])
{
	SERVICE_TABLE_ENTRY dispatchTable[] = 
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)serviceMain },
		{ NULL, NULL }
	};
	
	mainThread = NULL;
	serviceStatus = NULL;

	if (!StartServiceCtrlDispatcher(dispatchTable))
	{
		if (GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			systemError(TEXT("StartServiceCtrlDispatcher"), TRUE);
	}
	else
		return 0;
		
	if (argc < 2)
		usage();
		
	if (!_strcmpi(argv[1], "install"))
		serviceInstall();
	else if (!_strcmpi(argv[1], "remove"))
		serviceRemove();
	else if (!_strcmpi(argv[1], "run"))
		loop();
	else
		usage();
		
	return 0;
}

void quit(void)
{
	if (serviceStatus)
		serviceReport(SERVICE_STOPPED, NO_ERROR);
	ExitProcess(1);
}
