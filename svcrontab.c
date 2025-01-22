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

/*
 * crontab - install and manage per-user crontab files
 */

#define	MAIN_PROGRAM

#include <stralloc.h>
#include <qprintf.h>
#include <subfd.h>
#include <strerr.h>
#include <fmt.h>
#include <setuserid.h>
#include <getln.h>
#include "cron.h"

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: svcrontab.c,v 1.2 2025-01-22 17:57:44+05:30 Cprogrammer Exp mbhangui $";
#endif

#define FATAL "svcrontab: fatal: "
#define WARN  "svcrontab: warn: "

#define NHEADER_LINES 3

enum opt_t { opt_unknown, opt_list, opt_delete, opt_edit, opt_replace, opt_service };

static char    *getoptargs = "u:lerd:";

static pid_t    Pid;
static char    *User, *spool_dir = SPOOL_DIR;
stralloc        realuser = {0};
stralloc        Filename = {0}, TempFilename = {0}, line = {0};
static FILE    *NewCrontab;
static int      CheckErrorCount;
static enum opt_t Option;
static struct passwd *pw;
stralloc        n = {0}, q = {0};

static void     list_cmd(void);
static void     delete_cmd(void);
static void     edit_cmd(void);
static void     poke_daemon(void);
static void     check_error(const char *);
static void     parse_args(int c, char *v[], char **);
static void     die(int);
static int      replace_cmd(void);

static void
usage(const char *msg)
{
	subprintf(subfderr, "%s: usage error: %s\n", ProgramName, msg);
	subprintf(subfderr, "usage:\t%s [-u user] file\n", ProgramName);
	subprintf(subfderr, "\t%s [-u user] [ -e | -l | -r ]\n", ProgramName);
	subprintf(subfderr, "\t\t(default operation is replace, per 1003.2)\n");
	subprintf(subfderr, "\t-e\t(edit user's crontab)\n");
	subprintf(subfderr, "\t-l\t(list user's crontab)\n");
	subprintf(subfderr, "\t-r\t(delete user's crontab)\n");
	substdio_flush(subfderr);
	_exit(100);
}

int
main(int argc, char *argv[])
{
	int             exitstatus;
	char           *svdir = NULL;

	Pid = getpid();
	ProgramName = "svcrontab";

	setlocale(LC_ALL, "");

	parse_args(argc, argv, &svdir);		/* sets many globals, opens a file */
	if (!svdir) {
		set_cron_cwd(FATAL);
		if (!allowed(realuser.s, CRON_ALLOW, CRON_DENY)) {
			subprintf(subfderr, "You (%s) are not allowed to use this program (%s)\n", User, ProgramName);
			subprintf(subfderr, "See crontab(1) for more information\n");
			log_it2(realuser.s, Pid, "AUTH", "crontab command not allowed");
			substdio_flush(subfderr);
			_exit(111);
		}
	}
	if (svdir && chdir(svdir) == -1)
		strerr_die4sys(111, FATAL, "chdir: ", svdir, ": ");
	exitstatus = OK_EXIT;
	switch (Option)
	{
	case opt_unknown:
		exitstatus = ERROR_EXIT;
		break;
	case opt_list:
		list_cmd();
		break;
	case opt_delete:
		delete_cmd();
		break;
	case opt_edit:
		edit_cmd();
		break;
	case opt_replace:
		if (replace_cmd() < 0)
			exitstatus = ERROR_EXIT;
		break;
	default:
		abort();
	}
	_exit(exitstatus);
	/* NOTREACHED*/
}

static void
parse_args(int argc, char *argv[], char **svdir)
{
	int             argch;
	struct stat     sb;

	*svdir = NULL;
	if (!(pw = getpwuid(getuid()))) {
		subprintf(subfderr, "%s: your UID isn't in the passwd file.\n", ProgramName);
		subprintf(subfderr, "bailing out.\n");
		substdio_flush(subfderr);
		_exit(100);
	}
	User = pw->pw_name;
	if (!stralloc_copys(&realuser, User) ||
			!stralloc_0(&realuser))
		die_nomem(FATAL);
	Filename.len = 0;
	Option = opt_unknown;
	while (-1 != (argch = getopt(argc, argv, getoptargs))) {
		switch (argch)
		{
		case 'd': /*- this overrides set_cron_cwd */
			*svdir = optarg;
			spool_dir = optarg;
			if (getuid() != geteuid()) { /* running as setuid */
				if (setgid(MY_GID(pw)) < 0)
					strerr_die3sys(111, "setgid: ", pw->pw_name, ": ");
				else
				if (setuid(MY_UID(pw)) < 0)
					strerr_die3sys(111, "setuid: ", pw->pw_name, ": ");
			}
			break;
		case 'u':
			if (MY_UID(pw) != ROOT_UID) {
				subprintf(subfderr, "must be privileged to use -u\n");
				substdio_flush(subfderr);
				_exit(100);
			}
			if (!(pw = getpwnam(optarg))) {
				subprintf(subfderr, "%s: user `%s' unknown\n", ProgramName, optarg);
				substdio_flush(subfderr);
				_exit(100);
			}
			User = optarg;
			break;
		case 'l':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_list;
			break;
		case 'r':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_delete;
			break;
		case 'e':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_edit;
			break;
		default:
			usage("unrecognized option");
		}
	}

	endpwent();

	if (Option != opt_unknown) {
		if (argv[optind] != NULL)
			usage("no arguments permitted after this option");
	} else {
		if (argv[optind] != NULL) {
			Option = opt_replace;
			if (!stralloc_copys(&Filename, argv[optind]) ||
					!stralloc_0(&Filename))
				die_nomem(FATAL);
		} else
			usage("file name must be specified for replace");
	}

	if (Option == opt_replace) {
		/*
		 * we have to open the file here because we're going to
		 * chdir(2) into /etc/indimail/cron before we get around to
		 * reading the file.
		 */
		if (!strcmp(Filename.s, "-"))
			NewCrontab = stdin;
		else {
			/*
			 * relinquish the setuid status of the binary during
			 * the open, lest nonroot users read files they should
			 * not be able to read. we can't use access() here
			 * since there's a race condition. thanks go out to
			 * Arnt Gulbrandsen <agulbra@pvv.unit.no> for spotting
			 * the race.
			 */

			if (swap_uids() < OK)
				strerr_die2sys(111, FATAL, "set uid/gid: ");
			if (!(NewCrontab = fopen(Filename.s, "r")))
				strerr_die3sys(111, "open ", Filename.s, ": ");
			if (fstat(fileno(NewCrontab), &sb) < 0)
				strerr_die3sys(111, "unable to stat ", Filename.s, ": ");
			if ((sb.st_mode & S_IFMT) == S_IFDIR)
				strerr_die4sys(100, FATAL, "invalid crontab file: ", Filename.s, "is a directory");
			if (swap_uids_back() < OK)
				strerr_die2sys(111, FATAL, "restore uid/gid: ");
		}
	}
}

void
myglue_string(stralloc *s, char *s1, char *sep, char *s2)
{
	if (!stralloc_copys(s, s1) ||
			!stralloc_append(s, sep) ||
			!stralloc_cats(s, s2) ||
			!stralloc_0(s))
		die_nomem(FATAL);
	return;
}

static void
list_cmd(void)
{
	FILE           *f;
	int             ch;

	log_it2(realuser.s, Pid, "LIST", User);
	myglue_string(&n, spool_dir, "/", User);
	if (!(f = fopen(n.s, "r"))) {
		if (errno == ENOENT) {
			subprintf(subfderr, "no crontab for %s\n", User);
			substdio_flush(subfderr);
		} else
			strerr_die4sys(111, FATAL, "error opening file ", n.s, ": ");
	}

	/*- file is open. copy to stdout, close. */
	Set_LineNum(1)
	while (EOF != (ch = get_char(f)))
		putchar(ch);
	fclose(f);
}

static void
delete_cmd(void)
{
	log_it2(realuser.s, Pid, "DELETE", User);
	myglue_string(&n, spool_dir, "/", User);
	if (unlink(n.s) == -1) {
		if (errno == ENOENT) {
			subprintf(subfderr, "no crontab for %s\n", User);
			substdio_flush(subfderr);
		} else
			strerr_die4sys(111, FATAL, "error opening file ", n.s, ": ");
	}
	poke_daemon();
}

static void
check_error(const char *msg)
{
	CheckErrorCount++;
	subprintf(subfderr, "\"%s\":%d: %s\n", Filename.s, LineNumber - 1, msg);
	substdio_flush(subfderr);
}

static void
edit_cmd(void)
{
	char           *editor;
	FILE           *f;
	int             ch, t, x, waiter, match;
	struct stat     statbuf;
	struct utimbuf  utimebuf;
	pid_t           pid, xpid;
	char            strnum1[FMT_ULONG], strnum2[FMT_ULONG];

	log_it2(realuser.s, Pid, "BEGIN EDIT", User);
	myglue_string(&n, spool_dir, "/", User);
	if (!(f = fopen(n.s, "r"))) {
		if (errno != ENOENT)
			strerr_die4sys(111, FATAL, "error opening file ", n.s, ": ");
		subprintf(subfderr, "no crontab for %s - using an empty one\n", User);
		substdio_flush(subfderr);
		if (!(f = fopen(_PATH_DEVNULL, "r")))
			strerr_die2sys(111, FATAL, "error opening /dev/null: ");
	}

	if (fstat(fileno(f), &statbuf) < 0)
		strerr_die4sys(111, FATAL, "stat: ", n.s, ": ");
	utimebuf.actime = statbuf.st_atime;
	utimebuf.modtime = statbuf.st_mtime;

	/*- Turn off signals. */
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);

	if (!stralloc_copys(&Filename, _PATH_TMP))
		die_nomem(FATAL);
	if (Filename.s[Filename.len - 1] == '/')
		Filename.len--;
	if (!stralloc_catb(&Filename, "/crontab.XXXXXXXXXX", 19) ||
			!stralloc_0(&Filename))
		die_nomem(FATAL);
	if (-1 == (t = mkstemp(Filename.s))) {
		strerr_warn4(WARN, "mkstemp: ", Filename.s, ": ", &strerr_sys);
		goto fatal;
	}
	if (!getuid() || getuid() != geteuid()) { /* running as root or setuid */
#ifdef HAVE_FCHOWN
		if (fchown(t, MY_UID(pw), MY_GID(pw)) < 0) {
			strnum1[fmt_ushort(strnum1, pw->pw_uid)] = 0;
			strnum2[fmt_ushort(strnum2, pw->pw_uid)] = 0;
			strerr_warn8(WARN, "fchown: ", strnum1, ":", strnum2, " ", Filename.s, ": ", &strerr_sys);
			goto fatal;
		}
#else
		if (chown(Filename.s, MY_UID(pw), MY_GID(pw)) < 0) {
			strnum1[fmt_short(strnum1, pw->pw_uid)] = 0;
			strnum2[fmt_short(strnum2, pw->pw_uid)] = 0;
			strerr_warn8(WARN, "chown: ", strnum1, ":", strnum2, " ", Filename.s, ": ", &strerr_sys);
			goto fatal;
		}
#endif
	}
	if (!(NewCrontab = fdopen(t, "r+"))) {
		strerr_warn2(WARN, "mkstemp: crontab.XXXXXXXXXX: ", &strerr_sys);
		goto fatal;
	}

	Set_LineNum(1)
	/*- ignore the top few comments since we probably put them there. */
	x = 0;
	while (EOF != (ch = get_char(f))) {
		if ('#' != ch) {
			putc(ch, NewCrontab);
			break;
		}
		while (EOF != (ch = get_char(f)))
			if (ch == '\n')
				break;
		if (++x >= NHEADER_LINES)
			break;
	}

	/*- copy the rest of the crontab (if any) to the temp file. */
	if (EOF != ch)
		while (EOF != (ch = get_char(f)))
			putc(ch, NewCrontab);
	fclose(f);
	if (fflush(NewCrontab) < OK)
		strerr_die4sys(111, WARN, "flush: ", Filename.s, ": ");
	if (utime(Filename.s, &utimebuf) == -1)
		strerr_die4sys(111, WARN, "utime: ", Filename.s, ": ");
again:
	rewind(NewCrontab);
	if (ferror(NewCrontab)) {
		subprintf(subfderr, "%s: error while writing new crontab to %s: %s\n", ProgramName, Filename.s, strerror(errno));
		substdio_flush(subfderr);
fatal:
		unlink(Filename.s);
		_exit(111);
	}

	if (((editor = getenv("VISUAL")) == NULL || *editor == '\0') && ((editor = getenv("EDITOR")) == NULL || *editor == '\0')) {
		editor = EDITOR;
	}

	/*
	 * we still have the file open. editors will generally rewrite the
	 * original file rather than renaming/unlinking it and starting a
	 * new one; even backup files are supposed to be made by copying
	 * rather than by renaming. if some editor does not support this,
	 * then don't use it. the security problems are more severe if we
	 * close and reopen the file around the edit.
	 */

	switch (pid = fork())
	{
	case -1:
		strerr_die2sys(111, FATAL, "unable to produce a child: ");
		goto fatal;
	case 0:
		/* child */
		if (setgid(MY_GID(pw)) < 0)
			strerr_die3sys(111, "setgid: ", pw->pw_name, ": ");
		if (setuid(MY_UID(pw)) < 0)
			strerr_die3sys(111, "setuid: ", pw->pw_name, ": ");
		if (chdir(_PATH_TMP) < 0)
			strerr_die3sys(111, "chdir: ", _PATH_TMP, ": ");
		myglue_string(&q, editor, " ", Filename.s);
		execlp(_PATH_BSHELL, _PATH_BSHELL, "-c", q.s, (char *) 0);
		strerr_die3sys(111, FATAL, q.s, ": ");
	 	/* NOTREACHED*/
	default:
		break;
	}

	/*- parent */
	for (;;) {
		xpid = waitpid(pid, &waiter, WUNTRACED);
		if (xpid == -1) {
			if (errno != EINTR) {
				subprintf(subfderr, "%s: waitpid() failed waiting for PID %ld from \"%s\": %s\n", ProgramName, (long) pid, editor,
						strerror(errno));
				substdio_flush(subfderr);
			}
		} else
		if (xpid != pid) {
			subprintf(subfderr, "%s: wrong PID (%ld != %ld) from \"%s\"\n", ProgramName, (long) xpid, (long) pid, editor);
			substdio_flush(subfderr);
			goto fatal;
		} else 
		if (WIFSTOPPED(waiter)) {
			kill(getpid(), WSTOPSIG(waiter));
		} else 
		if (WIFEXITED(waiter) && WEXITSTATUS(waiter)) {
			subprintf(subfderr, "%s: \"%s\" exited with status %d\n", ProgramName, editor, WEXITSTATUS(waiter));
			substdio_flush(subfderr);
			goto fatal;
		} else 
		if (WIFSIGNALED(waiter)) {
			subprintf(subfderr, "%s: \"%s\" killed; signal %d (%score dumped)\n", ProgramName, editor, WTERMSIG(waiter),
					WCOREDUMP(waiter) ? "" : "no ");
			substdio_flush(subfderr);
			goto fatal;
		} else
			break;
	}
	(void) signal(SIGHUP, SIG_DFL);
	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	if (fstat(t, &statbuf) < 0) {
		strerr_warn4(WARN, "unable to stat ", Filename.s, ": ", &strerr_sys);
		goto fatal;
	}
	if (utimebuf.modtime == statbuf.st_mtime) {
		strerr_warn2(WARN, "no changes made to crontab", 0);
		goto remove;
	}
	strerr_warn2(ProgramName, ": installing new crontab", 0);
	switch (replace_cmd())
	{
	case 0:
		break;
	case -1:
		for (;;) {
			printf("Do you want to retry the same edit? ");
			fflush(stdout);
			if (getln(subfdinsmall, &line, &match, '\n') == -1)
				strerr_die2sys(111, FATAL, "unable to read input: ");
			if (!line.len)
				strerr_die2sys(111, FATAL, "unable to read input: ");
			switch (line.s[0])
			{
			case 'y':
			case 'Y':
				goto again;
			case 'n':
			case 'N':
				goto abandon;
			default:
				substdio_puts(subfderr, "Enter Y or N - ");
				substdio_flush(subfderr);
			}
		}
		 /*- NOTREACHED */
	case -2:
abandon:
		strerr_warn3(WARN, "edits left in ", Filename.s, 0);
		goto done;
	default:
		strerr_warn2(WARN, "panic: bad switch() in replace_cmd()", 0);
		goto fatal;
	}
remove:
	unlink(Filename.s);
done:
	log_it2(realuser.s, Pid, "END EDIT", User);
}

/*
 * returns  0  on success
 * *       -1  on syntax error
 * *       -2  on install error
 */
static int
replace_cmd(void)
{
	FILE           *tmp;
	int             ch, eof, fd, error = 0;
	entry          *e;
	uid_t           file_uid;
	time_t          now = time(NULL);
	char            strnum[FMT_ULONG];
	char           *envstr;
	char          **envp = myenv_init();

	if (envp == NULL)
		die_nomem(FATAL);
	myglue_string(&TempFilename, spool_dir, "/", "tmp.XXXXXXXXXX");
	if ((fd = mkstemp(TempFilename.s)) == -1 || !(tmp = fdopen(fd, "w+"))) {
		strerr_warn4(WARN, "mkstemp: ", TempFilename.s, ": ", &strerr_sys);
		if (fd != -1) {
			close(fd);
			unlink(TempFilename.s);
		}
		TempFilename.len = 0;
		return (-2);
	}
	(void) signal(SIGHUP, die);
	(void) signal(SIGINT, die);
	(void) signal(SIGQUIT, die);

	/*
	 * write a signature at the top of the file.
	 *
	 * VERY IMPORTANT: make sure NHEADER_LINES agrees with this code.
	 */
	fprintf(tmp, "# DO NOT EDIT THIS FILE - edit the master and reinstall.\n");
	fprintf(tmp, "# (%s installed on %-24.24s)\n", Filename.s, ctime(&now));
	fprintf(tmp, "# (svcron version %s -- %s)\n", CRON_VERSION, rcsid);

	/*- copy the crontab to the tmp */
	rewind(NewCrontab);
	Set_LineNum(1)
	while (EOF != (ch = get_char(NewCrontab)))
		putc(ch, tmp);
	if (ftruncate(fileno(tmp), ftell(tmp)) == -1) {	/* XXX redundant with "w+"? */
		strerr_warn4(WARN, "error while truncating new crontab ", TempFilename.s, ": ", &strerr_sys);
	}
	fflush(tmp);
	rewind(tmp);

	if (ferror(tmp)) {
		strerr_warn4(WARN, "error while writing new crontab to ", TempFilename.s, ": ", &strerr_sys);
		fclose(tmp);
		error = -2;
		goto done;
	}

	/*- check the syntax of the file being installed. */

	/*
	 * BUG: was reporting errors after the EOF if there were any errors
	 * in the file proper -- kludged it by stopping after first error.
	 * vix 31mar87
	 */
	Set_LineNum(1 - NHEADER_LINES)
	CheckErrorCount = 0;
	eof = FALSE;
	while (!CheckErrorCount && !eof) {
		switch (load_env(&envstr, tmp))
		{
		case ERR:
			/*- check for data before the EOF */
			if (envstr && *envstr) {
				Set_LineNum(LineNumber + 1)
				check_error("premature EOF");
			}
			eof = TRUE;
			break;
		case FALSE:
			e = load_entry(tmp, check_error, pw, envp);
			if (e)
				free(e);
			break;
		case TRUE:
			break;
		}
	}

	if (CheckErrorCount != 0) {
		strerr_warn2(WARN, "errors in crontab file, can't install.", 0);
		fclose(tmp);
		error = -1;
		goto done;
	}

	file_uid = (getuid() == geteuid()) ? ROOT_UID : pw->pw_uid;
	if (!getuid() || getuid() != geteuid()) { /* running as root or setuid */
#ifdef HAVE_FCHOWN
		if (fchown(fileno(tmp), file_uid, -1) < OK) {
			strnum[fmt_ushort(strnum, file_uid)] = 0;
			strerr_warn4(WARN, "fchown ", strnum, ": ", &strerr_sys);
			fclose(tmp);
			error = -2;
			goto done;
		}
#else
		if (chown(TempFilename, file_uid, -1) < OK) {
			strnum[fmt_ushort(strnum, file_uid)] = 0;
			strerr_warn4(WARN, "chown ", strnum, ": ", &strerr_sys);
			fclose(tmp);
			error = -2;
			goto done;
		}
#endif
	}
	if (fclose(tmp) == EOF) {
		strerr_warn4(WARN, "unable to save ", TempFilename.s, ": ", &strerr_sys);
		error = -2;
		goto done;
	}

	myglue_string(&n, spool_dir, "/", User);
	if (rename(TempFilename.s, n.s)) {
		strerr_warn6(WARN, "error renaming ", ProgramName, " to ", TempFilename.s, ": ", &strerr_sys);
		error = -2;
		goto done;
	}
	TempFilename.len = 0;
	log_it2(realuser.s, Pid, "REPLACE", User);
	poke_daemon();
done:
	(void) signal(SIGHUP, SIG_DFL);
	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	if (TempFilename.len) {
		if (unlink(TempFilename.s) == -1)
			strerr_warn4(WARN, "unlink: ", TempFilename.s, ": ", &strerr_sys);
		TempFilename.len = 0;
	}
	return (error);
}

static void
poke_daemon(void)
{
	if (utime(spool_dir, NULL) < OK) {
		strerr_warn5(WARN, ProgramName, ": can't update mtime on ", spool_dir, ": ", &strerr_sys);
		return;
	}
}

static void
die(int x)
{
	if (TempFilename.len)
		if (unlink(TempFilename.s) == -1)
			strerr_warn4(WARN, "unlink: ", TempFilename.s, ": ", &strerr_sys);
	_exit(111);
}

void
getversion_crontab_c()
{
	const char     *x = rcsid;
	x++;
}

/*-
 * $Log: svcrontab.c,v $
 * Revision 1.2  2025-01-22 17:57:44+05:30  Cprogrammer
 * fix argument to subprintf
 *
 * Revision 1.1  2024-06-09 01:04:28+05:30  Cprogrammer
 * Initial revision
 *
 */
