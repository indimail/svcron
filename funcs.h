/*
 * $Id: funcs.h,v 1.2 2024-06-23 23:49:46+05:30 Cprogrammer Exp mbhangui $
 */

/*
 * Copyright (c) 2021 by Paul Vixie ("VIXIE")
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

/* Notes:
 *	This file has to be included by svcron.h after data structure defs.
 *	We should reorg this into sections by module.
 */

#ifndef _FUNCS_H
#define _FUNCS_H

void		set_cron_uid(void),
		set_cron_cwd(const char *),
		load_database(cron_db *, char *),
		open_logfile(void),
		sigpipe_func(void),
		job_add(entry *, const user *),
		do_command(entry *, const user *),
		link_user(cron_db *, user *),
		unlink_user(cron_db *, user *),
		free_user(user *),
		myenv_free(char **),
		unget_char(int, FILE *),
		free_entry(entry *),
		skip_comments(FILE *),
		log_it1(const char *, int, const char *, const char *, int),
		log_it2(const char *, int, const char *, const char *),
		log_close(void),
		die_nomem(char *);
void            sigchld_reaper(char *, const entry *);

int		job_runqueue(void),
		get_char(FILE *),
		get_string(char *, int, FILE *, char *),
		swap_uids(void),
		swap_uids_back(void),
		load_env(char **, FILE *),
		svcron_pclose(FILE *),
		glue_strings(char *, size_t, const char *, const char *, char),
		strcmp_until(const char *, const char *, char),
		allowed(const char *, const char *, const char *),
		strdtb(char *),
		get_lock(char **, const char *, const char *),
		strcountstr(const char *, const char *);

size_t		strlens(const char *, ...);

char		*myenv_get(char *, char **),
		*arpadate(time_t *),
		*mkprints(unsigned char *, unsigned int),
		*first_word(char *, char *),
		**myenv_init(void),
		**myenv_copy(char **),
		**myenv_set(char **, char *);

user		*load_user(int, struct passwd *, const char *),
		*find_user(cron_db *, const char *);

entry		*load_entry(FILE *, void (*)(const char *),
			    struct passwd *, char **);

FILE		*svcron_popen(char *, char *, struct passwd *, pid_t *);

struct passwd	*pw_dup(const struct passwd *);

#ifndef HAVE_TM_GMTOFF
long		get_gmtoff(time_t *, struct tm *);
#endif

#endif
