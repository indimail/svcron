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
static char     rcsid[] = "$Id: database.c,v 1.7 2004/01/23 18:56:42 vixie Exp $";
#endif

/*
 * vix 26jan87 [RCS has the log]
 */

#include <strerr.h>
#include <stralloc.h>
#include "cron.h"

#define FATAL "sched: fatal: "
#define WARN  "sched: warn: "

#define TMAX(a,b) (is_greater_than(a,b)?(a):(b))
#define TEQUAL(a,b) (a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec)

static bool
is_greater_than(struct timespec left, struct timespec right)
{
	if (left.tv_sec > right.tv_sec)
		return TRUE;
	else
	if (left.tv_sec < right.tv_sec)
		return FALSE;
	return left.tv_nsec > right.tv_nsec;
}

static void
process_crontab(const char *uname, const char *fname, const char *tabname,
		struct stat *statbuf, cron_db *new_db, cron_db *old_db)
{
	struct passwd  *pw = NULL;
	int             crontab_fd = OK - 1;
	user           *u;

	if (fname == NULL)	 /*- must be set to something for logging purposes. */
		fname = "*system*";
	else
	if ((pw = getpwnam(uname)) == NULL) { /*- file doesn't have a user in passwd file. */
		log_it1(fname, getpid(), "ORPHAN", "no passwd entry", 0);
		goto next_crontab;
	}

	if ((crontab_fd = open(tabname, O_RDONLY | O_NONBLOCK | O_NOFOLLOW, 0)) < OK) { /*- crontab not accessible? */
		log_it1(fname, getpid(), "CAN'T OPEN", tabname, errno);
		goto next_crontab;
	}

	if (fstat(crontab_fd, statbuf) < OK) {
		log_it1(fname, getpid(), "FSTAT FAILED", tabname, errno);
		goto next_crontab;
	}
	if (!S_ISREG(statbuf->st_mode)) {
		log_it1(fname, getpid(), "NOT REGULAR", tabname, 0);
		goto next_crontab;
	}
	if ((statbuf->st_mode & 07777) != 0600) {
		log_it1(fname, getpid(), "BAD FILE MODE", tabname, 0);
		goto next_crontab;
	}
	if (statbuf->st_uid != ROOT_UID && (pw == NULL || statbuf->st_uid != pw->pw_uid || strcmp(uname, pw->pw_name) != 0)) {
		log_it1(fname, getpid(), "WRONG FILE OWNER", tabname, 0);
		goto next_crontab;
	}
	if (statbuf->st_nlink != 1) {
		log_it1(fname, getpid(), "BAD LINK COUNT", tabname, 0);
		goto next_crontab;
	}

	u = find_user(old_db, fname);
	if (u != NULL) {
		/*
		 * if crontab has not changed since we last read it
		 * in, then we can just use our existing entry.
		 */
		if (TEQUAL(u->mtim, statbuf->st_mtim)) {
			unlink_user(old_db, u);
			link_user(new_db, u);
			goto next_crontab;
		}

		/*
		 * before we fall through to the code that will reload
		 * the user, let's deallocate and unlink the user in
		 * the old database. This is more a point of memory
		 * efficiency than anything else, since all leftover
		 * users will be deleted from the old database when
		 * we finish with the crontab...
		 */
		unlink_user(old_db, u);
		free_user(u);
		log_it1(fname, getpid(), "RELOAD", tabname, 0);
	}
	u = load_user(crontab_fd, pw, fname);
	if (u != NULL) {
		u->mtim = statbuf->st_mtim;
		link_user(new_db, u);
	}

next_crontab:
	if (crontab_fd >= OK) {
		close(crontab_fd);
	}
}

void
load_database(cron_db *old_db, char *dbdir)
{
	struct stat     spool_stat, syscron_stat, crond_stat;
	cron_db         new_db;
	struct dirent  *dp;
	DIR            *dir;
	user           *u, *nu;
	char           *spool_dir;
	static stralloc tabname = {0}, fname = {0};
	time_t          now;

	now = time(NULL);
	/*-
	 * before we start loading any data, do a stat on spool_dir
	 * so that if anything changes as of this moment (i.e., before we've
	 * cached any of the database), we'll see the changes next time.
	 */
	spool_dir = dbdir ? dbdir : SPOOL_DIR;
	if (stat(spool_dir, &spool_stat) < OK)
		strerr_die4sys(111, FATAL, "stat: ", spool_dir, ": ");

#ifdef SYSCRONTAB
	/*- track system crontab file */
	if (dbdir || stat(SYSCRONTAB, &syscron_stat) < OK)
		syscron_stat.st_mtim = ts_zero;
#else
	syscron_stat.st_mtim = ts_zero;
#endif

#ifdef SYS_CROND_DIR
	if (dbdir || stat(SYS_CROND_DIR, &crond_stat) < OK)
		crond_stat.st_mtim = ts_zero;
#else
	crond_stat.st_mtim = ts_zero;
#endif

	/*
	 * if spooldir's mtime has not changed, we don't need to fiddle with
	 * the database.
	 *
	 * Note that old_db->mtime is initialized to 0 in main(), and
	 * so is guaranteed to be different than the stat() mtime the first
	 * time this function is called.
	 */
	if (TEQUAL(old_db->mtim, TMAX(crond_stat.st_mtim, TMAX(spool_stat.st_mtim, syscron_stat.st_mtim))))
		return;

	/*
	 * something's different. make a new database, moving unchanged
	 * elements from the old database, reloading elements that have
	 * actually changed. Whatever is left in the old database when
	 * we're done is chaff -- crontabs that disappeared.
	 */
	new_db.mtim = TMAX(spool_stat.st_mtim, syscron_stat.st_mtim);
	new_db.head = new_db.tail = NULL;

	if (!dbdir && !TEQUAL(syscron_stat.st_mtim, ts_zero))
		process_crontab("root", NULL, SYSCRONTAB, &syscron_stat, &new_db, old_db);

	/*
	 * we used to keep this dir open all the time, for the sake of
	 * efficiency. however, we need to close it in every fork, and
	 * we fork a lot more often than the mtime of the dir changes.
	 */
	if (!(dir = opendir(spool_dir)))
		strerr_die4sys(111, FATAL, "unable to opendir ", spool_dir, ": ");

	while (NULL != (dp = readdir(dir))) {
		/*
		 * avoid file names beginning with ".". this is good
		 * because we would otherwise waste two guaranteed calls
		 * to getpwnam() for . and .., and also because user names
		 * starting with a period are just too nasty to consider.
		 */
		if (dp->d_name[0] == '.')
			continue;
		if (!stralloc_copys(&fname, dp->d_name) ||
				!stralloc_0(&fname))
			die_nomem(FATAL);
		if (!stralloc_copys(&tabname, spool_dir) ||
				!stralloc_append(&tabname, "/") ||
				!stralloc_cat(&tabname, &fname))
			die_nomem(FATAL);
		process_crontab(fname.s, fname.s, tabname.s, &crond_stat, &new_db, old_db);
	}
	closedir(dir);

#ifdef SYS_CROND_DIR
	if (!(dir = opendir(SYS_CROND_DIR)))
		strerr_die4sys(111, FATAL, "unable to opendir ", SYS_CROND_DIR, ": ");
	while (NULL != (dp = readdir(dir))) {
		if (dp->d_name[0] == '.')
			continue;
		if (!stralloc_copys(&fname, dp->d_name) ||
				!stralloc_0(&fname))
			die_nomem(FATAL);
		if (!stralloc_copys(&tabname, spool_dir) ||
				!stralloc_append(&tabname, "/") ||
				!stralloc_cat(&tabname, &fname))
			die_nomem(FATAL);
		process_crontab(fname.s, fname.s, tabname.s, &spool_stat, &new_db, old_db);
	}
	closedir(dir);
#endif

	/*
	 * if we don't do this, then when our children eventually call
	 * getpwnam() in do_command.c's child_process to verify MAILTO=,
	 * they will screw us up (and v-v).
	 */
	endpwent();

	/*- whatever's left in the old database is now junk. */
	for (u = old_db->head; u != NULL; u = nu) {
		nu = u->next;
		unlink_user(old_db, u);
		free_user(u);
	}

	/*- overwrite the database control block with the new one. */
	*old_db = new_db;
}

void
link_user(cron_db *db, user *u)
{
	if (db->head == NULL)
		db->head = u;
	if (db->tail)
		db->tail->next = u;
	u->prev = db->tail;
	u->next = NULL;
	db->tail = u;
}

void
unlink_user(cron_db *db, user *u)
{
	if (u->prev == NULL)
		db->head = u->next;
	else
		u->prev->next = u->next;

	if (u->next == NULL)
		db->tail = u->prev;
	else
		u->next->prev = u->prev;
}

user           *
find_user(cron_db *db, const char *name)
{
	user           *u;

	for (u = db->head; u != NULL; u = u->next) {
		if (strcmp(u->name, name) == 0)
			break;
	}
	return (u);
}

void
getversion_database_c()
{
	const char     *x = rcsid;
	x++;
}
