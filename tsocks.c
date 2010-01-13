/*

    TSOCKS - Wrapper library for transparent SOCKS 

    Copyright (C) 2000 Shaun Clowes 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* PreProcessor Defines */
#include <config.h>

#ifdef USE_GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Global configuration variables */
char *progname = "libtsocks: ";      	   /* Name used in err msgs    */

/* Header Files */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <common.h>
#include <stdarg.h>
#ifdef USE_SOCKS_DNS
#include <resolv.h>
#endif
#include <parser.h>
#include <tsocks.h>

/* Global Declarations */
REALCONNECT_PROTOTYPE
#ifdef USE_SOCKS_DNS
static int (*realresinit)(void);
#endif
static int initialized = 0; 

/* Exported Function Prototypes */
void _init(void);
CONNECT_PROTOTYPE
#ifdef USE_SOCKS_DNS
int res_init(void);
#endif

/* Private Function Prototypes */
static int initialize();
static int connect_socks(int, struct sockaddr_in *, struct sockaddr_in *);
static int connect_socksv4(int, struct sockaddr_in *, struct sockaddr_in *);
static int connect_socksv5(int, struct sockaddr_in *, struct sockaddr_in *);

void _init(void) {
#ifdef USE_OLD_DLSYM
	void *libc;
#endif

	/* We could do all our initialization here, but to be honest */
	/* most programs that are run won't use our services, so     */
	/* we do our general initialization on first call            */

#ifndef USE_OLD_DLSYM
	realconnect = dlsym(RTLD_NEXT, "connect");
	#ifdef USE_SOCKS_DNS
	realresinit = dlsym(RTLD_NEXT, "res_init");
	#endif
#else
	libc = dlopen(LIBC, RTLD_LAZY);
	realconnect = dlsym(libc, "connect");
	#ifdef USE_SOCKS_DNS
	realresinit = dlsym(libc, "res_init");
	#endif
	dlclose(libc);	
#endif
#ifndef DEBUG
	setenv("TSOCKS_NO_DEBUG", "yes", 1);
#endif
	
}

static int initialize () {

	/* Read in the config file */
	read_config(NULL);
	initialized = 1;

	return(0);

}

CONNECT_FUNCTION
	struct sockaddr_in *connaddr;
	int sock_type = -1;
	int sock_type_len = sizeof(sock_type);
	unsigned int res = -1;
	struct sockaddr_in server_address;
	struct serverent *path;

	/* If the real connect doesn't exist, we're stuffed */
	if (realconnect == NULL) {
		show_error("Unresolved symbol: connect\n");
		return(-1);
	}

	/* Linux uses a union of all different types of sockaddr   */
	/* structures not a simple sockaddr                        */
#ifndef USE_UNION
	connaddr = (struct sockaddr_in *) __addr;
#else
	connaddr = (struct sockaddr_in *) __addr.__sockaddr_in__;
#endif

	/* Get the type of the socket */
	getsockopt(__fd, SOL_SOCKET, SO_TYPE, 
		   (void *) &sock_type, &sock_type_len);

	/* Only try and use socks if the socket is an INET socket, */
	/* and is a TCP stream                                     */
	/* socks server in the config was valid                    */
	if ((connaddr->sin_family == AF_INET) &&
	    (sock_type == SOCK_STREAM)) {

		/* If we haven't initialized yet, do it now */
		if (initialized == 0) {
			initialize();
		}

		/* If the address is local connect */
		if (!(is_local(&(connaddr->sin_addr)))) 
			return(realconnect(__fd, __addr, __len));

		/* Ok, so its not local, we need a path to the net */
		pick_server(&path, &connaddr->sin_addr);
						
		if (path->address == NULL) {
			if (path == &defaultserver) 
				show_error("Connection needs to be made "
					   "via default server but "
					   "the default server has not "
					   "been specified\n");
			else 
				show_error("Connection needs to be made "
					   "via path specified at line "
					   "%d in configuration file but "
					   "the server has not been "
					   "specified for this path\n",
					   path->lineno);
		} else if ((res = resolve_ip(path->address, 0, HOSTNAMES)) == -1) {
			show_error("The SOCKS server (%s) in configuration "
				   "file is invalid\n", path->address);
		} else {	
			/* Construct the addr for the socks server */
			server_address.sin_family = AF_INET; /* host byte order */
			server_address.sin_addr.s_addr = res;
			server_address.sin_port = path->port;
			bzero(&(server_address.sin_zero), 8);

			/* Complain if this server isn't on a localnet */
			if (is_local(&server_address.sin_addr)) {
				show_error("SOCKS server %s (%s) is not on a local subnet!\n", path->address, inet_ntoa(server_address.sin_addr));
			}
		}

		/* Call the real connect only if the config */
		/* read failed 				    */
		if (res == -1)  
			return(realconnect(__fd, __addr, __len));
		else
			return(connect_socks(__fd, connaddr, &server_address));
	} else {
		return(realconnect(__fd, __addr, __len));
	} 

}

static int connect_socks(int sockid, struct sockaddr_in *connaddr, struct sockaddr_in *serveraddr) {
	int rc = 0, rerrno = 0;
	int sockflags;
	
	/* Get the flags of the socket, (incase its non blocking */
	if ((sockflags = fcntl(sockid, F_GETFL)) == -1) {
		sockflags = 0;
	}

	/* If the flags show the socket as blocking, set it to   */
	/* blocking for our connection to the socks server       */
	if ((sockflags & O_NONBLOCK) != 0) {
		fcntl(sockid, F_SETFL, sockflags & (~(O_NONBLOCK)));
	}

	if (defaultserver.type == 4) 
		rc = connect_socksv4(sockid, connaddr, serveraddr);
	else
		rc = connect_socksv5(sockid, connaddr, serveraddr);
	rerrno = errno;

	/* If the socket was in non blocking mode, restore that */
	if ((sockflags & O_NONBLOCK) != 0) {
		fcntl(sockid, F_SETFL, sockflags);
	}

	if (rc != 0) {
		errno = rerrno;
		return(-1);
	}

	return(0);   

}			

static int connect_socksv4(int sockid, struct sockaddr_in *connaddr, struct sockaddr_in *serveraddr) {
	int rc = 0, rerrno = 0;
	int length = 0;
	char *realreq;
	struct passwd *user;
	struct sockreq *thisreq;
	struct sockrep thisrep;
	
	/* Determine the current username */
	user = getpwuid(getuid());	

	/* Allocate enough space for the request and the null */
	/* terminated username */
	length = sizeof(struct sockreq) + 
			(user == NULL ? 1 : strlen(user->pw_name) + 1);
	if ((realreq = malloc(length)) == NULL) {
		/* Could not malloc, bail */
		exit(1);
	}
	thisreq = (struct sockreq *) realreq;

	/* Create the request */
	thisreq->version = 4;
	thisreq->command = 1;
	thisreq->dstport = connaddr->sin_port;
	thisreq->dstip   = connaddr->sin_addr.s_addr;

	/* Copy the username */
	strcpy(realreq + sizeof(struct sockreq), 
	       (user == NULL ? "" : user->pw_name));

	/* Connect this socket to the socks server instead */
	if ((rc = realconnect(sockid, (struct sockaddr *) serveraddr, 
		              sizeof(struct sockaddr_in))) != 0) {
		show_error("Error %d attempting to connect to SOCKS "
				   "server (%s)\n", errno, strerror(errno));
		rc = -1;
		rerrno = errno;
	} else {
		rc = send(sockid, (void *) thisreq, length,0);
		if (rc < 0) {
			show_error("Error %d attempting to send SOCKS "
				   "request\n", errno);
			rc = -1;
			rerrno = ECONNREFUSED;
		} else if ((rc = recv(sockid, (void *) &thisrep, 
				      sizeof(struct sockrep), 0)) < 0) {
			show_error("Error %d attempting to receive SOCKS "
				   "reply\n", errno);
			rc = -1;
			rerrno = ECONNREFUSED;
		} else if (rc < sizeof(struct sockrep)) {
			show_error("Short reply from SOCKS server\n");
			/* Let the application try and see how they */
			/* go                                       */
			rc = 0;
			rerrno = 0;
		} else if (thisrep.result == 91) {
			show_error("SOCKS server refused connection\n");
			rc = -1;
			rerrno = ECONNREFUSED;
		} else if (thisrep.result == 92) {
			show_error("SOCKS server refused connection "
				   "because of failed connect to identd "
				   "on this machine\n");
			rc = -1;
			rerrno = ECONNREFUSED;
		} else if (thisrep.result == 93) {
			show_error("SOCKS server refused connection "
				   "because identd and this library "
				   "reported different user-ids\n");
			rc = -1;
			rerrno = ECONNREFUSED;
		} else {
			rc = 0;
			rerrno = 0;
		}
	}

	/* Free the SOCKS request storage that was malloced */
	free(realreq);

	/* Set errno to the rerrno value so that the application */
	/* gets a meaningful error message                       */
	errno = rerrno;

	return(rc);   
}			

static int connect_socksv5(int sockid, struct sockaddr_in *connaddr, struct sockaddr_in *serveraddr) {
	int rc = 0;
	int offset = 0;
	char *verstring = "\x05\x02\x00\x02";
	char *uname, *upass;
	struct passwd *nixuser;
	char buf[200];

	/* Connect this socket to the socks server */
	if ((rc = realconnect(sockid, (struct sockaddr *) serveraddr, 
		              sizeof(struct sockaddr_in))) != 0) {
		show_error("Error %d attempting to connect to SOCKS "
				   "server\n", errno);
		return(rc);
	}

        /* Now send the method negotiation */
	if ((rc = send(sockid, (void *) verstring, 4,0)) < 0) {
		show_error("Error %d attempting to send SOCKS "
			   "method negotiation\n", errno);
		return(rc);
	}

	/* Now receive the reply as to which method we're using */
	if ((rc = recv(sockid, (void *) buf, 2, 0)) < 0) {
		show_error("Error %d attempting to receive SOCKS "
			   "method negotiation reply\n", errno);
		rc = ECONNREFUSED;
		return(rc);
	}

	if (rc < 2) {
		show_error("Short reply from SOCKS server\n");
		rc = ECONNREFUSED;
		return(rc);
	}

	/* See if we offered an acceptable method */
	if (buf[1] == '\xff') {
		show_error("SOCKS server refused authentication methods\n");
		rc = ECONNREFUSED;
		return(rc);
	}

	/* If the socks server chose username/password authentication */
	/* (method 2) then do that */
	if ((unsigned short int) buf[1] == 2) {

		/* Determine the current *nix username */
		nixuser = getpwuid(getuid());	

		if (((uname = getenv("TSOCKS_USERNAME")) == NULL) &&
		    ((uname = defaultserver.defuser) == NULL) &&
		    ((uname = (nixuser == NULL ? NULL : nixuser->pw_name)) == NULL)) {
			show_error("Could not get SOCKS username from "
				   "local passwd file, tsocks.conf "
				   "or $TSOCKS_USERNAME to authenticate "
				   "with"); 
			rc = ECONNREFUSED;
			return(rc);
		} 

		if (((upass = getenv("TSOCKS_PASSWORD")) == NULL) &&
		    ((upass = defaultserver.defpass) == NULL)) {
			show_error("Need a password in tsocks.conf or "
				   "$TSOCKS_PASSWORD to authenticate with");
			rc = ECONNREFUSED;
			return(rc);
		} 

		/* Check that the username / pass specified will */
		/* fit into the buffer				 */
		if ((3 + strlen(uname) + strlen(upass)) >= sizeof(buf)) {
			show_error("The supplied socks username or "
				   "password is too long");
			exit(1);
		}
		
		offset = 0;
		buf[offset] = '\x01';
		offset++;
		buf[offset] = (int8_t) strlen(uname);
		offset++;
		memcpy(&buf[offset], uname, strlen(uname));
		offset = offset + strlen(uname);
		buf[offset] = (int8_t) strlen(upass);
		offset++;
		memcpy(&buf[offset], upass, strlen(upass));
		offset = offset + strlen(upass);

		/* Send out the authentication */
		if ((rc = send(sockid, (void *) buf, offset,0)) < 0) {
			show_error("Error %d attempting to send SOCKS "
				   "authentication\n", errno);
			return(rc);
		}

		/* Receive the authentication response */
		if ((rc = recv(sockid, (void *) buf, 2, 0)) < 0) {
			show_error("Error %d attempting to receive SOCKS "
				   "authentication reply\n", errno);
			rc = ECONNREFUSED;
			return(rc);
		}

		if (rc < 2) {
			show_error("Short reply from SOCKS server\n");
			rc = ECONNREFUSED;
			return(rc);
		}

		if (buf[1] != '\x00') {
			show_error("SOCKS authentication failed, "
				   "check username and password\n");
			rc = ECONNREFUSED;
			return(rc);
		}
		
	} 

	/* Now send the connect */
	buf[0] = '\x05';		/* Version 5 SOCKS */
	buf[1] = '\x01'; 		/* Connect request */
	buf[2] = '\x00';		/* Reserved	   */
	buf[3] = '\x01';		/* IP version 4    */
	memcpy(&buf[4], &connaddr->sin_addr.s_addr, 4);
	memcpy(&buf[8], &connaddr->sin_port, 2);

        /* Now send the connection */
	if ((rc = send(sockid, (void *) buf, 10,0)) < 0) {
		show_error("Error %d attempting to send SOCKS "
			   "connect command\n", errno);
		return(rc);
	}

	/* Now receive the reply to see if we connected */
	if ((rc = recv(sockid, (void *) buf, 10, 0)) < 0) {
		show_error("Error %d attempting to receive SOCKS "
			   "connection reply\n", errno);
		rc = ECONNREFUSED;
		return(rc);
	}

	if (rc < 10) {
		show_error("Short reply from SOCKS server\n");
		rc = ECONNREFUSED;
		return(rc);
	}

	/* See the connection succeeded */
	if (buf[1] != '\x00') {
		show_error("SOCKS connect failed: ");
		switch ((int8_t) buf[1]) {
			case 1:
				show_error("General SOCKS server failure\n");
				return(ECONNABORTED);
			case 2:
				show_error("Connection denied by rule\n");
				return(ECONNABORTED);
			case 3:
				show_error("Network unreachable\n");
				return(ENETUNREACH);
			case 4:
				show_error("Host unreachable\n");
				return(EHOSTUNREACH);
			case 5:
				show_error("Connection refused\n");
				return(ECONNREFUSED);
			case 6: 
				show_error("TTL Expired\n");
				return(ETIMEDOUT);
			case 7:
				show_error("Command not supported\n");
				return(ECONNABORTED);
			case 8:
				show_error("Address type not supported\n");
				return(ECONNABORTED);
			default:
				show_error("Unknown error\n");
				return(ECONNABORTED);
		}	
	}

	return(0);   

}			

#ifdef USE_SOCKS_DNS
int res_init(void) {
        int rc;

	if (realresinit == NULL) {
		show_error("Unresolved symbol: res_init\n");
		return(-1);
	}
        
	/* Call normal res_init */
	rc = realresinit();

        /* Force using TCP protocol for DNS queries */
        _res.options |= RES_USEVC;

        return rc;
}
#endif

