#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([svcron],1.0,[Manvendra Bhangui manvendra@indimail.org],svcron, [http://www.indimail.org])
AC_CONFIG_SRCDIR([svcron.c])
AC_CONFIG_HEADERS([config.h])
LT_INIT
AM_INIT_AUTOMAKE([foreign silent-rules no-dist])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB(qmail, substdio_fdbuf, [AC_SUBST([LIB_QMAIL], ["-lqmail"]) AC_DEFINE([HAVE_QMAIL], [1],[qmail Library])],nqmail=t,)
if test " $noqmail" = " t"
then
  AC_MSG_ERROR(Could not find qmail library.)
fi

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h paths.h sys/file.h sys/param.h sys/time.h syslog.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero chown fchown dup2 endpwent ftruncate gethostname isascii mkdir putenv setlocale strcasecmp strchr strdup strerror strstr strtol utime])

crondir="${localstatedir}/spool/cron"
AC_ARG_ENABLE(crondir, [  --enable-crondir=dir         directory where cron directories are present.],
	crondir="$enableval",
	[
		if test "$crondir" = ""
		then
			AC_MSG_ERROR([cron directory not set, specify --enable-crondir.])
		fi
	])
AC_DEFINE_UNQUOTED(CRONDIR,"$crondir", cron's base directory)

spooldir="crontabs"
AC_ARG_ENABLE(spooldir, [  --enable-spooldir=dir    user's crontab sub directory.],
	spooldir="$enableval",
	[
		if test "$spooldir" = ""
		then
			AC_MSG_ERROR([user's crontab sub directory not set, specify --enable-spooldir.])
		fi
	])
AC_DEFINE_UNQUOTED(SPOOL_DIR, "$spooldir", user crontab sub directory)

syscrondir="${sysconfdir}/cron.d"
AC_ARG_ENABLE(syscrondir, [  --enable-syscrondir=dir     system crontab directory.],
	syscrondir="$enableval",
	[
		if test "$syscrondir" = ""
		then
			AC_MSG_ERROR([system crontab directory not set, specify --enable-syscrondir.])
		fi
	])
AC_DEFINE_UNQUOTED(SYS_CROND_DIR,"$syscrondir", system crontab directory)

syscrontab="${sysconfdir}/crontab"
AC_ARG_ENABLE(syscrontab, [  --enable-syscrontab=file     system crontab file.],
	syscrontab="$enableval",
	[
		if test "$syscrontab" = ""
		then
			AC_MSG_ERROR([system crontab not set, specify --enable-syscrontab.])
		fi
	])
AC_DEFINE_UNQUOTED(SYSCRONTAB,"$syscrontab", system crontab file)

AC_DEFINE(MAILARG,"/usr/bin/sendmail",[There will be path to sendmail.])

AC_DEFINE(MAILFMT,"%s -FCronDaemon -odi -oem -oi -t",
[-Fx   = Set full-name of sender
-odi  = Option Deliverymode Interactive
-oem  = Option Errors Mailedtosender
-oi   = Ignore "." alone on a line
-t    = Get recipient from headers])

dnl CRONIE_VAR_DEFAULT (VAR, DESCRIPTION, DEFAULT)
dnl --------------------------------------------
AC_DEFUN([CRONIE_CONF_VAR],
[AC_ARG_VAR([$1], [$2 @<:@$3@:>@])
if test "$$1" = ""; then
	$1='$3'
fi
])

AC_DEFUN([ANACRON_CONF_VAR],
[AC_ARG_VAR([$1], [$2 @<:@$3@:>@])
if test "$$1" = ""; then
	    $1='$3'
		fi
])

CRONIE_CONF_VAR([CRONDIR], [the directory where all the cron directories reside], [${localstatedir}/spool/cron])
CRONIE_CONF_VAR([SYSCRONTAB], [the current working directory of the running daemon], [${sysconfdir}/crontab])
CRONIE_CONF_VAR([SYS_CROND_DIR], [the current working directory of the running daemon], [${sysconfdir}/cron.d])
CRONIE_CONF_VAR([SPOOL_DIR], [the directory where all the user cron tabs reside], [${localstatedir}/spool/cron/crontabs])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
