.SUFFIXES: .c .py
TESTS	= libtbl_example	\
	  libtbl_test

CFLAGS	+= -g -Og -I..
# Force libtbl to be linked statically
LDFLAGS	+= -L.. -Wl,-Bstatic -ltbl -Wl,-Bdynamic

all: check

clean:
	rm -f ${TESTS}

check: ${TESTS}
	@for t in ${TESTS}; do		\
		echo "$$ ./$$t";	\
		./$$t;			\
	done

.c:
	${CC} -o $@ $< ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}

.py:
	cp -f $< $@
	chmod 0755 $@
