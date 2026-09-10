NAME	= libtbl
MAJOR	= 5
MINOR	= 0
VERSION	= ${MAJOR}.${MINOR}

# Installation Directories
PREFIX	= /usr/local
LIBDIR	= ${PREFIX}/lib
INCDIR	= ${PREFIX}/include
PCDIR	= ${LIBDIR}/pkgconfig

CFLAGS	+= -Os -std=gnu11 -Wall -Wextra
SHLIB	 = ${NAME}.so.${MAJOR}.${MINOR}
SHLIBS	 = ${SHLIB} ${NAME}.so.${MAJOR} ${NAME}.so
LIBS	 = ${NAME}.a ${SHLIBS}

all: ${LIBS} ${NAME}.pc
	${MAKE} -C examples all

clean:
	rm -f ${NAME}.a ${NAME}.so* ${NAME}.pc *.o
	${MAKE} -C test clean
	${MAKE} -C examples clean

check: all
	${MAKE} -C test check

install: ${LIBS} ${NAME}.pc
	mkdir -p ${DESTDIR}${LIBDIR} ${DESTDIR}${INCDIR} ${DESTDIR}${PCDIR}
	cp -f ${LIBS} ${DESTDIR}${LIBDIR}/
	cp -f ${NAME}.h ${DESTDIR}${INCDIR}/
	cp -f ${NAME}.pc ${DESTDIR}${PCDIR}/

${NAME}.pc: ${NAME}.pc.in Makefile
	sed -e 's|@PREFIX@|${PREFIX}|g'		\
	    -e 's|@LIBDIR@|${LIBDIR}|g'		\
	    -e 's|@INCDIR@|${INCDIR}|g'		\
	    -e 's|@VERSION@|${VERSION}|g'	\
	    ${NAME}.pc.in > $@

${NAME}.so ${NAME}.so.${MAJOR}: ${SHLIB}
	ln -sf ${SHLIB} $@

${SHLIB}: ${NAME}.h ${NAME}.c
	${CC} -shared -o $@ ${NAME}.c ${CFLAGS} ${LDFLAGS}

${NAME}.a: ${NAME}.o
	${AR} rcs $@ ${NAME}.o
