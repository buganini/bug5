#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	script
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
