prefix=/usr
version=$(shell cat ../conf-version)
release=$(shell cat ../conf-release)
email="$(shell cat ../conf-email)"
date=`date -R`

edit = sed \
	-e 's|@sysconfdir\@|$(sysconfdir)|g' \
	-e 's|@version\@|$(version)|g' \
	-e 's|@release\@|$(release)|g' \
	-e 's|@email\@|$(email)|g' \
	-e "s|@date\@|$(date)|g" \
	-e 's|@prefix\@|$(prefix)|g'

all: debian.tar.gz README.Debian control copyright \
	svcron.dsc
clean:
	/bin/rm -f debian.dsc changelog \
	README.debian copyright control debian.tar.gz

README.Debian: ../conf-email README.Debian.in
	(cat $@.in) | $(edit) > $@
control: ../conf-version ../conf-release ../conf-email control.in
	(cat $@.in) | $(edit) > $@
copyright: ../conf-email copyright.in
	(cat $@.in) | $(edit) > $@
svcron.dsc: ../conf-version ../conf-release ../conf-email svcron.dsc.in
	(cat $@.in) | $(edit) > $@

debian.tar.gz: copyright rules changelog compat control \
svcron.install svcron.docs README.Debian source/format
	cd .. && tar cf - debian/copyright debian/rules debian/changelog \
	debian/compat debian/svcron.install debian/control \
	debian/svcron.docs debian/README.Debian debian/source/format | gzip > debian/$@
