/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net_wins.c

#include "winsock.h"
#include "wsipx.h"
#include "../qcommon/qcommon.h"

#define MAX_LOOPBACK	4

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get;
	int send;
} loopback_t;

static cvar_t *noudp;
static cvar_t *noipx;

loopback_t loopbacks[2];
int ip_sockets[2];
int ipx_sockets[2];

char *NET_ErrorString(void);

#pragma region ======================= ADDRESS CONVERSION

static void NetadrToSockadr(netadr_t *a, struct sockaddr *s)
{
	memset(s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if (a->type == NA_IP)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
		((struct sockaddr_in *)s)->sin_port = a->port;
	}
	else if (a->type == NA_IPX)
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memcpy(((struct sockaddr_ipx *)s)->sa_netnum, &a->ipx[0], 4);
		memcpy(((struct sockaddr_ipx *)s)->sa_nodenum, &a->ipx[4], 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
	else if (a->type == NA_BROADCAST_IPX)
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memset(((struct sockaddr_ipx *)s)->sa_netnum, 0, 4);
		memset(((struct sockaddr_ipx *)s)->sa_nodenum, 0xff, 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
}

static void SockadrToNetadr(struct sockaddr *s, netadr_t *a)
{
	if (s->sa_family == AF_INET)
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
	else if (s->sa_family == AF_IPX)
	{
		a->type = NA_IPX;
		memcpy(&a->ipx[0], ((struct sockaddr_ipx *)s)->sa_netnum, 4);
		memcpy(&a->ipx[4], ((struct sockaddr_ipx *)s)->sa_nodenum, 6);
		a->port = ((struct sockaddr_ipx *)s)->sa_socket;
	}
}

qboolean NET_CompareAdr(const netadr_t a, const netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

	if (a.type == NA_IP)
		return (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port);

	if (a.type == NA_IPX)
		return ((memcmp(a.ipx, b.ipx, 10) == 0) && a.port == b.port);

	//mxd. Fixes C4715: 'NET_CompareAdr': not all control paths return a value
	// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/net_chan.c#L519 
	Com_Printf(S_COLOR_YELLOW"NET_CompareAdr: bad address type: %i\n", a.type);
	return false;
}

// Compares without the port
qboolean NET_CompareBaseAdr(const netadr_t a, const netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

	if (a.type == NA_IP)
		return (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3]);

	if (a.type == NA_IPX)
		return ((memcmp(a.ipx, b.ipx, 10) == 0));

	//mxd. Fixes C4715: 'NET_CompareBaseAdr': not all control paths return a value
	// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/net_chan.c#L471 
	Com_Printf(S_COLOR_YELLOW"NET_CompareBaseAdr: bad address type: %i\n", a.type);
	return false;
}

char *NET_AdrToString(const netadr_t a)
{
	static char s[64];

	if (a.type == NA_LOOPBACK)
		Com_sprintf(s, sizeof(s), "loopback");
	else if (a.type == NA_IP)
		Com_sprintf(s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	else
		Com_sprintf(s, sizeof(s), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i", a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9], ntohs(a.port));

	return s;
}

// localhost
// idnewt
// idnewt:28000
// 192.246.40.70
// 192.246.40.70:28000
#define DO(src,dest)	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf(copy, "%x", &val);	\
	((struct sockaddr_ipx *)sadr)->dest = val

static qboolean NET_StringToSockaddr(char *s, struct sockaddr *sadr)
{
	uint val;
	char copy[128];
	
	memset(sadr, 0, sizeof(*sadr));

	if ((strlen(s) >= 23) && (s[8] == ':') && (s[21] == ':')) // Check for an IPX address
	{
		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
		sscanf(&s[22], "%u", &val);
		((struct sockaddr_ipx *)sadr)->sa_socket = htons((ushort)val);
	}
	else
	{
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;
		((struct sockaddr_in *)sadr)->sin_port = 0;

		Q_strncpyz(copy, s, sizeof(copy));
		
		// Strip off a trailing :port if present
		for (char *colon = copy; *colon; colon++)
		{
			if (*colon == ':')
			{
				*colon = 0;
				((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon + 1));
			}
		}
		
		if (copy[0] >= '0' && copy[0] <= '9')
		{
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
		}
		else
		{
			struct hostent *h = gethostbyname(copy);
			if (!h)
				return 0;

			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
	}
	
	return true;
}

#undef DO

// localhost
// idnewt
// idnewt:28000
// 192.246.40.70
// 192.246.40.70:28000
qboolean NET_StringToAdr(char *s, netadr_t *a)
{
	if (!strcmp(s, "localhost"))
	{
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;

		return true;
	}

	struct sockaddr sadr = { 0, 0 };
	if (!NET_StringToSockaddr(s, &sadr))
		return false;
	
	SockadrToNetadr(&sadr, a);
	return true;
}

qboolean NET_IsLocalAddress(const netadr_t adr)
{
	return adr.type == NA_LOOPBACK;
}

#pragma endregion

#pragma region ======================= LOOPBACK BUFFERS FOR LOCAL PLAYER

static qboolean NET_GetLoopPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	loopback_t *loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	const int i = loop->get & (MAX_LOOPBACK - 1);
	loop->get++;

	memcpy(net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset(net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;

	return true;
}

static void NET_SendLoopPacket(netsrc_t sock, int length, void *data)
{
	loopback_t *loop = &loopbacks[sock ^ 1];

	const int i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

#pragma endregion 

#pragma region ======================= GET/SEND PACKET

extern void SV_DropClientFromAdr(netadr_t address);

qboolean NET_GetPacket(netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	struct sockaddr from = { 0, 0 };
	int net_socket;

	if (NET_GetLoopPacket(sock, net_from, net_message))
		return true;

	for (int protocol = 0; protocol < 2; protocol++)
	{
		if (protocol == 0)
			net_socket = ip_sockets[sock];
		else
			net_socket = ipx_sockets[sock];

		if (!net_socket)
			continue;

		int fromlen = sizeof(from);
		const int ret = recvfrom(net_socket, (char*)net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen);

		SockadrToNetadr(&from, net_from);

		if (ret == -1)
		{
			switch(WSAGetLastError())
			{
				case WSAEWOULDBLOCK:
					continue;

				case WSAEMSGSIZE:
					Com_Printf(S_COLOR_YELLOW"Warning: oversize packet from %s\n", NET_AdrToString(*net_from));
					continue;

				// Knightmare- added Jitspoe's fix for WSAECONNRESET bomb-out
				case WSAECONNRESET: 
					Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(), NET_AdrToString(*net_from));
					SV_DropClientFromAdr(*net_from); // Drop this client to not flood the console with above message
					continue;

				default:
					// Let servers continue after errors
					Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(), NET_AdrToString(*net_from));
					continue;
			}
		}

		if (ret == net_message->maxsize)
		{
			Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
			continue;
		}

		net_message->cursize = ret;
		return true;
	}

	return false;
}

void NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
{
	struct sockaddr	addr = { 0, 0 };
	int net_socket;

	switch(to.type)
	{
		case NA_LOOPBACK:
			NET_SendLoopPacket(sock, length, data);
			return;

		case NA_IP:
		case NA_BROADCAST:
			net_socket = ip_sockets[sock];
			if (!net_socket)
				return;
			break;

		case NA_IPX:
		case NA_BROADCAST_IPX:
			net_socket = ipx_sockets[sock];
			if (!net_socket)
				return;
			break;

		default:
			Com_Error(ERR_FATAL, "%s: bad address type: %i", __func__, to.type);
			return;
	}

	NetadrToSockadr(&to, &addr);

	const int ret = sendto(net_socket, data, length, 0, &addr, sizeof(addr));
	if (ret == -1)
	{
		const int err = WSAGetLastError();

		// wouldblock is silent
		if (err == WSAEWOULDBLOCK)
			return;

		// Some PPP links dont allow broadcasts
		// NO ERROR fix for pinging servers w/ unplugged LAN cable
		// Pat- WSAEHOSTUNREACH, this can occur if there is no network, or unplug?
		if ((err == WSAEADDRNOTAVAIL || err == WSAEHOSTUNREACH)	&& (to.type == NA_BROADCAST || to.type == NA_BROADCAST_IPX))
			return;

		if (dedicated->integer) // Let dedicated servers continue after errors
			Com_Printf("NET_SendPacket ERROR: %s to %s\n", NET_ErrorString(), NET_AdrToString(to));
		else if (err == WSAEADDRNOTAVAIL)
			Com_DPrintf(S_COLOR_YELLOW"NET_SendPacket Warning: %s : %s\n", NET_ErrorString(), NET_AdrToString(to));
		else
			Com_Error(ERR_DROP, "NET_SendPacket ERROR: %s to %s", NET_ErrorString(), NET_AdrToString(to));
	}
}

#pragma endregion 

int NET_IPSocket(char *net_interface, int port)
{
	const int newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (newsocket == -1)
	{
		if (WSAGetLastError() != WSAEAFNOSUPPORT)
			Com_Printf(S_COLOR_YELLOW"WARNING: UDP_OpenSocket: socket: %s", NET_ErrorString());

		return 0;
	}

	// Make it non-blocking
	u_long lval = 1;
	if (ioctlsocket(newsocket, FIONBIO, &lval) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString());
		return 0;
	}

	// Make it broadcast capable
	int ival = 1;
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&ival, sizeof(ival)) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}

	struct sockaddr_in address = {0, 0, 0, 0, 0, 0, 0};
	if (!net_interface || !net_interface[0] || !Q_stricmp(net_interface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr(net_interface, (struct sockaddr *)&address);

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	address.sin_family = AF_INET;

	if(bind(newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: UDP_OpenSocket: bind: %s\n", NET_ErrorString());
		closesocket(newsocket);

		return 0;
	}

	return newsocket;
}

void NET_OpenIP(void)
{
	cvar_t *ip = Cvar_Get("ip", "localhost", CVAR_NOSET);
	const int dedicated = Cvar_VariableInteger("dedicated");

	if (!ip_sockets[NS_SERVER])
	{
		int port = Cvar_Get("ip_hostport", "0", CVAR_NOSET)->integer;

		if (!port)
			port = Cvar_Get("hostport", "0", CVAR_NOSET)->integer;

		if (!port)
			port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET)->integer;

		ip_sockets[NS_SERVER] = NET_IPSocket(ip->string, port);

		if (!ip_sockets[NS_SERVER] && dedicated)
		{
			Com_Error(ERR_FATAL, "Couldn't allocate dedicated server IP port");
			return;
		}
	}

	// Dedicated servers don't need client ports
	if (dedicated)
		return;

	if (!ip_sockets[NS_CLIENT])
	{
		int port = Cvar_Get("ip_clientport", "0", CVAR_NOSET)->integer;

		if (!port)
			port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET)->integer;

		if (!port)
			port = PORT_ANY;

		ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, port);

		if (!ip_sockets[NS_CLIENT])
			ip_sockets[NS_CLIENT] = NET_IPSocket(ip->string, PORT_ANY);
	}
}

int NET_IPXSocket(int port)
{
	const int newsocket = socket(PF_IPX, SOCK_DGRAM, NSPROTO_IPX);
	if (newsocket == -1)
	{
		if (WSAGetLastError() != WSAEAFNOSUPPORT)
			Com_Printf(S_COLOR_YELLOW"WARNING: IPX_Socket: socket: %s\n", NET_ErrorString());

		return 0;
	}

	// Make it non-blocking
	u_long lval = 1;
	if (ioctlsocket(newsocket, FIONBIO, &lval) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: IPX_Socket: ioctl FIONBIO: %s\n", NET_ErrorString());
		return 0;
	}

	// Make it broadcast capable
	int	ival = 1;
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&ival, sizeof(ival)) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: IPX_Socket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}

	struct sockaddr_ipx	address = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	address.sa_family = AF_IPX;
	memset(address.sa_netnum, 0, 4);
	memset(address.sa_nodenum, 0, 6);

	if (port == PORT_ANY)
		address.sa_socket = 0;
	else
		address.sa_socket = htons((short)port);

	if(bind(newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Com_Printf(S_COLOR_YELLOW"WARNING: IPX_Socket: bind: %s\n", NET_ErrorString());
		closesocket(newsocket);

		return 0;
	}

	return newsocket;
}

void NET_OpenIPX(void)
{
	const int dedicated = Cvar_VariableInteger("dedicated");

	if (!ipx_sockets[NS_SERVER])
	{
		int port = Cvar_Get("ipx_hostport", "0", CVAR_NOSET)->integer;

		if (!port)
			port = Cvar_Get("hostport", "0", CVAR_NOSET)->integer;

		if (!port)
			port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET)->integer;

		ipx_sockets[NS_SERVER] = NET_IPXSocket(port);
	}

	// Dedicated servers don't need client ports
	if (dedicated)
		return;

	if (!ipx_sockets[NS_CLIENT])
	{
		int port = Cvar_Get("ipx_clientport", "0", CVAR_NOSET)->integer;
		if (!port)
			port = Cvar_Get("clientport", va("%i", PORT_CLIENT), CVAR_NOSET)->integer;

		if (!port)
			port = PORT_ANY;

		ipx_sockets[NS_CLIENT] = NET_IPXSocket(port);

		if (!ipx_sockets[NS_CLIENT])
			ipx_sockets[NS_CLIENT] = NET_IPXSocket(PORT_ANY);
	}
}

// A single player game will only use the loopback code
void NET_Config(qboolean multiplayer)
{
	static qboolean old_config;

	if (old_config == multiplayer)
		return;

	old_config = multiplayer;

	if (!multiplayer)
	{	
		// Shut down any existing sockets
		for (int i = 0; i < 2 ; i++)
		{
			if (ip_sockets[i])
			{
				closesocket(ip_sockets[i]);
				ip_sockets[i] = 0;
			}

			if (ipx_sockets[i])
			{
				closesocket(ipx_sockets[i]);
				ipx_sockets[i] = 0;
			}
		}
	}
	else
	{
		// Open sockets
		if (!noudp->integer)
			NET_OpenIP();

		if (!noipx->integer)
			NET_OpenIPX();
	}
}

// Sleeps msec or until net socket is ready
void NET_Sleep(int msec)
{
	fd_set fdset;

	if (!dedicated || !dedicated->integer)
		return; // We're not a server, just run full speed

	FD_ZERO(&fdset);
	int i = 0;

	if (ip_sockets[NS_SERVER])
	{
		FD_SET(ip_sockets[NS_SERVER], &fdset); // Network socket
		i = ip_sockets[NS_SERVER];
	}

	if (ipx_sockets[NS_SERVER])
	{
		FD_SET(ipx_sockets[NS_SERVER], &fdset); // Network socket
		if (ipx_sockets[NS_SERVER] > i)
			i = ipx_sockets[NS_SERVER];
	}

	struct timeval timeout =
	{ 
		.tv_sec = msec / 1000, 
		.tv_usec = (msec % 1000) * 1000
	};
	select(i + 1, &fdset, NULL, NULL, &timeout);
}

//===================================================================

void NET_Init(void)
{
	WSADATA winsockdata;
	const int r = WSAStartup(MAKEWORD(1, 1), &winsockdata);

	if (r)
		Com_Error(ERR_FATAL, "Winsock initialization failed.");

	Com_Printf("Winsock Initialized\n");

	noudp = Cvar_Get("noudp", "0", CVAR_NOSET);
	noipx = Cvar_Get("noipx", "0", CVAR_NOSET);
}

void NET_Shutdown(void)
{
	NET_Config(false); // Close sockets
	WSACleanup ();
}

char *NET_ErrorString(void)
{
	static char errstr[64];
	const int code = WSAGetLastError();

	switch (code)
	{
		// Jitspoe's more complete error codes
		case WSAEINTR: return "WSAEINTR";
		case WSAEBADF: return "WSAEBADF";
		case WSAEACCES: return "WSAEACCES";
		case WSAEFAULT: return "WSAEFAULT";
		case WSAEINVAL: return "WSAEINVAL";
		case WSAEMFILE: return "WSAEMFILE";
		case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS: return "WSAEINPROGRESS";
		case WSAEALREADY: return "WSAEALREADY";
		case WSAENOTSOCK: return "WSAENOTSOCK";
		case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE: return "WSAEMSGSIZE";
		case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE: return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN: return "WSAENETDOWN";
		case WSAENETUNREACH: return "WSAENETUNREACH";
		case WSAENETRESET: return "WSAENETRESET";
		case WSAECONNABORTED: return "WSAECONNABORTED";
		case WSAECONNRESET: return "WSAECONNRESET";
		case WSAENOBUFS: return "WSAENOBUFS";
		case WSAEISCONN: return "WSAEISCONN";
		case WSAENOTCONN: return "WSAENOTCONN";
		case WSAESHUTDOWN: return "WSAESHUTDOWN";
		case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
		case WSAETIMEDOUT: return "WSAETIMEDOUT";
		case WSAECONNREFUSED: return "WSAECONNREFUSED";
		case WSAELOOP: return "WSAELOOP";
		case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
		case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH";
		case WSAENOTEMPTY: return "WSAENOTEMPTY";
		case WSAEPROCLIM: return "WSAEPROCLIM";
		case WSAEUSERS: return "WSAEUSERS";
		case WSAEDQUOT: return "WSAEDQUOT";
		case WSAESTALE: return "WSAESTALE";
		case WSAEREMOTE: return "WSAEREMOTE";
		case WSASYSNOTREADY: return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED: return "WSANOTINITIALISED";
		case WSAEDISCON: return "WSAEDISCON";
		case WSAENOMORE: return "WSAENOMORE";
		case WSAECANCELLED: return "WSAECANCELLED";
		case WSAEINVALIDPROCTABLE: return "WSAEINVALIDPROCTABLE";
		case WSAEINVALIDPROVIDER: return "WSAEINVALIDPROVIDER";
		case WSAEPROVIDERFAILEDINIT: return "WSAEPROVIDERFAILEDINIT";
		case WSASYSCALLFAILURE: return "WSASYSCALLFAILURE";
		case WSASERVICE_NOT_FOUND: return "WSASERVICE_NOT_FOUND";
		case WSATYPE_NOT_FOUND: return "WSATYPE_NOT_FOUND";
		case WSA_E_NO_MORE: return "WSA_E_NO_MORE";
		case WSA_E_CANCELLED: return "WSA_E_CANCELLED";
		case WSAEREFUSED: return "WSAEREFUSED";
		case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN: return "WSATRY_AGAIN";
		case WSANO_RECOVERY: return "WSANO_RECOVERY";
		case WSANO_DATA: return "WSANO_DATA"; 
		case WSA_QOS_RECEIVERS: return "WSA_QOS_RECEIVERS";
		case WSA_QOS_SENDERS: return "WSA_QOS_SENDERS";
		case WSA_QOS_NO_SENDERS: return "WSA_QOS_NO_SENDERS";
		case WSA_QOS_NO_RECEIVERS: return "WSA_QOS_NO_RECEIVERS";
		case WSA_QOS_REQUEST_CONFIRMED: return "WSA_QOS_REQUEST_CONFIRMED";
		case WSA_QOS_ADMISSION_FAILURE: return "WSA_QOS_ADMISSION_FAILURE";
		case WSA_QOS_POLICY_FAILURE: return "WSA_QOS_POLICY_FAILURE";
		case WSA_QOS_BAD_STYLE: return "WSA_QOS_BAD_STYLE";
		case WSA_QOS_BAD_OBJECT: return "WSA_QOS_BAD_OBJECT";
		case WSA_QOS_TRAFFIC_CTRL_ERROR: return "WSA_QOS_TRAFFIC_CTRL_ERROR";
		case WSA_QOS_GENERIC_ERROR: return "WSA_QOS_GENERIC_ERROR";
		case WSA_QOS_ESERVICETYPE: return "WSA_QOS_ESERVICETYPE";
		case WSA_QOS_EFLOWSPEC: return "WSA_QOS_EFLOWSPEC";
		case WSA_QOS_EPROVSPECBUF: return "WSA_QOS_EPROVSPECBUF";
		case WSA_QOS_EFILTERSTYLE: return "WSA_QOS_EFILTERSTYLE";
		case WSA_QOS_EFILTERTYPE: return "WSA_QOS_EFILTERTYPE";
		case WSA_QOS_EFILTERCOUNT: return "WSA_QOS_EFILTERCOUNT";
		case WSA_QOS_EOBJLENGTH: return "WSA_QOS_EOBJLENGTH";
		case WSA_QOS_EFLOWCOUNT: return "WSA_QOS_EFLOWCOUNT";
		case WSA_QOS_EUNKOWNPSOBJ: return "WSA_QOS_EUNKOWNPSOBJ";
		case WSA_QOS_EPOLICYOBJ: return "WSA_QOS_EPOLICYOBJ";
		case WSA_QOS_EFLOWDESC: return "WSA_QOS_EFLOWDESC";
		case WSA_QOS_EPSFLOWSPEC: return "WSA_QOS_EPSFLOWSPEC";
		case WSA_QOS_EPSFILTERSPEC: return "WSA_QOS_EPSFILTERSPEC";
		case WSA_QOS_ESDMODEOBJ: return "WSA_QOS_ESDMODEOBJ";
		case WSA_QOS_ESHAPERATEOBJ: return "WSA_QOS_ESHAPERATEOBJ";
		case WSA_QOS_RESERVED_PETYPE: return "WSA_QOS_RESERVED_PETYPE";
		default:
			Com_sprintf(errstr, sizeof(errstr), "Unhandled WSA code: %i", code);
			return errstr;
	}
}