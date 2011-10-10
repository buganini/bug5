PREFIX?=/usr/local
CFLAGS+=-Wall -I${PREFIX}/include
LDFLAGS+=-lbsdconv -lutil -L${PREFIX}/lib

all:
	$(CC) ${CFLAGS} ${LDFLAGS} bug5.c -o bug5

install:
	install -m 555 bug5 ${PREFIX}/bin

clean:
	rm -f bug5
