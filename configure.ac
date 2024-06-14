#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([svcron],m4_normalize(m4_include([conf-version])),m4_normalize(m4_include([conf-email])),svcron, [https://github.com/indimail/svcron])
AC_CONFIG_SRCDIR([svcron.c])
AC_CONFIG_HEADERS([config.h])
LT_INIT
AM_INIT_AUTOMAKE([foreign silent-rules no-dist])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

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

case "$host" in
*-*-sunos4.1.1*)
	CPPFLAGS="$CPPFLAGS -DSUNOS4"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
*-*-solaris*)
	CPPFLAGS="$CPPFLAGS -DSOLARIS"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
*-*-linux*)
	CPPFLAGS="$CPPFLAGS -DLINUX"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -DLINUX -O4 -Wno-delete-non-virtual-dtor -Wno-reorder -Wall -fPIC"
	LDFLAGS="$LDFLAGS -pie"
	;;
*-*-freebsd*)
	CPPFLAGS="$CPPFLAGS -DFREEBSD -I/usr/local/include -I/usr/local/include/qmail"
	CFLAGS="$CFLAGS -I/usr/local/include -I/usr/local/include/qmail -Wall"
	CXXFLAGS="$CXXFLAGS -DFREEBSD -I/usr/local/include -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-virtual-dtor -Wno-deprecated-register -Wno-reorder"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-abstract-non-virtual-dtor"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	;;
*-*-darwin*)
	CPPFLAGS="$CPPFLAGS -DDARWIN -I/opt/local/include"
	CFLAGS="$CFLAGS -I/opt/local/include -I/usr/local/include -I/opt/local/include/qmail -Wall"
	CXXFLAGS="$CXXFLAGS -DDARWIN -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -DBIND_8_COMPAT -I/opt/local/include -Wno-c++11-extensions"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-virtual-dtor -Wno-reorder-ctor -Wno-reorder"
	LDFLAGS="$LDFLAGS -L/opt/local/lib -L/usr/local/lib"
	;;
*)
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
esac

# Checks for libraries.
AC_CHECK_LIB(qmail, substdio_fdbuf, [AC_SUBST([LIB_QMAIL], ["-lqmail"]) AC_DEFINE([HAVE_QMAIL], [1],[qmail Library])],nqmail=t,)
if test " $noqmail" = " t"
then
  AC_MSG_ERROR(Could not find qmail library.)
fi

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

sendmail="/usr/bin/sendmail"
AC_ARG_ENABLE(sendmail, [  --enable-sendmail=file     sendmail path.],
	sendmail="$enableval",
	[
		if test "$sendmail" = ""
		then
			AC_MSG_ERROR([sendmail path set, specify --enable-sendmail.])
		fi
	])
AC_DEFINE_UNQUOTED(MAILARG,"$sendmail",[This will be path to sendmail.])

AC_DEFINE(MAILFMT,"%s -FCronDaemon -odi -oem -oi -t",
[-Fx   = Set full-name of sender
-odi  = Option Deliverymode Interactive
-oem  = Option Errors Mailedtosender
-oi   = Ignore "." alone on a line
-t    = Get recipient from headers])

AC_SUBST(crondir)
AC_SUBST(syscrontab)
AC_SUBST(syscrondir)
AC_SUBST(spooldir)
AC_SUBST(sendmail)

AC_CONFIG_FILES([svcron.8 svcrontab.1 crontab.5 Makefile])
AC_OUTPUT
