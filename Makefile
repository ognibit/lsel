# lsel - line selector
# See LICENSE file for copyright and license details.

include config.mk

SRC = lsel.c
OBJ = ${SRC:.c=.o}

all: options lsel

options:
	@echo lsel build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

lsel: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f lsel ${OBJ} lsel-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p lsel-${VERSION}
	@cp -R LICENSE Makefile README.md lsel.1 config.mk \
		${SRC} config.def.h lsel-${VERSION}
	@tar -cf lsel-${VERSION}.tar lsel-${VERSION}
	@gzip lsel-${VERSION}.tar
	@rm -rf lsel-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f lsel ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/lsel
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" <lsel.1 >${DESTDIR}${MANPREFIX}/man1/lsel.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/lsel.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/lsel
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/lsel.1

.PHONY: all options clean dist install uninstall
