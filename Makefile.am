@SET_MAKE@

COFLAGS=-z+05:30
ACLOCAL_AMFLAGS=-I m4

edit = sed \
	-e 's}@PACKAGE\@}$(PACKAGE)}g' \
	-e 's}@PACKAGE_VERSION\@}$(PACKAGE_VERSION)}g' \
	-e 's}@version\@}$(VERSION)}g' \
	-e "s|@release\@|"`cat conf-release`"|g" \
	-e 's|@email\@|'"`cat conf-email`"'|g' \
	-e 's|@date\@|'"`date -R`"'|g' \
	-e 's}@pkgconfigdir\@}$(pkgconfigdir)}g' \
	-e 's}@VERSION\@}$(VERSION)}g' \
	-e 's}@localstatedir\@}$(localstatedir)}g' \
	-e 's}@prefix\@}$(prefix)}g'

DEFS=@DEFS@ -fno-strict-aliasing -I/usr/include/qmail -I.

INSTALL_PROGRAM = ${INSTALL}          -c
install_sh_PROGRAM = $(install_sh)    -c
INSTALL_SCRIPT = ${INSTALL}           -c
install_sh_SCRIPT = $(install_sh)     -c
INSTALL_STRIP_PROGRAM = $(install_sh) -c $(STRIP_OPT)

man_MANS = svcron.8 svcrontab.1 svcrontab.5
doc_list = AUTHORS ChangeLog LICENSE

svcronbindir=@prefix@/bin
svcronsbindir=@prefix@/sbin

svcronbin_PROGRAMS = svcrontab
svcronsbin_PROGRAMS = svcron

svcron_SOURCES = svcron.c
svcron_LDADD = database.lo user.lo entry.lo job.lo do_command.lo \
			misc.lo env.lo popen.lo pw_dup.lo $(LIB_QMAIL)

svcrontab_SOURCES = svcrontab.c
svcrontab_LDADD = misc.lo entry.lo env.lo pw_dup.lo $(LIB_QMAIL)

svcron.spec: svcron.spec.in catChangeLog doc/ChangeLog conf-version conf-release conf-email
	(cat $@.in;./catChangeLog) | $(edit) > $@
svcron.changes: doc/ChangeLog conf-version conf-release conf-email
	./catChangeLog --changes doc/ChangeLog > $@
debian/changelog: doc/ChangeLog conf-version conf-release conf-email
	./catChangeLog --debian --name=svcron --state=unreleased \
		--urgency=low doc/ChangeLog > $@

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/indimail || exit 1
	$(mkinstalldirs) $(DESTDIR)$(datarootdir)/doc/svcron || exit 1
	for f in $(doc_list); do \
		$(INSTALL_DATA) $$f $(DESTDIR)$(datarootdir)/doc/svcron/$$f || exit 1 ;\
	done
	$(INSTALL_DATA) crontab $(DESTDIR)$(sysconfdir)/indimail/crontab || exit 1
