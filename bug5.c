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
#ifdef __linux
#define __dead2 __attribute__ ((noreturn))
#include <pty.h>
#include <time.h>
#include <utmp.h>
#else
#include <libutil.h>
#endif
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
void detect_ambipad(void);

char ambipad=0;
int col=0, row=0;
char obuf[BUFSIZ];
char ibuf[BUFSIZ];

int
main(int argc, char *argv[])
{
	int cc=0;
	struct termios rtt, stt;
	struct winsize win;
	int ch, n;
	struct timeval tv, *tvp;
	time_t tvec, start;
	fd_set rfd;
	int flushtime = 30;
	int readstdin;
	int sw=0;
	char *icv=NULL, *ocv=NULL;
	int cus_i=0, cus_o=0;
	char utf8=0;
	char *_u2b[]={
	/*      */		"utf-8,00,byte:zhtw:big5,cp950_trans,00,any#3f",
	/*    p */		"utf-8,00,byte:zhtw:ambiguous-unpad:big5,cp950_trans,00,any#3f",
	/*   u  */		"utf-8,00,byte:zhtw:big5,cp950_trans,uao,00,any#3f",
	/*   up */		"utf-8,00,byte:zhtw:ambiguous-unpad:big5,cp950_trans,uao,00,any#3f",
	/*  t   */		"utf-8,00,byte:zhtw:zhtw_words:big5,cp950_trans,00,any#3f",
	/*  t p */		"utf-8,00,byte:zhtw:zhtw_words:ambiguous-unpad:big5,cp950_trans,00,any#3f",
	/*  tu  */		"utf-8,00,byte:zhtw:zhtw_words:big5,cp950_trans,uao,00,any#3f",
	/*  tup */		"utf-8,00,byte:zhtw:zhtw_words:ambiguous-unpad:big5,cp950_trans,uao,00,any#3f",
	/* g    */		"utf-8,00,byte:zhcn:gbk,cp936_trans,00,any#3f",
	/* g  p */		"utf-8,00,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,00,any#3f",
	/* g u  */		"utf-8,00,byte:zhcn:gbk,cp936_trans,00,any#3f",
	/* g up */		"utf-8,00,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,00,any#3f",
	/* gt   */		"utf-8,00,byte:zhcn:gbk,cp936_trans,00,any#3f",
	/* gt p */		"utf-8,00,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,00,any#3f",
	/* gtu  */		"utf-8,00,byte:zhcn:gbk,cp936_trans,00,any#3f",
	/* gtup */		"utf-8,00,byte:zhcn:ambiguous-unpad:gbk,cp936_trans,00,any#3f",
	};
	char *_b2u[]={
	/*      */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:utf-8,pass#for=1b",
	/*    p */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:ambiguous-pad:utf-8,pass#for=1b",
	/*   u  */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:utf-8,pass#for=1b",
	/*   up */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:ambiguous-pad:utf-8,pass#for=1b",
	/*  t   */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:zhcn:utf-8,pass#for=1b",
	/*  t p */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:zhcn:ambiguous-pad:utf-8,pass#for=1b",
	/*  tu  */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:zhcn:utf-8,pass#for=1b",
	/*  tup */		"ansi-control,byte:big5-defrag:byte,pass#mark&for=1b|pass#unmark,big5:zhcn:ambiguous-pad:utf-8,pass#for=1b",
	/* g    */		"gbk:utf-8",
	/* g  p */		"gbk:ambiguous-pad:utf-8",
	/* g u  */		"gbk:utf-8",
	/* g up */		"gbk:ambiguous-pad:utf-8",
	/* gt   */		"gbk:zhtw:zhtw_words:utf-8",
	/* gt p */		"gbk:zhtw:zhtw_words:ambiguous-pad:utf-8",
	/* gtu  */		"gbk:zhtw:zhtw_words:utf-8",
	/* gtup */		"gbk:zhtw:zhtw_words:ambiguous-pad:utf-8",
	};

	locale="zh_TW.Big5";

	char *posixly_correct=getenv("POSIXLY_CORRECT");
	setenv("POSIXLY_CORRECT","",1);
	while ((ch = getopt(argc, argv, "gptu8i:o:l:s:")) != -1)
		switch(ch) {
		case '8':
			utf8=1;
			locale="en_US.UTF-8";
			break;
		case 'p':
			sw |= 1;
			if(ambipad<2);
				ambipad+=1;
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
		case 's':
			if(sscanf(optarg, "%dx%d", &col, &row)!=2)
				usage();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if(posixly_correct)
		setenv("POSIXLY_CORRECT",posixly_correct,1);
	else
		unsetenv("POSIXLY_CORRECT");

	if(icv==NULL)
		icv=_u2b[sw];
	if(ocv==NULL)
		ocv=_b2u[sw];

	if(utf8){
		if(sw & 1){
			icv="utf-8,00:ambiguous-unpad:utf-8,00";
			ocv="utf-8:ambiguous-pad:utf-8";
		}else{
			icv="utf-8,00:utf-8,00";
			ocv="utf-8:utf-8";
		}
	}

	u2b=bsdconv_create(icv);
	if(u2b==NULL){
		if(cus_i){
			fprintf(stderr, "Failed:  %s: %s\n", bsdconv_error(), icv);
		}else{
			fprintf(stderr,"Failed creating bsdconv instance, please make sure you are running latest release of bug5 and bsdconv.\n");
		}
		exit(1);
	}
	b2u=bsdconv_create(ocv);
	if(b2u==NULL){
		if(cus_o){
			fprintf(stderr, "Failed:  %s: %s\n", bsdconv_error(), ocv);
		}else{
			fprintf(stderr,"Failed creating bsdconv instance, please make sure you are running latest release of bug5 and bsdconv.\n");
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
		if(col!=0)
			win.ws_col=col;
		if(row!=0)
			win.ws_row=row;
		cc=win.ws_row;
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

	detect_ambipad();

	signal(SIGINT, &sigforwarder);
	signal(SIGQUIT, &sigforwarder);
	signal(SIGPIPE, &sigforwarder);
#ifndef __linux
	signal(SIGINFO, &sigforwarder);
#endif
	signal(SIGUSR1, &sigforwarder);
	signal(SIGUSR2, &sigforwarder);
	signal(SIGWINCH, &winchforwarder);

	start = tvec = time(0);
	readstdin = 1;

	if(cc)
		write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[%d;%dr", 1, cc));
	else
		write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[r"));
	write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[H\033[2J"));
	for (;;) {
		FD_ZERO(&rfd);
		FD_SET(master, &rfd);
		if (readstdin)
			FD_SET(STDIN_FILENO, &rfd);
		if (!readstdin && ttyflg) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			tvp = &tv;
			readstdin = 1;
		} else if (flushtime > 0) {
			tv.tv_sec = flushtime - (tvec - start);
			tv.tv_usec = 0;
			tvp = &tv;
		} else {
			tvp = NULL;
		}
		n = select(master + 1, &rfd, 0, 0, tvp);
		if (n < 0 && errno != EINTR)
			break;
		if (n > 0 && FD_ISSET(STDIN_FILENO, &rfd)) {
			cc = read(STDIN_FILENO, ibuf, BUFSIZ);
			if (cc < 0)
				break;
			if (cc == 0) {
				if (tcgetattr(master, &stt) == 0 && (stt.c_lflag & ICANON) != 0) {
					(void)write(master, &stt.c_cc[VEOF], 1);
				}
				readstdin = 0;
			}
			if (cc > 0) {
				u2b->input.data=ibuf;
				u2b->input.len=cc;
				u2b->input.flags=0;
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

	detect_ambipad();

	ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	if(col!=0)
		win.ws_col=col;
	if(row!=0)
		win.ws_row=row;
	write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[%d;%dr", 1, win.ws_row));
	ioctl(master, TIOCSWINSZ, &win);
	kill(child, sig);
}


void
detect_ambipad(void){
	int r, c, r0, c0, r1, c1;
	if(ambipad==1){
		printf("\033[6n");
		fflush(stdout);
		scanf("\033[%d;%dR", &r, &c);
		printf("\033[H\033[6n");
		fflush(stdout);
		scanf("\033[%d;%dR", &r0, &c0);
		printf("\xe2\x96\xbd");
		printf("\033[6n");
		fflush(stdout);
		scanf("\033[%d;%dR", &r1, &c1);
		printf("\033[H \033[%d;%dH", r, c);

		r=c1-c0;
		if(r==1){
			bsdconv_ctl(b2u, BSDCONV_AMBIGUOUS_PAD, NULL, 1);
			bsdconv_ctl(u2b, BSDCONV_AMBIGUOUS_PAD, NULL, 1);
		}else if(r==2){
			bsdconv_ctl(b2u, BSDCONV_AMBIGUOUS_PAD, NULL, 0);
			bsdconv_ctl(u2b, BSDCONV_AMBIGUOUS_PAD, NULL, 0);
		}
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: bug5 [-8gptus] [-i conversion] [-o conversion] [-l locale] [command ...]\n"
	    "\t -8\tUTF-8 based profile\n"
	    "\t -g\tGBK based profile\n"
	    "\t -p\tauto pad ambiguous-width characters\n"
	    "\t -pp\talways pad ambiguous-width characters\n"
	    "\t -t\tconversion for traditional/simplified chinese\n"
	    "\t -u\tallow using UAO (no operation with -g)\n"
	    "\t -i\tspecify input conversion\n"
	    "\t -o\tspecify output conversion\n"
	    "\t -l\tset LC_CTYPE before executing program\n"
	    "\t -s\tterminal size (eg. 80x24), 0=auto\n"
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
	struct winsize win;

	if (ttyflg)
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
	bsdconv_destroy(b2u);
	bsdconv_destroy(u2b);
	(void)close(master);
	write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[r"));
	ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	write(STDOUT_FILENO, obuf, sprintf(obuf, "\033[%dH", win.ws_row));
	putchar('\n');
	exit(eno);
}
