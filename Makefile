CFLAGS+=-g -Wall -I/usr/local/include
LDFLAGS+=-lbsdconv -lutil -L/usr/local/lib

all:
	$(CC) ${CFLAGS} ${LDFLAGS} bug5.c -o bug5

clean:
	rm bug5
