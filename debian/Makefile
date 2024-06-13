prefix=$(shell head -1 ../conf-prefix)
version=$(shell cat ../conf-version)
release=$(shell cat ../conf-release)
email=$(shell cat ../conf-email)

all: debian.tar.gz
clean:
	/bin/rm -f debian.dsc changelog \
	copyright control debian.tar.gz

debian.tar.gz: copyright rules changelog compat control \
svcron.install svcron.docs README.Debian source/format
	cd .. && tar cf - debian/copyright debian/rules debian/changelog \
	debian/compat debian/svcron.install debian/control \
	debian/svcron.docs debian/README.Debian debian/source/format | gzip > debian/$@
