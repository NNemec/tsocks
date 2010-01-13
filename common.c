/*

    commmon.c    - Common routines for the tsocks package 

*/

#include <config.h>
#include <stdio.h>
#include <netdb.h>
#include <common.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>

unsigned int resolve_ip(char *host, int showmsg, int allownames) {
	struct hostent *new;
	unsigned int	hostaddr;
	struct in_addr *ip;

	if ((hostaddr = inet_addr(host)) == (unsigned int) -1) {
		/* We couldn't convert it as a numerical ip so */
		/* try it as a dns name                        */
		if (allownames) {
			#ifdef HAVE_GETHOSTBYNAME
			if ((new = gethostbyname(host)) == (struct hostent *) 0) {
			#endif
				return(-1);
			#ifdef HAVE_GETHOSTBYNAME
			} else {
				ip = ((struct in_addr *) * new->h_addr_list);
				hostaddr = ip -> s_addr;
				if (showmsg) 
					printf("Connecting to %s...\n", inet_ntoa(*ip));
			}
			#endif
		} else
			return(-1);
	}

	return (hostaddr);
}

void show_error(char *fmt, ...) {
	va_list ap;
	int saveerr;
	char *newfmt;
	extern char *progname;

	/* If --disable-debug was specified at compile time or */
	/* TSOCKS_NO_DEBUG is set to 'yes' in the enviroment   */
	/* don't actually display this message                 */
	if ((getenv("TSOCKS_NO_DEBUG")) &&
	    (!strcmp(getenv("TSOCKS_NO_DEBUG"), "yes")))
		return;

	if ((newfmt = malloc(strlen(fmt) + strlen(progname) + 1)) == NULL) {
		/* Could not malloc, bail */
		exit(1);
	}
	
	strcpy(newfmt, progname);
	strcpy(newfmt + strlen(progname), fmt);

	va_start(ap, fmt);

	/* Save errno */
	saveerr = errno;

	vfprintf(stderr, newfmt, ap);
	
	errno = saveerr;

	va_end(ap);

	free(newfmt);
}

