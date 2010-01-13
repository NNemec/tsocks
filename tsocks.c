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
#define DEBUG		1		   /* Show error messages?     */
#define	CONFFILE	"/etc/tsocks.conf" /* Location of config file  */
#define MAXLINE		BUFSIZ		   /* Max length of conf line  */
#define MYNAME		"libtsocks: "      /* Name used in err msgs    */
#define OSNAME 		linux		   /* Linux/Solaris	       */
#define OSVER 		1		   /* Version if Solaris       */

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
#include <stdarg.h>
#if OSNAME == solaris
#include <strings.h>
#endif
#ifdef SOCKSDNS
#if OSNAME == solaris
#include <arpa/nameser.h>
#endif
#include <resolv.h>
#endif
#include "tsocks.h"

/* Global Declarations */
#if OSNAME != solaris
static int (*realconnect)(int __fd, __CONST_SOCKADDR_ARG __addr, 
			  socklen_t __len);
#elif OSVER == 6
static int (*realconnect)(int, struct sockaddr *, size_t);
#else
static int (*realconnect)(int, const struct sockaddr *, socklen_t);
#endif
#ifdef SOCKSDNS
static int (*realresinit)(void);
#endif
static struct socksent *localnets = NULL; 
static int initialized = 0;
static int servertype = 0;
static struct sockaddr_in server;
static char *default_user = NULL;
static char *default_pass = NULL;

/* Exported Function Prototypes */
void _init(void);
#if OSNAME != solaris 
int connect (int, __CONST_SOCKADDR_ARG, socklen_t);
#elif OSVER == 6
int connect (int, struct sockaddr *, int);
#else
int connect (int, const struct sockaddr *, socklen_t);
#endif
#ifdef SOCKSDNS
int res_init(void);
#endif

/* Private Function Prototypes */
static int initialize();
static int read_config();
static int handle_line(char *);
static int break_pair(char *, char **, char **);
static int handle_server(char *);
static int handle_type(char *);
static int handle_port(char *);
static int handle_local(char *);
static int handle_defuser(char *);
static int handle_defpass(char *);
static int is_local(struct in_addr *);
static int connect_socks(int, struct sockaddr_in *);
static int connect_socksv4(int, struct sockaddr_in *);
static int connect_socksv5(int, struct sockaddr_in *);
static void show_error(char *, ...);
#if OSNAME == solaris 
static char *strsep(char **, const char *);
#endif

void _init(void) {

	/* We could do all our initialization here, but to be honest */
	/* most programs that are run won't use our services, so     */
	/* we do our general initialization on first call            */

	server.sin_family = 0;

	/* We use an 'undocumented' dlsym parameter to get the       */
	/* location of the 'real' libc connect. I found this code on */
	/* the kernel mailing list and its used in liblogwrites.     */
	/* I have no idea what compatibility issues it may bring,    */
	/* but we can always revert to the old code                  */

	realconnect = dlsym(RTLD_NEXT, "connect");
	#ifdef SOCKSDNS
	realresinit = dlsym(RTLD_NEXT, "res_init");
	#endif
	
}

static int initialize () {

	/* Read in the config file */
	read_config();

	initialized = 1;

	return(0);

}

static int read_config () {
	FILE *conf;
	char line[MAXLINE];
	int cleardefs = 0;

	/* Read the local nets file */
	if ((conf = fopen(CONFFILE, "r")) == NULL) {
		show_error("Could not open socks configuration file "
			   "(%s)\n", CONFFILE);
	}	
	else {
		while (NULL != fgets(line, MAXLINE, conf)) {
			/* This line _SHOULD_ end in \n so we  */
			/* just chop off the \n and hand it on */
			if (strlen(line) > 0)
				line[strlen(line) - 1] = '\0';
			handle_line(line);
		} 
		fclose(conf);

		/* Always add the 127.0.0.1/255.0.0.0 subnet to local */
		handle_local("127.0.0.0/255.0.0.0");

		if (server.sin_family == 0) {
			show_error("No valid SOCKS server specified in "
				   "configuration file\n");
		} else if (is_local(&server.sin_addr)) {
			show_error("SOCKS server is not on a local subnet!\n");
			server.sin_family = 0;
		}

		/* Default to the default SOCKS port */
		if (server.sin_port == 0) {
			server.sin_port = htons(1080);
		}

		/* Default to SOCKS V4 */
		if (servertype == 0) {
			servertype = 4;
		}

		if ((default_user != NULL) || (default_pass != NULL)) {
			/* If the server type is version 4, neither */
			/* username or password is valid */
			if (servertype == 4) {
				show_error("Default username/pass "
					   "only valid for version 5 "
					   "SOCKS servers\n");
				cleardefs = 1;
			} else 
			/* If default user and pass not specified */
			/* together then show an error */
			if (!((default_user != NULL) && 
			      (default_pass != NULL))) {
				show_error("Default username and "
					   "pass MUST be specified "
					   "together\n");
				cleardefs = 1;
			}
			if (cleardefs == 1) {
				if (default_user != NULL) {
					free(default_user);
					default_user = NULL;
				}
				if (default_pass != NULL) {
					free(default_user);
					default_user = NULL;
				}
			}
		}
	}

	return(0);
}

static int handle_line(char *line) {
	char *type;
	char *value;
	char *tokenize;

	tokenize = line;

	/* We need to ignore all spaces at beginning of line */
	line = line + strspn(line, " \t");

	if (*line == '#') {
		/* Comment Line, ignore it */
	} else if (strcspn(line, " \n\t") == 0) {
		/* Blank Line, ignore it */
	} else {
		/* This is a pair */
		break_pair(line, &type, &value);
		if ((type != NULL) && (value != NULL)) {
			if (!strcmp(type, "server")) {
				handle_server(value);
			} else if (!strcmp(type, "server_port")) {
				handle_port(value);
			} else if (!strcmp(type, "server_type")) {
				handle_type(value);
			} else if (!strcmp(type, "default_user")) {
				handle_defuser(value);
			} else if (!strcmp(type, "default_pass")) {
				handle_defpass(value);
			} else if (!strcmp(type, "local")) {
				handle_local(value);
			} else {
				show_error("Invalid pair type (%s) in "
					   "configuration file\n", type);
			}
		} else {
			show_error("Invalid line in configuration file " 
				   "(%s)\n", line);
		}
	}

	return(0);
}

static int break_pair(char *line, char **type, char **value) {

	/* Now split the string up at the equals sign */
	*type = strsep(&line, "=");
	if (line == NULL) {
		*value = NULL;
	} else {
		/* Strip off leading spaces from type */
		*type = *type + strspn(*type, "\t");
		/* Strip off any trailing spaces etc from the type */
		*(*type + strcspn(*type," \t")) = '\0';
		
		*value = line;
		/* Strip off any leading spaces from value */
		*value = *value + strspn(*value, " \t");
		/* Strip off trailing spaces from value */
		*(*value + strcspn(*value," \t\n")) = '\0';
	}

	return(0);
}	

static int handle_server(char *value) {
	char *ip;

	ip = strsep(&value, " ");

	if (server.sin_family == 0) {
#if OSNAME == solaris
		if ((server.sin_addr.s_addr = inet_addr(ip)) == -1) {
#else
		if (!(inet_aton(ip, &server.sin_addr))) {
#endif
			show_error("The SOCKS server (%s) in configuration "
				   "file is invalid\n", ip);
		} else {		
			/* Construct the addr for the socks server */
			server.sin_family = AF_INET; /* host byte order */
	        	server.sin_port = 0;
			bzero(&(server.sin_zero), 8);/* zero the rest of the struct */    
		}
	} else {
		show_error("Only one SOCKS server may be specified "
			   "in the configuration file\n");
	}

	return(0);
}

static int handle_port(char *value) {

	if (server.sin_port != 0) {
		show_error("Server port may only be specified once "
			   "in configuration file\n");
	} else {
		errno = 0;
		server.sin_port = (unsigned short int)
				  htons(strtol(value, (char **)NULL, 10));
		if ((errno != 0) || (server.sin_port == 0)) {
			show_error("Invalid server port number "
				   "specified in configuration file "
				   "(%s)\n", value);
			server.sin_port = 0;
		}
	}
	
	return(0);
}

static int handle_defuser(char *value) {

	if (default_user != NULL) {
		show_error("Default username may only be specified "
			   "once in configuration file\n");
	} else {
		default_user = strdup(value);
	}
		
	return(0);
}

static int handle_defpass(char *value) {

	if (default_pass != NULL) {
		show_error("Default password may only be specified "
			   "once in configuration file\n");
	} else {
		default_pass = strdup(value);
	}
	
	return(0);
}

static int handle_type(char *value) {

	if (servertype != 0) {
		show_error("Server type may only be specified once "
			   "in configuration file\n");
	} else {
		errno = 0;
		servertype = (int) strtol(value, (char **)NULL, 10);
		if ((errno != 0) || (servertype == 0) ||
		    ((servertype != 4) && (servertype != 5))) {
			show_error("%d %d\n", errno, servertype);
			show_error("Invalid server type "
				   "specified in configuration file "
				   "(%s) only 4 or 5 may be "
				   "specified\n", value);
			servertype = 0;
		}
	}
	
	return(0);
}

static int handle_local(char *value) {
	char *ip;
	char *subnet;
	char buf[100];
	char *split;
	struct socksent *ent;

	strncpy(buf, value, sizeof(buf) - 1);
	split = buf;
	ip = strsep(&split, "/");
	subnet = strsep(&split, " \n");

	if ((ip == NULL) || (subnet == NULL)) {
		show_error("Local network pair (%s) is not validly "
			   "constructed\n", value);
		return(1);
	} else {
		/* Allocate the new entry */
		if ((ent = (struct socksent *) malloc(sizeof(struct socksent)))
		   == NULL) {
			/* If we couldn't malloc some storage, leave */
			exit(-1);
		}

#if OSNAME == solaris 
		if ((ent->localip.s_addr = inet_addr(ip)) == -1) {
#else
		if (!(inet_aton(ip, &(ent->localip)))) {
#endif
			show_error("In configuration file IP for local "
				   "network (%s) is not valid\n", ip);
			free(ent);
			return(1);
		} 
#if OSNAME == solaris
		else if ((ent->localnet.s_addr = inet_addr(subnet)) == -1) {
#else
		else if (!(inet_aton(subnet, &(ent->localnet)))) {
#endif
			show_error("In configuration file SUBNET for " 
				   "local network (%s) is not valid\n", ip);
			free(ent);
			return(1);
		}
		else if ((ent->localip.s_addr &
			  ent->localnet.s_addr) != 
	                 ent->localip.s_addr) {
			show_error("In configuration file IP (%s) & "
				   "SUBNET (%s) != IP. Ignored.\n",
				   ip, subnet);
			free(ent);
			return(1);
		}
		else {
			/* The entry is valid so add it to linked list */
			ent -> next = localnets;
			localnets = ent;
		}			
	}

	return(0);
}
	
	
static int is_local(struct in_addr *testip) {
	struct socksent *ent;

	for (ent = localnets; ent != NULL; ent = ent -> next) {
		if ((testip->s_addr & ent->localnet.s_addr) ==
		    (ent->localip.s_addr & ent->localnet.s_addr))  {
			return(0);
		}
	}

	return(1);
}

#if OSNAME != solaris 
int connect (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len) {
#elif OSVER == 6
int connect (int __fd, struct sockaddr * __addr, int __len) {
#else
int connect (int __fd, const struct sockaddr * __addr, socklen_t __len) {
#endif
	struct sockaddr_in *connaddr;
#if OSNAME != solaris
	void **kludge;
#endif
	int sock_type = -1;
	int sock_type_len = sizeof(sock_type);

	/* If the real connect doesn't exist, we're stuffed */
	if (realconnect == NULL) {
		show_error("Unresolved symbol: connect\n");
		return(-1);
	}

	/* If we haven't initialized yet, do it now */
	if (initialized == 0) {
		initialize();
	}

	/* Ok, so this method sucks, but it's all I can think of */
#if OSNAME == solaris 
	connaddr = (struct sockaddr_in *) __addr;
#else
	kludge = (void *) & __addr; 
	connaddr = (struct sockaddr_in *) *kludge;
#endif

	/* Get the type of the socket */
	getsockopt(__fd, SOL_SOCKET, SO_TYPE, 
		   (void *) &sock_type, &sock_type_len);

	/* Only try and use socks if the socket is an INET socket, */
	/* is a TCP stream, isn't on a local subnet and the        */
	/* socks server in the config was valid                    */
	if ((connaddr->sin_family != AF_INET) ||
	    (sock_type != SOCK_STREAM) ||
	    (server.sin_family == 0) || 
	    !(is_local(&(connaddr->sin_addr)))) {
		return(realconnect(__fd, __addr, __len));
	} else {
		return(connect_socks(__fd, connaddr));
	} 

}

static int connect_socks(int sockid, struct sockaddr_in *connaddr) {
	int rc = 0;
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

	if (servertype == 4) 
		rc = connect_socksv4(sockid, connaddr);
	else
		rc = connect_socksv5(sockid, connaddr);

	/* If the socket was in non blocking mode, restore that */
	if ((sockflags & O_NONBLOCK) != 0) {
		fcntl(sockid, F_SETFL, sockflags);
	}

	if (rc != 0) {
		errno = rc;
		return(-1);
	}

	return(0);   

}			

static int connect_socksv4(int sockid, struct sockaddr_in *connaddr) {
	int rc = 0;
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
	if ((rc = realconnect(sockid, (struct sockaddr *) &server, 
		              sizeof(struct sockaddr_in))) != 0) {
		show_error("Error %d attempting to connect to SOCKS "
				   "server\n", errno);
		rc = rc;
	} else {
		rc = send(sockid, (void *) thisreq, length,0);
		if (rc < 0) {
			show_error("Error %d attempting to send SOCKS "
				   "request\n", errno);
			rc = rc;
		} else if ((rc = recv(sockid, (void *) &thisrep, 
				      sizeof(struct sockrep), 0)) < 0) {
			show_error("Error %d attempting to receive SOCKS "
				   "reply\n", errno);
			rc = ECONNREFUSED;
		} else if (rc < sizeof(struct sockrep)) {
			show_error("Short reply from SOCKS server\n");
			/* Let the application try and see how they */
			/* go                                       */
			rc = 0;
		} else if (thisrep.result == 91) {
			show_error("SOCKS server refused connection\n");
			rc = ECONNREFUSED;
		} else if (thisrep.result == 92) {
			show_error("SOCKS server refused connection "
				   "because of failed connect to identd "
				   "on this machine\n");
			rc = ECONNREFUSED;
		} else if (thisrep.result == 93) {
			show_error("SOCKS server refused connection "
				   "because identd and this library "
				   "reported different user-ids\n");
			rc = ECONNREFUSED;
		} else {
			rc = 0;
		}
	}

	/* Free the SOCKS request storage that was malloced */
	free(realreq);

	return(rc);   

}			

static int connect_socksv5(int sockid, struct sockaddr_in *connaddr) {
	int rc = 0;
	int offset = 0;
	char *verstring = "\x05\x02\x02\x00";
	char *uname, *upass;
	struct passwd *nixuser;
	char buf[200];

	/* Connect this socket to the socks server */
	if ((rc = realconnect(sockid, (struct sockaddr *) &server, 
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
		    ((uname = default_user) == NULL) &&
		    ((uname = (nixuser == NULL ? NULL : nixuser->pw_name)) == NULL)) {
			show_error("Could not get SOCKS username from "
				   "local passwd file, tsocks.conf "
				   "or $TSOCKS_USERNAME to authenticate "
				   "with"); 
			rc = ECONNREFUSED;
			return(rc);
		} 

		if (((upass = getenv("TSOCKS_PASSWORD")) == NULL) &&
		    ((upass = default_pass) == NULL)) {
			show_error("Need a password in tsocks.conf or "
				   "$TSOCKS_PASSWORD to authenticate with");
			rc = ECONNREFUSED;
			return(rc);
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

#ifdef SOCKSDNS
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

static void show_error(char *fmt, ...) {
#ifdef DEBUG
	va_list ap;
	int saveerr;
	char *newfmt;


	if ((newfmt = malloc(strlen(fmt) + strlen(MYNAME) + 1)) == NULL) {
		/* Could not malloc, bail */
		exit(1);
	}
	
	strcpy(newfmt, MYNAME);
	strcpy(newfmt + strlen(MYNAME), fmt);

	va_start(ap, fmt);

	/* Save errno */
	saveerr = errno;

	vfprintf(stderr, newfmt, ap);
	
	errno = saveerr;

	va_end(ap);

	free(newfmt);
#endif
}
	
#if OSNAME == solaris 
/* A bug for bug copy of strsep for Linux (oh, but I wrote this one) */
static char *strsep(char **text, const char *search) {
        int len;
        char *ret;

        ret = *text;

	if (*text == NULL) {
		return(NULL);
	} else {
        	len = strcspn(*text, search);
	        if (len == strlen(*text)) {
	                *text = NULL;
	        } else {
	                *text = *text + len;
	                **text = '\0';
	                *text = *text + 1;
	        }
	}
        return(ret);
}
#endif
