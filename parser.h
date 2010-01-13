/* parser.h - Structures, functions and global variables for the */
/* tsocks parsing routines                                       */

#ifndef _PARSER_H

#define _PARSER_H	1

/* Structure definitions */

/* Structure representing one server specified in the config */
struct serverent {
	char *address; /* Address/hostname of server */
	int port; /* Port number of server */
	int type; /* Type of server (4/5) */
	char *defuser; /* Default username for this socks server */
	char *defpass; /* Default password for this socks server */
	struct netent *reachnets; /* Linked list of nets from this server */
	int lineno; /* Line number in conf file this path started on */
	struct serverent *next; /* Pointer to next server entry */
};

/* Structure representing a network */
struct netent {
        struct in_addr localip; /* Base IP of the network */
        struct in_addr localnet; /* Mask for the network */
	struct netent *next; /* Pointer to next network entry */
};

/* Variables provided by parser module */
extern struct netent *localnets;
extern struct serverent defaultserver;
extern struct serverent *paths;

/* Functions provided by parser module */
int read_config(char *);
int is_local(struct in_addr *);
int pick_server(struct serverent **, struct in_addr *);

#endif
