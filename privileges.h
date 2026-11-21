/*
 * $Id: privileges.h,v 1.1 2026-11-21 08:00:55+05:30 Cprogrammer Exp mbhangui $
 */
#ifndef _PRIVILEGES_H_
#define _PRIVILEGES_H_

#ifdef __CYGWIN__
#define ROOT_UID      18 /* SYSTEM account UID under Cygwin */
#define ROOT_USER     "SYSTEM"
#define IS_PRIVILEGED_UID(u) ((u) == 18 || (u) == 544 || (u) == 0)
#else
#define ROOT_UID       0      /* don't change this, it really must be root */
#define ROOT_USER      "root" /* ditto */
#endif

#endif

/*
 * $Log: privileges.h,v $
 * Revision 1.1  2026-11-21 08:00:55+05:30  Cprogrammer
 * Initial revision
 *
 */
