/*
 * Copyright (c) 1988,1990,1993,1994,2021 by Paul Vixie ("VIXIE")
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "cron.h"

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: user.c,v 1.2 2024-06-12 23:58:55+05:30 Cprogrammer Exp mbhangui $";
#endif

void
free_user(user *u)
{
	entry          *e, *ne;

	free(u->name);
	for (e = u->crontab; e != NULL; e = ne) {
		ne = e->next;
		free_entry(e);
	}
	free(u);
}

user           *
load_user(int crontab_fd, struct passwd *pw, const char *name)
{
	char           *envstr;
	FILE           *file;
	user           *u;
	entry          *e;
	int             status, save_errno;
	char          **envp, **tenvp;

	if (!(file = fdopen(crontab_fd, "r"))) {
		perror("fdopen on crontab_fd in load_user");
		return (NULL);
	}

	/*- file is open.  build user entry, then read the crontab file.  */
	if ((u = (user *) malloc(sizeof (user))) == NULL)
		return (NULL);
	if ((u->name = strdup(name)) == NULL) {
		save_errno = errno;
		free(u);
		errno = save_errno;
		return (NULL);
	}
	u->crontab = NULL;

	/*- init environment.  this will be copied/augmented for each entry.  */
	if ((envp = myenv_init()) == NULL) {
		save_errno = errno;
		free(u->name);
		free(u);
		errno = save_errno;
		return (NULL);
	}

	/*- load the crontab */
	while ((status = load_env(&envstr, file)) >= OK) {
		switch (status)
		{
		case ERR:
			free_user(u);
			u = NULL;
			goto done;
		case FALSE:
			e = load_entry(file, NULL, pw, envp);
			if (e) {
				e->next = u->crontab;
				u->crontab = e;
			}
			break;
		case TRUE:
			if ((tenvp = myenv_set(envp, envstr)) == NULL) {
				save_errno = errno;
				free_user(u);
				u = NULL;
				errno = save_errno;
				goto done;
			}
			envp = tenvp;
			break;
		}
	}

done:
	myenv_free(envp);
	fclose(file);
	return (u);
}

void
getversion_user_c()
{
	const char     *x = rcsid;
	x++;
}

/*-
 * $Log: user.c,v $
 * Revision 1.2  2024-06-12 23:58:55+05:30  Cprogrammer
 * removed redundant code
 *
 * Revision 1.1  2024-06-09 01:04:30+05:30  Cprogrammer
 * Initial revision
 *
 */
