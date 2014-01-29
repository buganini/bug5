PREFIX?=/usr/local
LOCALBASE?=${PREFIX}
CFLAGS+=-Wall -I${LOCALBASE}/include
LDFLAGS+=-lbsdconv -lutil -L${LOCALBASE}/lib

all:
	$(CC) ${CFLAGS} bug5.c -o bug5 ${LDFLAGS}

install:
	install -m 555 bug5 ${PREFIX}/bin

clean:
	rm -f bug5
