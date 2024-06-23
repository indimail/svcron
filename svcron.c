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

#define	MAIN_PROGRAM
#define FATAL "svcron: fatal: "
#define WARN  "svcron: warn: "

#include <strerr.h>
#include <env.h>
#include <fmt.h>
#include <error.h>
#include <qprintf.h>
#include <subfd.h>
#include "cron.h"

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: svcron.c,v 1.4 2024-06-23 23:51:04+05:30 Cprogrammer Exp mbhangui $";
#endif

enum timejump { negative, small, medium, large };

static volatile sig_atomic_t got_sighup, got_sigchld;
static int      timeRunning, virtualTime, clockTime;
static long     GMToff;
static char    *dbdir = NULL, *pidfile = NULL;

static void     usage(void);
static void     run_reboot_jobs(cron_db *);
static void     find_jobs(int, cron_db *, int, int);
static void     set_time(int);
static void     cron_sleep(int);
static void     sigchld_handler(int);
static void     sighup_handler(int);
static void     quit(int);
static void     parse_args(int c, char *v[]);

static void
usage(void)
{
	strerr_die4x(100, FATAL, "usage: ", ProgramName, "[-n]\n");
}

int
main(int argc, char *argv[])
{
	struct sigaction sact = {0};
	cron_db        database;
	char            strnum[FMT_ULONG], dirbuf[256];
	char           *sdir;

	ProgramName = argv[0];

	if (!setlocale(LC_ALL, ""))
		strerr_die2sys(111, FATAL, "unable to set locale to LC_ALL: ");

	parse_args(argc, argv);

	if (sigemptyset(&sact.sa_mask) == -1)
		strerr_die2sys(111, FATAL, "sigemptyset");
	sact.sa_flags = 0;
#ifdef SA_RESTART
	sact.sa_flags |= SA_RESTART;
#endif
	sact.sa_handler = sigchld_handler;
	if (sigaction(SIGCHLD, &sact, NULL) == -1)
		strerr_die2sys(111, FATAL, "sigaction failed for SIGCHLD: ");
	sact.sa_handler = sighup_handler;
	if (sigaction(SIGHUP, &sact, NULL) == -1)
		strerr_die2sys(111, FATAL, "sigaction failed for SIGHUP: ");
	sact.sa_handler = quit;
	if (sigaction(SIGINT, &sact, NULL) == -1)
		strerr_die2sys(111, FATAL, "sigaction failed for SIGINT: ");
	if (sigaction(SIGTERM, &sact, NULL) == -1)
		strerr_die2sys(111, FATAL, "sigaction failed for SIGTERM: ");

	if (!((sdir = getcwd(dirbuf, 255))))
		strerr_die2sys(111, FATAL, "unable to get current working directory: ");
	if (getuid() != geteuid())
		set_cron_uid();
	if (!dbdir)
		set_cron_cwd(FATAL);
	else
	if (chdir(dbdir) == -1)
		strerr_warn4(WARN, "unable to switch to ", dbdir, ": ", &strerr_sys);
	while (get_lock(&pidfile, sdir, dbdir));

	if (!env_put2("PATH", _PATH_DEFPATH))
		die_nomem(FATAL);
	strnum[fmt_ulong(strnum, getpid())] = 0;
	strerr_warn6(ProgramName, ": pid ", strnum, " STARTUP ", CRON_VERSION, ": ", 0);
	database.head = NULL;
	database.tail = NULL;
#ifdef LINUX
	database.mtim = ts_zero;
#else
	database.mtime = ts_zero;
#endif
	load_database(&database, dbdir);
	set_time(TRUE);
	run_reboot_jobs(&database);
	timeRunning = virtualTime = clockTime;

	/*-
	 * Too many clocks, not enough time (Al. Einstein)
	 * These clocks are in minutes since the epoch, adjusted for timezone.
	 *
	 * virtualTime: is the time it *would* be if we woke up
	 * promptly and nobody ever changed the clock. It is
	 * monotonically increasing... unless a timejump happens.
	 * At the top of the loop, all jobs for 'virtualTime' have run.
	 *
	 * timeRunning: is the time we last awakened.
	 *
	 * clockTime: is the time when set_time was last called.
	 */
	while (TRUE) {
		int             timeDiff;
		enum timejump   wakeupKind;

		/* ... wait for the time (in minutes) to change ... */
		do {
			cron_sleep(timeRunning + 1);
			set_time(FALSE);
		} while (clockTime == timeRunning);
		timeRunning = clockTime;

		/*
		 * Calculate how the current time differs from our virtual
		 * clock. Classify the change into one of 4 cases.
		 */
		timeDiff = timeRunning - virtualTime;

		/* shortcut for the most common case */
		if (timeDiff == 1) {
			virtualTime = timeRunning;
			find_jobs(virtualTime, &database, TRUE, TRUE);
		} else {
			if (timeDiff > (3 * MINUTE_COUNT) || timeDiff < -(3 * MINUTE_COUNT))
				wakeupKind = large;
			else
			if (timeDiff > 5)
				wakeupKind = medium;
			else
			if (timeDiff > 0)
				wakeupKind = small;
			else
				wakeupKind = negative;

			switch (wakeupKind)
			{
			case small:
				/*
				 * case 1: timeDiff is a small positive number
				 * (wokeup late) run jobs for each virtual
				 * minute until caught up.
				 */
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database, TRUE, TRUE);
				} while (virtualTime < timeRunning);
				break;

			case medium:
				/*
				 * case 2: timeDiff is a medium-sized positive
				 * number, for example because we went to DST
				 * run wildcard jobs once, then run any
				 * fixed-time jobs that would otherwise be
				 * skipped if we use up our minute (possible,
				 * if there are a lot of jobs to run) go
				 * around the loop again so that wildcard jobs
				 * have a chance to run, and we do our
				 * housekeeping.
				 *
				 * run wildcard jobs for current minute
				 */
				find_jobs(timeRunning, &database, TRUE, FALSE);

				/*
				 * run fixed-time jobs for each minute missed 
				 */
				do {
					if (job_runqueue())
						sleep(10);
					virtualTime++;
					find_jobs(virtualTime, &database, FALSE, TRUE);
					set_time(FALSE);
				} while (virtualTime < timeRunning && clockTime == timeRunning);
				break;

			case negative:
				/*
				 * case 3: timeDiff is a small or medium-sized
				 * negative num, eg. because of DST ending.
				 * Just run the wildcard jobs. The fixed-time
				 * jobs probably have already run, and should
				 * not be repeated. Virtual time does not
				 * change until we are caught up.
				 */
				find_jobs(timeRunning, &database, TRUE, FALSE);
				break;
			default:
				/*
				 * other: time has changed a *lot*,
				 * jump virtual time, and run everything
				 */
				virtualTime = timeRunning;
				find_jobs(timeRunning, &database, TRUE, TRUE);
			}
		}

		/*- Jobs to be run (if any) are loaded; clear the queue. */
		job_runqueue();

		/*- Check to see if we received a signal while running jobs. */
		if (got_sighup) {
			got_sighup = 0;
		}
		if (got_sigchld) {
			got_sigchld = 0;
			sigchld_reaper("child", NULL);
		}
		load_database(&database, dbdir);
	}
}

static void
run_reboot_jobs(cron_db *db)
{
	user           *u;
	entry          *e;

	for (u = db->head; u != NULL; u = u->next) {
		for (e = u->crontab; e != NULL; e = e->next) {
			if (e->flags & WHEN_REBOOT)
				job_add(e, u);
		}
	}
	(void) job_runqueue();
}

static void
find_jobs(int vtime, cron_db *db, int doWild, int doNonWild)
{
	const time_t    virtualSecond = vtime * SECONDS_PER_MINUTE;
	const time_t    virtualTomorrow = virtualSecond + SECONDS_PER_DAY;
	entry          *e;
	const user     *u;
	struct tm       now = {0}, tom = {0};

	gmtime_r(&virtualSecond, &now);
	gmtime_r(&virtualTomorrow, &tom);
	/*- make 0-based values out of these so we can use them as indicies */
	const int       minute = now.tm_min - FIRST_MINUTE;
	const int       hour = now.tm_hour - FIRST_HOUR;
	const int       dom = now.tm_mday - FIRST_DOM;
	const int       month = now.tm_mon + 1 /* 0..11 -> 1..12 */  - FIRST_MONTH;
	const int       dow = now.tm_wday - FIRST_DOW;

	/*
	 * the dom/dow situation is odd. '* * 1,15 * Sun' will run on the
	 * first and fifteenth AND every Sunday; '* * * * Sun' will run *only*
	 * on Sundays; '* * 1,15 * *' will run *only* the 1st and 15th. this
	 * is why we keep 'e->dow_star' and 'e->dom_star'. yes, it's bizarre.
	 * like many bizarre things, it's the standard.
	 */
	const bool      is_lastdom = (tom.tm_mday == 1);
	for (u = db->head; u != NULL; u = u->next) {
		for (e = u->crontab; e != NULL; e = e->next) {
			bool            thisdom = bit_test(e->dom, dom) || (is_lastdom && (e->flags & DOM_LAST) != 0);
			bool            thisdow = bit_test(e->dow, dow);
			if (bit_test(e->minute, minute) && bit_test(e->hour, hour) &&
					bit_test(e->month, month) &&
					((e->flags & (DOM_STAR | DOW_STAR)) != 0 ? (thisdom && thisdow) : (thisdom || thisdow))) {
				if ((doNonWild && (e->flags & (MIN_STAR | HR_STAR)) == 0) || (doWild && (e->flags & (MIN_STAR | HR_STAR)) != 0))
					job_add(e, u);
			}
		}
	}
}

/*
 * Set StartTime and clockTime to the current time.
 * These are used for computing what time it really is right now.
 * Note that clockTime is a unix wallclock time converted to minutes.
 */
static void
set_time(int initialize)
{
	struct tm       tm;
	static int      isdst;

	StartTime = time(NULL);

	/*
	 * We adjust the time to GMT so we can catch DST changes. 
	 */
	tm = *localtime(&StartTime);
	if (initialize || tm.tm_isdst != isdst) {
		isdst = tm.tm_isdst;
		GMToff = get_gmtoff(&StartTime, &tm);
	}
	clockTime = (StartTime + GMToff) / (time_t) SECONDS_PER_MINUTE;
}

/*
 * Try to just hit the next minute.
 */
static void
cron_sleep(int target)
{
	time_t          t1, t2;
	int             seconds_to_wait;

	t1 = time(NULL) + GMToff;
	seconds_to_wait = (int) (target * SECONDS_PER_MINUTE - t1) + 1;
	while (seconds_to_wait > 0 && seconds_to_wait < 65) {
		sleep((unsigned int) seconds_to_wait);
		/*
		 * Check to see if we were interrupted by a signal.
		 * If so, service the signal(s) then continue sleeping
		 * where we left off.
		 */
		if (got_sighup)
			got_sighup = 0;
		if (got_sigchld) {
			got_sigchld = 0;
			sigchld_reaper("child", NULL);
		}
		t2 = time(NULL) + GMToff;
		seconds_to_wait -= (int) (t2 - t1);
		t1 = t2;
	}
}

static void
sighup_handler(int x)
{
	got_sighup = 1;
}

static void
sigchld_handler(int x)
{
	got_sigchld = 1;
}

static void
quit(int x)
{
	if (pidfile)
		(void) unlink(pidfile);
	_exit(0);
}

static void
parse_args(int argc, char *argv[])
{
	int             argch;

	while (-1 != (argch = getopt(argc, argv, "vM:d:"))) {
		switch (argch)
		{
		default:
			usage();
		case 'v':
			verbose = 1;
			break;
		case 'M':
			if (strlen(optarg) == 0)
				usage();
			Mailer = optarg;
			break;
		case 'd':
			dbdir = optarg;
			break;
		}
	}
}

void
getversion_svcron_c()
{
	const char *x = rcsid;
	x++;
}

/*-
 * $Log: svcron.c,v $
 * Revision 1.4  2024-06-23 23:51:04+05:30  Cprogrammer
 * added entry argument to sigchld_reaper function
 *
 * Revision 1.3  2024-06-14 08:20:57+05:30  Cprogrammer
 * declare variables outside for loop
 *
 * Revision 1.2  2024-06-12 23:58:33+05:30  Cprogrammer
 * darwin port
 *
 * Revision 1.1  2024-06-09 01:04:26+05:30  Cprogrammer
 * Initial revision
 *
 */
