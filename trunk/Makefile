CC = gcc
TARGET = google-contact
LIBPURPLE_CFLAGS = -I/usr/include/libpurple -I/usr/local/include/libpurple
GLIB_CFLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/include 


all:   ${TARGET}.so 

install: ${TARGET}.so
	cp ${TARGET}.so ~/.purple/plugins/

clean:
	rm -f ${TARGET}.so ${TARGET}.dll

${TARGET}.so: ${TARGET}.c
	${CC} ${LIBPURPLE_CFLAGS} -Wall ${GLIB_CFLAGS} -I. -g -O2 ${TARGET}.c -o ${TARGET}.so -shared -fPIC -DPIC -ggdb

dll:
	make -f Makefile.mingw
