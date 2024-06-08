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

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: misc.c,v 1.16 2004/01/23 18:56:43 vixie Exp $";
#endif

/*
 * vix 26jan87 [RCS has the rest of the log]
 * vix 30dec86 [written]
 */

#include <limits.h>
#include <subfd.h>
#include <strerr.h>
#include <error.h>
#include <qprintf.h>
#include <fmt.h>
#include <lock.h>
#include <qprintf.h>
#include <sys/ipc.h>
#include "cron.h"

#define FATAL "svcron: fatal: "
#define WARN  "svcron: warn: "

/*-
 * glue_strings is the overflow-safe equivalent of
 * sprintf(buffer, "%s%c%s", a, separator, b);
 *
 * returns 1 on success, 0 on failure. 'buffer' MUST NOT be used if
 * glue_strings fails.
 */
int
glue_strings(char *buffer, size_t buffer_size, const char *a, const char *b, char separator)
{
	char           *buf;
	char           *buf_end;

	if (buffer_size <= 0)
		return (0);
	buf_end = buffer + buffer_size;
	buf = buffer;

	for ( /* nothing */ ; buf < buf_end && *a != '\0'; buf++, a++)
		*buf = *a;
	if (buf == buf_end)
		return (0);
	if (separator != '/' || buf == buffer || buf[-1] != '/')
		*buf++ = separator;
	if (buf == buf_end)
		return (0);
	for ( /* nothing */ ; buf < buf_end && *b != '\0'; buf++, b++)
		*buf = *b;
	if (buf == buf_end)
		return (0);
	*buf = '\0';
	return (1);
}

int
strcmp_until(const char *left, const char *right, char until)
{
	while (*left && *left != until && *left == *right) {
		left++;
		right++;
	}

	if ((*left == '\0' || *left == until) && (*right == '\0' || *right == until)) {
		return (0);
	}
	return (*left - *right);
}

/*
 * strdtb(s) - delete trailing blanks in string 's' and return new length
 */
int
strdtb(char *s)
{
	char           *x = s;

	/*- scan forward to the null */
	while (*x)
		x++;

	/*-
	 * scan backward to either the first character before the string,
	 * or the last non-blank in the string, whichever comes first.
	 */
	do {
		x--;
	}
	while (x >= s && isspace((unsigned char) *x));

	/*-
	 * one character beyond where we stopped above is where the null
	 * goes.
	 */
	*++x = '\0';

	/*-
	 * the difference between the position of the null character and
	 * the position of the first character of the string is the length.
	 */
	return (x - s);
}

int
strcountstr(const char *s, const char *t)
{
	const char     *u;
	int             ret = 0;

	while ((u = strstr(s, t)) != NULL) {
		s = u + strlen(t);
		ret++;
	}
	return (ret);
}

void
set_cron_uid(void)
{
#if defined(BSD) || defined(POSIX)
	if (seteuid(ROOT_UID) < OK)
		strerr_die2sys(111, FATAL, "seteuid: ");
#else
	if (setuid(ROOT_UID) < OK)
		strerr_die2sys(111, FATAL, "setuid: ");
#endif
}

void
set_cron_cwd(const char *ident)
{
	struct stat     sb;
#ifdef CRON_GROUP
	struct group   *grp = NULL;
#endif

#ifdef CRON_GROUP
	errno = 0;
	grp = getgrnam(CRON_GROUP);
	if (!grp && errno)
		strerr_die4sys(111, ident, "failed to get group entry for ", CRON_GROUP, ": ");
#endif
	/*- first check for CRONDIR ("/etc/indimail/cron" or some such) */
	if (stat(CRONDIR, &sb) < OK && errno == ENOENT) {
		strerr_warn4(WARN, "unable to stat ", CRONDIR, ": ", &strerr_sys);
		if (OK == mkdir(CRONDIR, 0710)) {
			subprintf(subfderr, "created directory %s\n", CRONDIR);
			substdio_flush(subfderr);
			if (stat(CRONDIR, &sb) == -1)
				strerr_die4sys(111, ident, "unable to stat ", CRONDIR, ": ");
		} else
			strerr_die3sys(111, ident, CRONDIR, ": ");
	}
	if (!S_ISDIR(sb.st_mode))
		strerr_die3x(111, ident, CRONDIR, ": not a directory, bailing out");
	if (chdir(CRONDIR) < OK) /*- /etc/indimail/cron */
		strerr_die4sys(111, ident, "unable to switch to ", CRONDIR, ": ");

	/*
	 * CRONDIR okay (now==CWD), now look at SPOOL_DIR ("crontabs" or some such) 
	 */
	if (stat(SPOOL_DIR, &sb) < OK && errno == ENOENT) {
		strerr_warn6(WARN, "directory ", CRONDIR, "/", SPOOL_DIR, ": ", &strerr_sys);
		if (OK == mkdir(SPOOL_DIR, 0700)) {
			subprintf(subfderr, "created directory %s\n", SPOOL_DIR);
			substdio_flush(subfderr);
			if (stat(SPOOL_DIR, &sb) == -1)
				strerr_die6sys(111, ident, "unable to stat ", CRONDIR, "/", SPOOL_DIR, ": ");
		} else
			strerr_die6sys(111, ident, "mkdir ", CRONDIR, "/", SPOOL_DIR, ": ");
	}
	if (!S_ISDIR(sb.st_mode))
		strerr_die5x(111, ident, CRONDIR, "/", SPOOL_DIR, ": not a directory, bailing out");
#ifdef CRON_GROUP
	if (sb.st_gid != grp->gr_gid && chown(SPOOL_DIR, -1, grp->gr_gid) == -1)
		strerr_die8sys(111, ident, "chgrp ", CRON_GROUP, " ", CRONDIR, "/", SPOOL_DIR, ": ");
#endif
	if (sb.st_mode != 01730) {
		if (chmod(SPOOL_DIR, 01730) == -1)
			strerr_warn6(ident, "chmod 01730 ", CRONDIR, "/", SPOOL_DIR, ": ", &strerr_sys);
	}
}

/*
 * get_lock() - write our PID into /var/run/svcron.pid, unless
 * another daemon is already running, which we detect here.
 *
 * note: main() calls us twice; once before forking, once after.
 * we maintain static storage of the file pointer so that we
 * can rewrite our PID into _PATH_SCHED_PID after the fork.
 */
int
get_lock(char **pidfile, const char *sdir, const char *dbdir)
{
	static int      fd = -1;
	char           *run_dir;
	char            strnum[FMT_ULONG], buf[7];
	pid_t           pid;
	int             n, fdsource;
	key_t           dir;
	static stralloc d = { 0 };

	if (!access(run_dir = "/run", F_OK)) {
		if (!dbdir)
			*pidfile = "/run/svcron/"PIDFILE;
		if (access("/run/svcron", F_OK) && mkdir("/run/svcron", 01777) == -1)
			strerr_die2sys(111, FATAL, "unable to mkdir /run/svcron: ");
		run_dir = "/run";
	} else
	if (!access(run_dir = "/var/run", F_OK)) {
		if (!dbdir)
			*pidfile = "/var/run/svcron/"PIDFILE;
		if (access("/var/run/svcron", F_OK) && mkdir("/var/run/svcron", 01777) == -1)
			strerr_die2sys(111, FATAL, "unable to mkdir /var/run/svcron: ");
	} else {
		run_dir = "/tmp";
		if (!dbdir)
			*pidfile = "/tmp"PIDFILE;
	}
	if (dbdir) {
		if (!(dir = ftok(dbdir, 1)))
			strerr_die4sys(111, FATAL, "unable to get ftok for ", dbdir, ": ");
		if (!qsprintf(&d, "%s/svcron/%x.pid", run_dir, dir))
			die_nomem(FATAL);
		*pidfile = d.s;
	}

	if ((fd = open(*pidfile, O_CREAT|O_WRONLY|O_EXCL, 0644)) >= 0) {
		pid = getpid();
		strnum[n = fmt_ulong(strnum, pid)] = 0;
		if (write(fd, (char *) &pid, sizeof(pid_t)) == -1 ||
				write(fd, "\n", 1) == -1 || write(fd, strnum, n) == -1 ||
				write(fd, "\n", 1) == -1)
			strerr_die2sys(111, FATAL, "unable to write pid to lock file: ");
		close(fd);
		return (0);
	}
	if (errno != error_exist)
		strerr_die4sys(111, FATAL, "unable to obtain lock for ", *pidfile, ": ");
	if ((fd = open(*pidfile, O_RDONLY, 0644)) == -1)
		strerr_die4sys(111, FATAL, "unable to read ", *pidfile, ": ");
	if (read(fd, (char *) &pid, sizeof(pid)) == -1)
		strerr_die2sys(111, FATAL, "unable to get pid from lock: ");
	close(fd);
	if (pid == getpid()) /*- we again got the same pid */
		return (0);
	errno = 0;
	if (pid == -1 || (kill(pid, 0) == -1 && errno == error_srch)) { /*- process does not exist */
		if (unlink(*pidfile) == -1)
			strerr_die2sys(111, FATAL, "unable to delete lock: ");
		return (1);
	}
	strnum[fmt_ulong(strnum, pid)] = 0;
	if (errno)
		strerr_die4sys(111, FATAL, "unable to get status of pid ", strnum, ": ");

	if ((fdsource = open(".", O_RDONLY|O_NDELAY, 0)) == -1)
		strerr_die2sys(111, FATAL, "unable to open current directory: ");
	/*-
	 * let us find out if the process is svscan we will
	 * use the /proc filesystem to figure out command name
	 */
	if (chdir("/proc") == -1) /*- on systems without /proc filesystem, give up */
		strerr_die2sys(111, FATAL, "chdir: /proc: ");
	if (chdir(strnum) == -1) { /*- process is now dead */
		if (fchdir(fdsource) == -1)
			strerr_die4sys(111, FATAL, "unable to switch back to ", sdir, ": ");
		close(fdsource);
		if (unlink(*pidfile) == -1)
			strerr_die2sys(111, FATAL, "unable to delete lock: ");
		return (1);
	}
	/*- open 'comm' to get the command name */
	if ((fd = open("comm", O_RDONLY, 0)) == -1) { /*- process is now dead */
		if (fchdir(fdsource) == -1)
			strerr_die4sys(111, FATAL, "unable to switch back to ", sdir, ": ");
		close(fdsource);
		if (unlink(*pidfile) == -1)
			strerr_die2sys(111, FATAL, "unable to delete lock: ");
		return (1);
	}
	if ((n = read(fd, buf, 7)) != 6) { /*- non-svcron process is running with this pid */
		close(fd);
		if (fchdir(fdsource) == -1)
			strerr_die4sys(111, FATAL, "unable to switch back to ", sdir, ": ");
		close(fdsource);
		if (unlink(*pidfile) == -1)
			strerr_die2sys(111, FATAL, "unable to delete lock: ");
		return (1);
	}
	close(fd);
	if (buf[0] == 's' && buf[1] == 'c' && buf[2] == 'h' &&
		buf[3] == 'e' && buf[4] == 'd' && buf[5] == '\n') { /*- indeed pid is svcron process */
		buf[5] = 0;
		strerr_warn5(FATAL, "[", buf, "] ", "already running", 0);
		_exit (111);
	}
	/*- some non-svscan process is running with pid */
	if (fchdir(fdsource) == -1)
		strerr_die4sys(111, FATAL, "unable to switch back to ", sdir, ": ");
	close(fdsource);
	if (unlink(*pidfile) == -1)
		strerr_die2sys(111, FATAL, "unable to delete lock: ");
	return (1);
}

/*
 * get_char(file) : like getc() but increment LineNumber on newlines 
 */
int
get_char(FILE *file)
{
	int             ch;

	ch = getc(file);
	if (ch == '\n')
		Set_LineNum(LineNumber + 1)
	return (ch);
}

/*
 * unget_char(ch, file) : like ungetc but do LineNumber processing 
 */
void
unget_char(int ch, FILE *file)
{
	ungetc(ch, file);
	if (ch == '\n')
		Set_LineNum(LineNumber - 1)
}

/*
 * get_string(str, max, file, termstr) : like fgets() but
 *  (1) has terminator string which should include \n
 *  (2) will always leave room for the null
 *  (3) uses get_char() so LineNumber will be accurate
 *  (4) returns EOF or terminating character, whichever
 */
int
get_string(char *string, int size, FILE *file, char *terms)
{
	int             ch;

	while (EOF != (ch = get_char(file)) && !strchr(terms, ch)) {
		if (size > 1) {
			*string++ = (char) ch;
			size--;
		}
	}

	if (size > 0)
		*string = '\0';

	return (ch);
}

/*
 * skip_comments(file) : read past comment (if any)
 */
void
skip_comments(FILE *file)
{
	int             ch;

	while (EOF != (ch = get_char(file))) {
		/*- ch is now the first character of a line. */

		while (ch == ' ' || ch == '\t')
			ch = get_char(file);

		if (ch == EOF)
			break;

		/*- ch is now the first non-blank character of a line. */

		if (ch != '\n' && ch != '#')
			break;

		/*-
		 * ch must be a newline or comment as first non-blank
		 * character on a line.
		 */

		while (ch != '\n' && ch != EOF)
			ch = get_char(file);

		/*-
		 * ch is now the newline of a line which we're going to
		 * ignore.
		 */
	}
	if (ch != EOF)
		unget_char(ch, file);
}

/*
 * int in_file(const char *string, FILE *file, int error)
 * return TRUE if one of the lines in file matches string exactly,
 * FALSE if no lines match, and error on error.
 */
static int
in_file(const char *string, FILE *file, int error)
{
	char            line[MAX_TEMPSTR];
	char           *endp;

	if (fseek(file, 0L, SEEK_SET))
		return (error);
	while (fgets(line, MAX_TEMPSTR, file)) {
		if (line[0] != '\0') {
			endp = &line[strlen(line) - 1];
			if (*endp != '\n')
				return (error);
			*endp = '\0';
			if (0 == strcmp(line, string))
				return (TRUE);
		}
	}
	if (ferror(file))
		return (error);
	return (FALSE);
}

/*
 * int allowed(const char *username, const char *allow_file, const char *deny_file)
 * returns TRUE if (allow_file exists and user is listed)
 * or (deny_file exists and user is NOT listed).
 * root is always allowed.
 */
int
allowed(const char *username, const char *allow_file, const char *deny_file)
{
	FILE           *fp;
	int             isallowed;

	if (strcmp(username, ROOT_USER) == 0)
		return (TRUE);
	isallowed = FALSE;
	if ((fp = fopen(allow_file, "r")) != NULL) {
		isallowed = in_file(username, fp, FALSE);
		fclose(fp);
	} else
	if ((fp = fopen(deny_file, "r")) != NULL) {
		isallowed = !in_file(username, fp, FALSE);
		fclose(fp);
	}
	return (isallowed);
}

void
log_it1(const char *username, pid_t xpid, const char *event, const char *detail, int error)
{
	if (error) {
		if (subprintf(subfderr, "%s:            pid %10d: user %s %s[%s]: %s\n",
				ProgramName, xpid, username, event, detail, strerror(error)) == -1)
			strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
	} else
	if (subprintf(subfderr, "%s:            pid %10d: user %s %s[%s]\n",
			ProgramName, xpid, username, event, detail) == -1)
		strerr_die2sys(111, FATAL, "unable to write to descriptor 2: ");
	substdio_flush(subfderr);
}

static int LogFD = ERR;

void
log_it2(const char *username, pid_t xpid, const char *event, const char *detail)
{
	pid_t           pid = xpid;
	static stralloc msg = {0};
	time_t          now = time((time_t) 0);
	struct          tm *t = localtime(&now);

	if (LogFD < OK) {
		LogFD = open(LOG_FILE, O_CREAT|O_WRONLY|O_APPEND, 0600);
		if (LogFD < OK)
			strerr_die4sys(111, WARN, "can't open log file ", LOG_FILE, ": ");
		else
			(void) fcntl(LogFD, F_SETFD, 1);
		strerr_warn2(WARN, "waiting for lock", 0);
		if (lock_ex(LogFD) == -1)
			strerr_die4sys(111, FATAL, "unable to lock ", LOG_FILE, ": ");
	}

	/*-
	 * we have to sprintf() it because fprintf() doesn't always write
	 * everything out in one chunk and this has to be atomically appended
	 * to the log file.
	 */
	qsprintf(&msg, "%s (%02d-%02d-%04d-%02d:%02d:%02d-%d) %s (%s)\n",
			username, t->tm_mday, t->tm_mon+1, t->tm_year + 1900,
			t->tm_hour, t->tm_min, t->tm_sec, pid, event, detail);

	/*- we have to run strlen() because sprintf() returns (char*) on old BSD */
	if (LogFD < OK || write(LogFD, msg.s, msg.len) < OK) {
		strerr_warn4(WARN, "can't write to log file ", LOG_FILE, ": ", &strerr_sys);
		substdio_put(subfderr, msg.s, msg.len);
		substdio_flush(subfderr);
	}
}

/*
 * char *first_word(char *s, char *t)
 * return pointer to first word
 * parameters:
 *   s - string we want the first word of
 *   t - terminators, implicitly including \0
 * warnings:
 *  1. this routine is fairly slow
 *  2. it returns a pointer to static storage
 */
char           *
first_word(char *s, char *t)
{
	static char     retbuf[2][MAX_TEMPSTR + 1];	/* sure wish C had GC */
	static int      retsel = 0;
	char           *rb, *rp;

	/*- select a return buffer */
	retsel = 1 - retsel;
	rb = &retbuf[retsel][0];
	rp = rb;

	/*- skip any leading terminators */
	while (*s && (NULL != strchr(t, *s))) {
		s++;
	}

	/*- copy until next terminator or full buffer */
	while (*s && (NULL == strchr(t, *s)) && (rp < &rb[MAX_TEMPSTR])) {
		*rp++ = *s++;
	}

	/*- finish the return-string and return it */
	*rp = '\0';
	return (rb);
}

/*
 * warning:
 * heavily ascii-dependent.
 */
static void
mkprint(char *dst, unsigned char *src, int len)
{
	/*-
	 * XXX
	 * We know this routine can't overflow the dst buffer because mkprints()
	 * allocated enough space for the worst case.
	 */
	while (len-- > 0) {
		unsigned char   ch = *src++;

		if (ch < ' ') {			/* control character */
			*dst++ = '^';
			*dst++ = ch + '@';
		} else
		if (ch < 0177)	/* printable */
			*dst++ = ch;
		else
		if (ch == 0177) {	/* delete/rubout */
			*dst++ = '^';
			*dst++ = '?';
		} else {				/* parity character */
			sprintf(dst, "\\%03o", ch);
			dst += 4;
		}
	}
	*dst = '\0';
}

/*
 * warning:
 * returns a pointer to malloc'd storage, you must call free yourself.
 */
char           *
mkprints(unsigned char *src, unsigned int len)
{
	char           *dst = malloc(len * 4 + 1);

	if (dst)
		mkprint(dst, src, len);

	return (dst);
}

#ifdef MAIL_DATE
/*
 * Sat, 27 Feb 1993 11:44:51 -0800 (CST)
 * 1234567890123456789012345678901234567
 */
char           *
arpadate(clock)
	time_t         *clock;
{
	time_t          t = clock ? *clock : time((time_t) 0);
	struct tm       tm = *localtime(&t);
	long            gmtoff = get_gmtoff(&t, &tm);
	int             hours = gmtoff / SECONDS_PER_HOUR;
	int             minutes = (gmtoff - (hours * SECONDS_PER_HOUR)) / SECONDS_PER_MINUTE;
	static char     ret[64];	/* zone name might be >3 chars */

	(void) sprintf(ret, "%s, %2d %s %2d %02d:%02d:%02d %.2d%.2d (%s)",
			DowNames[tm.tm_wday], tm.tm_mday, MonthNames[tm.tm_mon],
			tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, hours,
			minutes, TZONE(*tm));
	return (ret);
}
#endif /*MAIL_DATE */

#ifdef HAVE_SAVED_UIDS
static uid_t save_euid;
static gid_t save_egid;

int swap_uids(void)
{
	save_egid = getegid();
	save_euid = geteuid();
	return ((setegid(getgid()) == -1 || seteuid(getuid()) == -1)? -1 : 0);
}

int swap_uids_back(void)
{
	return ((setegid(save_egid) == -1 || seteuid(save_euid) == -1) ? -1 : 0);
}

#else /*HAVE_SAVED_UIDS */
int swap_uids(void)
{
	return ((setregid(getegid(), getgid()) == -1
				|| setreuid(geteuid(), getuid()) == -1) ? -1 : 0);
}

int swap_uids_back(void)
{
	return (swap_uids());
}
#endif /*HAVE_SAVED_UIDS */

size_t
strlens(const char *last, ...)
{
	va_list         ap;
	size_t          ret = 0;
	const char     *str;

	va_start(ap, last);
	for (str = last; str != NULL; str = va_arg(ap, const char *)) ret += strlen(str);
	va_end(ap);
	return (ret);
}

void
die_nomem(char *arg)
{
	strerr_die2x(111, arg, ": out of memory");
}

/*
 * Return the offset from GMT in seconds (algorithm taken from sendmail).
 *
 * warning:
 *  clobbers the static storage space used by localtime() and gmtime().
 *  If the local pointer is non-NULL it *must* point to a local copy.
 */
#ifndef HAVE_TM_GMTOFF
long
get_gmtoff(time_t *clock, struct tm *local)
{
	struct tm       gmt;
	long            offset;

	gmt = *gmtime(clock);
	if (local == NULL)
		local = localtime(clock);

	offset = (local->tm_sec - gmt.tm_sec) + ((local->tm_min - gmt.tm_min) * 60) + ((local->tm_hour - gmt.tm_hour) * 3600);

	/*- Timezone may cause year rollover to happen on a different day. */
	if (local->tm_year < gmt.tm_year)
		offset -= 24 * 3600;
	else
	if (local->tm_year > gmt.tm_year)
		offset -= 24 * 3600;
	else
	if (local->tm_yday < gmt.tm_yday)
		offset -= 24 * 3600;
	else
	if (local->tm_yday > gmt.tm_yday)
		offset += 24 * 3600;

	return (offset);
}
#endif /*- HAVE_TM_GMTOFF */

void
getversion_misc_c()
{
	const char     *x = rcsid;
	x++;
}
