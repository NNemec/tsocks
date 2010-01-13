/* accconfig.h -- `autoheader' will generate config.h.in for tsocks . */

/* Allow tsocks to generate messages to stderr when errors are
encountered, this is really important and should only be disabled if
you're REALLY sure. It can also be turned off at run time, see the man
page for details */
#undef DEBUG

/* Use _GNU_SOURCE to define RTLD_NEXT, mostly for RH7 systems */
#undef USE_GNU_SOURCE

/* dlopen() the old libc to get connect() instead of RTLD_NEXT, 
hopefully shouldn't be needed */
#undef USE_OLD_DLSYM

/* libc location, needed if USE_OLD_DLSYM is enabled */
#undef LIBC

/* Configure the system resolver to use TCP queries on startup, this
allows socksified DNS */
#undef USE_SOCKS_DNS

/* Prototype and function header for connect function */
#undef CONNECT_PROTOTYPE
#undef CONNECT_FUNCTION
#undef REALCONNECT_PROTOTYPE

/* Work out which function we have for conversion from string IPs to 
numerical ones */
#undef HAVE_INET_ADDR
#undef HAVE_INET_ATON

/* Linux (and possibly other systems) use a union (including all the
different types of sockaddr structures as the argument to connect instead
of a typical generic sockaddr. We need to read the sockaddr_in from this
union on those systems */
#undef USE_UNION

/* We use strsep which isn't on all machines, but we provide our own
definition of it for those which don't have it, this causes us to define
our version */
#undef DEFINE_STRSEP

/* Allow the use of DNS names in the socks configuration file for socks
servers. This doesn't work if socksified DNS is enabled for obvious
reasons, it also introduces overhead, but people seem to want it */
#define HOSTNAMES 0

/* We need the gethostbyname() function to do dns lookups in tsocks or 
in inspectsocks */
#undef HAVE_GETHOSTBYNAME

/* Location of configuration file (typically /etc/tsocks.conf) */
#undef CONF_FILE 
