/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Allow tsocks to generate messages to stderr when errors are
encountered, this is really important and should only be disabled if
you're REALLY sure. It can also be turned off at run time, see the man
page for details */
#define DEBUG 1

/* Use _GNU_SOURCE to define RTLD_NEXT, mostly for RH7 systems */
/* #undef USE_GNU_SOURCE */

/* dlopen() the old libc to get connect() instead of RTLD_NEXT, 
hopefully shouldn't be needed */
/* #undef USE_OLD_DLSYM */

/* libc location, needed if USE_OLD_DLSYM is enabled */
/* #undef LIBC */

/* Configure the system resolver to use TCP queries on startup, this
allows socksified DNS */
#define USE_SOCKS_DNS 1

/* Prototype and function header for connect function */
#define CONNECT_PROTOTYPE int connect(int, __CONST_SOCKADDR_ARG, socklen_t);
#define CONNECT_FUNCTION int connect(int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len) {
#define REALCONNECT_PROTOTYPE static int (*realconnect)(int, __CONST_SOCKADDR_ARG, socklen_t);

/* Work out which function we have for conversion from string IPs to 
numerical ones */
/* #undef HAVE_INET_ADDR */
#define HAVE_INET_ATON 1

/* Linux (and possibly other systems) use a union (including all the
different types of sockaddr structures as the argument to connect instead
of a typical generic sockaddr. We need to read the sockaddr_in from this
union on those systems */
#define USE_UNION 1

/* We use strsep which isn't on all machines, but we provide our own
definition of it for those which don't have it, this causes us to define
our version */
/* #undef DEFINE_STRSEP */

/* Allow the use of DNS names in the socks configuration file for socks
servers. This doesn't work if socksified DNS is enabled for obvious
reasons, it also introduces overhead, but people seem to want it */
#define HOSTNAMES 0

/* We need the gethostbyname() function to do dns lookups in tsocks or 
in inspectsocks */
#define HAVE_GETHOSTBYNAME 1

/* Location of configuration file (typically /etc/tsocks.conf) */
#define CONF_FILE "/etc/tsocks.conf" 

/* Define if you have the strcspn function.  */
#define HAVE_STRCSPN 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strspn function.  */
#define HAVE_STRSPN 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the dl library (-ldl).  */
#define HAVE_LIBDL 1

/* Define if you have the xnet library (-lxnet).  */
/* #undef HAVE_LIBXNET */
