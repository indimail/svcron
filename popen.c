/*
 * Copyright (c) 1988, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * this came out of the ftpd sources; it's been modified to avoid the
 * globbing stuff since we don't need it. also execvp instead of execv.
 */

#include <strerr.h>
#include <qprintf.h>
#include <subfd.h>
#include <substdio.h>
#include "cron.h"

#ifndef lint
#if 0
static          sccsid[] = "@(#)popen.c	8.3 (Berkeley) 4/6/94";
#else
static char     rcsid[] = "$Id: popen.c,v 1.1 2024-06-09 01:04:23+05:30 Cprogrammer Exp mbhangui $";
#endif
#endif /* not lint */

#define FATAL "svcron: fatal: "
#define WARN  "svcron: warn: "

#define MAX_ARGV	100
#define MAX_GARGV	1000

/*
 * Special version of popen which avoids call to shell. This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */
static pid_t   *pids;
static int      fds;

FILE           *
svcron_popen(char *program, char *type, struct passwd *pw, pid_t *mailpid)
{
	char           *cp;
	FILE           *iop;
	int             argc, pdes[2];
	pid_t           pid;
	char           *argv[MAX_ARGV];
#ifndef LOGIN_CAP
	uid_t           uid1, uid2;
#endif

	*mailpid = -1;
	if ((*type != 'r' && *type != 'w') || type[1] != '\0')
		return (NULL);

	if (!pids) {
		if ((fds = sysconf(_SC_OPEN_MAX)) <= 0)
			return (NULL);
		if (!(pids = (pid_t *) malloc((size_t) (fds * sizeof (pid_t)))))
			return (NULL);
		bzero(pids, fds * sizeof (pid_t));
	}
	if (pipe(pdes) < 0)
		return (NULL);

	/*- break up string into pieces */
	for (argc = 0, cp = program; argc < MAX_ARGV - 1; cp = NULL) {
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;
	}
	argv[MAX_ARGV - 1] = NULL;

	switch (pid = vfork())
	{
	case -1: /* error */
		close(pdes[0]);
		close(pdes[1]);
		return (NULL);
		/*- NOTREACHED */
	case 0:	/* child */
		if (pw) {
#ifdef LOGIN_CAP
			if (setusercontext(0, pw, pw->pw_uid, LOGIN_SETALL) < 0)
				strerr_die4sys(111, FATAL, "setusercontext failed for ", pw->pw_name, ": ");
#else
			uid1 = getuid();
			uid2 = geteuid();
			if (uid1 != uid2 || !uid1 || !uid2) {
				if (setgid(pw->pw_gid) < 0 || initgroups(pw->pw_name, pw->pw_gid) < 0)
					strerr_die4sys(111, FATAL, "unable to set groups for ", pw->pw_name, ": ");
#if (defined(BSD)) && (BSD >= 199103)
				setlogin(pw->pw_name);
#endif /* BSD */
				if (setuid(pw->pw_uid))
					strerr_die4sys(111, FATAL, "unable to set uid for ", pw->pw_name, ": ");
			}
#endif /* LOGIN_CAP */
		}	/*- if (pw) */
		if (*type == 'r') {
			if (pdes[1] != STDOUT) {
				dup2(pdes[1], STDOUT);
				close(pdes[1]);
			}
			dup2(STDOUT, STDERR); /* stderr too! */
			close(pdes[0]);
		} else {
			if (pdes[0] != STDIN) {
				dup2(pdes[0], STDIN);
				close(pdes[0]);
			}
			close(pdes[1]);
		}
		execvp(argv[0], argv);
		_exit(1);
	} /*- switch (pid = vfork()) */
	*mailpid = pid;

	/*- parent; assume fdopen can't fail... */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		close(pdes[0]);
	}
	pids[fileno(iop)] = pid;
	return (iop);
}

int
svcron_pclose(FILE *iop)
{
	int             fdes;
	pid_t           pid;
	int             status;
	sigset_t        sigset, osigset;

	/*-
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, or, if already `pclosed'.
	 */
	if (pids == 0 || pids[fdes = fileno(iop)] == 0)
		return (-1);
	fclose(iop);
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGHUP);
	sigprocmask(SIG_BLOCK, &sigset, &osigset);
	while ((pid = waitpid(pids[fdes], &status, 0)) < 0 && errno == EINTR)
		continue;
	if (verbose && substdio_flush(subfderr) == -1)
		strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
	sigprocmask(SIG_SETMASK, &osigset, NULL);
	pids[fdes] = 0;
	if (pid < 0)
		return (pid);
	if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
		if (verbose)
			subprintf(subfderr, "%s: %-10s pid %10d %s by signal %d\n",
					ProgramName, "mail", pid, WIFSTOPPED(status) ? "stopped" : "started",
					WIFSTOPPED(status) ? WSTOPSIG(status) : SIGCONT);
		return (1);
	} else
	if (WIFSIGNALED(status)) {
		if (verbose)
			subprintf(subfderr, "%s: %-10s pid %10d killed by signal %d\n",
					ProgramName, "mail", pid, WTERMSIG(status));
		return (1);
	} else
	if (WIFEXITED(status)) {
		if (verbose)
			subprintf(subfderr, "%s: %-10s pid %10d: normal exit return status %d\n",
					ProgramName, "mail", pid, WEXITSTATUS(status));
		return (WEXITSTATUS(status));
	}
	return (1);
}

void
getversion_popen_c()
{
	const char     *x = rcsid;
	x++;
}

/*-
 * $Log: popen.c,v $
 * Revision 1.1  2024-06-09 01:04:23+05:30  Cprogrammer
 * Initial revision
 *
 */
