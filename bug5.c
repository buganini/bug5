/*
 * Copyright (c) 2011 buganini@gmail.com . All rights reserved.
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <bsdconv.h>

char *locale;
static int master, slave;
static int child;
static int qflg, ttyflg;

static struct termios tt;

static struct bsdconv_instance *b2u;
static struct bsdconv_instance *u2b;

static void done(int) __dead2;
static void doshell(char **);
static void fail(void);
static void finish(void);
static void usage(void);
void sigforwarder(int);
void winchforwarder(int);

int
main(int argc, char *argv[])
{
	int cc;
	struct termios rtt;
	struct winsize win;
	int ch, n;
	struct timeval tv, *tvp;
	time_t tvec, start;
	char obuf[BUFSIZ];
	char ibuf[BUFSIZ];
	fd_set rfd;
	int flushtime = 30;
	int sw=0;
	char *icv=NULL, *ocv=NULL;
	int cus_i=0, cus_o=0;
	char *_u2b[]={
	/*      */		"utf-8,ascii,nul,byte:zhtw:big5,cp950_trans,ascii,nul,3f",
	/*    p */		"utf-8,ascii,nul,byte:zhtw:ambiguous-unpad:big5,cp950_trans,ascii,nul,3f",
	/*   u  */		"utf-8,ascii,nul,byte:zhtw:big5,cp950_trans,uao,ascii,nul,3f",
	/*   up */		"utf-8,ascii,nul,byte:zhtw:ambiguous-unpad:big5,cp950_trans,uao,ascii,nul,3f",
	/*  t   */		"utf-8,ascii,nul,byte:zhtw:zhtw_words:big5,cp950_trans,ascii,nul,3f",
	/*  t p */		"utf-8,ascii,nul,byte:zhtw:zhtw_words:ambiguous-unpad:big5,cp950_trans,ascii,nul,3f",
	/*  tu  */		"utf-8,ascii,nul,byte:zhtw:zhtw_words:big5,cp950_trans,uao,ascii,nul,3f",
	/*  tup */		"utf-8,ascii,nul,byte:zhtw:zhtw_words:ambiguous-unpad:big5,cp950_trans,uao,ascii,nul,3f",
	/* g    */		"utf-8,ascii,nul,byte:zhcn:gbk,cp936_trans,ascii,nul,3f",
	/* g  p */		"utf-8,ascii,nul,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,ascii,nul,3f",
	/* g u  */		"utf-8,ascii,nul,byte:zhcn:gbk,cp936_trans,ascii,nul,3f",
	/* g up */		"utf-8,ascii,nul,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,ascii,nul,3f",
	/* gt   */		"utf-8,ascii,nul,byte:zhcn:gbk,cp936_trans,ascii,nul,3f",
	/* gt p */		"utf-8,ascii,nul,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,ascii,nul,3f",
	/* gtu  */		"utf-8,ascii,nul,byte:zhcn:gbk,cp936_trans,ascii,nul,3f",
	/* gtup */		"utf-8,ascii,nul,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,ascii,nul,3f",
	};
	char *_b2u[]={
	/*      */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:utf-8,ascii,bsdconv_raw",
	/*    p */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:ambiguous-pad:utf-8,ascii,bsdconv_raw",
	/*   u  */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:utf-8,ascii,bsdconv_raw",
	/*   up */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:ambiguous-pad:utf-8,ascii,bsdconv_raw",
	/*  t   */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:zhcn:utf-8,ascii,bsdconv_raw",
	/*  t p */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:zhcn:ambiguous-pad:utf-8,ascii,bsdconv_raw",
	/*  tu  */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:zhcn:utf-8,ascii,bsdconv_raw",
	/*  tup */		"ansi-control,byte:big5-defrag:byte,ansi-control|skip,big5,ascii:zhcn:ambiguous-pad:utf-8,ascii,bsdconv_raw",
	/* g    */		"gbk,ascii:utf-8,ascii",
	/* g  p */		"gbk,ascii:ambiguous-pad:utf-8,ascii",
	/* g u  */		"gbk,ascii:utf-8,ascii",
	/* g up */		"gbk,ascii:ambiguous-pad:utf-8,ascii",
	/* gt   */		"gbk,ascii:zhtw:zhtw_words:utf-8,ascii",
	/* gt p */		"gbk,ascii:zhtw:zhtw_words:ambiguous-pad:utf-8,ascii",
	/* gtu  */		"gbk,ascii:zhtw:zhtw_words:utf-8,ascii",
	/* gtup */		"gbk,ascii:zhtw:zhtw_words:ambiguous-pad:utf-8,ascii",
	};

	locale="zh_TW.Big5";
	
	while ((ch = getopt(argc, argv, "gptui:o:l:")) != -1)
		switch(ch) {
		case 'p':
			sw |= 1;
			break;
		case 'u':
			sw |= 1<<1;
			break;
		case 't':
			sw |= 1<<2;
			break;
		case 'g':
			sw |= 1<<3;
			locale="zh_CN.GBK";
			break;
		case 'i':
			icv=optarg;
			cus_i=1;
			break;
		case 'o':
			ocv=optarg;
			cus_o=1;
			break;
		case 'l':
			locale=optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if(icv==NULL)
		icv=_u2b[sw];
	if(ocv==NULL)
		ocv=_b2u[sw];

	u2b=bsdconv_create(icv);
	if(u2b==NULL){
		if(cus_i){
			fprintf(stderr, "Failed:  %s: %s\n", bsdconv_error(), icv);
		}else{
			fprintf(stderr,"Failed creating bsdconv instance, you may need to update bsdconv.\n");
		}
		exit(1);
	}
	b2u=bsdconv_create(ocv);
	if(b2u==NULL){
		if(cus_o){
			fprintf(stderr, "Failed:  %s: %s\n", bsdconv_error(), ocv);
		}else{
			fprintf(stderr,"Failed creating bsdconv instance, you may need to update bsdconv.\n");
		}
		bsdconv_destroy(u2b);
		exit(1);
	}
	bsdconv_init(b2u);
	bsdconv_init(u2b);

	if ((ttyflg = isatty(STDIN_FILENO)) != 0) {
		if (tcgetattr(STDIN_FILENO, &tt) == -1)
			err(1, "tcgetattr");
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) == -1)
			err(1, "ioctl");
		if (openpty(&master, &slave, NULL, &tt, &win) == -1)
			err(1, "openpty");
	} else {
		if (openpty(&master, &slave, NULL, NULL, NULL) == -1)
			err(1, "openpty");
	}

	if (!qflg) {
		tvec = time(NULL);
	}
	if (ttyflg) {
		rtt = tt;
		cfmakeraw(&rtt);
		rtt.c_lflag &= ~ECHO;
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);
	}

	child = fork();
	if (child < 0) {
		warn("fork");
		done(1);
	}
	if (child == 0)
		doshell(argv);
	close(slave);

	signal(SIGINT, &sigforwarder);
	signal(SIGQUIT, &sigforwarder);
	signal(SIGPIPE, &sigforwarder);
	signal(SIGINFO, &sigforwarder);
	signal(SIGUSR1, &sigforwarder);
	signal(SIGUSR2, &sigforwarder);
	signal(SIGWINCH, &winchforwarder);

	if (flushtime > 0)
		tvp = &tv;
	else
		tvp = NULL;

	start = time(0);
	FD_ZERO(&rfd);
	for (;;) {
		FD_SET(master, &rfd);
		FD_SET(STDIN_FILENO, &rfd);
		if (flushtime > 0) {
			tv.tv_sec = flushtime;
			tv.tv_usec = 0;
		}
		n = select(master + 1, &rfd, 0, 0, tvp);
		if (n < 0 && errno != EINTR)
			break;
		if (n > 0 && FD_ISSET(STDIN_FILENO, &rfd)) {
			cc = read(STDIN_FILENO, ibuf, BUFSIZ);
			if (cc < 0)
				break;
			if (cc == 0)
				(void)write(master, ibuf, 0);
			if (cc > 0) {
				u2b->input.data=ibuf;
				u2b->input.len=cc;
				u2b->input.flags=0;
//				u2b->flush=1;
				u2b->output_mode=BSDCONV_FD;
				u2b->output.data=(void *)(uintptr_t)master;
				bsdconv(u2b);
			}
		}
		if (n > 0 && FD_ISSET(master, &rfd)) {
			cc = read(master, obuf, sizeof (obuf));
			if (cc <= 0)
				break;	
			b2u->input.data=obuf;
			b2u->input.len=cc;
			b2u->input.flags=0;
//			b2u->flush=1;
			b2u->output_mode=BSDCONV_FD;
			b2u->output.data=(void *)(uintptr_t)STDOUT_FILENO;
			bsdconv(b2u);			
		}
		tvec = time(0);
		if (tvec - start >= flushtime) {
			start = tvec;
		}
	}
	finish();
	done(0);
}

void
sigforwarder(int sig)
{
	kill(child, sig);
}

void
winchforwarder(int sig)
{
	struct winsize win;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	ioctl(master, TIOCSWINSZ, &win);
	kill(child, sig);	
}


static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: bug5 [-gptu] [-i conversion] [-o conversion] [-l locale] [command ...]\n"
	    "\t -g\tGBK based profile\n"
	    "\t -p\tpad ambiguous-width characters\n"
	    "\t -t\tconversion for traditional/simplified chinese\n"
	    "\t -u\tallow using UAO (no operation with -g)\n"
	    "\t -i\tspecify input conversion\n"
	    "\t -o\tspecify output conversion\n"
	    "\t -l\tset LC_CTYPE before executing program\n"
	);
	exit(1);
}

static void
finish(void)
{
	int e, status;

	if (waitpid(child, &status, 0) == child) {
		if (WIFEXITED(status))
			e = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			e = WTERMSIG(status);
		else /* can't happen */
			e = 1;
		done(e);
	}
}

static void
doshell(char **av)
{
	const char *shell;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	(void)close(master);
	setenv("LC_CTYPE",locale,1);
	login_tty(slave);
	if (av[0]) {
		execvp(av[0], av);
		warn("%s", av[0]);
	} else {
		execl(shell, shell, "-i", (char *)NULL);
		warn("%s", shell);
	}
	fail();
}

static void
fail(void)
{
	(void)kill(0, SIGTERM);
	done(1);
}

static void
done(int eno)
{
	time_t tvec;

	if (ttyflg)
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
	tvec = time(NULL);
	bsdconv_destroy(b2u);
	bsdconv_destroy(u2b);
	(void)close(master);
	exit(eno);
}
