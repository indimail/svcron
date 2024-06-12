/*
 * Copyright (c) 1993,1994,2021 by Paul Vixie ("VIXIE")
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND VIXIE DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL VIXIE BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*- $Id: pathnames.h,v 1.1 2024-06-12 20:38:53+05:30 Cprogrammer Exp mbhangui $ */

#ifndef _PATHNAMES_H_
#define _PATHNAMES_H_

#if (defined(BSD)) && (BSD >= 199103) || defined(__linux) || defined(AIX)
#include <paths.h>
#endif /*BSD*/

#ifndef CRONDIR
/*-
 * CRONDIR is where svcron(8) and svcrontab(1) both chdir
 * to; SPOOL_DIR, LOG_FILE are all relative to this directory.
 */
#define CRONDIR   "/var/spool/cron"
#endif

/* SPOOLDIR is where the crontabs live.
 * This directory will have its modtime updated
 * whenever crontab(1) changes a crontab; this is
 * the signal for svcron(8) to look at each individual
 * crontab file and reload those whose modtimes are
 * newer than they were last time around (or which
 * didn't exist last time around...)
 */
#ifndef SPOOL_DIR
#define SPOOL_DIR     "crontabs"
#endif

/*
 * svcron allow/deny file.  At least cron.deny must
 * exist for ordinary users to run crontab.
 */
#define CRON_ALLOW CRONDIR "/cron.allow"
#define CRON_DENY  CRONDIR "/cron.deny"

/* undefining this turns off logging to a file.  If
 * neither LOG_FILE or SYSLOG is defined, we don't log.
 * If both are defined, we log both ways.  Note that if
 * LOG_CRON is defined by <syslog.h>, LOG_FILE will not
 * be used.
 */
#define LOG_FILE ".log"

/*
 * where should the daemon stick its PID?
 * PIDDIR must end in '/'.
 */
#define PIDFILE          "crond.pid"

#ifndef SYS_CROND_DIR
#define SYS_CROND_DIR    "/etc/cron.d"
#endif

/* 4.3BSD-style crontab */
#ifndef SYSCRONTAB
#define SYSCRONTAB       "/etc/crontabs"
#endif

/*
 * what editor to use if no EDITOR or VISUAL
 * environment variable specified.
 */
#if defined(_PATH_VI)
#define EDITOR _PATH_VI
#else
#define EDITOR "/usr/bin/vi"
#endif

#ifndef _PATH_SENDMAIL
#define _PATH_SENDMAIL "/usr/bin/sendmail"
#endif

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH "/usr/bin:/bin"
#endif

#ifndef _PATH_TMP
#define _PATH_TMP "/tmp/"
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#endif /* _PATHNAMES_H_ */
