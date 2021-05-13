/**
 * Copyright (C) 2016-2018 Jessica James.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Written by Jessica James <jessica.aj@outlook.com>
 */

#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#include <Iphlpapi.h>

 /** Relative path to the launcher executable */
#define LAUNCHER_PATH "..\\..\\..\\Launcher\\Renegade X Launcher.exe"

bool init_wsa_success;

#pragma pack(push, 4)
struct socket_t {
	uint64_t sock;
} s_socket;

struct AcceptedSocket
{
	uint64_t sock;
	int32_t addr;
	int32_t port;
} accepted_socket;
#pragma pack(pop)

void init_wsa()
{
	WSADATA wsadata;
	init_wsa_success = WSAStartup(WINSOCK_VERSION, &wsadata);
}

void cleanup_wsa()
{
	if (init_wsa_success)
		WSACleanup();
}

__declspec(dllexport) struct socket_t* c_socket()
{
	s_socket.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	return &s_socket;
}

// since UDK only supports IPv4, just return the IP address as an integer (or 0 on failure).
/*int32_t c_resolve(unsigned char *hostname)
{
	struct hostent *host_entry;
	IN_ADDR address;

	host_entry = gethostbyname(hostname);
	if (host_entry == NULL)
		return 0;

	memcpy(&address, host_entry->h_addr_list[0], host_entry->h_length);

	return address.s_addr;
}*/

struct sockaddr_in helper_make_sockaddr_in(int32_t in_address, int32_t in_port)
{
	struct sockaddr_in address;
	IN_ADDR in_addr;

	memset(&address, 0, sizeof(address));

	in_addr.s_addr = htonl(in_address);
	address.sin_addr = in_addr;
	address.sin_family = AF_INET;
	address.sin_port = htons(in_port);

	return address;
}

// Binds to in_port, or fails
__declspec(dllexport) int32_t c_bind(struct socket_t* in_socket, int32_t in_port)
{
	struct sockaddr_in address;
	
	address = helper_make_sockaddr_in(INADDR_ANY, in_port);

	if (bind(in_socket->sock, (struct sockaddr *) &address, sizeof(address)) == 0)
		return in_port;

	return 0;
}

// Binds to in_port, or the next available
__declspec(dllexport) int32_t c_bind_next(struct socket_t* in_socket, int32_t in_port)
{
	struct sockaddr_in address;

	address = helper_make_sockaddr_in(INADDR_ANY, in_port);

	while (true)
	{
		if (bind(in_socket->sock, (struct sockaddr *) &address, sizeof(address)) == 0)
			return in_port;

		if (WSAGetLastError() != WSAEADDRINUSE)
			return 0;

		address.sin_port = htons(++in_port);
	}
}

// Binds to any port
__declspec(dllexport) int32_t c_bind_any(struct socket_t* in_socket)
{
	struct sockaddr_in address;

	address = helper_make_sockaddr_in(INADDR_ANY, 0);

	if (bind(in_socket->sock, (struct sockaddr *) &address, sizeof(address)) == 0)
		return ntohs(address.sin_port);

	return 0;
}

__declspec(dllexport) int32_t c_listen(struct socket_t* in_socket)
{
	return listen(in_socket->sock, SOMAXCONN);
}

__declspec(dllexport) struct AcceptedSocket *c_accept(struct socket_t* in_socket)
{
	struct sockaddr addr;
	int size;

	size = sizeof(addr);
	accepted_socket.sock = accept(in_socket->sock, &addr, &size);

	if (accepted_socket.sock != INVALID_SOCKET)
	{
		accepted_socket.addr = ((struct sockaddr_in *) &addr)->sin_addr.s_addr;
		accepted_socket.port = ((struct sockaddr_in *) &addr)->sin_port;
	}

	return &accepted_socket;
}

__declspec(dllexport) int32_t c_connect(struct socket_t* in_socket, int32_t in_address, int32_t in_port)
{
	struct sockaddr_in address;

	address = helper_make_sockaddr_in(in_address, in_port);
	
	return connect(in_socket->sock, (struct sockaddr *) &address, sizeof(address));
}

__declspec(dllexport) int32_t c_close(struct socket_t* in_socket)
{
	return closesocket(in_socket->sock);
}

__declspec(dllexport) int32_t c_recv(struct socket_t* in_socket, unsigned char *out_buffer, int32_t in_buffer_size)
{
	return recv(in_socket->sock, out_buffer, in_buffer_size, 0);
}

__declspec(dllexport) int32_t c_send(struct socket_t* in_socket, unsigned char *in_buffer, int32_t in_buffer_size)
{
	return send(in_socket->sock, in_buffer, in_buffer_size, 0);
}

__declspec(dllexport) int32_t c_set_blocking(struct socket_t* in_socket, int32_t in_value)
{
	unsigned long block_mode;
	block_mode = in_value == 0 ? 1 : 0;
	return ioctlsocket(in_socket->sock, FIONBIO, &block_mode);
}

__declspec(dllexport) int32_t c_get_last_error()
{
	return GetLastError();
}

__declspec(dllexport) int32_t c_check_status(struct socket_t* in_socket)
{
	fd_set set_read, set_err;
	struct timeval time_val;
	int value;

	time_val.tv_sec = 0;
	time_val.tv_usec = 0;
	FD_ZERO(&set_read);
	FD_ZERO(&set_err);
	FD_SET(in_socket->sock, &set_read);
	FD_SET(in_socket->sock, &set_err);

	// check for success
	value = select(in_socket->sock + 1, NULL, &set_read, &set_err, &time_val);
	if (value > 0) // socket can be written to
	{
		if (FD_ISSET(in_socket->sock, &set_read))
			return 1;
		if (FD_ISSET(in_socket->sock, &set_err))
			return -1;
	}

	return value;
}

__declspec(dllexport) bool UpdateGame() {
	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInformation;

	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&processInformation, 0, sizeof(processInformation));

	// Create process
	if (!CreateProcessA(LAUNCHER_PATH, NULL, NULL, NULL, false, 0, NULL, NULL, &startupInfo, &processInformation)) {
		return false;
	}

	// Process successfully created; exit the application so that patching doesn't fail horribly
	exit(EXIT_SUCCESS);
	return true; // unreachable
}
