#ifndef _NETDB_H
#define _NETDB_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/netdb.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <netinet/in.h>
#include <sys/socket.h>
#include <inttypes.h>

#define IPPORT_RESERVED 1024

#define AI_PASSIVE		0x01
#define AI_CANONNAME	0x02
#define AI_NUMERICHOST	0x04
#define AI_NUMERICSERV	0x08
#define AI_V4MAPPED		0x10
#define AI_ALL			0x20
#define AI_ADDRCONFIG	0x40

#define NI_NOFQDN		0x01
#define NI_NUMERICHOST	0x02
#define NI_NAMEREQD		0x04
#define NI_NUMERICSERV	0x08
#define NI_NUMERICSCOPE	0x10
#define NI_DGRAM		0x20

#define EAI_AGAIN		1
#define EAI_BADFLAGS	2
#define EAI_FAIL		3
#define EAI_FAMILY		4
#define EAI_MEMORY		5
#define EAI_NONAME		6
#define EAI_SERVICE		7
#define EAI_SOCKTYPE	8
#define EAI_SYSTEM		9
#define EAI_OVERFLOW	10

struct hostent
{
	char*	h_name;			/* Official name of the host. */
	char**	h_aliases;		/* A pointer to an array of pointers to alternative host names, terminated by a null pointer. */
	int		h_addrtype;		/* Address type. */
	int		h_length;		/* The length, in bytes, of the address. */
	char**	h_addr_list;	/* A pointer to an array of pointers to network addresses (in network byte order) for the host, terminated by a null pointer. */
};

struct netent
{
	char*		n_name;		/* Official, fully-qualified (including the domain) name of the host. */
	char**		n_aliases;	/* A pointer to an array of pointers to alternative network names, terminated by a null pointer. */
	int			n_addrtype;	/* The address type of the network. */
	uint32_t	n_net;		/* The network number, in host byte order. */
};

struct protoent
{
	char*	p_name;		/* Official name of the protocol. */
	char**	p_aliases;	/* A pointer to an array of pointers to alternative protocol names, terminated by a null pointer. */
	int		p_proto;	/* The protocol number. */
};

struct servent
{
	char*	s_name;		/* Official name of the service. */
	char**	s_aliases;	/* A pointer to an array of pointers to alternative service names, terminated by a null pointer. */
	int		s_port;		/* A value which, when converted to uint16_t, yields the port number in network byte order at which the service resides. */
	char*	s_proto;	/* The name of the protocol to use when contacting the service. */
};

struct addrinfo
{
	int					ai_flags;		/* Input flags. */
	int					ai_family;		/* Address family of socket. */
	int					ai_socktype;	/* Socket type. */
	int					ai_protocol;	/* Protocol of socket. */
	socklen_t			ai_addrlen;		/* Length of socket address. */
	struct sockaddr*	ai_addr;		/* Socket address of socket. */
	char*				ai_canonname;	/* Canonical name of service location. */
	struct addrinfo*	ai_next;		/* Pointer to next in list. */
};

void				endhostent(void);
void				endnetent(void);
void				endprotoent(void);
void				endservent(void);
void				freeaddrinfo(struct addrinfo* ai);
const char*			gai_strerror(int ecode);
int					getaddrinfo(const char* __restrict nodename, const char* __restrict servname, const struct addrinfo* __restrict hints, struct addrinfo** __restrict res);
struct hostent*		gethostbyname(const char* name);
struct hostent*		gethostent(void);
int					getnameinfo(const struct sockaddr* __restrict sa, socklen_t salen, char* __restrict node, socklen_t nodelen, char* __restrict service, socklen_t servicelen, int flags);
struct netent*		getnetbyaddr(uint32_t net, int type);
struct netent*		getnetbyname(const char* name);
struct netent*		getnetent(void);
struct protoent*	getprotobyname(const char* name);
struct protoent*	getprotobynumber(int proto);
struct protoent*	getprotoent(void);
struct servent*		getservbyname(const char* name, const char* proto);
struct servent*		getservbyport(int port, const char* proto);
struct servent*		getservent(void);
void				sethostent(int stayopen);
void				setnetent(int stayopen);
void				setprotoent(int stayopen);
void				setservent(int stayopen);

__END_DECLS

#endif
