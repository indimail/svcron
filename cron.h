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
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL VIXIE BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* cron.h - header for vixie's cron
 *
 * $Id: cron.h,v 1.1 2024-06-09 01:05:09+05:30 Cprogrammer Exp mbhangui $
 *
 * vix 14nov88 [rest of log is in RCS]
 * vix 14jan87 [0 or 7 can be sunday; thanks, mwm@berkeley]
 * vix 30dec86 [written]
 */

#ifndef _CRON_H
#define _CRON_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "externs.h"
#include "pathnames.h"
#include "macros.h"
#include "structs.h"
#include "funcs.h"
#include "globals.h"

#define CRON_VERSION VERSION
#define BUFSIZE_OUT 512
#define BUFSIZE_IN  1024

/*
 * choose one of these mailer commands. some use
 * /bin/mail for speed; it makes biff bark but doesn't
 * do aliasing. sendmail does do aliasing but is
 * a hog for short messages. aliasing is not needed
 * if you make use of the MAILTO= feature in crontabs.
 * (hint: MAILTO= was added for this reason).
 *
 * #define MAILFMT        "%s -d %s" */ /* -d = undocumented but common flag: deliver locally?
 * #define MAILARG        "/bin/mail",mailto
 * #define MAILFMT        "%s -mlrxto %s"
 * #define MAILARG        "/usr/mmdf/bin/submit",mailto
 * #define MAILFMT        "%s -FCronDaemon -odi -oem -oi -t"
 * #define MAILARG        _PATH_SENDMAIL

 * -Fx	 = Set full-name of sender
 * -odi	 = Option Deliverymode Interactive
 * -oem	 = Option Errors Mailedtosender
 * -oi   = Ignore "." alone on a line
 * -t    = Get recipient from headers
 */
#ifndef MAILFMT
#define MAILFMT        "%s -FCronDaemon -odi -oem -oi -t"
#endif
#ifndef MAILARG
#define MAILARG        _PATH_SENDMAIL
#endif

#endif
