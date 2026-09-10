EXAMPLES	= find	\
		  tree	\
		  netif
CFLAGS	+= -I..
LDFLAGS	+= -L.. -Wl,-Bstatic -ltbl -Wl,-Bdynamic

all: ${EXAMPLES}

clean:
	rm -f ${EXAMPLES}

.c:
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

${EXAMPLES}: bytes.h ../libtbl.h ../libtbl.a
