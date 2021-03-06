DESTDIR?=
PREFIX?=/usr/local
LOCALBASE?=${PREFIX}
CFLAGS+=-Wall -I${LOCALBASE}/include -O2
LDFLAGS+=-lbsdconv -lutil -L${LOCALBASE}/lib

all:
	$(CC) ${CFLAGS} bug5.c -o bug5 ${LDFLAGS}

install:
	install -m 555 bug5 ${DESTDIR}${PREFIX}/bin

clean:
	rm -f bug5
