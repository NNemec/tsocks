prefix=/usr
LIBDIR=$(prefix)/lib
MANDIR=$(prefix)/man

SHCC = ${CC} -fPIC 
SAVE=saveme
INSPECT=inspectsocks
LIB_NAME=libtsocks
SHLIB_MAJOR=1
SHLIB_MINOR=6
SHLIB=${LIB_NAME}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
STATICLIB=${LIB_NAME}.a

CFLAGS=-O2 -Wall

SHOBJS = ${OBJS:.o=.so}

OBJS= tsocks.o

TARGETS= ${SHLIB} ${STATICLIB} ${UTIL_LIB} ${SAVE} ${INSPECT}

all: ${TARGETS}

${INSPECT}: ${INSPECT}.c
	${SHCC} ${CFFLAGS} -o ${INSPECT} ${INSPECT}.c

${SAVE}: ${SAVE}.c
	${SHCC} ${CFFLAGS} -static -o ${SAVE} ${SAVE}.c

${SHLIB}: ${SHOBJS}
	${SHCC} ${CFLAGS} -nostdlib -shared -o ${SHLIB} -Wl,-soname -Wl,${LIB_NAME}.so.${SHLIB_MAJOR} ${SHOBJS} ${DYNLIB_FLAGS} -ldl
	ln -sf ${SHLIB} ${LIB_NAME}.so

${STATICLIB}: ${OBJS}
	-rm -f ${STATICLIB}
	ar cr ${STATICLIB} ${OBJS}

%.so: %.c
	${SHCC} ${CFLAGS} -c ${CC_SWITCHES} $< -o $@

install: ${TARGETS}
	install ${STATICLIB} ${SHLIB} ${LIBDIR}
	ln -sf ${SHLIB} ${LIBDIR}/${LIB_NAME}.so.${SHLIB_MAJOR}
	ln -sf ${LIB_NAME}.so.${SHLIB_MAJOR} ${LIBDIR}/${LIB_NAME}.so
	ldconfig -n ${LIBDIR}
	install tsocks.3 ${MANDIR}/man3/


clean:
	-rm -f *.so *.so.* *.o *~ ${TARGETS}

