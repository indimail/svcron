/*
 * Copyright (c) 1988,1990,1993,1994,2021 by Paul Vixie ("VIXIE")
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND VIXIE DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL VIXIE BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sig.h>
#include <substdio.h>
#include <subfd.h>
#include <fmt.h>
#include <strerr.h>
#include <qprintf.h>
#include <error.h>
#include "cron.h"
#define FATAL "svcron: fatal: "
#define WARN  "svcron: warn: "

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: do_command.c,v 1.1 2024-06-12 20:27:21+05:30 Cprogrammer Exp mbhangui $";
#endif

static void     child_process(const entry *, const user *);
static int      safe_p(const char *, const char *);

void
do_command(const entry *e, const user *u)
{
	/*
	 * fork to become asynchronous -- parent process is done immediately,
	 * and continues to run the normal svcron code, which means return to
	 * tick(). the child and grandchild don't leave this function, alive.
	 *
	 * vfork() is unsuitable, since we have much to do, and the parent
	 * needs to be able to run off and fork other processes.
	 */
	switch (fork())
	{
	case -1:
		strerr_die2sys(111, FATAL, "unable to produce a child: ");
		break;
	case 0:
		/* child process */
		child_process(e, u);
		_exit(OK_EXIT);
		break;
	default:
		/*- parent process */
		break;
	}
}

void
sigchld_reaper(char *ident)
{
	int             status;
	pid_t           pid;

	for (;(pid = waitpid(-1, &status, WNOHANG | WUNTRACED));) {
#ifdef ERESTART
		if (pid == -1 && (errno == error_intr || errno == error_restart))
#else
		if (pid == -1 && errno == error_intr)
#endif
			continue;
		if (pid == -1 && errno == error_child)
			break;
		if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
			if (verbose)
				if (subprintf(subfderr, "%s: %-10s pid %10d %s by signal %d\n",
						ProgramName, ident, pid, WIFSTOPPED(status) ? "stopped" : "started",
						WIFSTOPPED(status) ? WSTOPSIG(status) : SIGCONT) == -1)
					strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
			continue;
		} else
		if (WIFSIGNALED(status)) {
			if (verbose)
				if (subprintf(subfderr, "%s: %-10s pid %10d killed by signal %d\n",
						ProgramName, ident, pid, WTERMSIG(status)) == -1)
					strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
		} else
		if (WIFEXITED(status)) {
			if (verbose)
				if (subprintf(subfderr, "%s: %-10s pid %10d: normal exit return status %d\n",
						ProgramName, ident, pid, WEXITSTATUS(status)) == -1)
					strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
		}
		break;
	} /*- for (; pid = waitpid(-1, &status, WNOHANG | WUNTRACED);) -*/
	if (verbose && substdio_flush(subfderr) == -1)
		strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
}

static void
child_process(const entry *e, const user *u)
{
	int             stdin_pipe[2], stdout_pipe[2], r;
	char           *input_data, *usernm, *mailto, *x;
	int             children = 0;
	char            strnum[FMT_ULONG];
	pid_t           mailpid;
#ifndef LOGIN_CAP
	uid_t           uid1, uid2;
#endif

	/*- discover some useful and important environment settings */
	usernm = e->pwd->pw_name;
	mailto = myenv_get("MAILTO", e->envp);

	/*
	 * our parent is watching for our death by catching SIGCHLD. we
	 * do not care to watch for our childrens' deaths this way -- we
	 * use wait() explicitly. so we have to reset the signal (which
	 * was inherited from the parent).
	 */
	sig_childdefault();

	/* create some pipes to talk to our future child */
	if (pipe(stdin_pipe) == -1)
		strerr_die2sys(111, FATAL, "unable to create pipes for child's input: ");
	if (pipe(stdout_pipe) == -1)
		strerr_die2sys(111, FATAL, "unable to create pipes for child's output: ");

	/*
	 * since we are a forked process, we can modify the command string
	 * we were passed -- nobody else is going to use it again, right?
	 *
	 * if a % is present in the command, previous characters are the
	 * command, and subsequent characters are the additional input to
	 * the command. An escaped % will have the escape character stripped
	 * from it. Subsequent %'s will be transformed into newlines,
	 * but that happens later.
	 */
	/* local */ 
	{
		int             escaped = FALSE;
		int             ch;
		char           *p;

		for (input_data = p = e->cmd; (ch = *input_data) != '\0'; input_data++, p++) {
			if (p != input_data)
				*p = ch;
			if (escaped) {
				if (ch == '%')
					*--p = ch;
				escaped = FALSE;
				continue;
			}
			if (ch == '\\') {
				escaped = TRUE;
				continue;
			}
			if (ch == '%') {
				*input_data++ = '\0';
				break;
			}
		}
		*p = '\0';
	}

	/* fork again, this time so we can exec the user's command. */
	switch (vfork())
	{
	case -1:
		strerr_die2sys(111, FATAL, "unable to get a grandchild: ");
	case 0:
		/*
		 * write a log message. we've waited this long to do it
		 * because it was not until now that we knew the PID that
		 * the actual user command shell was going to get and the
		 * PID is part of the log message.
		 */
		if ((e->flags & DONT_LOG) == 0) {
			if (verbose && subprintf(subfderr, "%s: grandchild pid %10d: user %s command[", ProgramName, getpid(), usernm) == -1)
				strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
			for (x = e->cmd; *x; x++) {
				if (*x < ' ') { /*- control char */
					if (verbose && substdio_put(subfderr, "^", 1) == -1)
						strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
					*x += '@';
					if (verbose && substdio_put(subfderr, x, 1) == -1)
						strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
				} else
				if (*x < 0177) { /* printable */
					if (verbose && substdio_put(subfderr, x, 1) == -1)
						strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
				} else
				if (*x == 0177) { /* delete/rubout */
					if (verbose && substdio_put(subfderr, "^?", 2) == -1)
						strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
				} else
				if (verbose && subprintf(subfderr, "\\%03o", *x) == -1)
					strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
			}
			if (verbose && substdio_put(subfderr, "]\n", 2) == -1)
				strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
			if (verbose && substdio_flush(subfderr))
				strerr_die2sys(111, FATAL, "grandchild: unable to write to descriptor 2: ");
		}

		/* get new pgrp, void tty, etc. */
		if (setsid() == -1)
			strerr_die2sys(111, FATAL, "grandchild: setsid: ");

		/*
		 * close the pipe ends that we won't use. this doesn't affect
		 * the parent, who has to read and write them; it keeps the
		 * kernel from recording us as a potential client TWICE --
		 * which would keep it from sending SIGPIPE in otherwise
		 * appropriate circumstances.
		 */
		close(stdin_pipe[WRITE_PIPE]);
		close(stdout_pipe[READ_PIPE]);

		/*
		 * grandchild process. make std{in,out} be the ends of
		 * pipes opened by our daddy; make stderr go to stdout.
		 */
		if (stdin_pipe[READ_PIPE] != STDIN) {
			if (dup2(stdin_pipe[READ_PIPE], STDIN) == -1)
				strerr_die2sys(111, FATAL, "grandchild: unable to duplicate descriptors for descriptor 0: ");
			close(stdin_pipe[READ_PIPE]);
		}
		if (stdout_pipe[WRITE_PIPE] != STDOUT) {
			if (dup2(stdout_pipe[WRITE_PIPE], STDOUT) == -1)
				strerr_die2sys(111, FATAL, "grandchild: unable to duplicate descriptors for descriptor 1: ");
			close(stdout_pipe[WRITE_PIPE]);
		}
		if (dup2(STDOUT, STDERR) == -1)
			strerr_die2sys(111, FATAL, "grandchild: unable to duplicate descriptors for descriptor 2: ");

		/*
		 * set our directory, uid and gid. Set gid first, since once
		 * we set uid, we've lost root privledges.
		 */
#ifdef LOGIN_CAP
		{
#ifdef BSD_AUTH
			auth_session_t *as;
#endif
			login_cap_t    *lc;
			char          **p;
			extern char   **environ;

			if ((lc = login_getclass(e->pwd->pw_class)) == NULL)
				strerr_die3sys(111, FATAL, "grandchild: unable to get login class for ", e->pwd->pw_name);
			if (setusercontext(lc, e->pwd, e->pwd->pw_uid, LOGIN_SETALL) < 0)
				strerr_die3sys(111, FATAL, "grandchild: set user context failed for ", e->pwd->pw_name);
#ifdef BSD_AUTH
			if (!(as = auth_open()))
				die_nomem(FATAL);
			if (auth_setpwd(as, e->pwd) != 0)
				strerr_die3sys(111, FATAL, "grandchild: failed to get password entry for ", e->pwd->pw_name);
			if (auth_approval(as, lc, usernm, "cron") <= 0)
				strerr_die3sys(111, FATAL, "grandchild: failed to get approval for ", e->pwd->pw_name);
			auth_close(as);
#endif /* BSD_AUTH */
			login_close(lc);

			/*
			 * If no PATH specified in crontab file but
			 * we just added one via login.conf, add it to
			 * the crontab environment.
			 */
			if (env_get("PATH", e->envp) == NULL && environ != NULL) {
				for (p = environ; *p; p++) {
					if (strncmp(*p, "PATH=", 5) == 0) {
						e->envp = env_set(e->envp, *p);
						break;
					}
				}
			}
		}
#else
		uid1 = getuid();
		uid2 = geteuid();
		if (uid1 != uid2 || !uid1 || !uid2) {
			strnum[fmt_ushort(strnum, e->pwd->pw_gid)] = 0;
			if (setgid(e->pwd->pw_gid) == -1)
				strerr_die4sys(111, FATAL, "grandchild: setgid failed for gid ", strnum, ": ");
			if (initgroups(usernm, e->pwd->pw_gid) == -1)
				strerr_die4sys(111, FATAL, "grandchild: failed to set groups for ", usernm, ": ");
#if (defined(BSD)) && (BSD >= 199103)
			if (setlogin(usernm) == -1)
				strerr_die4sys(111, FATAL, "grandchild: failed to set login for ", usernm, ": ");
#endif							/* BSD */
			if (setuid(e->pwd->pw_uid) < 0)
				strerr_die4sys(111, FATAL, "grandchild: failed to set uid for ", usernm, ": ");
			/* we aren't root after this... */
		}

#endif	/* LOGIN_CAP */
		if (!(x = myenv_get("HOME", e->envp)))
			strerr_die2x(111, FATAL, "grandchild: HOME not set");
		if (chdir(x) == -1)
			strerr_die3sys(111, FATAL, "grandchild: chdir: ", x);

		/* Exec the command. */
		{
			char           *shell = myenv_get("SHELL", e->envp);

			if (!shell)
				strerr_die2x(111, FATAL, "grandchild: SHELL not set");
			execle(shell, shell, "-c", e->cmd, (char *) 0, e->envp);
			strerr_die3sys(111, FATAL, "grandchild: execle: ", shell);
		}
		break;
	default:
		/* parent process */
		break;
	}

	children++;

	/*
	 * middle process, child of original svcron, parent of process running
	 * the user's command.
	 *
	 * close the ends of the pipe that will only be referenced in the
	 * grandchild process...
	 */
	close(stdin_pipe[READ_PIPE]);
	close(stdout_pipe[WRITE_PIPE]);

	/*
	 * write, to the pipe connected to child's stdin, any input specified
	 * after a % in the crontab entry. while we copy, convert any
	 * additional %'s to newlines. when done, if some characters were
	 * written and the last one wasn't a newline, write a newline.
	 *
	 * Note that if the input data won't fit into one pipe buffer (2K
	 * or 4K on most BSD systems), and the child doesn't read its stdin,
	 * we would block here. thus we must fork again.
	 */
	r = 0;
	if (*input_data && (r = fork()) == 0) {
		substdio        ssout;
		char            ssoutbuf[BUFSIZE_OUT];
		int             need_newline = FALSE;
		int             escaped = FALSE;
		char            ch;

		/*
		 * close the pipe we don't use, since we inherited it and
		 * are part of its reference count now.
		 */
		close(stdout_pipe[READ_PIPE]);
		substdio_fdbuf(&ssout, write, stdin_pipe[WRITE_PIPE], ssoutbuf, sizeof(ssoutbuf));

		/*
		 * translation:
		 * \% -> %
		 * % -> \n
		 * \x -> \x for all x != %
		 */
		while ((ch = *input_data++) != '\0') {
			if (escaped) {
				if (ch != '%')
					substdio_put(&ssout, "\\", 1);
			} else
			if (ch == '%')
				ch = '\n';

			if (!(escaped = (ch == '\\'))) {
				substdio_put(&ssout, &ch, 1);
				need_newline = (ch != '\n');
			}
		}
		if (escaped)
			substdio_put(&ssout, "\\", 1);
		if (need_newline)
			substdio_put(&ssout, "\n", 1);

		/*
		 * close the pipe, causing an EOF condition.
		 */
		if (substdio_flush(&ssout) == -1)
			strerr_die2sys(111, FATAL, "unable to write to descriptor 1: ");
		close(stdin_pipe[WRITE_PIPE]);
		_exit(0);
	}
	if (r == -1)
		strerr_die2sys(111, FATAL, "unable to produce a child: ");

	/*
	 * close the pipe to the grandkiddie's stdin, since its wicked uncle
	 * ernie back there has it open and will close it when he's done.
	 */
	close(stdin_pipe[WRITE_PIPE]);

	children++;

	/*
	 * read output from the grandchild. its stderr has been redirected to
	 * it's stdout, which has been redirected to our pipe. if there is any
	 * output, we'll be mailing it to the user whose crontab this is...
	 * when the grandchild exits, we'll get EOF.
	 */

	/* local */
	{
		substdio        ssin;
		char            ssinbuf[BUFSIZE_IN];
		char            ch;
		int             c;

		substdio_fdbuf(&ssin, read, stdout_pipe[READ_PIPE], ssinbuf, sizeof(ssinbuf));

		c = substdio_get(&ssin, &ch, 1);
		if (c == 1) {
			FILE           *mail;
			int             bytes = 1;
			int             status = 0;

			/*
			 * get name of recipient. this is MAILTO if set to a
			 * valid local username; USER otherwise.
			 */
			if (mailto) { /* MAILTO was present in the environment */
				if (!*mailto) /* ... but it's empty. set to NULL */
					mailto = NULL;
			} else /*- MAILTO not present, set to USER. */
				mailto = usernm;

			/*- if the resulting mailto isn't safe, don't use it.  */
			if (mailto != NULL && !safe_p(usernm, mailto))
				mailto = NULL;

			/*
			 * if we are supposed to be mailing, MAILTO will
			 * be non-NULL.  only in this case should we set
			 * up the mail command and subjects and stuff...
			 */
			if (mailto != NULL) {
				char            mailcmd[MAX_COMMAND] = "";
				const char     *msg = NULL;

				if (Mailer != NULL) {
					if (strcountstr(Mailer, "%s") == 1) {
						if (strlens(Mailer, mailto, NULL) - strlen("%s") + sizeof "" > sizeof mailcmd)
							msg = "Mailer ovf 1";
						else
							(void) sprintf(mailcmd, Mailer, mailto);
					} else
					if (strlen(Mailer) + sizeof "" > sizeof mailcmd)
						msg = "Mailer ovf 2";
					else
						(void) strcpy(mailcmd, Mailer);
				} else
				if (strlens(MAILFMT, MAILARG, NULL) + sizeof "" > sizeof mailcmd)
					msg = "mailcmd too long";
				else
					(void) sprintf(mailcmd, MAILFMT, MAILARG);
				if (msg != NULL) {
					if (subprintf(subfderr, "%s %s\n", FATAL, msg) == -1)
						strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
					if (substdio_flush(subfderr) == -1)
						strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
					_exit(111);
				}
				if (!(mail = svcron_popen(mailcmd, "w", e->pwd, &mailpid))) {
					strerr_warn3(WARN, mailcmd, ": ", &strerr_sys);
					mailto = NULL;
				}
				if (verbose) {
					if (subprintf(subfderr, "%s: mail       pid %10d: user %s command[%s]\n",
							ProgramName, mailpid, usernm, mailcmd) == -1)
						strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
					if (substdio_flush(subfderr) == -1)
						strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
				}
			}

			/*
			 * if we succeeded in getting a mailer opened up,
			 * send the headers and first character of body.
			 */
			if (mailto != NULL) {
				char            hostname[MAXHOSTNAMELEN];
				char          **env;

				gethostname(hostname, MAXHOSTNAMELEN);
#ifdef MAIL_FROMUSER
				fprintf(mail, "From: %s\n", usernm);
#else
				fprintf(mail, "From: root (svcron Daemon)\n");
#endif
				fprintf(mail, "To: %s\n", mailto);
				fprintf(mail, "Subject: svcron <%s@%s> %s\n", usernm, first_word(hostname, "."), e->cmd);
#ifdef MAIL_DATE
				fprintf(mail, "Date: %s\n", arpadate(&StartTime));
#endif							/*MAIL_DATE */
				for (env = e->envp; *env; env++)
					fprintf(mail, "X-Cron-Env: <%s>\n", *env);
				fprintf(mail, "\n");

				/* this was the first char from the pipe */
				putc(ch, mail);
			}

			/*
			 * we have to read the input pipe no matter whether
			 * we mail or not, but obviously we only write to
			 * mail pipe if we ARE mailing.
			 */
			for (;;) {
				if (!(c = substdio_get(&ssin, &ch, 1)))
					break;
				bytes++;
				if (mailto != NULL)
					putc(ch, mail);
			}

			/*
			 * only close pipe if we opened it -- i.e., we're
			 * mailing...
			 */

			if (mailto != NULL) {
				/*
				 * Note: the pclose will probably see
				 * the termination of the grandchild
				 * in addition to the mail process, since
				 * it (the grandchild) is likely to exit
				 * after closing its stdout.
				 */
				status = svcron_pclose(mail);
			}

			/*
			 * if there was output and we could not mail it,
			 * log the facts so the poor user can figure out
			 * what's going on.
			 */
			if (mailto && status) {
				char            buf[MAX_TEMPSTR];

				sprintf(buf, "mailed %d byte%s of output but got status 0x%04x\n", bytes, (bytes == 1) ? "" : "s", status);
				log_it1(usernm, getpid(), "MAIL", buf, 0);
			}

		} /* if data from grandchild */
		close(stdout_pipe[READ_PIPE]); /* also closes stdout_pipe[READ_PIPE] */
	}

	/* wait for children to die. */
	for (; children > 0; children--)
		sigchld_reaper("grandchild");
}

static int
safe_p(const char *usernm, const char *s)
{
	static const char safe_delim[] = "@!:%-.,";	/* conservative! */
	const char     *t;
	int             ch, first;

	for (t = s, first = 1; (ch = *t++) != '\0'; first = 0) {
		if (isascii(ch) && isprint(ch) && (isalnum(ch) || (!first && strchr(safe_delim, ch))))
			continue;
		log_it1(usernm, getpid(), "UNSAFE", s, 0);
		return (FALSE);
	}
	return (TRUE);
}

void
getversion_do_command_c()
{
	const char *x = rcsid;
	x++;
}

/*-
 * $Log: do_command.c,v $
 * Revision 1.1  2024-06-12 20:27:21+05:30  Cprogrammer
 * Initial revision
 *
 */
