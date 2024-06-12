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

#include <stralloc.h>
#include "cron.h"

#if !defined(lint) && !defined(LINT)
static char     rcsid[] = "$Id: env.c,v 1.1 2024-06-09 01:04:17+05:30 Cprogrammer Exp mbhangui $";
#endif

char          **
myenv_init(void)
{
	char          **p = (char **) malloc(sizeof (char **));

	if (p != NULL)
		p[0] = NULL;
	return (p);
}

void
myenv_free(char **envp)
{
	char          **p;

	for (p = envp; *p != NULL; p++)
		free(*p);
	free(envp);
}

char          **
myenv_copy(char **envp)
{
	int             count, i, save_errno;
	char          **p;

	for (count = 0; envp[count] != NULL; count++);
	p = (char **) malloc((count + 1) * sizeof (char *));	/* 1 for the NULL */
	if (p != NULL) {
		for (i = 0; i < count; i++)
			if ((p[i] = strdup(envp[i])) == NULL) {
				save_errno = errno;
				while (--i >= 0)
					free(p[i]);
				free(p);
				errno = save_errno;
				return (NULL);
			}
		p[count] = NULL;
	}
	return (p);
}

char          **
myenv_set(char **envp, char *envstr)
{
	int             count, found;
	char          **p, *envtmp;

	/*-
	 * count the number of elements, including the null pointer;
	 * also set 'found' to -1 or index of entry if already in here.
	 */
	found = -1;
	for (count = 0; envp[count] != NULL; count++) {
		if (!strcmp_until(envp[count], envstr, '='))
			found = count;
	}
	count++;					/* for the NULL */

	if (found != -1) {
		/*-
		 * it exists already, so just free the existing setting,
		 * save our new one there, and return the existing array.
		 */
		if ((envtmp = strdup(envstr)) == NULL)
			return (NULL);
		free(envp[found]);
		envp[found] = envtmp;
		return (envp);
	}

	/*
	 * it doesn't exist yet, so resize the array, move null pointer over
	 * one, save our string over the old null pointer, and return resized
	 * array.
	 */
	if ((envtmp = strdup(envstr)) == NULL)
		return (NULL);
	p = (char **) realloc((void *) envp, (size_t) ((count + 1) * sizeof (char **)));
	if (p == NULL) {
		free(envtmp);
		return (NULL);
	}
	p[count] = p[count - 1];
	p[count - 1] = envtmp;
	return (p);
}

/*
 * The following states are used by load_env(), traversed in order: 
 */
enum env_state {
	NAMEI,						/* First char of NAME, may be quote */
	NAME,						/* Subsequent chars of NAME */
	EQ1,						/* After end of name, looking for '=' sign */
	EQ2,						/* After '=', skipping whitespace */
	VALUEI,						/* First char of VALUE, may be quote */
	VALUE,						/* Subsequent chars of VALUE */
	FINI,						/* All done, skipping trailing whitespace */
	ERROR,						/* Error */
};

/*
 * return   ERR = end of file
 *   FALSE = not an env setting (file was repositioned)
 *   TRUE = was an env setting
 */
int
load_env(char **envstr, FILE *f)
{
	long            filepos;
	int             fileline;
	enum env_state  state;
	char            name[MAX_ENVSTR], val[MAX_ENVSTR], tmp[MAX_ENVSTR];
	static stralloc env_t = { 0 };
	char            quotechar;
	char           *c, *str;

	*envstr = "\0";
	filepos = ftell(f);
	fileline = LineNumber;
	skip_comments(f);
	if (EOF == get_string(tmp, MAX_ENVSTR, f, "\n"))
		return (ERR);
	c = tmp;

	bzero(name, sizeof name);
	bzero(val, sizeof val);
	str = name;
	state = NAMEI;
	quotechar = '\0';
	while (state != ERROR && *c) {
		switch (state)
		{
		case NAMEI:
		case VALUEI:
			if (*c == '\'' || *c == '"')
				quotechar = *c++;
			state++;
			/*- FALLTHROUGH */
		case NAME:
		case VALUE:
			if (quotechar) {
				if (*c == quotechar) {
					state++;
					c++;
					break;
				}
				if (state == NAME && *c == '=') {
					state = ERROR;
					break;
				}
			} else {
				if (state == NAME) {
					if (isspace((unsigned char) *c)) {
						c++;
						state++;
						break;
					}
					if (*c == '=') {
						state++;
						break;
					}
				}
			}
			*str++ = *c++;
			break;

		case EQ1:
			if (*c == '=') {
				state++;
				str = val;
				quotechar = '\0';
			} else {
				if (!isspace((unsigned char) *c))
					state = ERROR;
			}
			c++;
			break;

		case EQ2:
		case FINI:
			if (isspace((unsigned char) *c))
				c++;
			else
				state++;
			break;

		default:
			abort();
		}
	}
	if (state != FINI && !(state == VALUE && !quotechar)) {
		fseek(f, filepos, 0);
		Set_LineNum(fileline)
		return (FALSE);
	}
	if (state == VALUE) {
		/*- End of unquoted value: trim trailing whitespace */
		c = val + strlen(val);
		while (c > val && isspace((unsigned char) c[-1]))
			*(--c) = '\0';
	}

	/*
	 * 2 fields from parser; looks like an env setting 
	 */

	if (!stralloc_copys(&env_t, name) ||
			!stralloc_append(&env_t, "=") ||
			!stralloc_cats(&env_t, val) ||
			!stralloc_0(&env_t))
		die_nomem("env: ");
	*envstr = env_t.s;
	return (TRUE);
}

char           *
myenv_get(char *name, char **envp)
{
	int             len = strlen(name);
	char           *p, *q;

	while ((p = *envp++) != NULL) {
		if (!(q = strchr(p, '=')))
			continue;
		if ((q - p) == len && !strncmp(p, name, len))
			return (q + 1);
	}
	return (NULL);
}

void
getversion_env_c()
{
	const char     *x = rcsid;
	x++;
}

/*-
 * $Log: env.c,v $
 * Revision 1.1  2024-06-09 01:04:17+05:30  Cprogrammer
 * Initial revision
 *
 */
