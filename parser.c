/*

   parser.c    - Parsing routines for tsocks.conf

*/

#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <config.h>
#include "common.h"
#include "parser.h"

/* Variables provided by parser module */
struct netent *localnets = NULL;
struct serverent defaultserver;
struct serverent *paths = NULL;

/* Global configuration variables */
#define MAXLINE         BUFSIZ             /* Max length of conf line  */
static struct serverent *currentcontext = NULL;

int read_config(char *);
int is_local(struct in_addr *);
int pick_server(struct serverent **, struct in_addr *); 
static int handle_line(char *, int);
static int check_server(struct serverent *);
static int tokenize(char *, int, char *[]);
static int handle_path(int, int, char *[]);
static int handle_endpath(int, int, char *[]);
static int handle_reaches(int, char *);
static int handle_server(int, char *);
static int handle_type(int, char *);
static int handle_port(int, char *);
static int handle_local(int, char *);
static int handle_defuser(int, char *);
static int handle_defpass(int, char *);
static void reset_serverent(struct serverent *);
#ifdef DEFINE_STRSEP
static char *strsep(char **, const char *);
#endif

int read_config (char *filename) {
	FILE *conf;
	char line[MAXLINE];
	int cleardefs = 0;
	int rc = 0;
	int lineno = 1;
	struct serverent *server;

	/* Initialize important structures */
        reset_serverent(&defaultserver);
        currentcontext = &defaultserver;

	/* If a filename wasn't provided, use the default */
	if (filename == NULL) {
		strncpy(line, CONF_FILE, sizeof(line) - 1);
		/* Insure null termination */
		line[sizeof(line) - 1] = (char) 0;
		filename = line;
	}

	/* Read the configuration file */
	if ((conf = fopen(filename, "r")) == NULL) {
		show_error("Could not open socks configuration file "
			   "(%s)\n", filename);
		rc = 1; /* Severe errors reading configuration */
	}	
	else {
		reset_serverent(&defaultserver);

		while (NULL != fgets(line, MAXLINE, conf)) {
			/* This line _SHOULD_ end in \n so we  */
			/* just chop off the \n and hand it on */
			if (strlen(line) > 0)
				line[strlen(line) - 1] = '\0';
			handle_line(line, lineno);
			lineno++;
		} 
		fclose(conf);

		/* Always add the 127.0.0.1/255.0.0.0 subnet to local */
		handle_local(0, "127.0.0.0/255.0.0.0");

		/* Check default server */
		check_server(&defaultserver);
		server = paths;
		while (server != NULL) {
			check_server(server);
			server = server->next;
		}

	}

	return(rc);
}

/* Check server entries (and establish defaults) */
static int check_server(struct serverent *server) {
	int cleardefs;

	/* Default to the default SOCKS port */
	if (server->port == 0) {
		server->port = htons(1080);
	}

	/* Default to SOCKS V4 */
	if (server->type == 0) {
		server->type = 4;
	}

	return(0);
}



static int handle_line(char *line, int lineno) {
	char *type;
	char *value;
	char *words[10];
	static char savedline[MAXLINE];
	int   nowords = 0, i;

	/* Save the input string */
	strncpy(savedline, line, MAXLINE - 1);
	savedline[MAXLINE - 1] = (char) 0;
	/* Tokenize the input string */
	nowords = tokenize(line, 10, words);	

	/* Set the spare slots to an empty string to simplify */
	/* processing                                         */
	for (i = nowords; i < 10; i++) 
		words[i] = "";

	if (nowords > 0) {
		/* Now this can either be a "path" block starter or */
		/* ender, otherwise it has to be a pair (<name> =   */
		/* <value>)                                         */
		if (!strcmp(words[0], "path")) {
			handle_path(lineno, nowords, words);
		} else if (!strcmp(words[0], "}")) {
			handle_endpath(lineno, nowords, words);
		} else {
			/* Has to be a pair */
			if ((nowords != 3) || (strcmp(words[1], "="))) {
				show_error("Malformed configuration pair "
					   "on line %d in configuration "
					   "file, \"%s\"\n", lineno, savedline);
			} else if (!strcmp(words[0], "reaches")) {
				handle_reaches(lineno, words[2]);
			} else if (!strcmp(words[0], "server")) {
				handle_server(lineno, words[2]);
			} else if (!strcmp(words[0], "server_port")) {
				handle_port(lineno, words[2]);
			} else if (!strcmp(words[0], "server_type")) {
				handle_type(lineno, words[2]);
			} else if (!strcmp(words[0], "default_user")) {
				handle_defuser(lineno, words[2]);
			} else if (!strcmp(words[0], "default_pass")) {
				handle_defpass(lineno, words[2]);
			} else if (!strcmp(words[0], "local")) {
				handle_local(lineno, words[2]);
			} else {
				show_error("Invalid pair type (%s) specified "
					   "on line %d in configuration file, "
					   "\"%s\"\n", words[0], lineno, 
					   savedline);
			}
		}
	}

	return(0);
}

/* This routines breaks up input lines into tokens  */
/* and places these tokens into the array specified */
/* by tokens                                        */
static int tokenize(char *line, int arrsize, char *tokens[]) {
	int tokenno = -1;
	int finished = 0;

	/* Strip any whitespace from the start of the string */
	line = line + strspn(line, " \t");
	while ((tokenno < (arrsize - 1)) && 
	       (*line != (char) 0) && 
	       (!finished)) {
		tokenno++;
		tokens[tokenno] = line;
		line = line + strcspn(line, " \t");
		*line = (char) 0;
		line++;

		/* We ignore everything after a # */
		if (*tokens[tokenno] == '#') { 
			finished = 1;
			tokenno--;
		}
	}

	return(tokenno + 1);
}

static int handle_path(int lineno, int nowords, char *words[]) {
	struct serverent *newserver;

	if ((nowords != 2) || (strcmp(words[1], "{"))) {
		show_error("Badly formed path open statement on line %d "
			   "in configuration file (should look like " 
			   "\"path {\")\n", lineno);
	} else if (currentcontext != &defaultserver) {
		/* You cannot nest path statements so check that */
		/* the current context is defaultserver          */
		show_error("Path statements cannot be nested on line %d "
			   "in configuration file\n", lineno);
	} else {
		/* Open up a new serverent, put it on the list   */
		/* then set the current context                  */
		if (((int) (newserver = (struct serverent *) malloc(sizeof(struct serverent)))) == -1) 
			exit(-1);	
	
		/* Initialize the structure */
		reset_serverent(newserver);
		newserver->next = paths;
		newserver->lineno = lineno;
		paths = newserver;
		currentcontext = newserver;
	}		
 
	return(0);
}

static int handle_endpath(int lineno, int nowords, char *words[]) {

	if (nowords != 1) {
		show_error("Badly formed path close statement on line "
			   "%d in configuration file (should look like "
			   "\"}\")\n", lineno);
	} else {
		currentcontext = &defaultserver;
	}

	/* We could perform some checking on the validty of data in */
	/* the completed path here, but thats what verifyconf is    */
	/* designed to do, no point in weighing down libtsocks      */

	return(0);
}

static int handle_reaches(int lineno, char *value) {
	int rc;
	struct netent *ent;

	rc = make_netent(value, &ent);
	switch(rc) {
		case 1:
			show_error("Local network pair (%s) is not validly "
				   "constructed in reach statement on line "
				   "%d in configuration "
				   "file\n", value, lineno);
			return(0);
			break;
		case 2:
			show_error("IP in reach statement "
				   "network pair (%s) is not valid on line "
				   "%d in configuration file\n", value, lineno);
			return(0);
			break;
		case 3:
			show_error("SUBNET in reach statement " 
				   "network pair (%s) is not valid on "
				   "line %d in configuration file\n", value, 
				   lineno);
			return(0);
			break;
		case 4:
			show_error("IP (%s) & ", inet_ntoa(ent->localip));
			show_error("SUBNET (%s) != IP on line %d in "
				   "configuration file, ignored\n",
				   inet_ntoa(ent->localnet));
			return(0);
	}

	/* The entry is valid so add it to linked list */
	ent -> next = currentcontext -> reachnets;
	currentcontext -> reachnets = ent;

	return(0);
}

static int handle_server(int lineno, char *value) {
	char *ip;
	unsigned int res;

	ip = strsep(&value, " ");

	/* We don't verify this ip/hostname at this stage, */
	/* its resolved immediately before use in tsocks.c */
	if (currentcontext->address == NULL) 
		currentcontext->address = strdup(ip);
	else {
		if (currentcontext == &defaultserver) 
			show_error("Only one default SOCKS server "
				   "may be specified at line %d in "
				   "configuration file\n", lineno);
		else 
			show_error("Only one SOCKS server may be specified "
				   "per path on lin %d in configuration "
				   "file. (Path begins on line %d)\n",
				   lineno, currentcontext->lineno);
	}

	return(0);
}

static int handle_port(int lineno, char *value) {

	if (currentcontext->port != 0) {
		if (currentcontext == &defaultserver) 
			show_error("Server port may only be specified "
				   "once for default server, at line %d "
				   "in configuration file\n", lineno);
		else 
			show_error("Server port may only be specified "
				   "once per path on line %d in configuration "
				   "file. (Path begins on line %d)\n",
				   lineno, currentcontext->lineno);
	} else {
		errno = 0;
		currentcontext->port = (unsigned short int)
				  htons(strtol(value, (char **)NULL, 10));
		if ((errno != 0) || (currentcontext->port == 0)) {
			show_error("Invalid server port number "
				   "specified in configuration file "
				   "(%s) on line %d\n", value, lineno);
			currentcontext->port = 0;
		}
	}
	
	return(0);
}

static int handle_defuser(int lineno, char *value) {

	if (currentcontext->defuser != NULL) {
		if (currentcontext == &defaultserver) 
			show_error("Default username may only be specified "
				   "once for default server, at line %d "
				   "in configuration file\n", lineno);
		else 
			show_error("Default username may only be specified "
				   "once per path on line %d in configuration "
				   "file. (Path begins on line %d)\n",
				   lineno, currentcontext->lineno);
	} else {
		currentcontext->defuser = strdup(value);
	}
		
	return(0);
}

static int handle_defpass(int lineno, char *value) {

	if (currentcontext->defpass != NULL) {
		if (currentcontext == &defaultserver) 
			show_error("Default password may only be specified "
				   "once for default server, at line %d "
				   "in configuration file\n", lineno);
		else 
			show_error("Default password may only be specified "
				   "once per path on line %d in configuration "
				   "file. (Path begins on line %d)\n",
				   lineno, currentcontext->lineno);
	} else {
		currentcontext->defpass = strdup(value);
	}
	
	return(0);
}

static int handle_type(int lineno, char *value) {

	if (currentcontext->type != 0) {
		if (currentcontext == &defaultserver) 
			show_error("Server type may only be specified "
				   "once for default server, at line %d "
				   "in configuration file\n", lineno);
		else 
			show_error("Server type may only be specified "
				   "once per path on line %d in configuration "
				   "file. (Path begins on line %d)\n",
				   lineno, currentcontext->lineno);
	} else {
		errno = 0;
		currentcontext->type = (int) strtol(value, (char **)NULL, 10);
		if ((errno != 0) || (currentcontext->type == 0) ||
		    ((currentcontext->type != 4) && (currentcontext->type != 5))) {
			show_error("Invalid server type (%s) "
				   "specified in configuration file "
				   "on line %d, only 4 or 5 may be "
				   "specified\n", value, lineno);
			currentcontext->type = 0;
		}
	}
	
	return(0);
}

static int handle_local(int lineno, char *value) {
	int rc;
	struct netent *ent;

	if (currentcontext != &defaultserver) {
		show_error("Local networks cannot be specified in path "
			   "block at like %d in configuration file. "
			   "(Path block started at line %d)\n",
			   lineno, currentcontext->lineno);
		return(0);
	}

	rc = make_netent(value, &ent);
	switch(rc) {
		case 1:
			show_error("Local network pair (%s) is not validly "
				   "constructed on line %d in configuration "
				   "file\n", value, lineno);
			return(0);
			break;
		case 2:
			show_error("IP for local "
				   "network pair (%s) is not valid on line "
				   "%d in configuration file\n", value, lineno);
			return(0);
			break;
		case 3:
			show_error("SUBNET for " 
				   "local network pair (%s) is not valid on "
				   "line %d in configuration file\n", value, 
				   lineno);
			return(0);
			break;
		case 4:
			show_error("IP (%s) & ", inet_ntoa(ent->localip));
			show_error("SUBNET (%s) != IP on line %d in "
				   "configuration file, ignored\n",
				   inet_ntoa(ent->localnet));
			return(0);
	}

	/* The entry is valid so add it to linked list */
	ent -> next = localnets;
	localnets = ent;

	return(0);
}

/* Construct a netent given a string like "198.126.0.1/255.255.255.0" */
int make_netent(char *value, struct netent **ent) {
	char *ip;
	char *subnet;
	static char buf[200];
	char *split;

	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = (char) 0;
	split = buf;
	ip = strsep(&split, "/");
	subnet = strsep(&split, " \n");

	if ((ip == NULL) || (subnet == NULL)) {
		/* Network pair not validly constructed */
		return(1);
	} else {
		/* Allocate the new entry */
		if ((*ent = (struct netent *) malloc(sizeof(struct netent)))
		   == NULL) {
			/* If we couldn't malloc some storage, leave */
			exit(1);
		}

#ifdef HAVE_INET_ADDR
		if (((*ent)->localip.s_addr = inet_addr(ip)) == -1) {
#elif defined(HAVE_INET_ATON)
		if (!(inet_aton(ip, &((*ent)->localip)))) {
#endif
			/* Badly constructed IP */
			free(*ent);
			return(2);
		}
#ifdef HAVE_INET_ADDR
		else if (((*ent)->localnet.s_addr = inet_addr(subnet)) == -1) {
#elif defined(HAVE_INET_ATON)
		else if (!(inet_aton(subnet, &((*ent)->localnet)))) {
#endif
			/* Badly constructed subnet */
			free(*ent);
			return(3);
		} else if (((*ent)->localip.s_addr &
			    (*ent)->localnet.s_addr) != 
	                   (*ent)->localip.s_addr) {
			/* Subnet and Ip != Ip */
			return(4);
		}
	}	

	return(0);
}

static void reset_serverent(struct serverent *ent) {

	ent->address = NULL;
	ent->port = 0;
	ent->type = 0;
	ent->defuser = (char *) 0;
	ent->defpass = (char *) 0;
	ent->reachnets = (struct netent *) 0;
	ent->lineno = 0;
	ent->next = (struct serverent *) 0;

}

int is_local(struct in_addr *testip) {
        struct netent *ent;

	for (ent = localnets; ent != NULL; ent = ent -> next) {
		if ((testip->s_addr & ent->localnet.s_addr) ==
		    (ent->localip.s_addr & ent->localnet.s_addr))  {
			return(0);
		}
	}

	return(1);
}

/* Find the appropriate server to reach an ip */
int pick_server(struct serverent **ent, struct in_addr *ip) {
	struct netent *net;	

	*ent = paths;
	while (*ent != NULL) {
		/* Go through all the servers looking for one */
		/* with a path to this network                */
		net = (*ent)->reachnets;
		while (net != NULL) {
			if ((ip->s_addr & net->localnet.s_addr) ==
		    	    (net->localip.s_addr & net->localnet.s_addr))  
				/* Found the net, return */
				return(0);
			net = net->next;
		}		
		(*ent) = (*ent)->next;
	}

	*ent = &defaultserver;

	return(0);
}
	
#ifdef DEFINE_STRSEP	
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
